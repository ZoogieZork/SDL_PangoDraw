// vim: set et ts=4 sts=4 sw=4:
#include <iostream>
#include <vector>
#include <boost/cast.hpp>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include "Animation.hpp"
#include "Arcball.hpp"
#include "Application.hpp"
#include "Context.hpp"
#include "Model.hpp"
#include "Surface_Format.hpp"
#include "SDL_Pango.hpp"
#include "Error.hpp"
#include "Text.hpp"
#include "Utility.hpp"
#include "link_vc.hpp"

using namespace SDL_PangoUtil;

///////////////////////////////////////////////////////////////////////////

class Vapors_Event
    : public Event
{
    Arcball arcball_;

public:
    virtual void keyboard(const SDL_KeyboardEvent&);
    virtual void mouse_motion(const SDL_MouseMotionEvent&);
    virtual void mouse_button(const SDL_MouseButtonEvent&);

    Arcball& get_arcball();
};

///////////////////////////////////////////////////////////////////////////

class Vapors_Screen
    : public Screen
{
public:
    typedef boost::shared_ptr<Model> Model_Ptr;
    typedef boost::shared_ptr<Text>  Text_Ptr;

private:
    Model_Ptr                    background_;
    std::vector<Model_Ptr>       page_;
    std::vector<Text_Ptr>        text_;
    int                          text_left_;
    int                          text_right_;

    Tuple2i                      size_;
    Tuple2i                      margin_;

    float                        distance_;
    boost::scoped_ptr<Animation> animation_;
    boost::tribool               animation_forward_;

    Arcball& get_arcball_();
    void setup_texture_matrix_        (const Texture&);
    void setup_texture_matrix_reverse_(const Texture&);

public:
    explicit Vapors_Screen()
        : background_       ()
        , page_             ()
        , text_             ()
        , text_left_        (0)
        , text_right_       (1)
        , size_             ()
        , margin_           ()
        , distance_         ()
        , animation_        ()
        , animation_forward_(boost::logic::indeterminate) {}

    void set_distance(float);
    float get_distance() const;
    void next();
    void prev();

    virtual void initialize();
    virtual void resize_after();
    virtual void draw();
};

///////////////////////////////////////////////////////////////////////////

typedef Context_Impl<Vapors_Event, Vapors_Screen> Vapors_Context;

///////////////////////////////////////////////////////////////////////////

Arcball&
Vapors_Event::get_arcball() {
    return arcball_;
}

void
Vapors_Event::keyboard(const SDL_KeyboardEvent& event) {
    if (event.state == SDL_PRESSED) {
        switch (event.keysym.sym) {
            case SDLK_PAGEUP:
                {
                    Vapors_Screen& screen(
                            Vapors_Context::cast(get_context()).get_screen());
                    screen.prev();
                }
                break;
            case SDLK_PAGEDOWN:
                {
                    Vapors_Screen& screen(
                            Vapors_Context::cast(get_context()).get_screen());
                    screen.next();
                }
                break;
        }
    }

    Event::keyboard(event);
}

void
Vapors_Event::mouse_motion(const SDL_MouseMotionEvent& event) {
    if (arcball_.is_dragging()) {
        Tuple2i size(get_size(get_context().get_screen_ptr()->get_surface()));
        arcball_.mouse(
                Arcball::MOUSE_DRAGGING, size.x, size.y, event.x, event.y);
    }
}

void
Vapors_Event::mouse_button(const SDL_MouseButtonEvent& event) {
    switch (event.button) {
        case SDL_BUTTON_RIGHT:
            {
                Tuple2i size(
                        get_size(
                                get_context().get_screen_ptr()->get_surface()));
                arcball_.mouse(event.state, size.x, size.y, event.x, event.y);
            }
            break;
        case SDL_BUTTON_WHEELUP:
            {
                Vapors_Screen& screen(
                        Vapors_Context::cast(get_context()).get_screen());
                screen.set_distance(screen.get_distance() / 1.05);
            }
            break;
        case SDL_BUTTON_WHEELDOWN:
            {
                Vapors_Screen& screen(
                        Vapors_Context::cast(get_context()).get_screen());
                screen.set_distance(screen.get_distance() * 1.05);
            }
            break;
    }
}

///////////////////////////////////////////////////////////////////////////

Arcball&
Vapors_Screen::get_arcball_() {
    return Vapors_Context::cast(get_context()).get_event().get_arcball();
}

void
Vapors_Screen::set_distance(float distance) {
    distance_ = distance;
}

float
Vapors_Screen::get_distance() const {
    return distance_;
}

void
Vapors_Screen::next() {
    if ((! animation_->is_active()) && text_right_ + 2 < text_.size()) {
        animation_->start();
        animation_forward_ = true;
        text_right_ += 2;
    }
}

void
Vapors_Screen::prev() {
    if ((! animation_->is_active()) && text_left_ - 2 >= 0) {
        animation_->start();
        animation_forward_ = false;
        text_left_ -= 2;
    }
}

