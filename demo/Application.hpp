// vim: set et ts=4 sts=4 sw=4:
#ifndef SDLPU_APPLICATION_HPP
#define SDLPU_APPLICATION_HPP

#include <iostream>
#include <sstream>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>
#include "Config.hpp"
#include "Context.hpp"
#include "Event.hpp"
#include "Import.hpp"
#include "Utility.hpp"

namespace SDL_PangoUtil {

    class Application
        : public boost::noncopyable
    {
        Config config_;

        void run_(int ac, char* av[]) {
            SDL_Init(SDL_INIT_VIDEO);
            SDLPango::init();
            boost::shared_ptr<void> sdl_quit(
                    static_cast<void*>(0), boost::bind(SDL_Quit));

            boost::scoped_ptr<Context> context(initialize(ac, av, config_));
            const PO::variables_map& vm = context->get_config().get_vm();

            std::string caption(vm["Window.Caption"].as<std::string>());
            SDL_WM_SetCaption(caption.c_str(), caption.c_str());

            bool grab_input(vm["Window.GrabInput"].as<bool>());
            SDL_WM_GrabInput(grab_input ? SDL_GRAB_ON : SDL_GRAB_OFF);

            SDL_EnableUNICODE(1);
            SDL_EnableKeyRepeat(
                    SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
            SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

            Tuple2i size(get_size(vm["Window.Size"].as<std::string>()));
            bool fullscreen(vm["Window.Fullscreen"].as<bool>());
            context->get_screen_ptr()->resize(size.x, size.y, fullscreen);

            while (true) {
                SDL_Event event;
                while (SDL_PollEvent(&event)) {
                    context->get_event_ptr()->handle(event);
                }
                context->get_screen_ptr()->draw();
            }
        }

    public:
        explicit Application()
            : config_() {}

        virtual ~Application() {}
        virtual Context* initialize(int, char* [], Config&) =0;

        int run(int ac, char* av[]) {
            FS::path::default_name_check(boost::filesystem::native);

            try {
                run_(ac, av);
            }
            catch (const Event::Quit_Main_Loop&) {
                // noop
            }
#ifdef NDEBUG
            catch (const std::exception& e) {
                std::cerr << e.what() << std::endl;
                return 1;
            }
#endif
            return 0;
        }
    };
}

#endif
