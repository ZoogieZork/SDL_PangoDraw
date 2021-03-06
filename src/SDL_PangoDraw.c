/* vim: set noet ai sw=4 sts=4 ts=8: */
/*  SDL_PangoDraw.c -- A companion library to SDL for working with Pango.
    Copyright (C) 2004 NAKAMURA Ken'ichi
    Copyright (C) 2013 Michael Imamura

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
*/

/*!
    \mainpage

    \section intro Introduction

    Pango is a cross-platform text layout engine that provides strong
    multi-lingual support and simple HTML-like markup to style text.

    GNOME 2 and 3 both use Pango for text rendering, as does Firefox on Linux.
    SDL_PangoDraw provides an API to use Pango to render the text in your
    SDL application.

    \subsection api High-level API

    \note
    SDL_PangoDraw is based on the SDL_Pango library by NAKAMURA Ken'ichi but
    contains numerous fixes and API additions.
    The function names have been changed to allow the two libraries to
    coexist, but in general if you wish to port code from SDL_Pango to
    SDL_PangoDraw you can simply change "SDLPango_" to "SDLPangoDraw_".

    From the viewpoint of text rendering, the heart of SDL_PangoDraw is a
    high-level API. Unlike other text rendering APIs (like DrawText() in
    Windows) font and text must be specified separately.
    In SDL_PangoDraw, font specification is embedded in text like HTML:

    \code
    <span font_family="Courier New"><i>This is Courier New and italic.</i></span>
    \endcode

    Color, size, subscript/superscript, obliquing, weight, and other many
    features are also available in same way.

    \subsection i18n Internationalized Text

    Internationalized text is another key feature.
    All text passed to SDL_PangoDraw must be encoded in UTF-8.
    RTL script (Arabic and Hebrew) and complicated rendering (Arabic, Indic
    and Thai) are supported.

    \section get Getting Started

    \subsection getlatest Get latest sources

    Get latest sources from https://github.com/ZoogieZork/SDL_PangoDraw

    \subsection install Build and install from source

    In Un*x, installation consists of:

    \code
    ./configure
    make
    make install
    \endcode

    To see all available configure options:

    \code
    ./configure --help
    \endcode

    \subsection inc Includes

    To use SDL_PangoDraw functions in a C/C++ source code file, you must use
    the SDL_PangoDraw.h include file:

    \code
    #include <SDL_PangoDraw.h>
    \endcode

    \subsection comp Compiling

    Both SDL and SDL_PangoDraw install pkg-config files to make compiling
    and linking simple.

    Simple Example for compiling an object file:

    \code
    cc -c `pkg-config --cflags sdl SDL_PangoDraw` mysource.c
    \endcode

    Simple Example for linking an object file:

    \code
    cc -o myprogram mysource.o `pkg-config --libs sdl SDL_PangoDraw`
    \endcode

    \section devel Development

    \subsection font Font Handling

    In Un*x, font handling depends on fontconfig of your system.

    \subsection example Step-by-step Example

    The operation of SDL_PangoDraw is done via context.

    \code
    SDLPangoDraw_Context *context = SDLPangoDraw_CreateContext();
    \endcode

    Specify default colors and minimum surface size.

    \code
    SDLPangoDraw_SetDefaultColor(context, MATRIX_TRANSPARENT_BACK_WHITE_LETTER);
    SDLPangoDraw_SetMinimumSize(context, 640, 0);
    \endcode

    Set markup text.

    \code
    SDLPangoDraw_SetMarkup(context, "This is <i>markup</i> text.", -1);
    \endcode

    Now you can get the size of surface.

    \code
    int w = SDLPangoDraw_GetLayoutWidth(context);
    int h = SDLPangoDraw_GetLayoutHeight(context);
    \endcode

    Create surface to draw.

    \code
    int margin_x = 10;
    int margin_y = 10;
    SDL_Surface *surface = SDL_CreateRGBSurface(SDL_SWSURFACE,
	w + margin_x * 2, h + margin_y * 2,
	32, (Uint32)(255 << (8 * 3)), (Uint32)(255 << (8 * 2)),
	(Uint32)(255 << (8 * 1)), 255);
    \endcode

    And draw on it.

    \code
    SDLPangoDraw_Draw(context, surface, margin_x, margin_y);
    \endcode

    You must free the surface by yourself.

    \code
    SDL_FreeSurface(surface);
    \endcode

    Free context.

    \code
    SDLPangoDraw_FreeContext(context);
    \endcode

    More examples can be found in \c test/testbench.c.

    \section ack Acknowledgments

    SDL_PangoDraw is maintained by Michael Imamura <zoogie@lugatgt.org>.
    It is a fork of the SDL_Pango project and the majority of the code is
    unchanged from that project.

    SDL_Pango was developed by NAKAMURA Ken'ichi with financial assistance of
    Information-technology Promotion Agency, Japan.
*/

