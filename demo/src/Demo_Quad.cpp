#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <boost/bind.hpp>
#include <boost/cast.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/program_options.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/static_assert.hpp>
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_Pango.h>
#include <vecmath.h>

#ifdef DG_USE_WIN32_VSNPRINTF
# define DG_VSNPRINTF _vsnprintf
#else
# define DG_VSNPRINTF vsnprintf
#endif

#define DG_THROW_RUNTIME_ERROR(args) \
    demogeot::Throw_Runtime_Error(__FILE__, __LINE__) args

#define DG_CHECK_OPENGL_ERROR \
    demogeot::Check_OpenGL_Error(__FILE__, __LINE__)()

namespace demogeot {

///////////////////////////////////////////////////////////////////////////

namespace boost_po = boost::program_options;

using namespace kh_vecmath;
typedef Tuple2<int> Tuple2i;
typedef Tuple3<int> Tuple3i;
typedef Tuple4<int> Tuple4i;

///////////////////////////////////////////////////////////////////////////

template <typename T>
inline int
size_cast(T value) {
    return boost::numeric_cast<int>(value);
}

///////////////////////////////////////////////////////////////////////////

namespace Scoped_ {

    class Base {
    public:
        virtual ~Base() {}
    };

    template <typename Function>
    class Impl
        : public Base
    {
        Function func_;
    public:
        explicit Impl(Function func)
            : func_(func) {}
        virtual ~Impl() {
            func_();
        }
    };

    template <typename Function>
    inline Impl<Function>*
    create_impl(Function func) {
        return new Impl<Function>(func);
    }
}

class Scoped
    : public boost::noncopyable
{
    boost::scoped_ptr<Scoped_::Base> impl_;

public:
    template <typename Function>
    explicit Scoped(Function func)
        : impl_(Scoped_::create_impl(func)) {}
};

///////////////////////////////////////////////////////////////////////////

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

        enum { BUFFER_SIZE = 1024 };
        char buffer[BUFFER_SIZE];
        std::fill(buffer, buffer+BUFFER_SIZE, 0);

        va_list list;
        va_start(list, format);
        DG_VSNPRINTF(buffer, BUFFER_SIZE, format, list);
        va_end(list);

        std::ostringstream out;
        out << "[" << file_ << "," << line_ << "] " << buffer;
        throw std::runtime_error(out.str());
    }
};

///////////////////////////////////////////////////////////////////////////

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
        GLenum err(glGetError());
        if (err != GL_NO_ERROR) {
            DG_THROW_RUNTIME_ERROR((
                    "[%s,%d] OpenGL error: %s",
                    file_,
                    line_,
                    gluErrorString(err)));
        }
    }
};

///////////////////////////////////////////////////////////////////////////

class Arcball_Model
    : public boost::noncopyable
{
    bool    dragging_;
    double  vsphere_radius_;
    Point2d vsphere_center_;
    Point2d start_mouse_;
    Quat4d  start_rotation_;
    Point2d current_mouse_;
    Quat4d  current_rotation_;

    Quat4d calc_mouse_on_vsphere_(const Point2d& mouse) {

        // v = (mouse - vsphere_center_) / vsphere_radius_
        Vector2d v(mouse);
        v.sub(vsphere_center_);
        v.scale(1 / vsphere_radius_);

        // mag = v dot v
        double mag(v.lengthSquared());

        Quat4d mouse_on_vsphere(v.x, v.y, 0, 0);
        if (mag > 1) {
            mouse_on_vsphere.scale(1 / std::sqrt(mag));
        } else {
            mouse_on_vsphere.z = std::sqrt(1 - mag);
        }

        return mouse_on_vsphere;
    }

public:
    explicit Arcball_Model()
        : dragging_        (false)
        , vsphere_radius_  (1)
        , start_rotation_  (0, 0, 0, 1)
        , current_rotation_(0, 0, 0, 1) {}

    bool is_dragging() const {
        return dragging_;
    }

    void set_mouse(const Point2d& mouse) {
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

            Quat4d q(calc_mouse_on_vsphere_(start_mouse_));
            q.mul(calc_mouse_on_vsphere_(current_mouse_));
            q.w *= -1;

            current_rotation_.set(q);
            current_rotation_.mul(start_rotation_);
        }
    }

    Quat4d get_rotation() const {
        return current_rotation_;
    }
};

class Arcball
    : public boost::noncopyable
{
    Arcball_Model model_;
public:
    enum Mouse_State {
        MOUSE_PRESSED,
        MOUSE_RELEASED,
        MOUSE_DRAGGING,
        MOUSE_UNKNOWN,
    };

    bool is_dragging() const {
        return model_.is_dragging();
    }

    void mouse(Mouse_State mouse_state, int w, int h, int x, int y) {

        Tuple2d regularized_mouse(
                +(2 * x / static_cast<double>(w) - 1),
                -(2 * y / static_cast<double>(h) - 1));

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
        Matrix4d rotation;
        rotation.set(model_.get_rotation());
        rotation.transpose();
        glMultMatrixd(&rotation.m00);
        DG_CHECK_OPENGL_ERROR;
    }

    void transform(Point3d& point) const {
        Matrix4d rotation;
        rotation.set(model_.get_rotation());
        rotation.transform(&point);
    }

    void transform(Point3f& point) const {
        Point3d point_d(point.x, point.y, point.z);
        transform(point_d);
        point.set(
                static_cast<float>(point_d.x),
                static_cast<float>(point_d.y),
                static_cast<float>(point_d.z));
    }
};

