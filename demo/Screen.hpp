// vim: set et ts=4 sts=4 sw=4:
#ifndef SDLPU_SCREEN_HPP
#define SDLPU_SCREEN_HPP

#include <list>
#include <boost/bind.hpp>
#include <boost/checked_delete.hpp>
#include <boost/noncopyable.hpp>
#include "Error.hpp"
#include "Import.hpp"
#include "Texture.hpp"

namespace SDL_PangoUtil {

    class Context;

    class Screen
        : public boost::noncopyable
    {
        Context*            context_;
        SDL_Surface*        surface_;
        std::list<Texture*> texture_;

        void resize_surface_(int width, int height, unsigned int flags) {
            surface_ = SDL_SetVideoMode(width, height, 32, flags);
            if (! surface_) {
                SDLPU_THROW_RUNTIME_ERROR((
                        "Could not SDL_SetVideoMode: %s", SDL_GetError()));
            }
            glViewport(0, 0, surface_->w, surface_->h);
            SDLPU_CHECK_OPENGL_ERROR();
        }

    public:
        explicit Screen()
            : context_()
            , surface_() {}

        virtual ~Screen() {
            std::for_each(
                    texture_.begin(),
                    texture_.end(),
                    boost::checked_deleter<Texture>());
        }

        virtual void initialize() {
        }

        virtual void resize_before() {
            std::for_each(
                    texture_.begin(),
                    texture_.end(),
                    boost::bind(&Texture::opengl_delete, _1));
        }

        virtual void resize_after() {
            glEnable(GL_BLEND);
            glEnable(GL_CULL_FACE);
            glEnable(GL_DEPTH_TEST);
            glEnable(GL_NORMALIZE);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glCullFace(GL_BACK);
            glDepthFunc(GL_LEQUAL);
            SDLPU_CHECK_OPENGL_ERROR();

            std::for_each(
                    texture_.begin(),
                    texture_.end(),
                    boost::bind(&Texture::opengl_generate, _1));
        }

        virtual void draw() {
            std::for_each(
                    texture_.begin(),
                    texture_.end(),
                    boost::bind(&Texture::update, _1));
        }

        void set_context(Context* context) {
            BOOST_ASSERT(! context_);
            context_ = context;
        }

        Context& get_context() const {
            BOOST_ASSERT(context_);
            return *context_;
        }

        SDL_Surface& get_surface() const {
            BOOST_ASSERT(surface_);
            return *surface_;
        }

        Texture* make_texture() {
            texture_.push_back(new Texture());
            return texture_.back();
        }

        void resize(const SDL_ResizeEvent& event) {
            resize(event.w, event.h, false);
        }

        void resize(int width, int height, bool fullscreen) {
            unsigned int flags(SDL_OPENGL | SDL_RESIZABLE);
            if (fullscreen) {
                flags |= SDL_FULLSCREEN;
            }

            if (! surface_) {
                resize_surface_(width, height, flags);
                initialize();
                resize_after();
            } else {
                resize_before();
                resize_surface_(width, height, flags);
                resize_after();
            }
        }
    };
}

#endif
