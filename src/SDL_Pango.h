/*  SDL_Pango.h -- A companion library to SDL for working with Pango.
    Copyright (C) 2004 NAKAMURA Ken'ichi

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
    @brief Header file of SDL_Pango

    @author NAKAMURA Ken'ichi
    @date   2004/08/26
    $Revision: 1.4 $
*/

#ifndef SDL_PANGO_H
#define SDL_PANGO_H

#include "SDL.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif



typedef struct _contextImpl SDLPango_Context;

/*!
    General 4 X 4 matrix struct.
*/
typedef struct _SDLPango_Matrix {
    Uint8 m[4][4];  /*! Matrix variables */
} SDLPango_Matrix;

const SDLPango_Matrix _MATRIX_WHITE_BACK
    = {255, 0, 0, 0,
       255, 0, 0, 0,
       255, 0, 0, 0,
       255, 255, 0, 0,};

/*!
    Specifies white back and black letter.
*/
const SDLPango_Matrix *MATRIX_WHITE_BACK = &_MATRIX_WHITE_BACK;

const SDLPango_Matrix _MATRIX_BLACK_BACK
    = {0, 255, 0, 0,
       0, 255, 0, 0,
       0, 255, 0, 0,
       255, 255, 0, 0,};
/*!
    Specifies black back and white letter.
*/
const SDLPango_Matrix *MATRIX_BLACK_BACK = &_MATRIX_BLACK_BACK;

const SDLPango_Matrix _MATRIX_TRANSPARENT_BACK_BLACK_LETTER
    = {0, 0, 0, 0,
       0, 0, 0, 0,
       0, 0, 0, 0,
       0, 255, 0, 0,};
/*!
    Specifies transparent back and black letter.
*/
const SDLPango_Matrix *MATRIX_TRANSPARENT_BACK_BLACK_LETTER = &_MATRIX_TRANSPARENT_BACK_BLACK_LETTER;

const SDLPango_Matrix _MATRIX_TRANSPARENT_BACK_WHITE_LETTER
    = {255, 255, 0, 0,
       255, 255, 0, 0,
       255, 255, 0, 0,
       0, 255, 0, 0,};
/*!
    Specifies transparent back and white letter.
*/
const SDLPango_Matrix *MATRIX_TRANSPARENT_BACK_WHITE_LETTER = &_MATRIX_TRANSPARENT_BACK_WHITE_LETTER;

const SDLPango_Matrix _MATRIX_TRANSPARENT_BACK_TRANSPARENT_LETTER
    = {255, 255, 0, 0,
       255, 255, 0, 0,
       255, 255, 0, 0,
       0, 0, 0, 0,};
/*!
    Specifies transparent back and transparent letter.
    This is useful for KARAOKE like rendering.
*/
const SDLPango_Matrix *MATRIX_TRANSPARENT_BACK_TRANSPARENT_LETTER = &_MATRIX_TRANSPARENT_BACK_TRANSPARENT_LETTER;

/*!
    Specifies direction of text. See Pango reference for detail
*/
typedef enum {
    SDLPANGO_DIRECTION_LTR, /*! Left to right */
    SDLPANGO_DIRECTION_RTL, /*! Right to left */
    SDLPANGO_DIRECTION_WEAK_LTR,    /*! Left to right (weak) */
    SDLPANGO_DIRECTION_WEAK_RTL,    /*! Right to left (weak) */
    SDLPANGO_DIRECTION_NEUTRAL	/*! Neutral */
} SDLPango_Direction;



extern DECLSPEC int SDLCALL SDLPango_Init();

extern DECLSPEC int SDLCALL SDLPango_WasInit();

extern DECLSPEC SDLPango_Context* SDLCALL SDLPango_CreateContext();

extern DECLSPEC void SDLCALL SDLPango_FreeContext(
    SDLPango_Context *context);

extern DECLSPEC void SDLCALL SDLPango_SetSurfaceCreateArgs(
    SDLPango_Context *context,
    Uint32 flags,
    int depth,
    Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);

extern DECLSPEC SDL_Surface * SDLCALL SDLPango_CreateSurfaceDraw(
    SDLPango_Context *context);

extern DECLSPEC void SDLCALL SDLPango_Draw(
    SDLPango_Context *context,
    SDL_Surface *surface,
    int x, int y);

extern DECLSPEC void SDLCALL SDLPango_SetDpi(
    SDLPango_Context *context,
    double dpi_x, double dpi_y);

extern DECLSPEC void SDLCALL SDLPango_SetMinimumSize(
    SDLPango_Context *context,
    int width, int height);

extern DECLSPEC void SDLCALL SDLPango_SetDefaultColor(
    SDLPango_Context *context,
    const SDLPango_Matrix *color_matrix);

extern DECLSPEC int SDLCALL SDLPango_GetLayoutWidth(
    SDLPango_Context *context);

extern DECLSPEC int SDLCALL SDLPango_GetLayoutHeight(
    SDLPango_Context *context);

extern DECLSPEC void SDLCALL SDLPango_SetMarkup(
    SDLPango_Context *context,
    const char *markup,
    int length);

extern DECLSPEC void SDLCALL SDLPango_SetText(
    SDLPango_Context *context,
    const char *markup,
    int length);

extern DECLSPEC void SDLCALL SDLPango_SetLanguage(
    SDLPango_Context *context,
    const char *language_tag);

extern DECLSPEC void SDLCALL SDLPango_SetBaseDirection(
    SDLPango_Context *context,
    SDLPango_Direction direction);

extern DECLSPEC void SDLCALL SDLPango_SetMinLineHeight(
    SDLPango_Context *context,
    int line_height);



#ifdef __FT2_BUILD_UNIX_H__

extern DECLSPEC void SDLCALL SDLPango_CopyFTBitmapToSurface(
    const FT_Bitmap *bitmap,
    SDL_Surface *surface,
    const SDLPango_Matrix *matrix,
    SDL_Rect *rect);

#endif	/* __FT2_BUILD_UNIX_H__ */


#ifdef __PANGO_H__

extern DECLSPEC PangoFontMap* SDLCALL SDLPango_GetPangoFontMap(
    SDLPango_Context *context);

extern DECLSPEC PangoFontDescription* SDLCALL SDLPango_GetPangoFontDescription(
    SDLPango_Context *context);

extern DECLSPEC PangoLayout* SDLCALL SDLPango_GetPangoLayout(
    SDLPango_Context *context);

#endif /* __PANGO_H__ */


#ifdef __cplusplus
}
#endif

#include "close_code.h"

#endif	/* SDL_PANGO_H */