///////////////////////////////////////////////////////////////////////////

class Draw_Cache {
public:
    enum Obj_Name {
        OBJ_FLOOR,
        OBJ_PANGO,
        OBJ_SHADOW,
        OBJ_SIZE,
    };

    class Vertex {
    public:
        enum {
            FORMAT         = GL_T2F_C4F_N3F_V3F,
            PREFERRED_SIZE = 48,
        };
    private:
        TexCoord2f texcoord_;
        Color4f    color_;
        Vector3f   normal_;
        Point3f    point_;
    public:
        explicit Vertex(
                float x, float y, float z,
                float u, float v,
                float r, float g, float b, float a)
            : texcoord_(u, v)
            , color_   (r, g, b, a)
            , point_   (x, y, z) {}

        const Point3f& get_point() const {
            return point_;
        }

        void set_point(const Point3f& point) {
            point_.set(point);
        }

        void set_normal(const Vector3f& normal) {
            normal_.set(normal);
        }
    };

    class Object {
        GLenum mode_;
        int    first_;
        int    count_;

    public:
        explicit Object()
            : mode_ (0)
            , first_(0)
            , count_(0) {}

        void set(GLenum mode, int first, int count) {
            mode_  = mode;
            first_ = first;
            count_ = count;
        }

        int get_first() const {
            return first_;
        }

        int get_count() const {
            return count_;
        }

        void draw() const {
            glDrawArrays(mode_, first_, count_);
            DG_CHECK_OPENGL_ERROR;
        }
    };

private:
    std::vector<Vertex> vertices_;
    std::vector<Object> objects_;

    Vector3f calc_normal_(int first) {
        const Point3f& p0 = vertices_.at(first+0).get_point();
        const Point3f& p1 = vertices_.at(first+1).get_point();
        const Point3f& p2 = vertices_.at(first+2).get_point();
        Vector3f v0(p1); v0.sub(p0);
        Vector3f v1(p2); v1.sub(p1);
        Vector3f normal;
        normal.cross(v0, v1);
        normal.normalize();
        return normal;
    }

    void set_normal_(int first, int count, const Vector3f normal) {
        int last(first + count);
        for (int i = first; i < last; ++i) {
            vertices_.at(i).set_normal(normal);
        }
    }

    void build_floor_() {

        Tuple2f fx(get_floor_x());
        float   fy(get_floor_y());
        Tuple2f fz(get_floor_z());

        int first(size_cast(vertices_.size()));
        vertices_.push_back(Vertex(fx.x, fy, fz.x,  0, 1,  1, 1, 1, 1));
        vertices_.push_back(Vertex(fx.y, fy, fz.x,  1, 1,  1, 1, 1, 1));
        vertices_.push_back(Vertex(fx.y, fy, fz.y,  1, 0,  1, 1, 1, 1));
        vertices_.push_back(Vertex(fx.x, fy, fz.y,  0, 0,  1, 1, 1, 1));
        int count(size_cast(vertices_.size()) - first);

        set_normal_(first, count, calc_normal_(first));
        objects_.at(OBJ_FLOOR).set(GL_QUADS, first, count);
    }

    void build_pango_(const Color4f& c) {

        int first(size_cast(vertices_.size()));
        vertices_.push_back(Vertex(-1, -1, 0,  0, 1,  c.x, c.y, c.z, c.w));
        vertices_.push_back(Vertex(+1, -1, 0,  1, 1,  c.x, c.y, c.z, c.w));
        vertices_.push_back(Vertex(+1, +1, 0,  1, 0,  c.x, c.y, c.z, c.w));
        vertices_.push_back(Vertex(-1, +1, 0,  0, 0,  c.x, c.y, c.z, c.w));
        int count(size_cast(vertices_.size()) - first);

        set_normal_(first, count, calc_normal_(first));
        objects_.at(OBJ_PANGO).set(GL_QUADS, first, count);
    }

    void build_shadow_(const Color4f& c) {

        int first(size_cast(vertices_.size()));
        vertices_.push_back(Vertex(0, 0, 0,  0, 1,  c.x, c.y, c.z, c.w));
        vertices_.push_back(Vertex(0, 0, 0,  1, 1,  c.x, c.y, c.z, c.w));
        vertices_.push_back(Vertex(0, 0, 0,  1, 0,  c.x, c.y, c.z, c.w));
        vertices_.push_back(Vertex(0, 0, 0,  0, 0,  c.x, c.y, c.z, c.w));
        int count(size_cast(vertices_.size()) - first);

        set_normal_(first, count, calc_normal_(first));
        objects_.at(OBJ_SHADOW).set(GL_QUADS, first, count);
    }

public:
    explicit Draw_Cache(
            const Color4f& color_pango,
            const Color4f& color_shadow)
        : objects_(OBJ_SIZE)
    {
        BOOST_STATIC_ASSERT(sizeof(Vertex) == Vertex::PREFERRED_SIZE);

        build_floor_ ();
        build_pango_ (color_pango);
        build_shadow_(color_shadow);

        int stride(0);
        {
            const char* ptr0(reinterpret_cast<const char*>(&vertices_.at(0)));
            const char* ptr1(reinterpret_cast<const char*>(&vertices_.at(1)));
            stride = static_cast<int>(ptr1 - ptr0 - Vertex::PREFERRED_SIZE);
            BOOST_ASSERT(stride == 0);
        }
        glInterleavedArrays(Vertex::FORMAT, stride, &vertices_.at(0));
        DG_CHECK_OPENGL_ERROR;
    }

