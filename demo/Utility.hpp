// vim: set et ts=4 sts=4 sw=4:
#ifndef SDLPU_UTILITY_HPP
#define SDLPU_UTILITY_HPP

#include <memory>
#include <SDL.h>
#include "Error.hpp"
#include "Import.hpp"

namespace SDL_PangoUtil {

    inline Tuple2i
    get_size(const SDL_Surface& surface) {
        return Tuple2i(surface.w, surface.h);
    }

    inline Tuple2i
    get_size(const std::string& value) {
        Tuple2i size;

        std::istringstream in(value);
        in >> size.x >> size.y;
        return size;
    }

    typedef std::auto_ptr<std::istream> Istream_Ptr;

    inline Istream_Ptr
    open_ifstream(
        const FS::path&    path,
        std::ios::openmode mode = std::ios::in | std::ios::binary)
    {
        Istream_Ptr istream(new FS::ifstream(path, mode));
        if (! *istream) {
            SDLPU_THROW_RUNTIME_ERROR((
                    "Could not open %s", path.string().c_str()));

        }
        return istream;
    }
}

#endif
