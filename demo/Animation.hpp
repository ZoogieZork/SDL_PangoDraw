// vim: set et ts=4 sts=4 sw=4:
#ifndef SDLPU_ANIMATION_HPP
#define SDLPU_ANIMATION_HPP

#include "Import.hpp"
#include <boost/logic/tribool.hpp>

namespace SDL_PangoUtil {

    class Animation {
        boost::logic::tribool done_;
        int                   start_;
        int                   current_;
        int                   length_;
        int                   frame_;

    public:
        explicit Animation(int length, int frame)
            : done_   (boost::logic::indeterminate)
            , start_  ()
            , current_(length)
            , length_ (length)
            , frame_  (frame) {}

        bool is_done() const {
            return done_;
        }

        void unset_done() {
            done_ = boost::logic::indeterminate;
        }

        bool is_active() const {
            return current_ - start_ < length_;
        }

        void start() {
            BOOST_ASSERT(! is_active());
            done_ = false;
            start_ = current_ = SDL_GetTicks();
        }

        void update() {
            current_ = SDL_GetTicks();
            if (is_active()) {
                if (indeterminate(done_)) {
                    done_ = false;
                }
            } else {
                if (! done_) {
                    done_ = true;
                }
            }
        }

        float get_alpha() const {
            BOOST_ASSERT(is_active());
            return (current_ - start_) / static_cast<float>(length_);
        }

        std::pair<int, float> get_frame() const {
            BOOST_ASSERT(is_active());
            float frame(get_alpha() * frame_);

            int   i_of_frame(static_cast<int>(frame));
            float f_of_frame(frame - i_of_frame);

            return std::make_pair(i_of_frame, f_of_frame);
        }
    };
}

#endif