/*! @file
    @brief Implementation of SDL_PangoDraw

    @author NAKAMURA Ken'ichi
    @date   2004/12/07
    $Revision: 1.6 $
*/

#include <pango/pango.h>
#include <pango/pangoft2.h>

#include "SDL_PangoDraw.h"

//! non-zero if initialized
static int IS_INITIALIZED = 0;

#define DEFAULT_FONT_FAMILY "sans-serif"
#define DEFAULT_FONT_SIZE 12
#define DEFAULT_DPI 96
#define _MAKE_FONT_NAME(family, size) family " " #size
#define MAKE_FONT_NAME(family, size) _MAKE_FONT_NAME(family, size)
#define DEFAULT_DEPTH 32
#define DEFAULT_RMASK (Uint32)(255 << (8 * 3))
#define DEFAULT_GMASK (Uint32)(255 << (8 * 2))
#define DEFAULT_BMASK (Uint32)(255 << (8 * 1))
#define DEFAULT_AMASK (Uint32)255

static FT_Bitmap *createFTBitmap(int width, int height);

static void freeFTBitmap(FT_Bitmap *bitmap);

static void getItemProperties (
    PangoItem      *item,
    PangoUnderline *uline,
    gboolean       *strikethrough,
    gint           *rise,
    PangoColor     *fg_color,
    gboolean       *fg_set,
    PangoColor     *bg_color,
    gboolean       *bg_set,
    gboolean       *shape_set,
    PangoRectangle *ink_rect,
    PangoRectangle *logical_rect);

static void clearFTBitmap(FT_Bitmap *bitmap);

typedef struct _surfaceArgs {
    Uint32 flags;
    int depth;
    Uint32 Rmask;
    Uint32 Gmask;
    Uint32 Bmask;
    Uint32 Amask;
} surfaceArgs;

typedef struct _contextImpl {
    PangoContext *context;
    PangoFontMap *font_map;
    PangoFontDescription *font_desc;
    PangoLayout *layout;
    surfaceArgs surface_args;
    FT_Bitmap *tmp_ftbitmap;
    SDLPangoDraw_Matrix color_matrix;
    int min_width;
    int min_height;
} contextImpl;


const SDLPangoDraw_Matrix _MATRIX_WHITE_BACK
    = {255, 0, 0, 0,
       255, 0, 0, 0,
       255, 0, 0, 0,
       255, 255, 0, 0,};

/*!
    Specifies white back and black letter.
*/
const SDLPangoDraw_Matrix *MATRIX_WHITE_BACK = &_MATRIX_WHITE_BACK;

const SDLPangoDraw_Matrix _MATRIX_BLACK_BACK
    = {0, 255, 0, 0,
       0, 255, 0, 0,
       0, 255, 0, 0,
       255, 255, 0, 0,};
/*!
    Specifies black back and white letter.
*/
const SDLPangoDraw_Matrix *MATRIX_BLACK_BACK = &_MATRIX_BLACK_BACK;

const SDLPangoDraw_Matrix _MATRIX_TRANSPARENT_BACK_BLACK_LETTER
    = {0, 0, 0, 0,
       0, 0, 0, 0,
       0, 0, 0, 0,
       0, 255, 0, 0,};
/*!
    Specifies transparent back and black letter.
*/
const SDLPangoDraw_Matrix *MATRIX_TRANSPARENT_BACK_BLACK_LETTER = &_MATRIX_TRANSPARENT_BACK_BLACK_LETTER;

const SDLPangoDraw_Matrix _MATRIX_TRANSPARENT_BACK_WHITE_LETTER
    = {255, 255, 0, 0,
       255, 255, 0, 0,
       255, 255, 0, 0,
       0, 255, 0, 0,};
/*!
    Specifies transparent back and white letter.
*/
const SDLPangoDraw_Matrix *MATRIX_TRANSPARENT_BACK_WHITE_LETTER = &_MATRIX_TRANSPARENT_BACK_WHITE_LETTER;

const SDLPangoDraw_Matrix _MATRIX_TRANSPARENT_BACK_TRANSPARENT_LETTER
    = {255, 255, 0, 0,
       255, 255, 0, 0,
       255, 255, 0, 0,
       0, 0, 0, 0,};
/*!
    Specifies transparent back and transparent letter.
    This is useful for KARAOKE like rendering.
*/
const SDLPangoDraw_Matrix *MATRIX_TRANSPARENT_BACK_TRANSPARENT_LETTER = &_MATRIX_TRANSPARENT_BACK_TRANSPARENT_LETTER;


