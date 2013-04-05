/* vim: set noet ai sw=4 sts=4 ts=8: */
/*  SDL_PangoDraw.h -- A companion library to SDL for working with Pango.
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

/*! @file
    @brief Header file of SDL_PangoDraw

    @author NAKAMURA Ken'ichi
    @author Michael Imamura
*/

#ifndef SDL_PANGODRAW_H
#define SDL_PANGODRAW_H

#include "SDL.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif



typedef struct _contextImpl SDLPangoDraw_Context;

/*!
    General 4 X 4 matrix struct.
*/
typedef struct _SDLPangoDraw_Matrix {
    Uint8 m[4][4];  /*! Matrix variables */
} SDLPangoDraw_Matrix;

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
    Specifies direction of text. See Pango reference for detail
*/
typedef enum {
    SDLPANGODRAW_DIRECTION_LTR, /*! Left to right */
    SDLPANGODRAW_DIRECTION_RTL, /*! Right to left */
    SDLPANGODRAW_DIRECTION_WEAK_LTR,    /*! Left to right (weak) */
    SDLPANGODRAW_DIRECTION_WEAK_RTL,    /*! Right to left (weak) */
    SDLPANGODRAW_DIRECTION_NEUTRAL	/*! Neutral */
} SDLPangoDraw_Direction;



extern DECLSPEC int SDLCALL SDLPangoDraw_Init();

extern DECLSPEC int SDLCALL SDLPangoDraw_WasInit();

extern DECLSPEC SDLPangoDraw_Context* SDLCALL SDLPangoDraw_CreateContext();

extern DECLSPEC void SDLCALL SDLPangoDraw_FreeContext(
    SDLPangoDraw_Context *context);

extern DECLSPEC void SDLCALL SDLPangoDraw_SetSurfaceCreateArgs(
    SDLPangoDraw_Context *context,
    Uint32 flags,
    int depth,
    Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);

extern DECLSPEC SDL_Surface * SDLCALL SDLPangoDraw_CreateSurfaceDraw(
    SDLPangoDraw_Context *context);

extern DECLSPEC void SDLCALL SDLPangoDraw_Draw(
    SDLPangoDraw_Context *context,
    SDL_Surface *surface,
    int x, int y);

extern DECLSPEC void SDLCALL SDLPangoDraw_SetDpi(
    SDLPangoDraw_Context *context,
    double dpi_x, double dpi_y);

extern DECLSPEC void SDLCALL SDLPangoDraw_SetMinimumSize(
    SDLPangoDraw_Context *context,
    int width, int height);

extern DECLSPEC void SDLCALL SDLPangoDraw_SetDefaultColor(
    SDLPangoDraw_Context *context,
    const SDLPangoDraw_Matrix *color_matrix);

extern DECLSPEC int SDLCALL SDLPangoDraw_GetLayoutWidth(
    SDLPangoDraw_Context *context);

extern DECLSPEC int SDLCALL SDLPangoDraw_GetLayoutHeight(
    SDLPangoDraw_Context *context);

extern DECLSPEC void SDLCALL SDLPangoDraw_SetMarkup(
    SDLPangoDraw_Context *context,
    const char *markup,
    int length);

extern DECLSPEC void SDLCALL SDLPangoDraw_SetText(
    SDLPangoDraw_Context *context,
    const char *markup,
    int length);

extern DECLSPEC void SDLCALL SDLPangoDraw_SetLanguage(
    SDLPangoDraw_Context *context,
    const char *language_tag);

extern DECLSPEC void SDLCALL SDLPangoDraw_SetBaseDirection(
    SDLPangoDraw_Context *context,
    SDLPangoDraw_Direction direction);


#ifdef __FT2_BUILD_UNIX_H__

extern DECLSPEC void SDLCALL SDLPangoDraw_CopyFTBitmapToSurface(
    const FT_Bitmap *bitmap,
    SDL_Surface *surface,
    const SDLPangoDraw_Matrix *matrix,
    SDL_Rect *rect);

#endif	/* __FT2_BUILD_UNIX_H__ */


#ifdef __PANGO_H__

extern DECLSPEC PangoFontMap* SDLCALL SDLPangoDraw_GetPangoFontMap(
    SDLPangoDraw_Context *context);

extern DECLSPEC PangoFontDescription* SDLCALL SDLPangoDraw_GetPangoFontDescription(
    SDLPangoDraw_Context *context);

extern DECLSPEC PangoLayout* SDLCALL SDLPangoDraw_GetPangoLayout(
    SDLPangoDraw_Context *context);

#endif /* __PANGO_H__ */


#ifdef __cplusplus
}
#endif

#include "close_code.h"

#endif	/* SDL_PANGO_H */
