// vim: set et ts=4 sts=4 sw=4:
#include <iostream>
#include <boost/scoped_ptr.hpp>
#include "Application.hpp"
#include "Context.hpp"
#include "SDL_Pango.hpp"
#include "Surface_Format.hpp"
#include "Error.hpp"
#include "Text.hpp"
#include "Utility.hpp"
#include "link_vc.hpp"

using namespace SDL_PangoUtil;

///////////////////////////////////////////////////////////////////////////

class Nieves_Event
    : public Event {};

///////////////////////////////////////////////////////////////////////////

class Nieves_Screen
    : public Screen
{
    boost::scoped_ptr<Text> text_;
    Texture*                texture_;
    Tuple2i                 margin_;
    FS::path                text_path_;
    std::time_t             text_written_;

    void resize_pango_();

public:
    explicit Nieves_Screen()
        : text_   ()
        , texture_() {}

    virtual void initialize();
    virtual void resize_after();
    virtual void draw();
};

///////////////////////////////////////////////////////////////////////////

typedef Context_Impl<Nieves_Event, Nieves_Screen> Nieves_Context;

///////////////////////////////////////////////////////////////////////////

void
Nieves_Screen::resize_pango_() {
    Tuple2i size(get_size(get_surface()));
    size -= margin_;
    size -= margin_;
    text_->get_pango().setMinimumSize(size.x, size.y);
    text_->set_modified();
}

void
Nieves_Screen::initialize() {
    Screen::initialize();

    std::cout
        << "Vendor    : " << glGetString(GL_VENDOR)     << std::endl
        << "Renderer  : " << glGetString(GL_RENDERER)   << std::endl
        << "Version   : " << glGetString(GL_VERSION)    << std::endl
        << "Extensions: " << glGetString(GL_EXTENSIONS) << std::endl
        ;

    int max_texture_units(0);
    glGetIntegerv(GL_MAX_TEXTURE_UNITS, &max_texture_units);
    std::cout
        << "MaxTextureUnits: " << max_texture_units << std::endl;

    Config& config = get_context().get_config();

    text_path_ = config.get_vm_path("File.Data");
    text_written_ = last_write_time(text_path_);

    BOOST_ASSERT(! text_);
    text_.reset(new Text(*this));
    text_->set_pango(get_context().make_pango());
    text_->load(text_path_);

    int dpi        (config.get_vm()["Pango.DPI"]       .as<int>());
    int line_height(config.get_vm()["Pango.LineHeight"].as<int>());
    SDLPango& pango(text_->get_pango());
    pango.setDefaultColor(MATRIX_TRANSPARENT_BACK_BLACK_LETTER);
    pango.setDpi(dpi, dpi);
    pango.setMinLineHeight(line_height);

    margin_ = get_size(config.get_vm()["Pango.Margin"].as<std::string>());
    resize_pango_();

    BOOST_ASSERT(! texture_);
    FS::path path(config.get_vm_path("File.Texture"));
    if (exists(path)) {
        texture_ = make_texture();
        texture_->load(path);
    }
}

void
Nieves_Screen::resize_after() {
    Screen::resize_after();

    if (texture_) {
        glActiveTexture(GL_TEXTURE0);
        Texture::opengl_parameter_mipmap_repeat();
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

        glActiveTexture(GL_TEXTURE1);
        Texture::opengl_parameter_mipmap_clamp();
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
    } else {
        glActiveTexture(GL_TEXTURE0);
        Texture::opengl_parameter_mipmap_clamp();
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
    }

    resize_pango_();
}