/*!
    Initialize the Glib and Pango API.
    This must be called before using other functions in this library,
    excepting SDLPangoDraw_WasInit.
    SDL does not have to be initialized before this call.


    @return always 0.
*/
int
SDLPangoDraw_Init()
{
    g_type_init();

    IS_INITIALIZED = -1;

    return 0;
}

/*!
    Query the initilization status of the Glib and Pango API.
    You may, of course, use this before SDLPangoDraw_Init to avoid
    initilizing twice in a row.

    @return zero when already initialized.
    non-zero when not initialized.
*/
int
SDLPangoDraw_WasInit()
{
    return IS_INITIALIZED;
}

/*!
    Draw glyphs on rect.

    @param *context [in] Context
    @param *surface [out] Surface to draw on it
    @param *color_matrix [in] Foreground and background color
    @param *font [in] Innter variable of Pango
    @param *glyphs [in] Innter variable of Pango
    @param *rect [in] Draw on this area
    @param baseline [in] Horizontal location of glyphs
*/
static void
drawGlyphString(
    SDLPangoDraw_Context *context,
    SDL_Surface *surface,
    SDLPangoDraw_Matrix *color_matrix,
    PangoFont *font,
    PangoGlyphString *glyphs,
    SDL_Rect *rect,
    int baseline)
{
    pango_ft2_render(context->tmp_ftbitmap, font, glyphs, rect->x, rect->y + baseline);

    SDLPangoDraw_CopyFTBitmapToSurface(
	context->tmp_ftbitmap,
	surface,
	color_matrix,
	rect);

    clearFTBitmap(context->tmp_ftbitmap);
}

/*!
    Draw horizontal line of a pixel.

    @param *surface [out] Surface to draw on it
    @param *color_matrix [in] Foreground and background color
    @param y [in] Y location of line
    @param start [in] Left of line
    @param end [in] Right of line
*/
static void drawHLine(
    SDL_Surface *surface,
    SDLPangoDraw_Matrix *color_matrix,
    int y,
    int start,
    int end)
{
    Uint8 *p;
    Uint16 *p16;
    Uint32 *p32;
    Uint32 color;
    int ix;
    int pixel_bytes = surface->format->BytesPerPixel;

    if (y < 0 || y >= surface->h)
	return;

    if (end <= 0 || start >= surface->w)
	return;

    if (start < 0)
	start = 0;

    if (end >= surface->w)
	end = surface->w;

    p = (Uint8 *)(surface->pixels) + y * surface->pitch + start * pixel_bytes;
    color = SDL_MapRGBA(surface->format,
	color_matrix->m[0][1],
	color_matrix->m[1][1], 
	color_matrix->m[2][1], 
	color_matrix->m[3][1]);

    switch(pixel_bytes) {
    case 2:
	p16 = (Uint16 *)p;
	for (ix = 0; ix < end - start; ix++)
	    *p16++ = (Uint16)color;
	break;
    case 4:
	p32 = (Uint32 *)p;
	for (ix = 0; ix < end - start; ix++)
	    *p32++ = color;
	break;
    default:
	SDL_SetError("surface->format->BytesPerPixel is invalid value");
	break;
    }
}

