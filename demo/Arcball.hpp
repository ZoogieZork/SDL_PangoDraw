// vim: set et ts=4 sts=4 sw=4:
#ifndef SDLPU_ARCBALL_HPP
#define SDLPU_ARCBALL_HPP

#include <boost/noncopyable.hpp>
#include "Error.hpp"
#include "Import.hpp"

namespace SDL_PangoUtil {

    class Arcball_Model
        : public boost::noncopyable
    {
        bool    dragging_;
        float   vsphere_radius_;
        Point2f vsphere_center_;
        Point2f start_mouse_;
        Quat4f  start_rotation_;
        Point2f current_mouse_;
        Quat4f  current_rotation_;

        Quat4f calc_mouse_on_vsphere_(const Point2f& mouse) {

            // v = (mouse - vsphere_center_) / vsphere_radius_
            Vector2f v(mouse);
            v.sub(vsphere_center_);
            v.scale(1 / vsphere_radius_);

            // mag = v dot v
            float mag(v.lengthSquared());

            Quat4f mouse_on_vsphere(v.x, v.y, 0, 0);
            if (mag > 1) {
                mouse_on_vsphere.scale(1 / std::sqrt(mag));
            } else {
                mouse_on_vsphere.z = std::sqrt(1 - mag);
            }

            return mouse_on_vsphere;
        }

    public:
        explicit Arcball_Model()
            : dragging_        ()
            , vsphere_radius_  (1)
            , vsphere_center_  ()
            , start_mouse_     ()
            , start_rotation_  (0, 0, 0, 1)
            , current_mouse_   ()
            , current_rotation_(0, 0, 0, 1) {}

        bool is_dragging() const {
            return dragging_;
        }

        void set_mouse(const Point2f& mouse) {
            current_mouse_.set(mouse);
        }

        void start_drag() {
            dragging_ = true;
            start_mouse_.set(current_mouse_);
        }

        void stop_drag() {
            dragging_ = false;
            start_rotation_.set(current_rotation_);
        }

        void update() {
            if (dragging_) {
                // construct a unit quaternion from 2 points on unit sphere
                //
                // (from Graphics GEMS IV P180)
                //     qu.x = from.y * to.z - from.z * to.y;
                //     qu.y = from.z * to.x - from.x * to.z;
                //     qu.z = from.x * to.y - from.y * to.x;
                //     qu.w = from.x * to.x + from.y * to.y + from.z*to.z;

                Quat4f q(calc_mouse_on_vsphere_(start_mouse_));
                q.mul(calc_mouse_on_vsphere_(current_mouse_));
                q.w *= -1;

                current_rotation_.set(q);
                current_rotation_.mul(start_rotation_);
            }
        }

        Quat4f get_rotation() const {
            return current_rotation_;
        }
    };

    class Arcball
        : public boost::noncopyable
    {
        Arcball_Model model_;

    public:
        enum Mouse_State {
            MOUSE_PRESSED  = SDL_PRESSED,
            MOUSE_RELEASED = SDL_RELEASED,
            MOUSE_DRAGGING = SDL_PRESSED + SDL_RELEASED + 1,
            MOUSE_UNKNOWN  = SDL_PRESSED + SDL_RELEASED + 2,
        };

        bool is_dragging() const {
            return model_.is_dragging();
        }

        void mouse(int mouse_state, int w, int h, int x, int y) {

            Tuple2f regularized_mouse(
                    +(2 * x / static_cast<float>(w) - 1),
                    -(2 * y / static_cast<float>(h) - 1));

            model_.set_mouse(regularized_mouse);
            model_.update();

            switch (mouse_state) {
                case MOUSE_PRESSED:
                    model_.start_drag();
                    break;
                case MOUSE_RELEASED:
                    model_.stop_drag();
                    break;
                case MOUSE_DRAGGING:
                    // noop
                    break;
                case MOUSE_UNKNOWN:
                    // noop
                    break;
            }
        }

        void transform() const {
            Matrix4f rotation;
            rotation.set(model_.get_rotation());
            rotation.transpose();
            glMultMatrixf(&rotation.m00);
            SDLPU_CHECK_OPENGL_ERROR();
        }

        void transform(Point3f& point) const {
            Matrix4f rotation;
            rotation.set(model_.get_rotation());
            rotation.transform(&point);
        }
    };
}

#endif