    void draw(Obj_Name obj_name) const {
        objects_.at(obj_name).draw();
    }

    void build_shadow(boost::function1<Point3f, const Point3f&> func) {
        int count(objects_.at(OBJ_PANGO).get_count());
        BOOST_ASSERT(count == objects_.at(OBJ_SHADOW).get_count());

        int pango_first (objects_.at(OBJ_PANGO ).get_first());
        int shadow_first(objects_.at(OBJ_SHADOW).get_first());

        for (int i = 0; i < count; ++i) {
            vertices_.at(shadow_first + i).set_point(
                    func(vertices_.at(pango_first + i).get_point()));
        }

        set_normal_(shadow_first, count, calc_normal_(shadow_first));
    }

    Tuple2f get_floor_x() const {
        return Tuple2f(-4, +4);
    }

    float get_floor_y() const {
        return -1.5;
    }

    Tuple2f get_floor_z() const {
        return Tuple2f(-4, +4);
    }
};

///////////////////////////////////////////////////////////////////////////

inline int
calc_of_two(int value) {
    int v = 1;
    while (v < value) {
        v <<= 1;
    }
    return v;
}

///////////////////////////////////////////////////////////////////////////

class Surface_Format {
public:
    enum Format_Type {
        UNKNOWN,
        RGB,
        BGR,
        RGBA,
        BGRA,
        ARGB,
        ABGR,
    };

private:
    Format_Type format_;

    void initialize_big_endian_(unsigned int value) {
        switch (value) {
        case 0x00112233:
            format_ = RGB;
            break;
        case 0x00332211:
            format_ = BGR;
            break;
        case 0x11223344:
            format_ = RGBA;
            break;
        case 0x33221144:
            format_ = BGRA;
            break;
        case 0x44112233:
            format_ = ARGB;
            break;
        case 0x44332211:
            format_ = ABGR;
            break;
        }
    }

    void initialize_little_endian_(unsigned int value) {
        switch (value) {
        case 0x00332211:
            format_ = RGB;
            break;
        case 0x00112233:
            format_ = BGR;
            break;
        case 0x44332211:
            format_ = RGBA;
            break;
        case 0x44112233:
            format_ = BGRA;
            break;
        case 0x33221144:
            format_ = ARGB;
            break;
        case 0x11223344:
            format_ = ABGR;
            break;
        }
    }

    void initialize_(
            int          bits_per_pixel,
            unsigned int rmask,
            unsigned int gmask,
            unsigned int bmask,
            unsigned int amask)
    {
        format_     = UNKNOWN;
        if (! (bits_per_pixel == 24 || bits_per_pixel == 32)) {
            return;
        }

        unsigned int value(0);
        value |= rmask & 0x11111111;
        value |= gmask & 0x22222222;
        value |= bmask & 0x33333333;
        value |= amask & 0x44444444;

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
        initialize_big_endian_   (value);
#else
        initialize_little_endian_(value);
#endif
    }

public:
    explicit Surface_Format(const SDL_Surface* surface)
    {
        initialize_(
                surface->format->BitsPerPixel,
                surface->format->Rmask,
                surface->format->Gmask,
                surface->format->Bmask,
                surface->format->Amask);
    }

    explicit Surface_Format(
            int          bits_per_pixel,
            unsigned int rmask,
            unsigned int gmask,
            unsigned int bmask,
            unsigned int amask)
    {
        initialize_(bits_per_pixel, rmask, gmask, bmask, amask);
    }

    Format_Type get_format() const {
        return format_;
    }

    std::string get_format_string() const {
        switch (format_) {
        case UNKNOWN: return "UNKNOWN";
        case RGB:     return "RGB";
        case BGR:     return "BGR";
        case RGBA:    return "RGBA";
        case BGRA:    return "BGRA";
        case ARGB:    return "ARGB";
        case ABGR:    return "ABGR";
        }
        return "UNKNOWN";
    }

    GLenum get_format_opengl() const {
        GLenum unknown(GL_NONE);
        switch (format_) {
        case UNKNOWN: return unknown;
        case RGB:     return GL_RGB;
        case BGR:     return GL_BGR;
        case RGBA:    return GL_RGBA; // OpenGL 1.2
        case BGRA:    return GL_BGRA; // OpenGL 1.2
        case ARGB:    return 0x8000;  // GL_ABGR_EXT
        case ABGR:    return unknown;
        }
        return unknown;
    }
};

class Texture {
    GLuint       id_;
    SDL_Surface* surface_;
    bool         surface_updated_;
public:
    Texture()
        : id_             (0)
        , surface_        (0)
        , surface_updated_(false) {}

    void generate_opengl() {
        glGenTextures(1, &id_);
        DG_CHECK_OPENGL_ERROR;
    }

    void delete_opengl() {
        glDeleteTextures(1, &id_);
        id_ = 0;
        DG_CHECK_OPENGL_ERROR;
    }

