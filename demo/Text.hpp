// vim: set et ts=4 sts=4 sw=4:
#ifndef SDLPU_TEXT_HPP
#define SDLPU_TEXT_HPP

#include <string>
#include <vector>
#include "Import.hpp"
#include "Screen.hpp"
#include "SDL_Pango.hpp"
#include "Texture.hpp"

namespace SDL_PangoUtil {

    class Text {
    public:
        enum Mode { MODE_MARKUP, MODE_TEXT };

        class Concat {
            std::string* result_;
        public:
            explicit Concat(std::string& result)
                : result_(&result) {}

            void operator()(const std::string& value) {
                *result_ += value;
                *result_ += "\n";
            }
        };

        static const SDLPango::Surface_Create_Args& get_surface_create_args() {
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
            static const SDLPango::Surface_Create_Args args(
                    SDL_SWSURFACE, 32,
                    0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
#else
            static const SDLPango::Surface_Create_Args args(
                    SDL_SWSURFACE, 32,
                    0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
#endif
            return args;
        }

    private:
        SDLPango*                pango_;
        Mode                     mode_;
        std::vector<std::string> text_;
        Texture*                 texture_;
        Tuple2i                  layout_;
        bool                     modified_;

    public:
        explicit Text(Screen& screen)
            : pango_   ()
            , mode_    (MODE_MARKUP)
            , text_    ()
            , texture_ (screen.make_texture())
            , layout_  ()
            , modified_() {}

        void set_pango(SDLPango* pango) {
            pango_ = pango;
            pango_->setSurfaceCreateArgs(get_surface_create_args());
        }

        void set_mode(Mode mode) {
            mode_     = mode;
            modified_ = true;
        }

        void set_modified() {
            modified_ = true;
        }

        SDLPango& get_pango() const {
            return *pango_;
        }

        Mode get_mode() const {
            return mode_;
        }

        std::vector<std::string>& get_text() {
            return text_;
        }

        Texture& get_texture() {
            return *texture_;
        }

        const Tuple2i& get_layout() const {
            return layout_;
        }

        void load(const FS::path& filepath) {
            FS::ifstream in(filepath);
            if (! in) {
                SDLPU_THROW_RUNTIME_ERROR((
                        "Could not open %s", filepath.string().c_str()));
            }

            text_.clear();
            while (in) {
                std::string line;
                std::getline(in, line);
                text_.push_back(line);
            }
            modified_ = true;
        }

        void update(const Tuple2i& margin = Tuple2i()) {
            if (! modified_) {
                texture_->update();
                return;
            }

            std::string text;
            std::for_each(text_.begin(), text_.end(), Concat(text));

            switch (mode_) {
                case MODE_MARKUP: pango_->setMarkup(text); break;
                case MODE_TEXT:   pango_->setText  (text); break;
            }

            layout_.x = pango_->getLayoutWidth ();
            layout_.y = pango_->getLayoutHeight();
            layout_ += margin;

            if (texture_->has_surface()) {
                Tuple2i size(get_size(texture_->get_surface()));
                if (size.x < layout_.x || size.y < layout_.y) {
                    texture_->resize(layout_.x, layout_.y);
                }
            } else {
                const SDLPango::Surface_Create_Args& args(
                        get_surface_create_args());

                texture_->set_surface(
                        SDL_CreateRGBSurface(
                                args.get_flags(),
                                layout_.x,
                                layout_.y,
                                args.get_depth(),
                                args.get_rmask(),
                                args.get_gmask(),
                                args.get_bmask(),
                                args.get_amask()));
            }

            pango_->draw(&texture_->get_surface(), margin.x, margin.y);
            texture_->set_modified();
            texture_->update();

            modified_ = false;
        }
    };
}

#endif
