// vim: set et ts=4 sts=4 sw=4:
#ifndef SDLPU_ERROR_HPP
#define SDLPU_ERROR_HPP

#include <cstdio>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <boost/assert.hpp>
#include <boost/noncopyable.hpp>
#include "Import.hpp"

#ifdef WIN32
# define SDLPU_VSNPRINTF _vsnprintf
#else
# define SDLPU_VSNPRINTF  vsnprintf
#endif

#define SDLPU_THROW_RUNTIME_ERROR(args) \
        SDL_PangoUtil::Throw_Runtime_Error(__FILE__, __LINE__) args

#define SDLPU_CHECK_OPENGL_ERROR() \
        SDL_PangoUtil::Check_OpenGL_Error(__FILE__, __LINE__)()

namespace SDL_PangoUtil {

    class Throw_Runtime_Error
        : public boost::noncopyable
    {
        const char* file_;
        int         line_;

    public:
        explicit Throw_Runtime_Error(const char* file, int line)
            : file_(file)
            , line_(line) {}

        void operator()(const char* format, ...) const {
            static const int BUFFER_SIZE = 1024;

            char buffer[BUFFER_SIZE];
            std::fill(buffer, buffer + BUFFER_SIZE, 0);

            va_list list;
            va_start(list, format);
            SDLPU_VSNPRINTF(buffer, BUFFER_SIZE, format, list);
            va_end(list);

            std::ostringstream out;
            out << "[" << file_ << "," << line_ << "] " << buffer;
            throw std::runtime_error(out.str());
        }
    };

    class Check_OpenGL_Error
        : public boost::noncopyable
    {
        const char* file_;
        int         line_;

    public:
        explicit Check_OpenGL_Error(const char* file, int line)
            : file_(file)
            , line_(line) {}

        void operator()() const {
#if 0
            GLenum err(glGetError());

            if (err != GL_NO_ERROR) {
                Throw_Runtime_Error thrower(file_, line_);
                thrower("OpenGL Error: %s", gluErrorString(err));
            }
#else
            GLenum err(GL_NO_ERROR);
            while ((err = glGetError()) != GL_NO_ERROR) {
                std::cerr
                    << "[" << file_ << "," << line_ << "] "
                    << gluErrorString(err) << std::endl;
            }
#endif
        }
    };
}

#endif