    void bind_opengl() {
        glBindTexture(GL_TEXTURE_2D, id_);
        DG_CHECK_OPENGL_ERROR;
    }

    void update_opengl() {
        if (! surface_updated_) {
            return;
        }

        Surface_Format format(surface_);
#ifdef DG_DEBUG
        std::cout << "format: " << format.get_format_string() << std::endl;
#endif
        GLenum format_opengl(format.get_format_opengl());
        if (format_opengl == GL_NONE) {
            DG_THROW_RUNTIME_ERROR(("Could not setup texture"));
        }

        int tw(calc_of_two(surface_->w));
        int th(calc_of_two(surface_->h));
        if (tw != surface_->w || th != surface_->h) {

            SDL_Surface* surface(
                    SDL_CreateRGBSurface(
                            SDL_SWSURFACE,
                            tw,
                            th,
                            surface_->format->BitsPerPixel,
                            surface_->format->Rmask,
                            surface_->format->Gmask,
                            surface_->format->Bmask,
                            surface_->format->Amask));
            if (surface == 0) {
                DG_THROW_RUNTIME_ERROR((
                        "Could not SDL_CreateRGBSurface: %s", SDL_GetError()));
            }

            SDL_FillRect   (surface,  0, 0);
            SDL_SetColorKey(surface_, 0, 0);
            SDL_SetAlpha   (surface_, 0, 0);
            SDL_BlitSurface(surface_, 0, surface, 0);

            delete_surface();
            surface_ = surface;
        }

        bind_opengl();
        gluBuild2DMipmaps(
                GL_TEXTURE_2D,
                surface_->format->BytesPerPixel,
                surface_->w,
                surface_->h,
                format_opengl,
                GL_UNSIGNED_BYTE,
                surface_->pixels);
        DG_CHECK_OPENGL_ERROR;

        surface_updated_ = false;
    }

    void set_surface(SDL_Surface* surface) {
        surface_         = surface;
        surface_updated_ = true;
    }

    void set_surface_updated() {
        surface_updated_ = true;
    }

    SDL_Surface* get_surface() const {
        return surface_;
    }

    void delete_surface() {
        if (surface_ != 0) {
            SDL_FreeSurface(surface_);
            surface_ = 0;
        }
    }
};

///////////////////////////////////////////////////////////////////////////

struct Surface_Create_Args {
    unsigned int flags;
    int          depth;
    unsigned int rmask;
    unsigned int gmask;
    unsigned int bmask;
    unsigned int amask;

    explicit Surface_Create_Args(
            unsigned int f,
            int          d,
            unsigned int r,
            unsigned int g,
            unsigned int b,
            unsigned int a)
        : flags(f)
        , depth(d)
        , rmask(r)
        , gmask(g)
        , bmask(b)
        , amask(a) {}
};

///////////////////////////////////////////////////////////////////////////

class Quit_Main_Loop {};

///////////////////////////////////////////////////////////////////////////