/*!
    Draw a line.

    @param *context [in] Context
    @param *surface [out] Surface to draw on it
    @param *line [in] Innter variable of Pango
    @param x [in] X location of line
    @param y [in] Y location of line
    @param height [in] Height of line
    @param baseline [in] Rise / sink of line (for super/subscript)
*/
static void
drawLine(
    SDLPangoDraw_Context *context,
    SDL_Surface *surface,
    PangoLayoutLine *line,
    gint x, 
    gint y, 
    gint height,
    gint baseline)
{
    GSList *tmp_list = line->runs;
    PangoColor fg_color, bg_color;
    PangoRectangle logical_rect;
    PangoRectangle ink_rect;
    int x_off = 0;

    while (tmp_list) {
	SDLPangoDraw_Matrix color_matrix = context->color_matrix;
	PangoUnderline uline = PANGO_UNDERLINE_NONE;
	gboolean strike, fg_set, bg_set, shape_set;
	gint rise, risen_y;
	PangoLayoutRun *run = tmp_list->data;
	SDL_Rect d_rect;

	tmp_list = tmp_list->next;

	getItemProperties(run->item,
	    &uline, &strike, &rise,
	    &fg_color, &fg_set, &bg_color, &bg_set,
	    &shape_set, &ink_rect, &logical_rect);

	risen_y = y + baseline - PANGO_PIXELS (rise);

	if(fg_set) {
	    color_matrix.m[0][1] = (Uint8)(fg_color.red >> 8);
	    color_matrix.m[1][1] = (Uint8)(fg_color.green >> 8);
	    color_matrix.m[2][1] = (Uint8)(fg_color.blue >> 8);
	    color_matrix.m[3][1] = 255;
	    if(color_matrix.m[3][0] == 0) {
		color_matrix.m[0][0] = (Uint8)(fg_color.red >> 8);
		color_matrix.m[1][0] = (Uint8)(fg_color.green >> 8);
		color_matrix.m[2][0] = (Uint8)(fg_color.blue >> 8);
	    }
	}

	if (bg_set) {
	    color_matrix.m[0][0] = (Uint8)(bg_color.red >> 8);
	    color_matrix.m[1][0] = (Uint8)(bg_color.green >> 8);
	    color_matrix.m[2][0] = (Uint8)(bg_color.blue >> 8);
	    color_matrix.m[3][0] = 255;
	}

	if(! shape_set) {
	    if (uline == PANGO_UNDERLINE_NONE)
		pango_glyph_string_extents (run->glyphs, run->item->analysis.font,
					    NULL, &logical_rect);
	    else
		pango_glyph_string_extents (run->glyphs, run->item->analysis.font,
					    &ink_rect, &logical_rect);

	    d_rect.w = (Uint16)PANGO_PIXELS(logical_rect.width);
	    d_rect.h = (Uint16)height;
	    d_rect.x = (Uint16)(x + PANGO_PIXELS (x_off));
	    d_rect.y = (Uint16)(risen_y - baseline);

	    if((! context->tmp_ftbitmap) || d_rect.w + d_rect.x > context->tmp_ftbitmap->width
		|| d_rect.h + d_rect.y > context->tmp_ftbitmap->rows)
	    {
		freeFTBitmap(context->tmp_ftbitmap);
		context->tmp_ftbitmap = createFTBitmap(d_rect.w + d_rect.x, d_rect.h + d_rect.y);
	    }

	    drawGlyphString(context, surface, 
		&color_matrix, 
		run->item->analysis.font, run->glyphs, &d_rect, baseline);
	}
        switch (uline) {
	case PANGO_UNDERLINE_NONE:
	    break;
	case PANGO_UNDERLINE_DOUBLE:
	    drawHLine(surface, &color_matrix,
		risen_y + 4,
		x + PANGO_PIXELS (x_off + ink_rect.x),
		x + PANGO_PIXELS (x_off + ink_rect.x + ink_rect.width));
	  /* Fall through */
	case PANGO_UNDERLINE_SINGLE:
	    drawHLine(surface, &color_matrix,
		risen_y + 2,
		x + PANGO_PIXELS (x_off + ink_rect.x),
		x + PANGO_PIXELS (x_off + ink_rect.x + ink_rect.width));
	    break;
	case PANGO_UNDERLINE_ERROR:
	    {
		int point_x;
		int counter = 0;
		int end_x = x + PANGO_PIXELS (x_off + ink_rect.x + ink_rect.width);

		for (point_x = x + PANGO_PIXELS (x_off + ink_rect.x) - 1;
		    point_x <= end_x;
		    point_x += 2)
		{
		    if (counter)
			drawHLine(surface, &color_matrix,
			    risen_y + 2,
			    point_x, MIN (point_x + 1, end_x));
		    else
			drawHLine(surface, &color_matrix,
			    risen_y + 3,
			    point_x, MIN (point_x + 1, end_x));
    		
		    counter = (counter + 1) % 2;
		}
	    }
	    break;
	case PANGO_UNDERLINE_LOW:
	    drawHLine(surface, &color_matrix,
		risen_y + PANGO_PIXELS (ink_rect.y + ink_rect.height),
		x + PANGO_PIXELS (x_off + ink_rect.x),
		x + PANGO_PIXELS (x_off + ink_rect.x + ink_rect.width));
	  break;
	}

        if (strike)
	    drawHLine(surface, &color_matrix,
		risen_y + PANGO_PIXELS (logical_rect.y + logical_rect.height / 2),
		x + PANGO_PIXELS (x_off + logical_rect.x),
		x + PANGO_PIXELS (x_off + logical_rect.x + logical_rect.width));

	x_off += logical_rect.width;
    }
}

