// vim: set et ts=4 sts=4 sw=4:
#ifndef SDLPU_IMPORT_HPP
#define SDLPU_IMPORT_HPP

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/convenience.hpp>
#include <boost/program_options.hpp>
#include <vecmath.h>

#define NO_SDL_GLEXT
#include <SDL.h>
#include <SDL_opengl.h>
#include <GL/glprocs.h>

namespace SDL_PangoUtil {
    namespace FS = boost::filesystem;
    namespace PO = boost::program_options;

    using namespace kh_vecmath;
    typedef Tuple2<int> Tuple2i;
    typedef Tuple3<int> Tuple3i;
    typedef Tuple4<int> Tuple4i;
}

#endif
