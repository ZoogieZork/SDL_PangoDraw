// vim: set et ts=4 sts=4 sw=4:
#ifndef SDLPU_TEXTURE_HPP
#define SDLPU_TEXTURE_HPP

#include "Error.hpp"
#include "Import.hpp"
#include "Surface_Format.hpp"

namespace SDL_PangoUtil {

    class Texture {
        GLuint       id_;
        SDL_Surface* surface_;
        bool         generated_;
        bool         modified_;

        void free_surface_() {
            if (! surface_) {
                SDL_FreeSurface(surface_);
                surface_ = 0;
            }
        }

    public:
        static int calc_power_of_two(int value) {
            int v = 1;
            while (v < value) {
                v <<= 1;
            }
            return v;
        }

        static void opengl_enable() {
            glEnable(GL_TEXTURE_2D);
            SDLPU_CHECK_OPENGL_ERROR();
        }

        static void opengl_disable() {
            glDisable(GL_TEXTURE_2D);
            SDLPU_CHECK_OPENGL_ERROR();
        }

        static void opengl_parameter(GLenum name, GLint param) {
            glTexParameteri(GL_TEXTURE_2D, name, param);
            SDLPU_CHECK_OPENGL_ERROR();
        }

        static void opengl_parameter_mipmap_clamp() {
            opengl_parameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            opengl_parameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            opengl_parameter(GL_TEXTURE_WRAP_S,     GL_CLAMP);
            opengl_parameter(GL_TEXTURE_WRAP_T,     GL_CLAMP);
        }

        static void opengl_parameter_mipmap_repeat() {
            opengl_parameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            opengl_parameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            opengl_parameter(GL_TEXTURE_WRAP_S,     GL_REPEAT);
            opengl_parameter(GL_TEXTURE_WRAP_T,     GL_REPEAT);
        }

        explicit Texture()
            : id_       ()
            , surface_  ()
            , generated_()
            , modified_ ()
        {
            opengl_generate();
        }

        ~Texture() {
            opengl_delete();
            free_surface_();
        }

        void set_surface(SDL_Surface* surface) {
            free_surface_();
            surface_ = surface;
            modified_ = true;
        }

        void set_modified() {
            modified_ = true;
        }

        bool has_surface() const {
            return surface_;
        }

        SDL_Surface& get_surface() const {
            BOOST_ASSERT(surface_);
            return *surface_;
        }

        bool is_modified() const {
            return modified_;
        }

        void opengl_generate() {
            if (! generated_) {
                glGenTextures(1, &id_);
                SDLPU_CHECK_OPENGL_ERROR();
                generated_ = true;
                modified_  = true;
            }
        }

        void opengl_delete() {
            if (generated_) {
                glDeleteTextures(1, &id_);
                SDLPU_CHECK_OPENGL_ERROR();
                generated_ = false;
                modified_  = true;
            }
        }

        void opengl_bind() {
            BOOST_ASSERT(generated_);
            glBindTexture(GL_TEXTURE_2D, id_);
            SDLPU_CHECK_OPENGL_ERROR();
        }

        void load(const FS::path& filepath) {
            SDL_Surface* surface(SDL_LoadBMP(filepath.string().c_str()));
            if (! surface) {
                SDLPU_THROW_RUNTIME_ERROR((
                        "Could not SDL_LoadBMP: %s", SDL_GetError()));
            }
            set_surface(surface);
        }

        void resize(int width, int height) {
            BOOST_ASSERT(surface_);

            SDL_Surface* surface(
                    SDL_CreateRGBSurface(
                            SDL_SWSURFACE,
                            width,
                            height,
                            surface_->format->BitsPerPixel,
                            surface_->format->Rmask,
                            surface_->format->Gmask,
                            surface_->format->Bmask,
                            surface_->format->Amask));
            if (! surface) {
                SDLPU_THROW_RUNTIME_ERROR((
                        "Could not SDL_CreateRGBSurface: %s", SDL_GetError()));
            }

            SDL_FillRect   (surface,  0, 0);
            SDL_SetColorKey(surface_, 0, 0);
            SDL_SetAlpha   (surface_, 0, 0);
            SDL_BlitSurface(surface_, 0, surface, 0);

            set_surface(surface);
        }

        void update() {
            if (! modified_) {
                return;
            }
            BOOST_ASSERT(surface_);
            BOOST_ASSERT(generated_);

            GLenum format(Surface_Format(surface_).get_format_opengl());
            if (! format) {
                SDLPU_THROW_RUNTIME_ERROR(("Invalid texture format"));
            }

            Tuple2i size(
                    calc_power_of_two(surface_->w),
                    calc_power_of_two(surface_->h));

            if (size.x != surface_->w || size.y != surface_->h) {
                resize(size.x, size.y);
            }

            opengl_bind();
            gluBuild2DMipmaps(
                    GL_TEXTURE_2D,
                    surface_->format->BytesPerPixel,
                    surface_->w,
                    surface_->h,
                    format,
                    GL_UNSIGNED_BYTE,
                    surface_->pixels);
            SDLPU_CHECK_OPENGL_ERROR();

            modified_ = false;
        }
    };
}

#endif