/*!
    Inner function of Pango. Stolen from GDK.

    @param *item [in] The item to get property
    @param *uline [out] Kind of underline
    @param *strikethrough [out] Strike-through line
    @param *rise [out] Rise/sink of line (for super/subscript)
    @param *fg_color [out] Color of foreground
    @param *fg_set [out] True if fg_color set
    @param *bg_color [out] Color of background
    @param *bg_set [out] True if bg_color valid
    @param *shape_set [out] True if ink_rect and logical_rect valid
    @param *ink_rect [out] Ink rect
    @param *logical_rect [out] Logical rect
*/
static void
getItemProperties (
    PangoItem *item,
    PangoUnderline *uline,
    gboolean *strikethrough,
    gint *rise,
    PangoColor *fg_color,
    gboolean *fg_set,
    PangoColor *bg_color,
    gboolean *bg_set,
    gboolean *shape_set,
    PangoRectangle *ink_rect,
    PangoRectangle *logical_rect)
{
    GSList *tmp_list = item->analysis.extra_attrs;

    if (strikethrough)
	*strikethrough = FALSE;
  
    if (fg_set)
        *fg_set = FALSE;

    if (bg_set)
	*bg_set = FALSE;

    if (shape_set)
	*shape_set = FALSE;

    if (rise)
	*rise = 0;

    while (tmp_list) {
	PangoAttribute *attr = tmp_list->data;

	switch (attr->klass->type) {
	case PANGO_ATTR_UNDERLINE:
	    if (uline)
		*uline = ((PangoAttrInt *)attr)->value;
	    break;

	case PANGO_ATTR_STRIKETHROUGH:
	    if (strikethrough)
		*strikethrough = ((PangoAttrInt *)attr)->value;
	    break;
    	
	case PANGO_ATTR_FOREGROUND:
	    if (fg_color)
		*fg_color = ((PangoAttrColor *)attr)->color;
	    if (fg_set)
		*fg_set = TRUE;
	    break;
    	
	case PANGO_ATTR_BACKGROUND:
	    if (bg_color)
		*bg_color = ((PangoAttrColor *)attr)->color;
	    if (bg_set)
		*bg_set = TRUE;
	    break;

	case PANGO_ATTR_SHAPE:
	    if (shape_set)
		*shape_set = TRUE;
	    if (logical_rect)
		*logical_rect = ((PangoAttrShape *)attr)->logical_rect;
	    if (ink_rect)
		*ink_rect = ((PangoAttrShape *)attr)->ink_rect;
	    break;

	case PANGO_ATTR_RISE:
	    if (rise)
		*rise = ((PangoAttrInt *)attr)->value;
	    break;
    	
	default:
	    break;
	}
	tmp_list = tmp_list->next;
    }
}

/*!
    Copy bitmap to surface. 
    From (x, y)-(w, h) to (x, y)-(w, h) of rect. 

    @param *bitmap [in] Grayscale bitmap
    @param *surface [out] Surface
    @param *matrix [in] Foreground and background color
    @param *rect [in] Rect to copy
*/
void
SDLPangoDraw_CopyFTBitmapToSurface(
    const FT_Bitmap *bitmap,
    SDL_Surface *surface,
    const SDLPangoDraw_Matrix *matrix,
    SDL_Rect *rect)
{
    int i;
    Uint8 *p_ft;
    Uint8 *p_sdl;
    int width = rect->w;
    int height = rect->h;
    int x = rect->x;
    int y = rect->y;

    if(x < 0) {
	width += x; x = 0;
    }
    if(x + width > surface->w) {
	width = surface->w - x;
    }
    if(width <= 0)
	return;

    if(y < 0) {
	height += y; y = 0;
    }
    if(y + height > surface->h) {
	height = surface->h - y;
    }
    if(height <= 0)
	return;

    if(SDL_LockSurface(surface)) {
	SDL_SetError("surface lock failed");
	SDL_FreeSurface(surface);
	return;
    }

    p_ft = (Uint8 *)bitmap->buffer + (bitmap->pitch * y);
    p_sdl = (Uint8 *)surface->pixels + (surface->pitch * y);
    for(i = 0; i < height; i ++) {
	int k;
	for(k = 0; k < width; k ++) {
	    /* TODO: rewrite by matrix calculation library */
	    Uint8 pixel[4];	/* 4: RGBA */
	    int n;

	    for(n = 0; n < 4; n ++) {
		Uint16 w;
		w = ((Uint16)matrix->m[n][0] * (256 - p_ft[k + x])) + ((Uint16)matrix->m[n][1] * p_ft[k + x]);
		pixel[n] = (Uint8)(w >> 8);
	    }

	    switch(surface->format->BytesPerPixel) {
	    case 2:
		((Uint16 *)p_sdl)[k + x] = (Uint16)SDL_MapRGBA(surface->format, pixel[0], pixel[1], pixel[2], pixel[3]);
		break;
	    case 4:
		((Uint32 *)p_sdl)[k + x] = SDL_MapRGBA(surface->format, pixel[0], pixel[1], pixel[2], pixel[3]);
		break;
	    default:
		SDL_SetError("surface->format->BytesPerPixel is invalid value");
		return;
	    }
	}
	p_ft += bitmap->pitch;
	p_sdl += surface->pitch;
    }

    SDL_UnlockSurface(surface);
}


