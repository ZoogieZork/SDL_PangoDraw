// vim: set et ts=4 sts=4 sw=4:
#ifndef SDLPU_CONFIG_HPP
#define SDLPU_CONFIG_HPP

#include <boost/noncopyable.hpp>
#include "Error.hpp"
#include "Import.hpp"
#include "Utility.hpp"

namespace SDL_PangoUtil {

    class Config
        : public boost::noncopyable
    {
        FS::path                dirpath_;
        FS::path                filepath_;
        PO::options_description desc_;
        PO::variables_map       vm_;

        void setup_dirpath_(const FS::path& program) {
            static const char* subdir[] = {
                "demo_data",
                "../demo_data",
                "../../demo_data",
                "../../../demo_data",
                0,
            };

            FS::path dirpath(program.branch_path());
            for (int i(0); subdir[i]; ++i) {
                FS::path path(dirpath);
                path /= subdir[i];
                if (exists(path) && is_directory(path)) {
                    dirpath_ = path;
                    return;
                }
            }

            SDLPU_THROW_RUNTIME_ERROR(("Could not setup_dirpath_"));
        }

        void setup_filepath_(const FS::path& program) {
            FS::path path(dirpath_);
            path /= basename(program);
            filepath_ = change_extension(path, ".ini");

            if (! exists(filepath_)) {
                SDLPU_THROW_RUNTIME_ERROR(("Could not setup_filepath_"));
            }
        }

    public:
        explicit Config()
            : desc_("Basic Parameters")
        {
            desc_.add_options()
                ("Window.Caption",    PO::value<std::string>(), "")
                ("Window.Size",       PO::value<std::string>(), "")
                ("Window.Fullscreen", PO::value<bool>       (), "")
                ("Window.GrabInput",  PO::value<bool>       (), "")
                ;
        }

        const FS::path& get_dirpath() const {
            return dirpath_;
        }

        const FS::path& get_filepath() const {
            return filepath_;
        }

        void add_desc(const PO::options_description& desc) {
            desc_.add(desc);
        }

        void load(int ac, char* av[]) {
            FS::path program(av[0]);
            setup_dirpath_ (program);
            setup_filepath_(program);

            Istream_Ptr in(open_ifstream(filepath_));
            PO::store(PO::parse_config_file(*in, desc_), vm_);
            PO::notify(vm_);
        }

        const PO::variables_map& get_vm() const {
            return vm_;
        }

        FS::path get_vm_path(const std::string& key) const {
            FS::path path(dirpath_);
            path /= vm_[key].as<std::string>();
            return path;
        }
    };
}

#endif
