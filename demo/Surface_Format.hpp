// vim: set et ts=4 sts=4 sw=4:
#ifndef SDLPU_SURFACE_FORMAT_HPP
#define SDLPU_SURFACE_FORMAT_HPP

#include <string>
#include "Import.hpp"

namespace SDL_PangoUtil {

    class Surface_Format {
    public:
        enum Format_Type { UNKNOWN, RGB, BGR, RGBA, BGRA, ARGB, ABGR };

    private:
        Format_Type format_;

        void initialize_big_endian_(unsigned int value) {
            switch (value) {
                case 0x00112233: format_ = RGB;  break;
                case 0x00332211: format_ = BGR;  break;
                case 0x11223344: format_ = RGBA; break;
                case 0x33221144: format_ = BGRA; break;
                case 0x44112233: format_ = ARGB; break;
                case 0x44332211: format_ = ABGR; break;
            }
        }

        void initialize_little_endian_(unsigned int value) {
            switch (value) {
                case 0x00332211: format_ = RGB;  break;
                case 0x00112233: format_ = BGR;  break;
                case 0x44332211: format_ = RGBA; break;
                case 0x44112233: format_ = BGRA; break;
                case 0x33221144: format_ = ARGB; break;
                case 0x11223344: format_ = ABGR; break;
            }
        }

        void initialize_(
                int          bits_per_pixel,
                unsigned int rmask,
                unsigned int gmask,
                unsigned int bmask,
                unsigned int amask)
        {
            format_ = UNKNOWN;
            if (! (bits_per_pixel == 24 || bits_per_pixel == 32)) {
                return;
            }

            unsigned int value(0);
            value |= rmask & 0x11111111;
            value |= gmask & 0x22222222;
            value |= bmask & 0x33333333;
            value |= amask & 0x44444444;

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
            initialize_big_endian_   (value);
#else
            initialize_little_endian_(value);
#endif
        }

    public:
        explicit Surface_Format(const SDL_Surface* surface) {
            initialize_(
                    surface->format->BitsPerPixel,
                    surface->format->Rmask,
                    surface->format->Gmask,
                    surface->format->Bmask,
                    surface->format->Amask);
        }

        explicit Surface_Format(
                int          bits_per_pixel,
                unsigned int rmask,
                unsigned int gmask,
                unsigned int bmask,
                unsigned int amask)
        {
            initialize_(bits_per_pixel, rmask, gmask, bmask, amask);
        }

        Format_Type get_format() const {
            return format_;
        }

        std::string get_format_string() const {
            switch (format_) {
                case UNKNOWN: return "UNKNOWN";
                case RGB:     return "RGB";
                case BGR:     return "BGR";
                case RGBA:    return "RGBA";
                case BGRA:    return "BGRA";
                case ARGB:    return "ARGB";
                case ABGR:    return "ABGR";
            }
            return "UNKNOWN";
        }

        GLenum get_format_opengl() const {
            GLenum unknown(0);
            switch (format_) {
                case UNKNOWN: return unknown;
                case RGB:     return GL_RGB;
                case BGR:     return GL_BGR;
                case RGBA:    return GL_RGBA; // OpenGL 1.2
                case BGRA:    return GL_BGRA; // OpenGL 1.2
                case ARGB:    return 0x8000;  // GL_ABGR_EXT
                case ABGR:    return unknown;
            }
            return unknown;
        }
    };
}

#endif