SDLPangoDraw_Context*
SDLPangoDraw_CreateContext_GivenFontDesc(const char* font_desc)
{
    SDLPangoDraw_Context *context = g_malloc(sizeof(SDLPangoDraw_Context));
    G_CONST_RETURN char *charset;

    context->font_map = pango_ft2_font_map_new ();
    pango_ft2_font_map_set_resolution (PANGO_FT2_FONT_MAP (context->font_map), DEFAULT_DPI, DEFAULT_DPI);

    context->context = pango_ft2_font_map_create_context (PANGO_FT2_FONT_MAP (context->font_map));

    g_get_charset(&charset);
    pango_context_set_language (context->context, pango_language_from_string (charset));
    pango_context_set_base_dir (context->context, PANGO_DIRECTION_LTR);

    context->font_desc = pango_font_description_from_string(font_desc);

    context->layout = pango_layout_new (context->context);

    SDLPangoDraw_SetSurfaceCreateArgs(context, SDL_SWSURFACE | SDL_SRCALPHA, DEFAULT_DEPTH,
	DEFAULT_RMASK, DEFAULT_GMASK, DEFAULT_BMASK, DEFAULT_AMASK);

    context->tmp_ftbitmap = NULL;

    context->color_matrix = *MATRIX_TRANSPARENT_BACK_BLACK_LETTER;

    context->min_height = 0;
    context->min_width = 0;

    return context;
}

/*!
    Create a context which contains Pango objects.

    @return A pointer to the context as a SDLPangoDraw_Context*.
*/
SDLPangoDraw_Context*
SDLPangoDraw_CreateContext()
{
    SDLPangoDraw_CreateContext_GivenFontDesc(MAKE_FONT_NAME(DEFAULT_FONT_FAMILY, DEFAULT_FONT_SIZE));
}

/*!
    Free a context.

    @param *context [i/o] Context to be free
*/
void
SDLPangoDraw_FreeContext(SDLPangoDraw_Context *context)
{
    freeFTBitmap(context->tmp_ftbitmap);

    g_object_unref (context->layout);

    pango_font_description_free(context->font_desc);

    g_object_unref(context->context);

    g_object_unref(context->font_map);

    g_free(context);
}

/*!
    Specify arguments to use when creating a surface.
    SDLPangoDraw_CreateSurfaceDraw will use these arguments to create the
    SDL surface.

    @param *context [i/o] Context
    @param flags [in] Same as SDL_CreateRGBSurface()
    @param depth [in] Same as SDL_CreateRGBSurface()
    @param Rmask [in] Same as SDL_CreateRGBSurface()
    @param Gmask [in] Same as SDL_CreateRGBSurface()
    @param Bmask [in] Same as SDL_CreateRGBSurface()
    @param Amask [in] Same as SDL_CreateRGBSurface()
*/
void
SDLPangoDraw_SetSurfaceCreateArgs(
    SDLPangoDraw_Context *context,
    Uint32 flags,
    int depth,
    Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask)
{
    context->surface_args.flags = flags;
    context->surface_args.depth = depth;
    context->surface_args.Rmask = Rmask;
    context->surface_args.Gmask = Gmask;
    context->surface_args.Bmask = Bmask;
    context->surface_args.Amask = Amask;
}

/*!
    Create a surface and draw text on it.
    The size of surface is same as lauout size.

    @param *context [in] Context
    @return A newly created surface
*/
SDL_Surface * SDLPangoDraw_CreateSurfaceDraw(
    SDLPangoDraw_Context *context)
{
    PangoRectangle logical_rect;
    SDL_Surface *surface;
    int width, height;

    pango_layout_get_extents (context->layout, NULL, &logical_rect);
    width = PANGO_PIXELS (logical_rect.width);
    height = PANGO_PIXELS (logical_rect.height);
    if(width < context->min_width)
	width = context->min_width;
    if(height < context->min_height)
	height = context->min_height;

    surface = SDL_CreateRGBSurface(
	context->surface_args.flags,
	width, height, context->surface_args.depth,
	context->surface_args.Rmask,
	context->surface_args.Gmask,
	context->surface_args.Bmask,
	context->surface_args.Amask);

    SDLPangoDraw_Draw(context, surface, 0, 0);

    return surface;
}

