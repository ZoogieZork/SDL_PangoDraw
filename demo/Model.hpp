// vim: set et ts=4 sts=4 sw=4:
#ifndef SDLPU_MODEL_HPP
#define SDLPU_MODEL_HPP

#include <string>
#include <boost/algorithm/string/split.hpp>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/scoped_ptr.hpp>
#include "Error.hpp"
#include "Import.hpp"
#include "Screen.hpp"
#include "Texture.hpp"

namespace SDL_PangoUtil {

    typedef boost::uint8_t Draw_Option;

    enum {
        DRAW_FRONT            = 0x00,
        DRAW_BACK             = 0x01,
        DRAW_REVERSE_INTERNAL = 0x02,
        MIRROR_X              = 0x10,
        MIRROR_Y              = 0x20,
        MIRROR_Z              = 0x40,
    };

    class Material {
        Color4f  ambient_;
        Color4f  diffuse_;
        Color4f  specular_;
        float    shininess_;
        Texture* texture_;

    public:
        explicit Material()
            : ambient_  ()
            , diffuse_  ()
            , specular_ ()
            , shininess_()
            , texture_  () {}

        void set_ambient(const Color4f& ambient) {
            ambient_ = ambient;
        }

        void set_diffuse(const Color4f& diffuse) {
            diffuse_ = diffuse;
        }

        void set_specular(const Color4f& specular) {
            specular_ = specular;
        }

        void set_shininess(float shininess) {
            shininess_ = shininess;
        }

        void set_texture(Texture* texture) {
            BOOST_ASSERT(! texture_);
            texture_ = texture;
        }

        Texture& get_texture() const {
            BOOST_ASSERT(texture_);
            return *texture_;
        }

        void draw() const {
            glMaterialfv(GL_FRONT, GL_AMBIENT,   &ambient_.x);
            glMaterialfv(GL_FRONT, GL_DIFFUSE,   &diffuse_.x);
            glMaterialfv(GL_FRONT, GL_SPECULAR,  &specular_.x);
            glMaterialf (GL_FRONT, GL_SHININESS, shininess_);
            SDLPU_CHECK_OPENGL_ERROR();

            if (texture_) {
                glActiveTexture(GL_TEXTURE0);
                Texture::opengl_enable();
                Texture::opengl_parameter_mipmap_repeat();

                texture_->opengl_bind();
                glMatrixMode(GL_TEXTURE);
                {
                    glLoadIdentity();
                }
                SDLPU_CHECK_OPENGL_ERROR();
            } else {
                glActiveTexture(GL_TEXTURE0);
                Texture::opengl_disable();
            }
        }
    };

    class Vertex {
        const Point3f*    vertex_;
        const TexCoord2f* texcoord_;
        const Vector3f*   normal_;

        static void mirror_(Draw_Option option, Tuple3f& value) {
            if (option & MIRROR_X) { value.x *= -1; }
            if (option & MIRROR_Y) { value.y *= -1; }
            if (option & MIRROR_Z) { value.z *= -1; }
        }

    public:
        explicit Vertex()
            : vertex_  ()
            , texcoord_()
            , normal_  () {}

        void set_vertex(const Point3f* vertex) {
            BOOST_ASSERT(! vertex_);
            vertex_ = vertex;
        }

        void set_texcoord(const TexCoord2f* texcoord) {
            BOOST_ASSERT(! texcoord_);
            texcoord_ = texcoord;
        }

        void set_normal(const Vector3f* normal) {
            BOOST_ASSERT(! normal_);
            normal_ = normal;
        }

        void draw(Draw_Option option) const {
            BOOST_ASSERT(vertex_);
            BOOST_ASSERT(texcoord_);
            BOOST_ASSERT(normal_);

            Point3f vertex(*vertex_);
            mirror_(option, vertex);

            Vector3f normal(*normal_);
            mirror_(option, normal);
            if (option & DRAW_BACK) {
                normal.scale(-1);
            }

            glNormal3fv(&normal.x);
            glMultiTexCoord2fv(GL_TEXTURE0, &texcoord_->x);
            glMultiTexCoord2fv(GL_TEXTURE1, &texcoord_->x);
            glVertex3fv(&vertex.x);
        }

