// vim: set et ts=4 sts=4 sw=4:
#ifndef SDLPU_EVENT_HPP
#define SDLPU_EVENT_HPP

#include <boost/blank.hpp>
#include <boost/noncopyable.hpp>
#include <SDL.h>
#include "Error.hpp"

namespace SDL_PangoUtil {

    class Context;

    class Event
        : public boost::noncopyable
    {
    public:
        typedef boost::blank Quit_Main_Loop;

    private:
        Context* context_;

    public:
        explicit Event()
            : context_() {}

        virtual ~Event() {}

        virtual void keyboard(const SDL_KeyboardEvent& event) {
            if (event.state == SDL_PRESSED && event.keysym.sym == SDLK_ESCAPE) {
                throw Quit_Main_Loop();
            }
        }

        virtual void mouse_motion(const SDL_MouseMotionEvent& event) {
            // noop
        }

        virtual void mouse_button(const SDL_MouseButtonEvent& event) {
            // noop
        }

        virtual void resize(const SDL_ResizeEvent&);
        // implemeted at Context.hpp

        void set_context(Context* context) {
            BOOST_ASSERT(! context_);
            context_ = context;
        }

        Context& get_context() {
            BOOST_ASSERT(context_);
            return *context_;
        }

        void handle(const SDL_Event& event) {
            switch (event.type) {
                case SDL_KEYDOWN:
                case SDL_KEYUP:
                    keyboard(event.key);
                    break;
                case SDL_MOUSEMOTION:
                    mouse_motion(event.motion);
                    break;
                case SDL_MOUSEBUTTONDOWN:
                case SDL_MOUSEBUTTONUP:
                    mouse_button(event.button);
                    break;
                case SDL_VIDEORESIZE:
                    resize(event.resize);
                    break;
                case SDL_QUIT:
                    throw Quit_Main_Loop();
                    break;
            }
        }
    };
}

#endif