/*!
    Draw text on an existing surface.
    The text must have been previously set via SDLPangoDraw_SetMarkup or
    SDLPangoDraw_SetText.

    @param *context [in] Context
    @param *surface [i/o] Surface to draw on
    @param x [in] X of left-top of drawing area
    @param y [in] Y of left-top of drawing area
*/
void
SDLPangoDraw_Draw(
    SDLPangoDraw_Context *context,
    SDL_Surface *surface,
    int x, int y)
{
    PangoLayoutIter *iter;
    PangoRectangle logical_rect;
    int width, height;

    if(! surface) {
	SDL_SetError("surface is NULL");
	return;
    }

    iter = pango_layout_get_iter (context->layout);

    pango_layout_get_extents (context->layout, NULL, &logical_rect);
    width = PANGO_PIXELS (logical_rect.width);
    height = PANGO_PIXELS (logical_rect.height);

    if(width && height) {
	SDL_FillRect(surface, NULL, SDL_MapRGBA(surface->format, 0, 0, 0, 0));
    }

    if((! context->tmp_ftbitmap) || context->tmp_ftbitmap->width < width
	|| context->tmp_ftbitmap->rows < height)
    {
	freeFTBitmap(context->tmp_ftbitmap);
        context->tmp_ftbitmap = createFTBitmap(width, height);
    }

    do {
	PangoLayoutLine *line;
	int baseline;

	line = pango_layout_iter_get_line (iter);

	pango_layout_iter_get_line_extents (iter, NULL, &logical_rect);
	baseline = pango_layout_iter_get_baseline (iter);

	drawLine(
	    context,
	    surface,
	    line,
	    x + PANGO_PIXELS (logical_rect.x),
	    y + PANGO_PIXELS (logical_rect.y),
	    PANGO_PIXELS (logical_rect.height),
	    PANGO_PIXELS (baseline - logical_rect.y));
    } while (pango_layout_iter_next_line (iter));

    pango_layout_iter_free (iter);
}

/*!
    Allocate buffer and create a FTBitmap object.

    @param width [in] Width
    @param height [in] Height
    @return FTBitmap object
*/
static FT_Bitmap *
createFTBitmap(
    int width, int height)
{
    FT_Bitmap *bitmap;
    guchar *buf;

    bitmap = g_malloc(sizeof(FT_Bitmap));
    bitmap->width = width;
    bitmap->rows = height;
    bitmap->pitch = (width + 3) & ~3;
    bitmap->num_grays = 256;
    bitmap->pixel_mode = FT_PIXEL_MODE_GRAY;
    buf = g_malloc (bitmap->pitch * bitmap->rows);
    memset (buf, 0x00, bitmap->pitch * bitmap->rows);
    bitmap->buffer = buf;

    return bitmap;
}

/*!
    Free a FTBitmap object.

    @param *bitmap [i/o] FTbitmap to be free
*/
static void
freeFTBitmap(
    FT_Bitmap *bitmap)
{
    if(bitmap) {
	g_free(bitmap->buffer);
	g_free(bitmap);
    }
}

/*!
    Clear a FTBitmap object.

    @param *bitmap [i/o] FTbitmap to be clear
*/
static void
clearFTBitmap(
    FT_Bitmap *bitmap)
{
    Uint8 *p = (Uint8 *)bitmap->buffer;
    int length = bitmap->pitch * bitmap->rows;

    memset(p, 0, length);
}

/*!
    Specify minimum size of drawing rect.

    @param *context [i/o] Context
    @param width [in] Width. -1 means no wrapping mode.
    @param height [in] Height. zero/minus value means non-specified.
*/
void
SDLPangoDraw_SetMinimumSize(
    SDLPangoDraw_Context *context,
    int width, int height)
{
    int pango_width;
    if(width > 0)
	pango_width = width * PANGO_SCALE;
    else
	pango_width = -1;
    pango_layout_set_width(context->layout, pango_width);

    context->min_width = width;
    context->min_height = height;
}

/*!
    Specify default color.

    @param *context [i/o] Context
    @param *color_matrix [in] Foreground and background color
*/
void
SDLPangoDraw_SetDefaultColor(
    SDLPangoDraw_Context *context,
    const SDLPangoDraw_Matrix *color_matrix)
{
    context->color_matrix = *color_matrix;
}

/*!
    Get layout width.

    @param *context [in] Context
    @return Width
*/
int
SDLPangoDraw_GetLayoutWidth(
    SDLPangoDraw_Context *context)
{
    PangoRectangle logical_rect;

    pango_layout_get_extents (context->layout, NULL, &logical_rect);

    return PANGO_PIXELS (logical_rect.width);
}

/*!
    Get layout height.

    @param *context [in] Context
    @return Height
*/
int
SDLPangoDraw_GetLayoutHeight(
    SDLPangoDraw_Context *context)
{
    PangoRectangle logical_rect;

    pango_layout_get_extents (context->layout, NULL, &logical_rect);

    return PANGO_PIXELS (logical_rect.height);
}