class Demo_Quad
    : public boost::noncopyable
{
    std::string                   data_dirpath_;
    boost_po::variables_map       data_config_;
    const Surface_Create_Args     data_args_;

    SDL_Surface*                  screen_;
    bool                          screen_initialized_;
    boost::scoped_ptr<Draw_Cache> cache_;
    float                         distance_;
    Arcball                       scene_arcball_;
    Arcball                       light_arcball_;
    Point3f                       light_point_;

    Tuple2i                       window_size_;
    Color4f                       color_pango_;
    Color4f                       color_shadow_;

    std::vector<std::string>      text_;
    bool                          text_updated_;

    Texture                       floor_texture_;
    Texture                       pango_texture_;

    bool                          pango_markup_;
    SDLPango_Context              pango_context_;
    int                           pango_layout_size_;
    Tuple2f                       pango_real_size_;
    Tuple2f                       pango_texture_size_;
    Matrix4f                      pango_texture_matrix_;
    Tuple2f                       pango_texture_move_;
    Tuple2f                       pango_texture_pitch_;
    Tuple2f                       pango_texture_max_;

    //
    // data functions
    //

    static bool is_data_dirpath_(const std::string& path) {
        std::ifstream in((path + "/Demo_Quad.ini").c_str());
        return ! (! in);
    }

    static std::string find_data_dirpath_(const std::string& program) {

        static const char* subdir[] = {
            "/data",
            "/../data",
            "/../../data",
            "/../../../data",
            0,
        };

        std::string::size_type pos(program.find_last_of('/'));
        if (pos == std::string::npos) {
            pos = program.find_last_of('\\');
        }

        std::string dirpath(
                pos == std::string::npos
                    ? "."
                    : program.substr(0, pos));

        for (int i = 0; subdir[i] != 0; ++i) {
            std::string path(dirpath + subdir[i]);
            if (is_data_dirpath_(path)) {
                return path;
            }
        }
        return std::string();
    }

    //
    // load functions
    //

    void load_text_(const std::string& key) {
        std::string path(data_dirpath_);
        path += "/";
        path += data_config_[key].as<std::string>();

#ifdef DG_DEBUG
        std::cout
            << "key: "  << key << std::endl
            << "path: " << path << std::endl;
#endif

        std::ifstream in(path.c_str());
        if (! in) {
            DG_THROW_RUNTIME_ERROR(("Could not open file: %s", path.c_str()));
        }

        text_.clear();
        while (in) {
            std::string line;
            std::getline(in, line);
            text_.push_back(line);
        }

        text_updated_ = true;
    }

    SDL_Surface* load_surface_(const std::string& path) {
        SDL_Surface* texture(SDL_LoadBMP(path.c_str()));
        if (texture == 0) {
            DG_THROW_RUNTIME_ERROR(("Could not SDL_LoadBMP: %s", path.c_str()));
        }
        return texture;
    }

    //
    // OpenGL calback functions (except draw function)
    //

    void initialize_() {

        if (screen_initialized_) {
            hide_();
            floor_texture_.set_surface_updated();
            pango_texture_.set_surface_updated();
        }

        glEnable(GL_COLOR_MATERIAL);
        glEnable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_LIGHTING);
        glEnable(GL_NORMALIZE);
        glEnable(GL_TEXTURE_2D);
        DG_CHECK_OPENGL_ERROR;

        if (! data_config_["OpenGL.blend"].as<bool>()) {
            glDisable(GL_BLEND);
        }
        if (! data_config_["OpenGL.texture"].as<bool>()) {
            glDisable(GL_TEXTURE_2D);
        }
        DG_CHECK_OPENGL_ERROR;

        glEnableClientState(GL_COLOR_ARRAY);
        glEnableClientState(GL_NORMAL_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glEnableClientState(GL_VERTEX_ARRAY);
        DG_CHECK_OPENGL_ERROR;

        // setup blending
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        DG_CHECK_OPENGL_ERROR;

        // setup textures
        floor_texture_.generate_opengl();
        pango_texture_.generate_opengl();

        floor_texture_.bind_opengl();
        glTexParameteri(
                GL_TEXTURE_2D,
                GL_TEXTURE_MIN_FILTER,
                GL_NEAREST);
        glTexParameteri(
                GL_TEXTURE_2D,
                GL_TEXTURE_MAG_FILTER,
                GL_NEAREST);
        DG_CHECK_OPENGL_ERROR;

        pango_texture_.bind_opengl();
        glTexParameteri(
                GL_TEXTURE_2D,
                GL_TEXTURE_MIN_FILTER,
                GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(
                GL_TEXTURE_2D,
                GL_TEXTURE_MAG_FILTER,
                GL_LINEAR);
        glTexParameteri(
                GL_TEXTURE_2D,
                GL_TEXTURE_WRAP_S,
                GL_CLAMP);
        glTexParameteri(
                GL_TEXTURE_2D,
                GL_TEXTURE_WRAP_T,
                GL_CLAMP);
        DG_CHECK_OPENGL_ERROR;

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
            DG_CHECK_OPENGL_ERROR;
        }
        {
            // light model
            static const float ambient[] = { 0.6f, 0.6f, 0.6f, 1.0f };
            glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
            DG_CHECK_OPENGL_ERROR;
        }

        cache_.reset(new Draw_Cache(color_pango_, color_shadow_));
        screen_initialized_ = true;
    }

    void resize_(const SDL_ResizeEvent& resize_event) {
        resize_(resize_event.w, resize_event.h, false);
    }

    void resize_(int width, int height, bool fullscreen) {

        unsigned int flags(SDL_OPENGL | SDL_RESIZABLE);
        if (fullscreen) {
            flags |= SDL_FULLSCREEN;
        }

        screen_ = SDL_SetVideoMode(width, height, 32, flags);
        if (screen_ == 0) {
            DG_THROW_RUNTIME_ERROR((
                    "Could not SDL_SetVideoMode: %s", SDL_GetError()));
        }

        glViewport(0, 0, screen_->w, screen_->h);
        DG_CHECK_OPENGL_ERROR;

        initialize_();
    }

    void keyboard_(const SDL_KeyboardEvent& keyboard_event) {

        if (keyboard_event.state != SDL_PRESSED) {
            return;
        }

        SDLKey sym    (keyboard_event.keysym.sym);
        SDLMod mod    (keyboard_event.keysym.mod);
        int    unicode(keyboard_event.keysym.unicode);

        if (sym == SDLK_BACKSPACE || sym == SDLK_DELETE) {
            if (! text_.empty()) {
                if ((mod & KMOD_CTRL) != 0) {
                    text_.pop_back();
                } else {
                    if (text_.back().empty()) {
                        text_.pop_back();
                    } else {
                        std::string& line = text_.back();
                        line.erase(line.size() - 1);
                    }
                }
                text_updated_ = true;
            }
        }
        else if (sym == SDLK_ESCAPE) {
            throw Quit_Main_Loop();
        }
        else if (sym == SDLK_RETURN) {
            text_.push_back(std::string());
            text_updated_ = true;
        }
        else if ((mod & KMOD_CTRL) != 0 && (SDLK_0 <= sym && sym <= SDLK_9)) {
            std::string key("File.file");
            key.push_back(sym);
            load_text_(key);
        }
        else if ((mod & KMOD_CTRL) != 0 && sym == SDLK_m) {
            pango_markup_ = true;
            text_updated_ = true;
        }
        else if ((mod & KMOD_CTRL) != 0 && sym == SDLK_t) {
            pango_markup_ = false;
            text_updated_ = true;
        }
        else if (0 < unicode && unicode < 256) {
            if (text_.empty()) {
                text_.push_back(std::string());
            }
            text_.back().push_back(static_cast<char>(unicode & 0xFF));
            text_updated_ = true;
        }
        else {
            return;
        }

#ifdef DG_DEBUG
        int text_size(text_.size());
        std::cout
            << "-----------------------------------"
               "-----------------------------------"
            << std::endl
            << "sym: "     << static_cast<int>(sym)     << std::endl
            << "mod: "     << static_cast<int>(mod)     << std::endl
            << "unicode: " << static_cast<int>(unicode) << std::endl;
        for (int i = 0; i < text_size; ++i) {
            std::cout
                << "[" << i           << "]"
                << "[" << text_.at(i) << "]"
                << std::endl;
        }
#endif
    }

    void mouse_motion_(const SDL_MouseMotionEvent& mouse_motion_event) {
        Arcball* arcball(0);
        if (scene_arcball_.is_dragging()) {
            arcball = &scene_arcball_;
        }
        if (light_arcball_.is_dragging()) {
            arcball = &light_arcball_;
        }
        if (arcball != 0) {
            arcball->mouse(
                    Arcball::MOUSE_DRAGGING,
                    screen_->w,
                    screen_->h,
                    mouse_motion_event.x,
                    mouse_motion_event.y);
        }
    }

    void mouse_button_(const SDL_MouseButtonEvent& mouse_button_event) {

        Arcball* arcball(0);
        SDLMod   mod    (SDL_GetModState());

        switch (mouse_button_event.button) {
        case SDL_BUTTON_LEFT:
            arcball = &scene_arcball_;
            break;
        case SDL_BUTTON_RIGHT:
            arcball = &light_arcball_;
            break;
        case SDL_BUTTON_WHEELUP:
            if (mouse_button_event.state == SDL_PRESSED) {
                if ((mod & KMOD_ALT) != 0) {
                    distance_ /= 1.1f;
                }
                else if ((mod & KMOD_CTRL) != 0) {
                    move_pango_texture_(-16, 0);
                }
                else {
                    move_pango_texture_(0, -16);
                }
            }
            break;
        case SDL_BUTTON_WHEELDOWN:
            if (mouse_button_event.state == SDL_PRESSED) {
                if ((mod & KMOD_ALT) != 0) {
                    distance_ *= 1.1f;
                }
                else if ((mod & KMOD_CTRL) != 0) {
                    move_pango_texture_(16, 0);
                }
                else {
                    move_pango_texture_(0, 16);
                }
            }
            break;
        }

        if (arcball != 0) {

            Arcball::Mouse_State mouse_state(Arcball::MOUSE_UNKNOWN);

            switch (mouse_button_event.state) {
            case SDL_PRESSED:
                mouse_state = Arcball::MOUSE_PRESSED;
                break;
            case SDL_RELEASED:
                mouse_state = Arcball::MOUSE_RELEASED;
                break;
            }

            if (mouse_state != Arcball::MOUSE_UNKNOWN) {
                arcball->mouse(
                        mouse_state,
                        screen_->w,
                        screen_->h,
                        mouse_button_event.x,
                        mouse_button_event.y);
            }
        }
    }

    void event_(const SDL_Event& event) {
        switch (event.type) {
            case SDL_KEYDOWN:
            case SDL_KEYUP:
                keyboard_(event.key);
                break;
            case SDL_MOUSEMOTION:
                mouse_motion_(event.motion);
                break;
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
                mouse_button_(event.button);
                break;
            case SDL_VIDEORESIZE:
                resize_(event.resize);
                break;
            case SDL_QUIT:
                throw Quit_Main_Loop();
                break;
        }
    }

    void hide_() {
        floor_texture_.delete_opengl();
        pango_texture_.delete_opengl();
    }

    //
    // pango functions
    //

    static void concatnate_text_(const std::string& source, std::string& dest) {
        dest += source;
        dest += '\n';
    }

    void update_pango_texture_() {

        if (text_updated_) {
            text_updated_ = false;

            std::string text;
            std::for_each(
                    text_.begin(),
                    text_.end  (),
                    boost::bind(
                            &Demo_Quad::concatnate_text_,
                            _1,
                            boost::ref(text)));

            if (pango_markup_) {
                SDLPango_SetMarkup(
                        pango_context_, text.c_str(), size_cast(text.size()));
            } else {
                SDLPango_SetText(
                        pango_context_, text.c_str(), size_cast(text.size()));
            }

            SDL_Rect layout_size = { 0, 0, 0, 0 };
            SDLPango_GetLayoutSize(pango_context_, &layout_size);

            int tw(calc_of_two(layout_size.w));
            int th(calc_of_two(layout_size.h));

            SDL_Surface* surface(pango_texture_.get_surface());
            if (surface == 0 || surface->w < tw || surface->h < th) {
                SDL_Surface* resized_surface(
                        SDL_CreateRGBSurface(
                                data_args_.flags,
                                tw,
                                th,
                                data_args_.depth,
                                data_args_.rmask,
                                data_args_.gmask,
                                data_args_.bmask,
                                data_args_.amask));
                SDL_FillRect(resized_surface, 0, 0);
                pango_texture_.delete_surface();
                pango_texture_.set_surface(resized_surface);
            }

            pango_texture_.set_surface(
                    SDLPango_Draw(
                            pango_texture_.get_surface(),
                            pango_context_,
                            0,
                            0));

            pango_real_size_.set(
                    layout_size.w,
                    layout_size.h);
            pango_texture_size_.set(
                    static_cast<float>(
                            calc_of_two(pango_texture_.get_surface()->w)),
                    static_cast<float>(
                            calc_of_two(pango_texture_.get_surface()->h)));

#ifdef DG_DEBUG
            std::cout
                << "pango texture size: "
                << "(" << pango_real_size_.x
                << "," << pango_real_size_.y
                << ") => "
                << "(" << pango_texture_size_.x
                << "," << pango_texture_size_.y
                << ")" << std::endl;
#endif

            pango_texture_matrix_.setIdentity();
            pango_texture_move_ .set(0, 0);
            pango_texture_pitch_.set(0, 0);
            pango_texture_max_  .set(0, 0);

            float lw(pango_layout_size_ / pango_texture_size_.x);
            float lh(pango_layout_size_ / pango_texture_size_.y);
            float rw(pango_real_size_.x / pango_texture_size_.x);
            float rh(pango_real_size_.y / pango_texture_size_.y);

            pango_texture_matrix_.m00 = lw;
            pango_texture_matrix_.m11 = lh;

            if (lw < rw) {
                pango_texture_pitch_.x = 1 / pango_texture_size_.x;
                pango_texture_max_  .x = rw - lw;
            }
            if (lh < rh) {
                pango_texture_pitch_.y = 1 / pango_texture_size_.y;
                pango_texture_max_  .y = rh - lh;
            }

        }
    }

    void move_pango_texture_(int x, int y) {

        Tuple2f move(pango_texture_pitch_);
        move.x *= x;
        move.y *= y;
        pango_texture_move_.add(move);

        if (pango_texture_move_.x < 0) {
            pango_texture_move_.x = 0;
        }
        if (pango_texture_move_.x > pango_texture_max_.x) {
            pango_texture_move_.x = pango_texture_max_.x;
        }

        if (pango_texture_move_.y < 0) {
            pango_texture_move_.y = 0;
        }
        if (pango_texture_move_.y > pango_texture_max_.y) {
            pango_texture_move_.y = pango_texture_max_.y;
        }
    }

    void update_texture_() {
        update_pango_texture_();
        floor_texture_.update_opengl();
        pango_texture_.update_opengl();
    }

    Point3f build_shadow_(const Point3f& p) {

        // R = n (Q - P) + P
        //
        // P: light
        // P: vertex of quad
        // R: vertex of shadow

        const Point3f& q = light_point_;
        Point3f r(0, cache_->get_floor_y(), 0);

        float n((r.y - p.y) / (q.y - p.y));
        r.set(q);
        r.sub(p);
        r.scale(n);
        r.add(p);
        return r;
    }

    void draw_() {

        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        DG_CHECK_OPENGL_ERROR;

        double aspect(screen_->w / static_cast<double>(screen_->h));

        glMatrixMode(GL_PROJECTION);
        {
            glLoadIdentity();
            gluPerspective(29.11, aspect, 0.5, 128);
        }
        glMatrixMode(GL_MODELVIEW);
        {
            glLoadIdentity();
            gluLookAt(0, 0, distance_, 0, 0, 0, 0, 1, 0);
        }
        glMatrixMode(GL_TEXTURE);
        {
            glLoadIdentity();
        }
        glMatrixMode(GL_MODELVIEW);
        DG_CHECK_OPENGL_ERROR;

        {
            light_point_.set(100, 100, 100);
            light_arcball_.transform(light_point_);

            float light_position[] = {
                light_point_.x,
                light_point_.y,
                light_point_.z,
                0,
            };
            glLightfv(GL_LIGHT0, GL_POSITION, light_position);
        }
        DG_CHECK_OPENGL_ERROR;

        cache_->build_shadow(boost::bind(&Demo_Quad::build_shadow_, this, _1));
        scene_arcball_.transform();

        {
            floor_texture_.bind_opengl();
            cache_->draw(Draw_Cache::OBJ_FLOOR);
        }
        DG_CHECK_OPENGL_ERROR;

        glMatrixMode(GL_TEXTURE);
        {
            Matrix4f texture_matrix(pango_texture_matrix_);
            texture_matrix.m03 = pango_texture_move_.x;
            texture_matrix.m13 = pango_texture_move_.y;
            texture_matrix.transpose();
            glMultMatrixf(&texture_matrix.m00);
        }
        glMatrixMode(GL_MODELVIEW);
        DG_CHECK_OPENGL_ERROR;

        {
            glDisable(GL_DEPTH_TEST);
            pango_texture_.bind_opengl();
            cache_->draw(Draw_Cache::OBJ_SHADOW);
            glEnable(GL_DEPTH_TEST);
        }
        DG_CHECK_OPENGL_ERROR;

        {
            pango_texture_.bind_opengl();
            cache_->draw(Draw_Cache::OBJ_PANGO);
        }
        DG_CHECK_OPENGL_ERROR;

        SDL_GL_SwapBuffers();
    }

public:
    explicit Demo_Quad(const std::string& program)
        : data_dirpath_            (find_data_dirpath_(program))
        , data_args_               (SDL_SWSURFACE, 32,
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
        0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF
#else
        0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000
#endif
        )
        , screen_                  (0)
        , screen_initialized_      (false)
        , distance_                (7)
        , text_updated_            (false)
        , pango_markup_            (true)
        , pango_context_           (0)
        , pango_layout_size_       (0)
    {
        if (data_dirpath_.empty()) {
            DG_THROW_RUNTIME_ERROR(("Data dirpath is empty"));
        }

        std::string path(data_dirpath_);
        path += "/Demo_Quad.ini";

        std::ifstream in(path.c_str());
        if (! in) {
            DG_THROW_RUNTIME_ERROR(("Could not open file: %s", path.c_str()));
        }

        boost_po::options_description desc("Configuration Parameters");
        desc.add_options()
            ("Window.size",       boost_po::value<std::string>(), "")
            ("Window.fullscreen", boost_po::value<bool>       (), "")
            ("Window.grab_input", boost_po::value<bool>       (), "")
            ("OpenGL.blend",      boost_po::value<bool>       (), "")
            ("OpenGL.texture",    boost_po::value<bool>       (), "")
            ("File.file0",        boost_po::value<std::string>(), "")
            ("File.file1",        boost_po::value<std::string>(), "")
            ("File.file2",        boost_po::value<std::string>(), "")
            ("File.file3",        boost_po::value<std::string>(), "")
            ("File.file4",        boost_po::value<std::string>(), "")
            ("File.file5",        boost_po::value<std::string>(), "")
            ("File.file6",        boost_po::value<std::string>(), "")
            ("File.file7",        boost_po::value<std::string>(), "")
            ("File.file8",        boost_po::value<std::string>(), "")
            ("File.file9",        boost_po::value<std::string>(), "")
            ("File.floor",        boost_po::value<std::string>(), "")
            ("Color.pango",       boost_po::value<std::string>(), "")
            ("Color.shadow",      boost_po::value<std::string>(), "")
            ("Pango.dpi",         boost_po::value<double>(),      "")
            ("Pango.size",        boost_po::value<int>(),         "")
            ;

        boost_po::store(boost_po::parse_config_file(in, desc), data_config_);
        boost_po::notify(data_config_);

        load_text_("File.file0");

        {
            std::istringstream in(
                    data_config_["Window.size"].as<std::string>());
            in  >> window_size_.x
                >> window_size_.y;
        }{
            std::istringstream in(
                    data_config_["Color.pango"].as<std::string>());
            in  >> color_pango_.x
                >> color_pango_.y
                >> color_pango_.z
                >> color_pango_.w;
        }{
            std::istringstream in(
                    data_config_["Color.shadow"].as<std::string>());
            in  >> color_shadow_.x
                >> color_shadow_.y
                >> color_shadow_.z
                >> color_shadow_.w;
        }

        pango_layout_size_ = data_config_["Pango.size"].as<int>();
    }

    void operator()() {

        // setup SDL

        SDL_Init(SDL_INIT_VIDEO);
        Scoped sdl_quit(SDL_Quit);
        Scoped hide(boost::bind(&Demo_Quad::hide_, this));

        SDL_WM_GrabInput(data_config_["Window.grab_input"].as<bool>()
                ? SDL_GRAB_ON
                : SDL_GRAB_OFF);
        SDL_WM_SetCaption("demogeot::Demo_Quad", "demogeot");

        SDL_EnableUNICODE(1);
        SDL_EnableKeyRepeat(
                SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

        // setup SDL OpenGL

        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

        // setup SDL Pango

        SDLPango_Init();
        pango_context_ = SDLPango_CreateContext();
        Scoped free_context(boost::bind(SDLPango_FreeContext, pango_context_));

        // GL_RGBA order
        SDLPango_SetSurfaceCreateArgs(
                pango_context_,
                data_args_.flags,
                data_args_.depth,
                data_args_.rmask,
                data_args_.gmask,
                data_args_.bmask,
                data_args_.amask);

        SDLPango_SetLayoutWidth(pango_context_, pango_layout_size_);

        double dpi(data_config_["Pango.dpi"].as<double>());
        SDLPango_SetDpi(dpi, dpi);

        // TRANSPARENT BACK WHITE LETTER
        static const SDLPango_Matrix PANGO_COLOR_MATRIX = {
            255, 255, 0, 0,
            255, 255, 0, 0,
            255, 255, 0, 0,
              0, 255, 0, 0,
        };
        SDLPango_SetDefaultColor(pango_context_, &PANGO_COLOR_MATRIX);

        // load texture
        {
            std::string path(data_dirpath_);
            path += "/";
            path += data_config_["File.floor"].as<std::string>();
            floor_texture_.set_surface(load_surface_(path));
        }

        // start demo
        resize_(
                window_size_.x,
                window_size_.y,
                data_config_["Window.fullscreen"].as<bool>());

        while (true) {

            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                event_(event);
            }
            update_texture_();
            draw_();
        }
    }
};

///////////////////////////////////////////////////////////////////////////

} // namespace demogeot

int
main(int ac, char* av[]) {

    using namespace demogeot;

    try {
        (Demo_Quad(av[0]))();
    }
    catch (const Quit_Main_Loop&) {
        // noop
    }
#ifndef DG_DEBUG
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
#endif

    return 0;
}