void
Vapors_Screen::initialize() {
    Screen::initialize();

    Config& config = get_context().get_config();
    background_.reset(new Model());
    background_->load(*this, config.get_vm_path("File.Background"));

    boost::format page(config.get_vm()["File.Page"].as<std::string>());
    int page_begin(config.get_vm()["File.PageBegin"].as<int>());
    int page_end  (config.get_vm()["File.PageEnd"]  .as<int>());
    for (int i(page_begin); i != page_end; ++i) {
        std::ostringstream out;
        out << page % i;

        FS::path path(config.get_dirpath());
        path /= out.str();

        Model_Ptr model(new Model());
        model->load(*this, path);
        page_.push_back(model);
    }

    int dpi        (config.get_vm()["Pango.DPI"]       .as<int>());
    int line_height(config.get_vm()["Pango.LineHeight"].as<int>());

    margin_ = get_size(config.get_vm()["Pango.Margin"]    .as<std::string>());
    size_   = get_size(config.get_vm()["Pango.LayoutSize"].as<std::string>());

    Tuple2i size(size_);
    size -= margin_;
    size -= margin_;

    for (int i(0); ; ++i) {
        FS::path path(config.get_vm_path("File.Data"));
        path /= boost::lexical_cast<std::string>(i);
        if (! exists(path)) {
            break;
        }

        Text_Ptr text(new Text(*this));
        text_.push_back(text);

        text->set_pango(get_context().make_pango());
        text->load(path);

        SDLPango& pango(text->get_pango());
        pango.setDefaultColor(MATRIX_TRANSPARENT_BACK_BLACK_LETTER);
        pango.setDpi(dpi, dpi);
        pango.setMinLineHeight(line_height);
        pango.setMinimumSize(size.x, size.y);
        text->set_modified();
    }

    distance_ = config.get_vm()["Animation.Distance"].as<int>();
    animation_.reset(
            new Animation(
                    config.get_vm()["Animation.Length"].as<int>(),
                    page_.size() - 1));
}

void
Vapors_Screen::resize_after() {
    Screen::resize_after();

    glActiveTexture(GL_TEXTURE0);
    Texture::opengl_parameter_mipmap_repeat();
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    glActiveTexture(GL_TEXTURE1);
    Texture::opengl_parameter_mipmap_clamp();
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);

    glEnable(GL_LIGHTING);
    // setup lighting
    {
        // light 0
        static const float ambient [] = { 0.1f, 0.1f, 0.1f, 1.0f };
        static const float diffuse [] = { 0.8f, 0.8f, 0.8f, 1.0f };
        static const float specular[] = { 0.0f, 0.0f, 0.0f, 1.0f };

        glEnable (GL_LIGHT0);
        glLightfv(GL_LIGHT0, GL_AMBIENT,  ambient);
        glLightfv(GL_LIGHT0, GL_DIFFUSE,  diffuse);
        glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
        SDLPU_CHECK_OPENGL_ERROR();
    }
    {
        // light model
        static const float ambient[] = { 0.6f, 0.6f, 0.6f, 1.0f };
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
        SDLPU_CHECK_OPENGL_ERROR();
    }
}

void
Vapors_Screen::setup_texture_matrix_(const Texture& texture) {
    glMatrixMode(GL_TEXTURE);
    {
        Tuple2i size(get_size(texture.get_surface()));

        Matrix4f matrix;
        matrix.setIdentity();
        matrix.m00 =   size_.x / static_cast<float>(size.x);
        matrix.m11 = - size_.y / static_cast<float>(size.y);
        matrix.m13 =   size_.y / static_cast<float>(size.y);
        matrix.transpose();

        glLoadIdentity();
        glMultMatrixf(&matrix.m00);
    }
    SDLPU_CHECK_OPENGL_ERROR();
}

void
Vapors_Screen::setup_texture_matrix_reverse_(const Texture& texture) {
    glMatrixMode(GL_TEXTURE);
    {
        Tuple2i size(get_size(texture.get_surface()));

        Matrix4f matrix;
        matrix.setIdentity();
        matrix.m00 = - size_.x / static_cast<float>(size.x);
        matrix.m03 =   size_.x / static_cast<float>(size.x);
        matrix.m11 = - size_.y / static_cast<float>(size.y);
        matrix.m13 =   size_.y / static_cast<float>(size.y);
        matrix.transpose();

        glLoadIdentity();
        glMultMatrixf(&matrix.m00);
    }
    SDLPU_CHECK_OPENGL_ERROR();
}