/*!
    Set the markup text to draw.
    Markup format is same as Pango.

    @param *context [i/o] Context
    @param *markup [in] Markup text (must be in UTF-8).
    @param length [in] Text length. -1 means NULL-terminated text.
*/
void
SDLPangoDraw_SetMarkup(
    SDLPangoDraw_Context *context,
    const char *markup,
    int length)
{
    pango_layout_set_markup (context->layout, markup, length);
    pango_layout_set_auto_dir (context->layout, TRUE);
    pango_layout_set_alignment (context->layout, PANGO_ALIGN_LEFT);
    pango_layout_set_font_description (context->layout, context->font_desc);
}

/*!
    Set the plain (non-markup) text to draw, using a specific text alignment.
    This only applies if the minimum size has been set using
    SDLPangoDraw_SetMinimumSize.

    @param *context [i/o] Context
    @param *text [in] The raw text (must be in UTF-8).
    @param length [in] Text length. -1 means NULL-terminated text.
    @param alignment The text alignment.
*/
void
SDLPangoDraw_SetText_GivenAlignment(
    SDLPangoDraw_Context *context,
    const char *text,
    int length,
    SDLPangoDraw_Alignment alignment)
{
    pango_layout_set_attributes(context->layout, NULL);
    pango_layout_set_text (context->layout, text, length);
    pango_layout_set_auto_dir (context->layout, TRUE);
    pango_layout_set_alignment (context->layout, alignment);
    pango_layout_set_font_description (context->layout, context->font_desc);
}

/*!
    Set the plain (non-markup) text to draw.

    @param *context [i/o] Context
    @param *text [in] The raw text (must be in UTF-8).
    @param length [in] Text length. -1 means NULL-terminated text.
*/
void
SDLPangoDraw_SetText(
    SDLPangoDraw_Context *context,
    const char *text,
    int length)
{
    SDLPangoDraw_SetText_GivenAlignment(context, text, length, SDLPANGODRAW_ALIGN_LEFT);
}

/*!
    Set the DPI.

    @param *context [i/o] Context
    @param dpi_x [in] X dpi
    @param dpi_y [in] Y dpi
*/
void
SDLPangoDraw_SetDpi(
    SDLPangoDraw_Context *context,
    double dpi_x, double dpi_y)
{
    pango_ft2_font_map_set_resolution (PANGO_FT2_FONT_MAP (context->font_map), dpi_x, dpi_y);
}

/*!
    Set language to context.

    @param *context [i/o] Context
    @param *language_tag [in] A RFC-3066 format language tag 
*/
void SDLCALL
SDLPangoDraw_SetLanguage(
    SDLPangoDraw_Context *context,
    const char *language_tag)
{
    pango_context_set_language (context->context, pango_language_from_string (language_tag));
}

/*!
    Set base direction to context.

    @param *context [i/o] Context
    @param direction [in] Direction
*/
void SDLCALL
SDLPangoDraw_SetBaseDirection(
    SDLPangoDraw_Context *context,
    SDLPangoDraw_Direction direction)
{
    PangoDirection pango_dir;

    switch(direction) {
    case SDLPANGODRAW_DIRECTION_LTR:
	pango_dir = PANGO_DIRECTION_LTR;
	break;
    case SDLPANGODRAW_DIRECTION_RTL:
	pango_dir = PANGO_DIRECTION_RTL;
	break;
    case SDLPANGODRAW_DIRECTION_WEAK_LTR:
	pango_dir = PANGO_DIRECTION_WEAK_LTR;
	break;
    case SDLPANGODRAW_DIRECTION_WEAK_RTL:
	pango_dir = PANGO_DIRECTION_WEAK_RTL;
	break;
    case SDLPANGODRAW_DIRECTION_NEUTRAL:
	pango_dir = PANGO_DIRECTION_NEUTRAL;
	break;
    default:
	SDL_SetError("unknown direction value");
	return;
    }

    pango_context_set_base_dir (context->context, pango_dir);
}

/*!
    Get font map from context.

    @param *context [in] Context
    @return Font map
*/
PangoFontMap* SDLCALL
SDLPangoDraw_GetPangoFontMap(
    SDLPangoDraw_Context *context)
{
    return context->font_map;
}

/*!
    Get font description from context.

    @param *context [in] Context
    @return Font description
*/
PangoFontDescription* SDLCALL
SDLPangoDraw_GetPangoFontDescription(
    SDLPangoDraw_Context *context)
{
    return context->font_desc;
}

/*!
    Get layout from context.

    @param *context [in] Context
    @return Layout
*/
PangoLayout* SDLCALL
SDLPangoDraw_GetPangoLayout(
    SDLPangoDraw_Context *context)
{
    return context->layout;
}