void
Nieves_Screen::draw() {
    std::time_t text_written(last_write_time(text_path_));
    if (text_written > text_written_) {
        text_written_ = text_written;
        text_->load(text_path_);
    }

    text_->update(margin_);
    Screen::draw();

    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    SDLPU_CHECK_OPENGL_ERROR();

    glMatrixMode(GL_PROJECTION);
    {
        Tuple2i size(get_size(get_surface()));
        glLoadIdentity();
        glOrtho(0, size.x, -size.y, 0, -1, 1);
    }
    SDLPU_CHECK_OPENGL_ERROR();

    Tuple2i layout(text_->get_layout());
    layout += margin_;

    if (texture_) {
        glActiveTexture(GL_TEXTURE1);
    } else {
        glActiveTexture(GL_TEXTURE0);
    }
    Texture::opengl_enable();
    Texture& texture = text_->get_texture();
    texture.opengl_bind();
    glMatrixMode(GL_TEXTURE);
    {
        Tuple2i size(get_size(texture.get_surface()));

        Matrix4f matrix;
        matrix.setIdentity();
        matrix.m00 = layout.x / static_cast<float>(size.x);
        matrix.m11 = layout.y / static_cast<float>(size.y);
        matrix.transpose();

        glLoadIdentity();
        glMultMatrixf(&matrix.m00);
    }
    SDLPU_CHECK_OPENGL_ERROR();

    if (texture_) {
        glActiveTexture(GL_TEXTURE0);
        Texture::opengl_enable();
        texture_->opengl_bind();
        glMatrixMode(GL_TEXTURE);
        {
            Tuple2i size(get_size(texture_->get_surface()));

            Matrix4f matrix;
            matrix.setIdentity();
            matrix.m00 = layout.x / static_cast<float>(size.x);
            matrix.m11 = layout.y / static_cast<float>(size.y);
            matrix.transpose();

            glLoadIdentity();
            glMultMatrixf(&matrix.m00);
        }
        SDLPU_CHECK_OPENGL_ERROR();
    }

    glMatrixMode(GL_MODELVIEW);
    {
        glLoadIdentity();
    }
    SDLPU_CHECK_OPENGL_ERROR();

    glBegin(GL_QUADS);
    glColor4f(1, 1, 1, 1);
    {
        glMultiTexCoord2f(GL_TEXTURE0, 0, 0);
        glMultiTexCoord2f(GL_TEXTURE1, 0, 0);
        glVertex2f(0, 0);
        glMultiTexCoord2f(GL_TEXTURE0, 0, 1);
        glMultiTexCoord2f(GL_TEXTURE1, 0, 1);
        glVertex2f(0, -layout.y);
        glMultiTexCoord2f(GL_TEXTURE0, 1, 1);
        glMultiTexCoord2f(GL_TEXTURE1, 1, 1);
        glVertex2f(layout.x, -layout.y);
        glMultiTexCoord2f(GL_TEXTURE0, 1, 0);
        glMultiTexCoord2f(GL_TEXTURE1, 1, 0);
        glVertex2f(layout.x, 0);
    }
    glEnd();
    SDLPU_CHECK_OPENGL_ERROR();

    glActiveTexture(GL_TEXTURE0);
    Texture::opengl_disable();
    if (texture_) {
        glActiveTexture(GL_TEXTURE1);
        Texture::opengl_disable();
    }

    SDL_GL_SwapBuffers();
}

///////////////////////////////////////////////////////////////////////////

class Nieves_Application
    : public Application
{
public:
    virtual Context* initialize(int ac, char* av[], Config& config) {
        PO::options_description desc("Additional Parameters");
        desc.add_options()
                ("Pango.DPI",        PO::value<int>        (), "")
                ("Pango.Margin",     PO::value<std::string>(), "")
                ("Pango.LineHeight", PO::value<int>        (), "")
                ("File.Data",        PO::value<std::string>(), "")
                ("File.Texture",     PO::value<std::string>(), "")
                ;
        config.add_desc(desc);
        config.load(ac, av);

        Nieves_Context* context(new Nieves_Context());
        context->set_config(&config);
        context->set_screen(new Nieves_Screen());
        context->set_event (new Nieves_Event ());
        return context;
    }
};

///////////////////////////////////////////////////////////////////////////

int
main(int ac, char* av[]) {
    boost::scoped_ptr<Nieves_Application> app(new Nieves_Application());
    return app->run(ac, av);
}

///////////////////////////////////////////////////////////////////////////