void
Vapors_Screen::draw() {
    std::for_each(
            text_.begin(),
            text_.end(),
            boost::bind(&Text::update, _1, boost::cref(margin_)));
    Screen::draw();

    animation_->update();
    if (animation_->is_done()) {
        animation_->unset_done();

        if (animation_forward_) {
            text_left_ += 2;
        }
        else if (! animation_forward_) {
            text_right_ -= 2;
        }
    }

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    SDLPU_CHECK_OPENGL_ERROR();

    glMatrixMode(GL_PROJECTION);
    {
        Tuple2i size(get_size(get_surface()));
        double aspect(size.x / static_cast<double>(size.y));

        glLoadIdentity();
        gluPerspective(29.11, aspect, 256, 4096);
    }
    SDLPU_CHECK_OPENGL_ERROR();

    glActiveTexture(GL_TEXTURE0);
    glMatrixMode(GL_TEXTURE);
    {
        glLoadIdentity();
    }
    SDLPU_CHECK_OPENGL_ERROR();

    glMatrixMode(GL_MODELVIEW);
    {
        glLoadIdentity();
        gluLookAt(0, 0, distance_, 0, 0, 0, 0, 1, 0);
        get_arcball_().transform();
    }
    SDLPU_CHECK_OPENGL_ERROR();

    background_->draw(DRAW_FRONT);

    {
        glActiveTexture(GL_TEXTURE1);
        Texture::opengl_enable();
        Texture& texture = text_.at(text_left_)->get_texture();
        texture.opengl_bind();
        setup_texture_matrix_reverse_(texture);
        {
            page_.front()->draw(DRAW_FRONT | MIRROR_X);
        }
        glActiveTexture(GL_TEXTURE1);
        Texture::opengl_disable();
    }

    {
        glActiveTexture(GL_TEXTURE1);
        Texture::opengl_enable();
        Texture& texture = text_.at(text_right_)->get_texture();
        texture.opengl_bind();
        setup_texture_matrix_(texture);
        {
            page_.front()->draw(DRAW_FRONT);
        }
        glActiveTexture(GL_TEXTURE1);
        Texture::opengl_disable();
    }

    if (animation_->is_active()) {
        std::pair<int, float> frame(animation_->get_frame());

        if (animation_forward_) {
            {
                glActiveTexture(GL_TEXTURE1);
                Texture::opengl_enable();
                Texture& texture = text_.at(text_left_ + 1)->get_texture();
                texture.opengl_bind();
                setup_texture_matrix_(texture);
                {
                    page_.at(frame.first)->interpolating_draw(
                            *page_.at(frame.first + 1),
                            frame.second,
                            DRAW_FRONT);
                }
                glActiveTexture(GL_TEXTURE1);
                Texture::opengl_disable();            
            }

            {
                glActiveTexture(GL_TEXTURE1);
                Texture::opengl_enable();
                Texture& texture = text_.at(text_left_ + 2)->get_texture();
                texture.opengl_bind();
                setup_texture_matrix_reverse_(texture);
                {
                    page_.at(frame.first)->interpolating_draw(
                            *page_.at(frame.first + 1),
                            frame.second,
                            DRAW_BACK);
                }
                glActiveTexture(GL_TEXTURE1);
                Texture::opengl_disable();            
            }
        } else {
            {
                glActiveTexture(GL_TEXTURE1);
                Texture::opengl_enable();
                Texture& texture = text_.at(text_right_ - 1)->get_texture();
                texture.opengl_bind();
                setup_texture_matrix_reverse_(texture);
                {
                    page_.at(frame.first)->interpolating_draw(
                            *page_.at(frame.first + 1),
                            frame.second,
                            DRAW_FRONT | MIRROR_X);
                }
                glActiveTexture(GL_TEXTURE1);
                Texture::opengl_disable();            
            }

            {
                glActiveTexture(GL_TEXTURE1);
                Texture::opengl_enable();
                Texture& texture = text_.at(text_right_ - 2)->get_texture();
                texture.opengl_bind();
                setup_texture_matrix_(texture);
                {
                    page_.at(frame.first)->interpolating_draw(
                            *page_.at(frame.first + 1),
                            frame.second,
                            DRAW_BACK | MIRROR_X);
                }
                glActiveTexture(GL_TEXTURE1);
                Texture::opengl_disable();            
            }
        }
    }

    SDL_GL_SwapBuffers();
}

///////////////////////////////////////////////////////////////////////////

class Vapors_Application
    : public Application
{
public:
    virtual Context* initialize(int ac, char* av[], Config& config) {
        PO::options_description desc("Additional Parameters");
        desc.add_options()
                ("Pango.DPI",          PO::value<int>        (), "")
                ("Pango.Margin",       PO::value<std::string>(), "")
                ("Pango.LayoutSize",   PO::value<std::string>(), "")
                ("Pango.LineHeight",   PO::value<int>        (), "")
                ("File.Data",          PO::value<std::string>(), "")
                ("File.Background",    PO::value<std::string>(), "")
                ("File.Page",          PO::value<std::string>(), "")
                ("File.PageBegin",     PO::value<int>        (), "")
                ("File.PageEnd",       PO::value<int>        (), "")
                ("Animation.Distance", PO::value<int>        (), "")
                ("Animation.Length",   PO::value<int>        (), "")
                ;
        config.add_desc(desc);
        config.load(ac, av);

        Vapors_Context* context(new Vapors_Context());
        context->set_config(&config);
        context->set_screen(new Vapors_Screen());
        context->set_event (new Vapors_Event ());
        return context;
    }
};

///////////////////////////////////////////////////////////////////////////

int
main(int ac, char* av[]) {
    boost::scoped_ptr<Vapors_Application> app(new Vapors_Application());
    return app->run(ac, av);
}

///////////////////////////////////////////////////////////////////////////