        void interpolating_draw(
                const Vertex& rhs, float alpha, Draw_Option option) const
        {
            BOOST_ASSERT(vertex_   && rhs.vertex_);
            BOOST_ASSERT(texcoord_ && rhs.texcoord_);
            BOOST_ASSERT(normal_   && rhs.normal_);

            Point3f vertex(*vertex_);
            vertex.interpolate(*rhs.vertex_, alpha);
            mirror_(option, vertex);

            Vector3f normal(*normal_);
            normal.interpolate(*rhs.normal_, alpha);
            normal.normalize();
            mirror_(option, normal);
            if (option & DRAW_BACK) {
                normal.scale(-1);
            }

            glNormal3fv(&normal.x);
            glMultiTexCoord2fv(GL_TEXTURE0, &texcoord_->x);
            glMultiTexCoord2fv(GL_TEXTURE1, &texcoord_->x);
            glVertex3fv(&vertex.x);
        }
    };

    class Triangle {
        Vertex vertex_[3];

    public:
        explicit Triangle() {}

        void set_vertex(int id, const Vertex& vertex) {
            BOOST_ASSERT(0 <= id && id < 3);
            vertex_[id] = vertex;
        }

        void draw(Draw_Option option) const {
            if (! (option & DRAW_REVERSE_INTERNAL)) {
                vertex_[0].draw(option);
                vertex_[1].draw(option);
                vertex_[2].draw(option);
            } else {
                vertex_[2].draw(option);
                vertex_[1].draw(option);
                vertex_[0].draw(option);
            }
        }

        void interpolating_draw(
                const Triangle& rhs, float alpha, Draw_Option option) const
        {
            if (! (option & DRAW_REVERSE_INTERNAL)) {
                vertex_[0].interpolating_draw(vertex_[0], alpha, option);
                vertex_[1].interpolating_draw(vertex_[1], alpha, option);
                vertex_[2].interpolating_draw(vertex_[2], alpha, option);
            } else {
                vertex_[2].interpolating_draw(vertex_[2], alpha, option);
                vertex_[1].interpolating_draw(vertex_[1], alpha, option);
                vertex_[0].interpolating_draw(vertex_[0], alpha, option);
            }
        }
    };

    class Mesh {
        Material*              material_;
        std::vector<Triangle*> triangle_;

    public:
        explicit Mesh()
            : material_()
            , triangle_() {}

        ~Mesh() {
            std::for_each(
                    triangle_.begin(),
                    triangle_.end(),
                    boost::checked_deleter<Triangle>());
        }

        void set_material(Material* material) {
            BOOST_ASSERT(! material_);
            material_ = material;
        }

        void add_triangle(Triangle* triangle) {
            triangle_.push_back(triangle);
        }

        void draw(Draw_Option option) const {
            if (material_) {
                material_->draw();
            }

            glBegin(GL_TRIANGLES);
            std::for_each(
                    triangle_.begin(),
                    triangle_.end(),
                    boost::bind(&Triangle::draw, _1, option));
            glEnd();
            SDLPU_CHECK_OPENGL_ERROR();
        }

        void interpolating_draw(
                const Mesh& rhs, float alpha, Draw_Option option) const
        {
            if (material_) {
                material_->draw();
            }

            int size(triangle_.size());
            glBegin(GL_TRIANGLES);
            for (int i(0); i < size; ++i) {
                triangle_.at(i)->interpolating_draw(
                        *rhs.triangle_.at(i), alpha, option);
            }
            glEnd();
            SDLPU_CHECK_OPENGL_ERROR();
        }
    };

    class Model {
        std::vector<Point3f>             vertex_;
        std::vector<TexCoord2f>          texcoord_;
        std::vector<Vector3f>            normal_;
        std::vector<Material*>           material_;
        std::map<std::string, Material*> material_map_;
        std::vector<Mesh*>               mesh_;

        static void setup_option_(Draw_Option& option) {
            int mirror_count(0);
            if (option & MIRROR_X) { ++mirror_count; }
            if (option & MIRROR_Y) { ++mirror_count; }
            if (option & MIRROR_Z) { ++mirror_count; }

            if (mirror_count % 2 == 1) {
                if (! (option & DRAW_BACK)) {
                    option |= DRAW_REVERSE_INTERNAL;
                }
            } else {
                if (option & DRAW_BACK) {
                    option |= DRAW_REVERSE_INTERNAL;
                }
            }
        }

        void load_mtl_(Screen& screen, const FS::path&);
        void load_obj_(const FS::path&);

    public:
        explicit Model()
            : vertex_      ()
            , texcoord_    ()
            , normal_      ()
            , material_    ()
            , material_map_()
            , mesh_        () {}

        ~Model() {
            std::for_each(
                    material_.begin(),
                    material_.end(),
                    boost::checked_deleter<Material>());
            std::for_each(
                    mesh_.begin(),
                    mesh_.end(),
                    boost::checked_deleter<Mesh>());
        }

        void load(Screen& screen, const FS::path& filepath) {
            FS::path mtl_filepath(change_extension(filepath, ".mtl"));
            if (exists(mtl_filepath)) {
                load_mtl_(screen, mtl_filepath);
            }
            load_obj_(filepath);
        }

