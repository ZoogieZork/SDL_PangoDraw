// vim: set et ts=4 sts=4 sw=4:
#ifndef SDL_PANGO_HPP
#define SDL_PANGO_HPP

#include <string>
#include "SDL_Pango.h"

class SDLPango {
public:
    typedef SDLPango_Matrix Matrix;

    enum Direction {
        DIRECTION_LTR      = SDLPANGO_DIRECTION_LTR,
        DIRECTION_RTL      = SDLPANGO_DIRECTION_RTL,
        DIRECTION_WEAK_LTR = SDLPANGO_DIRECTION_WEAK_LTR,
        DIRECTION_WEAK_RTL = SDLPANGO_DIRECTION_WEAK_RTL,
        DIRECTION_NEUTRAL  = SDLPANGO_DIRECTION_NEUTRAL,
    };

    class Surface_Create_Args {
        unsigned int flags_;
        int          depth_;
        unsigned int rmask_;
        unsigned int gmask_;
        unsigned int bmask_;
        unsigned int amask_;

    public:
        explicit Surface_Create_Args(
                unsigned int flags,
                int          depth,
                unsigned int rmask,
                unsigned int gmask,
                unsigned int bmask,
                unsigned int amask)
            : flags_(flags)
            , depth_(depth)
            , rmask_(rmask)
            , gmask_(gmask)
            , bmask_(bmask)
            , amask_(amask) {}

        unsigned int get_flags() const { return flags_; }
        int          get_depth() const { return depth_; }
        unsigned int get_rmask() const { return rmask_; }
        unsigned int get_gmask() const { return gmask_; }
        unsigned int get_bmask() const { return bmask_; }
        unsigned int get_amask() const { return amask_; }
    };

private:
    SDLPango_Context* context_;

    SDLPango(const SDLPango&);
    SDLPango& operator=(const SDLPango&);

public:
    static int init() {
        return SDLPango_Init();
    }

    static int wasInit() {
        return SDLPango_WasInit();
    }

    explicit SDLPango()
        : context_(SDLPango_CreateContext()) {}

    ~SDLPango() {
        SDLPango_FreeContext(context_);
    }

    SDLPango_Context* getContext() {
        return context_;
    }

    void setSurfaceCreateArgs(const Surface_Create_Args& args) {
        SDLPango_SetSurfaceCreateArgs(
                context_,
                args.get_flags(),
                args.get_depth(),
                args.get_rmask(),
                args.get_gmask(),
                args.get_bmask(),
                args.get_amask());
    }

    SDL_Surface* createSurfaceDraw() {
        return SDLPango_CreateSurfaceDraw(context_);
    }

    void draw(SDL_Surface* surface, int x, int y) {
        SDLPango_Draw(context_, surface, x, y);
    }

    void setDpi(double dpi_x, double dpi_y) {
        SDLPango_SetDpi(context_, dpi_x, dpi_y);
    }

    void setMinimumSize(int width, int height) {
        SDLPango_SetMinimumSize(context_, width, height);
    }

    void setDefaultColor(const Matrix* color_matrix) {
        SDLPango_SetDefaultColor(context_, color_matrix);
    }

    int getLayoutWidth() {
        return SDLPango_GetLayoutWidth(context_);
    }

    int getLayoutHeight() {
        return SDLPango_GetLayoutHeight(context_);
    }

    void setMarkup(const std::string& markup) {
        SDLPango_SetMarkup(context_, markup.data(), markup.size());
    }

    void setText(const std::string& text) {
        SDLPango_SetText(context_, text.data(), text.size());
    }

    void setLanguage(const std::string& language_tag) {
        SDLPango_SetLanguage(context_, language_tag.c_str());
    }

    void setBaseDirection(Direction direction) {
        SDLPango_SetBaseDirection(
                context_, static_cast<SDLPango_Direction>(direction));
    }

    void setMinLineHeight(int line_height) {
        SDLPango_SetMinLineHeight(context_, line_height);
    }

#ifdef __PANGO_H__
    PangoFontMap* getPangoFontMap() {
        return SDLPango_GetPangoFontMap(context_);
    }

    PangoFontDescription* getPangoFontDescription() {
        return SDLPango_GetPangoFontDescription(context_);
    }

    PangoLayout* getPangoLayout() {
        return SDLPango_GetPangoLayout(context_);
    }
#endif
};

#endif
