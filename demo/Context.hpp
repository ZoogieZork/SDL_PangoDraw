// vim: set et ts=4 sts=4 sw=4:
#ifndef SDLPU_CONTEXT_HPP
#define SDLPU_CONTEXT_HPP

#include <list>
#include <boost/cast.hpp>
#include <boost/checked_delete.hpp>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>
#include "Config.hpp"
#include "Error.hpp"
#include "Event.hpp"
#include "Screen.hpp"
#include "SDL_Pango.hpp"

namespace SDL_PangoUtil {

    class Context
        : public boost::noncopyable
    {
        Config*                   config_;
        boost::scoped_ptr<Event>  event_;
        boost::scoped_ptr<Screen> screen_;
        std::list<SDLPango*>      pango_;

    public:
        explicit Context()
            : config_()
            , event_ ()
            , screen_()
            , pango_ () {}

        virtual ~Context() {
            std::for_each(
                    pango_.begin(),
                    pango_.end(),
                    boost::checked_deleter<SDLPango>());
        }

        void set_config(Config* config) {
            BOOST_ASSERT(! config_);
            config_ = config;
        }

        void set_event(Event* event) {
            BOOST_ASSERT(! event_);
            event_.reset(event);
            event_->set_context(this);
        }

        void set_screen(Screen* screen) {
            BOOST_ASSERT(! screen_);
            screen_.reset(screen);
            screen_->set_context(this);
        }

        Config& get_config() const {
            BOOST_ASSERT(config_);
            return *config_;
        }

        Event* get_event_ptr() const {
            BOOST_ASSERT(event_);
            return event_.get();
        }

        Screen* get_screen_ptr() const {
            BOOST_ASSERT(screen_);
            return screen_.get();
        }

        SDLPango* make_pango() {
            pango_.push_back(new SDLPango());
            return pango_.back();
        }
    };

    template <typename T_Event, typename T_Screen>
    class Context_Impl
        : public Context
    {
    public:
        static Context_Impl& cast(Context& ptr) {
            return *boost::polymorphic_cast<Context_Impl*>(&ptr);
        }

        T_Event& get_event() {
            return *boost::polymorphic_cast<T_Event*>(get_event_ptr());
        }

        T_Screen& get_screen() {
            return *boost::polymorphic_cast<T_Screen*>(get_screen_ptr());
        }
    };

    void
    Event::resize(const SDL_ResizeEvent& event) {
        get_context().get_screen_ptr()->resize(event);
    }
}

#endif