        void draw(Draw_Option option) const {
            setup_option_(option);
            std::for_each(
                    mesh_.begin(),
                    mesh_.end(),
                    boost::bind(&Mesh::draw, _1, option));
        }

        void interpolating_draw(
                const Model& rhs, float alpha, Draw_Option option) const
        {
            setup_option_(option);
            int size(mesh_.size());
            for (int i(0); i < size; ++i) {
                mesh_.at(i)->interpolating_draw(
                        *rhs.mesh_.at(i), alpha, option);
            }
        }
    };

    void
    Model::load_mtl_(Screen& screen, const FS::path& filepath) {
        FS::ifstream fin(filepath);
        if (! fin) {
            SDLPU_THROW_RUNTIME_ERROR((
                    "Could not open %s", filepath.string().c_str()));
        }

        Material* material(0);
        while (fin) {
            std::string line;
            std::getline(fin, line);
            std::istringstream in(line);

            std::string token;
            in >> token;
            if (token == "newmtl") {
                in >> token;
                material_.push_back(new Material());
                material = material_map_[token] = material_.back();
            }
            else if (token == "Ka") {
                Color4f color(0, 0, 0, 1);
                in >> color.x >> color.y >> color.z;
                material->set_ambient(color);
            }
            else if (token == "Kd") {
                Color4f color(0, 0, 0, 1);
                in >> color.x >> color.y >> color.z;
                material->set_diffuse(color);
            }
            else if (token == "Ks") {
                Color4f color(0, 0, 0, 1);
                in >> color.x >> color.y >> color.z;
                material->set_specular(color);
            }
            else if (token == "Ns") {
                float shininess(0);
                in >> shininess;
                material->set_shininess(shininess);
            }
            else if (token == "map_Kd") {
                in >> token;
                FS::path path(filepath.branch_path());
                path /= token;

                Texture* texture(screen.make_texture());
                texture->load(path);
                material->set_texture(texture);
            }
        }
    }

    void
    Model::load_obj_(const FS::path& filepath) {
        FS::ifstream fin(filepath);
        if (! fin) {
            SDLPU_THROW_RUNTIME_ERROR((
                    "Could not open %s", filepath.string().c_str()));
        }

        Mesh* mesh(0);
        while (fin) {
            std::string line;
            std::getline(fin, line);
            std::istringstream in(line);

            std::string token;
            in >> token;
            if (token == "v") {
                Point3f vertex;
                in >> vertex.x >> vertex.y >> vertex.z;
                vertex_.push_back(vertex);
            }
            else if (token == "vt") {
                TexCoord2f texcoord;
                in >> texcoord.x >> texcoord.y;
                texcoord_.push_back(texcoord);
            }
            else if (token == "vn") {
                Vector3f normal;
                in >> normal.x >> normal.y >> normal.z;
                normal_.push_back(normal);
            }
            else if (token == "usemtl") {
                in >> token;
                mesh_.push_back(new Mesh());
                mesh = mesh_.back();
                mesh->set_material(material_map_[token]);
            }
            else if (token == "f") {
                std::vector<Vertex> vertex;

                while (in) {
                    std::vector<std::string> split;

                    in >> token;
                    boost::split(
                            split,
                            token,
                            boost::bind(std::equal_to<char>(), _1, '/'));

                    BOOST_ASSERT(split.size() == 1 || split.size() == 3);
                    Vertex v;
                    v.set_vertex(
                            &vertex_.at(
                                    boost::lexical_cast<int>(
                                            split.at(0)) - 1));

                    if (! texcoord_.empty()) {
                        BOOST_ASSERT(split.size() == 3);
                        BOOST_ASSERT(! split.at(1).empty());
                        v.set_texcoord(
                                &texcoord_.at(
                                        boost::lexical_cast<int>(
                                                split.at(1)) - 1));
                    }
                    if (! normal_.empty()) {
                        BOOST_ASSERT(split.size() == 3);
                        BOOST_ASSERT(! split.at(2).empty());
                        v.set_normal(
                                &normal_.at(
                                        boost::lexical_cast<int>(
                                                split.at(2)) - 1));
                    }

                    vertex.push_back(v);
                }

                for (int i(2); i < vertex.size(); ++i) {
                    Triangle* triangle(new Triangle());
                    triangle->set_vertex(0, vertex.at(0));
                    triangle->set_vertex(1, vertex.at(i - 1));
                    triangle->set_vertex(2, vertex.at(i));
                    mesh->add_triangle(triangle);
                }
            }
        }
    }
}

#endif
