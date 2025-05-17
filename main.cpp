#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GLFW/glfw3.h>

#include <assert.h>
#include <math.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <float.h>

#define unreachable _unreachable(__FILE__, __LINE__)
#define _unreachable(f, l) __unreachable(f, l)
#define __unreachable(f, l) assert(0 && ("Unreachable at " #f #l))

#define FACE_COUNT (6)
#define TRIANGLE_VERT_COUNT (FACE_COUNT * (2 * 3))

GLfloat const verts[TRIANGLE_VERT_COUNT * 4] = {
    -1.0f, -1.0f, -1.0f, 0.0f,
    -1.0f, +1.0f, -1.0f, 0.0f,
    +1.0f, +1.0f, -1.0f, 0.0f,
    -1.0f, -1.0f, -1.0f, 0.0f,
    +1.0f, +1.0f, -1.0f, 0.0f,
    +1.0f, -1.0f, -1.0f, 0.0f,

    +1.0f, -1.0f, -1.0f, 1.0f,
    +1.0f, +1.0f, -1.0f, 1.0f,
    +1.0f, +1.0f, +1.0f, 1.0f,
    +1.0f, -1.0f, -1.0f, 1.0f,
    +1.0f, +1.0f, +1.0f, 1.0f,
    +1.0f, -1.0f, +1.0f, 1.0f,

    +1.0f, +1.0f, -1.0f, 2.0f,
    -1.0f, +1.0f, -1.0f, 2.0f,
    -1.0f, +1.0f, +1.0f, 2.0f,
    +1.0f, +1.0f, -1.0f, 2.0f,
    -1.0f, +1.0f, +1.0f, 2.0f,
    +1.0f, +1.0f, +1.0f, 2.0f,

    -1.0f, +1.0f, -1.0f, 3.0f,
    -1.0f, -1.0f, -1.0f, 3.0f,
    -1.0f, -1.0f, +1.0f, 3.0f,
    -1.0f, +1.0f, -1.0f, 3.0f,
    -1.0f, -1.0f, +1.0f, 3.0f,
    -1.0f, +1.0f, +1.0f, 3.0f,

    -1.0f, -1.0f, -1.0f, 4.0f,
    +1.0f, -1.0f, -1.0f, 4.0f,
    +1.0f, -1.0f, +1.0f, 4.0f,
    -1.0f, -1.0f, -1.0f, 4.0f,
    +1.0f, -1.0f, +1.0f, 4.0f,
    -1.0f, -1.0f, +1.0f, 4.0f,

    +1.0f, -1.0f, +1.0f, 5.0f,
    +1.0f, +1.0f, +1.0f, 5.0f,
    -1.0f, +1.0f, +1.0f, 5.0f,
    +1.0f, -1.0f, +1.0f, 5.0f,
    -1.0f, +1.0f, +1.0f, 5.0f,
    -1.0f, -1.0f, +1.0f, 5.0f,
};

#define PIX_TO_SCREEN (1.0 / 320.0)

struct PerspectiveProps {
    GLfloat near;
    GLfloat far;
    GLfloat width;
    GLfloat height;
};

// requires the matrix is zero-initialized
inline void init_perspective_mat(PerspectiveProps props, GLfloat mat[16]) {
    // GLfloat perspective_mat[16] = {
    //     2.0f * near / width, 0.0f,                 0.0f,                         0.0f,
    //     0.0f,                2.0f * near / height, 0.0f,                         0.0f,
    //     0.0f,                0.0f,                 -(far + near) / (far - near), -2.0f * far * near / (far - near),
    //     0.0f,                0.0f,                 -1.0f,                        0.0f,
    // };

    // fill in the values we know
    mat[0] = 2.0f * props.near / props.width;
    mat[5] = 2.0f * props.near / props.height;
    mat[10] = -(props.far + props.near) / (props.far - props.near);
    mat[11] = -2.0f * props.far * props.near / (props.far - props.near);
    mat[14] = -1.0f;
}

struct Context {
    int width, height;
    double xpos, ypos;

    PerspectiveProps props;
};

void init_context(Context *ctx, int width, int height) {
    double d_width = width;
    double d_height = height;

    ctx->width = width;
    ctx->height = height;
    ctx->xpos = 0.0f;
    ctx->ypos = 0.0f;
    ctx->props = (PerspectiveProps){
        .near = 12.0f,
        .far = 24.0f,
        .width = GLfloat(d_width * PIX_TO_SCREEN),
        .height = GLfloat(d_height * PIX_TO_SCREEN),
    };
}

Context *get_context(GLFWwindow *window) {
    assert(window != nullptr);
    void *usr_ptr = glfwGetWindowUserPointer(window);

    assert(usr_ptr != nullptr);
    return static_cast<Context *>(usr_ptr);
}

void handle_error(int error, char const *description) {
    fprintf(stderr, "GLFW Error (%d)): %s\n", error, description);
}

void _handle_framebuffer_size(GLFWwindow *window, int width, int height) {
    Context *ctx = get_context(window);

    double d_width = width;
    double d_height = height;

    ctx->width = width;
    ctx->height = height;
    ctx->props.width = GLfloat(d_width * PIX_TO_SCREEN),
    ctx->props.height = GLfloat(d_height * PIX_TO_SCREEN),

    glViewport(0, 0, width, height);
}

void _handle_cursor_pos(GLFWwindow* window, double xpos, double ypos) {
    Context *ctx = get_context(window);
    ctx->xpos = xpos;
    ctx->ypos = ypos;
}

GLFWframebuffersizefun handle_framebuffer_size = &_handle_framebuffer_size;
GLFWcursorposfun handle_cursor_pos = &_handle_cursor_pos;

enum Face {
    YELLOW = 0,
    RED    = 1,
    BLUE   = 2,
    ORANGE = 3,
    GREEN  = 4,
    WHITE  = 5,
};

inline GLuint face_idx(Face face) {
    assert(face >= YELLOW && face <= WHITE);
    switch (face) {
    case YELLOW: return 0;
    case RED:    return 1;
    case BLUE:   return 2;
    case ORANGE: return 3;
    case GREEN:  return 4;
    case WHITE:  return 5;
    }

    unreachable;
}

struct FaceIter {
    bool done;
    Face face;
};

inline FaceIter begin_iter_faces() {
    return FaceIter{false, YELLOW};
}

inline FaceIter next_face(FaceIter cur) {
    if (cur.done) return cur;

    switch (cur.face) {
    case YELLOW: return FaceIter{false, RED};
    case RED:    return FaceIter{false, BLUE};
    case BLUE:   return FaceIter{false, ORANGE};
    case ORANGE: return FaceIter{false, GREEN};
    case GREEN:  return FaceIter{false, WHITE};
    case WHITE:  return FaceIter{true,  YELLOW};
    }

    unreachable;
}

struct CubeModel {
    Face faces[FACE_COUNT][3][3];

    // will have to do some management of these to make sure we don't have
    // physically incapable motions
    GLfloat xrot[3];
    GLfloat yrot[3];
    GLfloat zrot[3];

    GLfloat radius;
};

void init_cube(CubeModel *cube) {
    FaceIter iter = begin_iter_faces();
    while (!iter.done) {
        GLuint idx = face_idx(iter.face);
        for (GLuint r = 0; r < 3; ++r) {
            for (GLuint c = 0; c < 3; ++c) {
                cube->faces[idx][r][c] = iter.face;
            }
        }

        iter = next_face(iter);
    }
}

char const *vert_shader_text = R"(
#version 330 core

in vec3 pos;
in float tex_ind;

out vec3 pos_v;
out float tex_ind_v;

uniform mat4 object;
uniform mat4 camera;
uniform mat4 perspective;

void main() {
    vec4 pos_hom = vec4(pos, 1.0);

    pos_v = pos;
    tex_ind_v = tex_ind;
    gl_Position = perspective * camera * object * pos_hom;
}
)";

char const *frag_shader_text = R"(
#version 330 core

in vec3 pos_v;
in float tex_ind_v;
out vec4 color_out;

uniform sampler1D face_colors;
uniform float intersection_face;

void main() {
    vec3 mapped = (pos_v + 1.0) * 0.5;

    int c = 0;

    float cube_radius = (1.0 / 7.0); // FIXME(bhester): get cube radius from uniform
    float check_val = cube_radius * 0.8;

    if (abs(pos_v.x) > check_val) ++c;
    if (abs(pos_v.y) > check_val) ++c;
    if (abs(pos_v.z) > check_val) ++c;

    color_out = (c > 1)
        ? vec4(0.2, 0.2, 0.2, 1.0)
        : texture(face_colors, (tex_ind_v + 0.5) / 6.0);

    if (intersection_face > 0 && abs(tex_ind_v - intersection_face) < 1e-3) {
        // dim the intersected face
        color_out.rgb = 0.6 * color_out.rgb;
    }
}
)";

struct Camera {
    GLfloat tx, ty, tz;
    GLfloat rotx, roty, rotz;
};

inline void set_identity(GLfloat mat[16]) {
    static GLfloat identity[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    memmove(mat, identity, sizeof(identity));
}

inline GLfloat row_mult(GLfloat lhs[16], GLfloat rhs[16], GLuint r, GLuint c) {
    GLfloat val = 0.0f;
    for (GLuint i = 0; i < 4; ++i) {
        val += lhs[r * 4 + i] * rhs[i * 4 + c];
    }
    return val;
}

void mat_mult(GLfloat lhs[16], GLfloat rhs[16], GLfloat res[16]) {
    for (GLuint r = 0; r < 4; ++r) {
        for (GLuint c = 0 ; c < 4; ++c) {
            res[r * 4 + c] = row_mult(lhs, rhs, r, c);
        }
    }
}

void mat_mult_left(GLfloat lhs[16], GLfloat rhs[16]) {
    GLfloat res[16];
    mat_mult(lhs, rhs, res);

    memmove(rhs, res, sizeof(res));
}

template <GLuint N>
inline GLfloat row_apply(GLfloat lhs[N * N], GLfloat rhs[N], GLuint r) {
    GLfloat val = 0.0f;
    for (GLuint i = 0; i < N; ++i) {
        val += lhs[r * N + i] * rhs[i];
    }
    return val;
}

template <GLuint N>
void mat_apply(GLfloat lhs[N * N], GLfloat rhs[N], GLfloat res[N]) {
    for (GLuint r = 0; r < N; ++r) {
        res[r] = row_apply<N>(lhs, rhs, r);
    }
}

template <GLuint N>
void mat_apply_left(GLfloat lhs[N * N], GLfloat rhs[N]) {
    GLfloat res[N];
    mat_apply<N>(lhs, rhs, res);

    memmove(rhs, res, sizeof(res));
}

template <GLuint N>
inline GLfloat vecdot(GLfloat lhs[N], GLfloat rhs[N]) {
    GLfloat res = 0.0f;
    for (GLuint i = 0; i < N; ++i) {
        res += lhs[i] * rhs[i];
    }
    return res;
}

template <GLuint N>
inline GLfloat veclen(GLfloat vec[N]) {
    return vecdot<N>(vec, vec);
}

void get_camera(GLfloat camera[16], Camera const *cam) {
    // init to identity
    for (GLuint i = 0; i < 16; ++i) {
        camera[i] = 0.0f;
    }

    camera[0 * 4 + 0] = 1.0f;
    camera[1 * 4 + 1] = 1.0f;
    camera[2 * 4 + 2] = 1.0f;
    camera[3 * 4 + 3] = 1.0f;

    // define matrices

    GLfloat rotx_mat[16] = {
        1.0f, 0.0f,             0.0f,              0.0f,
        0.0f, cosf(-cam->rotx), -sinf(-cam->rotx), 0.0f,
        0.0f, sinf(-cam->rotx), cosf(-cam->rotx),  0.0f,
        0.0f, 0.0f,             0.0f,              1.0f,
    };

    GLfloat roty_mat[16] = {
        cosf(-cam->roty),  0.0f, sinf(-cam->roty), 0.0f,
        0.0f,              1.0f, 0.0f,             0.0f,
        -sinf(-cam->roty), 0.0f, cosf(-cam->roty), 0.0f,
        0.0f,              0.0f, 0.0f,             1.0f,
    };

    GLfloat rotz_mat[16] = {
        cosf(-cam->rotz), -sinf(-cam->rotz), 0.0f, 0.0f,
        sinf(-cam->rotz), cosf(-cam->rotz),  0.0f, 0.0f,
        0.0f,             0.0f,              1.0f, 0.0f,
        0.0f,             0.0f,              0.0f, 1.0f,
    };

    GLfloat translation_mat[16] = {
        1.0f, 0.0f, 0.0f, -cam->tx,
        0.0f, 1.0f, 0.0f, -cam->ty,
        0.0f, 0.0f, 1.0f, -cam->tz,
        0.0f, 0.0f, 0.0f, 1.0f,
    };

    // apply the matrix mults
    mat_mult_left(translation_mat, camera);
    mat_mult_left(rotz_mat, camera);
    mat_mult_left(roty_mat, camera);
    mat_mult_left(rotx_mat, camera);
}

GLuint compile_shader(GLenum type, char const *text) {
    GLuint shader = glCreateShader(type);

    glShaderSource(shader, 1, &text, nullptr);
    glCompileShader(shader);

    GLint status = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

    if (status != GL_TRUE) {
        GLint len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);

        char *log = new char[len];
        glGetShaderInfoLog(shader, len, nullptr, log);
        fprintf(stderr, "Shader Log: %.*s\n", len, log);
    }
    assert(status == GL_TRUE);

    return shader;
}

GLuint compile_link_program(char const *vert, char const *frag) {
    GLuint program = glCreateProgram();

    GLuint vert_shader = compile_shader(GL_VERTEX_SHADER, vert);
    GLuint frag_shader = compile_shader(GL_FRAGMENT_SHADER, frag);

    glAttachShader(program, vert_shader);
    glAttachShader(program, frag_shader);

    glLinkProgram(program);

    GLint status = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &status);

    if (status != GL_TRUE) {
        GLint len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);

        char *log = new char[len];
        glGetProgramInfoLog(program, len, nullptr, log);
        fprintf(stderr, "Program Log: %.*s\n", len, log);
    }
    assert(status == GL_TRUE);

    return program;
}

struct Vec3 {
    GLfloat x, y, z;
};

void bind_cube_pos_data(GLfloat cube_radius, GLuint vbo) {
    GLfloat scaled[TRIANGLE_VERT_COUNT * 4] = {};
    memmove(scaled, verts, sizeof(verts));

    for (GLuint i = 0; i < TRIANGLE_VERT_COUNT; ++i) {
        scaled[i * 4 + 0] *= cube_radius;
        scaled[i * 4 + 1] *= cube_radius;
        scaled[i * 4 + 2] *= cube_radius;
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(scaled), scaled, GL_STATIC_DRAW);
}

void set_cube_obj_matrix(CubeModel const* model, int x, int y, int z,
                             GLfloat mat[16]) {
    GLfloat xrot = model->xrot[x];
    GLfloat yrot = model->yrot[y];
    GLfloat zrot = model->zrot[z];

    GLfloat xrot_mat[16] = {
        1.0f, 0.0f,       0.0f,        0.0f,
        0.0f, cosf(xrot), -sinf(xrot), 0.0f,
        0.0f, sinf(xrot), cosf(xrot),  0.0f,
        0.0f, 0.0f,       0.0f,        1.0f,
    };

    GLfloat yrot_mat[16] = {
        cosf(yrot),  0.0f, sinf(yrot), 0.0f,
        0.0f,        1.0f, 0.0f,       0.0f,
        -sinf(yrot), 0.0f, cosf(yrot), 0.0f,
        0.0f,        0.0f, 0.0f,       1.0f,
    };

    GLfloat zrot_mat[16] = {
        cosf(zrot), -sinf(zrot), 0.0f, 0.0f,
        sinf(zrot), cosf(zrot),  0.0f, 0.0f,
        0.0f,       0.0f,        1.0f, 0.0f,
        0.0f,       0.0f,        0.0f, 1.0f,
    };

    GLfloat translation_mat[16] = {
        1.0f, 0.0f, 0.0f, (GLfloat(x) - 1.0f) * 2.0f * model->radius,
        0.0f, 1.0f, 0.0f, (GLfloat(y) - 1.0f) * 2.0f * model->radius,
        0.0f, 0.0f, 1.0f, (GLfloat(z) - 1.0f) * 2.0f * model->radius,
        0.0f, 0.0f, 0.0f, 1.0f,
    };

    set_identity(mat);
    mat_mult_left(translation_mat, mat);
    mat_mult_left(xrot_mat, mat);
    mat_mult_left(yrot_mat, mat);
    mat_mult_left(zrot_mat, mat);
}

void set_color(GLubyte dest[3], Face face) {
    switch (face) {
    case YELLOW: {dest[0] = 0xFF; dest[1] = 0xFF; dest[2] = 0x00;} break;
    case RED:    {dest[0] = 0xFF; dest[1] = 0x00; dest[2] = 0x00;} break;
    case BLUE:   {dest[0] = 0x00; dest[1] = 0x00; dest[2] = 0xFF;} break;
    case ORANGE: {dest[0] = 0xFF; dest[1] = 0xA5; dest[2] = 0x00;} break;
    case GREEN:  {dest[0] = 0x00; dest[1] = 0xFF; dest[2] = 0x00;} break;
    case WHITE:  {dest[0] = 0xFF; dest[1] = 0xFF; dest[2] = 0xFF;} break;
    default:
        unreachable;
    }
}

void fill_cube_tex_colors(CubeModel const *model, GLuint x, GLuint y, GLuint z,
                          GLubyte colors[6 * 3]) {
    switch (x) {
    case 0: {
        set_color(&colors[3 * 3], model->faces[3][y][z]);
    } break;
    case 1: {} break; // do nothing
    case 2: {
        set_color(&colors[1 * 3], model->faces[1][y][z]);
    } break;
    default:
        unreachable;
    }

    switch (y) {
    case 0: {
        set_color(&colors[4 * 3], model->faces[4][z][x]);
    } break;
    case 1: {} break; // do nothing
    case 2: {
        set_color(&colors[2 * 3], model->faces[2][z][x]);
    } break;
    default:
        unreachable;
    }

    switch (z) {
    case 0: {
        set_color(&colors[0 * 3], model->faces[0][x][y]);
    } break;
    case 1: {} break; // do nothing
    case 2: {
        set_color(&colors[5 * 3], model->faces[5][x][y]);
    } break;
    default:
        unreachable;
    }
}

inline GLfloat screen_to_near_x(double x, double screen_width,
                                PerspectiveProps props) {
    return GLfloat((x / screen_width - 0.5) * props.width);
}

inline GLfloat screen_to_near_y(double y, double screen_height,
                                PerspectiveProps props) {
    double inv_y = screen_height - y;
    return GLfloat((inv_y / screen_height - 0.5) * props.height);
}

struct CubeProgramInfo {
    // uniforms
    GLint obj_loc;     // the cube's object matrix
    GLint cam_loc;     // the camera matrix
    GLint per_loc;     // the perspective matrix
    GLint tex_loc;     // the face color texture
    GLint ix_face_loc; // the intersection face

    // attributes
    GLint pos_loc;     // the position attribute (x,y,z)
    GLint tex_ind_loc; // the face color texture index
};

struct CubeProgram {
    GLuint prog;
    CubeProgramInfo info;

    GLuint vao;
    GLuint vbo;
    GLuint texture;
};

CubeProgram get_cube_program() {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint texture = 0;

    GLuint prog = compile_link_program(vert_shader_text, frag_shader_text);
    glUseProgram(prog);

    GLint obj_loc = glGetUniformLocation(prog, "object");
    GLint cam_loc = glGetUniformLocation(prog, "camera");
    GLint per_loc = glGetUniformLocation(prog, "perspective");
    GLint tex_loc = glGetUniformLocation(prog, "face_colors");
    GLint ix_face_loc = glGetUniformLocation(prog, "intersection_face");

    GLint pos_loc = glGetAttribLocation(prog, "pos");
    GLint tex_ind_loc = glGetAttribLocation(prog, "tex_ind");

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenTextures(1, &texture);

    CubeProgram cube_program = {
        prog,
        {obj_loc, cam_loc, per_loc, tex_loc, ix_face_loc, pos_loc, tex_ind_loc},
        vao,
        vbo,
        texture,
    };
    return cube_program;
}

void cleanup_cube_program(CubeProgram cube_program) {
    glDeleteTextures(1, &cube_program.texture);
    glDeleteBuffers(1, &cube_program.vbo);
    glDeleteVertexArrays(1, &cube_program.vao);
    glDeleteProgram(cube_program.prog);
}

bool triangle_intersects(GLfloat x, GLfloat y, GLfloat z, GLfloat a[3],
                         GLfloat b[3], GLfloat c[3], GLfloat ix[3]) {
    // We have a ray from the origin, and a plane in space. If the plane went
    // through the origin as well, then the intersection point would be the
    // origin. We can define the plane with (a) and the normal of the plane
    // N = (b-a) x (c-a). We see that the vector a shifts the plane by some
    // amount in the direction of N (a . N / |N|). Additionally, (x,y,z) has
    // some component in the direction of N, and we can find that similarly as
    // ((x,y,z) . N / |N|). Then for an intersection, we would have (x,y,z) * t
    // on the plane, which means the distance in the normal direction would be
    // the same as (a), so we have t = (a . N) / ((x,y,z) . N)
    //
    // Now that we have a point on the plane, we need to see if it's in the
    // triangle. We want to find the components of (b-a) and (c-a) in
    // (intersection-a) The following matrix maps e_1 to b-a, e_2 to c-a, and
    // e_3 to their cross product. So if we take the inverse of this, and then
    // apply it to the intersection point, we should find the component we want.
    // [ (b-a)_x  (c-a)_x  cross_x ]
    // [ (b-a)_y  (c-a)_y  cross_y ]
    // [ (b-a)_z  (c-a)_z  cross_z ]
    // Using the formula A * adj(A) = det(A) * I, we need to find the adjugate
    // matrix and the determinant to get the inverse. The determinant appears to
    // just be the length of the cross product vector for some reason.

    GLfloat ray[3] = {x, y, z};

    GLfloat ba[3] = {
        b[0] - a[0],
        b[1] - a[1],
        b[2] - a[2],
    };

    GLfloat ca[3] = {
        c[0] - a[0],
        c[1] - a[1],
        c[2] - a[2],
    };

    GLfloat cross[3] = {
        +(ba[1] * ca[2] - ba[2] * ca[1]),
        -(ba[0] * ca[2] - ba[2] * ca[0]),
        +(ba[0] * ca[1] - ba[1] * ca[0]),
    };

    GLfloat det = veclen<3>(cross);
    if (fabsf(det) < 1e-3) {
        // singular matrix, we have a degenerate triangle
        return false;
    }

    GLfloat det_inv = 1.0f / det;
    GLfloat inv[9] = {
        +det_inv * (ca[1] * cross[2] - ca[2] * cross[1]),
        -det_inv * (ca[0] * cross[2] - ca[2] * cross[0]),
        +det_inv * (ca[0] * cross[1] - ca[1] * cross[0]),

        -det_inv * (ba[1] * cross[2] - ba[2] * cross[1]),
        +det_inv * (ba[0] * cross[2] - ba[2] * cross[0]),
        -det_inv * (ba[0] * cross[1] - ba[1] * cross[0]),

        +det_inv * (ba[1] * ca[2] - ba[2] * ca[1]),
        -det_inv * (ba[0] * ca[2] - ba[2] * ca[0]),
        +det_inv * (ba[0] * ca[1] - ba[1] * ca[0]),
    };

    GLfloat a_cross = vecdot<3>(a, cross);
    GLfloat xyz_cross = vecdot<3>(ray, cross);

    if (xyz_cross > 0.0f) {
        // pretty sure there is no way for this to be intersecting?
        return false;
    }

    GLfloat t = a_cross / xyz_cross;
    if (t < 0) {
        return false;
    }

    GLfloat ix_adjusted[3] = {x * t - a[0], y * t - a[1], z * t - a[2]};
    GLfloat components[3] = {};

    mat_apply<3>(inv, ix_adjusted, components);

    GLfloat x_comp = components[0];
    GLfloat y_comp = components[1];
    if (x_comp > 0 && y_comp > 0 && ((x_comp + y_comp) <= 1.0f)) {
        GLfloat unadjusted[3] = {x * t, y * t, z * t};
        memmove(ix, unadjusted, sizeof(unadjusted));

        return true;
    }
    return false;
}

// Tests whether a ray given by (x, y, z) intersects with the given cube
bool cube_intersects(GLfloat x, GLfloat y, GLfloat z, CubeModel const *model,
                     GLfloat obj_mat[16], GLfloat cam_mat[16], GLfloat *face) {
    bool found = false;

    GLfloat nearest_dist = FLT_MAX;
    GLfloat nearest_face = {};

    GLuint triangle_count = TRIANGLE_VERT_COUNT / 3;
    for (GLuint t = 0; t < triangle_count; ++t) {
        GLuint tbase = t * 3 * 4;

        GLfloat ca_face = verts[tbase + 0 * 4 + 3];
        GLfloat cb_face = verts[tbase + 1 * 4 + 3];
        GLfloat cc_face = verts[tbase + 2 * 4 + 3];

        assert(ca_face == cb_face);
        assert(ca_face == cc_face);

        GLfloat ca_x = verts[tbase + 0 * 4 + 0] * model->radius;
        GLfloat ca_y = verts[tbase + 0 * 4 + 1] * model->radius;
        GLfloat ca_z = verts[tbase + 0 * 4 + 2] * model->radius;

        GLfloat cb_x = verts[tbase + 1 * 4 + 0] * model->radius;
        GLfloat cb_y = verts[tbase + 1 * 4 + 1] * model->radius;
        GLfloat cb_z = verts[tbase + 1 * 4 + 2] * model->radius;

        GLfloat cc_x = verts[tbase + 2 * 4 + 0] * model->radius;
        GLfloat cc_y = verts[tbase + 2 * 4 + 1] * model->radius;
        GLfloat cc_z = verts[tbase + 2 * 4 + 2] * model->radius;

        GLfloat pos_a[4] = {ca_x, ca_y, ca_z, 1.0f};
        GLfloat pos_b[4] = {cb_x, cb_y, cb_z, 1.0f};
        GLfloat pos_c[4] = {cc_x, cc_y, cc_z, 1.0f};

        mat_apply_left<4>(obj_mat, pos_a);
        mat_apply_left<4>(obj_mat, pos_b);
        mat_apply_left<4>(obj_mat, pos_c);

        mat_apply_left<4>(cam_mat, pos_a);
        mat_apply_left<4>(cam_mat, pos_b);
        mat_apply_left<4>(cam_mat, pos_c);

        GLfloat tri_ix[3] = {};
        if (triangle_intersects(x, y, z, pos_a, pos_b, pos_c, tri_ix)) {
            GLfloat lensq = veclen<3>(tri_ix);
            if (lensq < nearest_dist) {
                nearest_dist = lensq;
                nearest_face = ca_face;
            }
            found = true;
        }
    }

    if (found) {
        *face = nearest_face;
    }
    return found;
}

int main() {
    glfwSetErrorCallback(&handle_error);

    if (glfwInit() < 0) {
        fprintf(stderr, "Failed to init glfw\n");
        return 1;
    }

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    int init_width = 640;
    int init_height = 640;
    GLFWwindow *window = glfwCreateWindow(init_width, init_height,
                                          "Hello, world", nullptr, nullptr);

    if (window == nullptr) {
        fprintf(stderr, "Failed to create window\n");
        glfwTerminate();
        return 1;
    }

    glfwSetWindowPos(window, 600, 400);
    glfwShowWindow(window);
    glfwMakeContextCurrent(window);

    glClearColor(0.0, 0.0, 0.0, 0.0);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    GLint max_tex_image_units = 0;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_tex_image_units);
    assert(max_tex_image_units >= 27);

    GLfloat cube_radius = (1.0f / 7.0f);
    CubeModel _model = {{}, {}, {}, {}, cube_radius};
    CubeModel *model = &_model;
    init_cube(model);

    Context _ctx = {};
    Context *ctx = &_ctx;
    init_context(ctx, init_width, init_height);

    glfwSetWindowUserPointer(window, ctx);

    // callbacks
    glfwSetFramebufferSizeCallback(window, handle_framebuffer_size);
    glfwSetCursorPosCallback(window, _handle_cursor_pos);

    // TODO(bhester): tune these...
    GLfloat perspective_mat[16] = {};
    init_perspective_mat(ctx->props, perspective_mat);

    CubeProgram programs[27] = {};
    for (GLuint i = 0; i < 27; ++i) {
        CubeProgram *program = programs + i;

        *program = get_cube_program();

        glUseProgram(program->prog);
        glBindVertexArray(program->vao);
        glActiveTexture((GL_TEXTURE0) + i);

        glBindBuffer(GL_ARRAY_BUFFER, program->vbo);

        glVertexAttribPointer(program->info.pos_loc, 3, GL_FLOAT, GL_FALSE,
                              4 * sizeof(GLfloat), (void const *)0);
        glEnableVertexAttribArray(program->info.pos_loc);

        glVertexAttribPointer(program->info.tex_ind_loc, 1, GL_FLOAT, GL_FALSE,
                              4 * sizeof(GLfloat),
                              (void const *)(3 * sizeof(GLfloat)));
        glEnableVertexAttribArray(program->info.tex_ind_loc);

        bind_cube_pos_data(model->radius, program->vbo);

        glUniform1i(program->info.tex_loc, i);

        glBindTexture(GL_TEXTURE_1D, program->texture);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    Camera cam = {};
    GLfloat cam_mat[16];

    cam.tz = 16.0f;
    get_camera(cam_mat, &cam);

    int idx = 0;

    double target_time = 1.0 / 16.0 ; // seconds per frame
    double cur_time = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // FIXME(bhester): do I want to have some flag that tells me when I need
        // to do this? Or just do it anyway since its like 5 numbers
        init_perspective_mat(ctx->props, perspective_mat);

        GLfloat near_x = screen_to_near_x(ctx->xpos, ctx->width, ctx->props);
        GLfloat near_y = screen_to_near_y(ctx->ypos, ctx->height, ctx->props);
        GLfloat near_z = -ctx->props.near;

        for (int x = 0; x < 3; ++x) {
            for (int y = 0; y < 3; ++y) {
                for (int z = 0; z < 3; ++z) {
                    int index = 3 * (3 * (x) + y) + z;

                    CubeProgram program = programs[index];

                    GLfloat obj_mat[16] = {};
                    GLubyte colors[6 * 3] = {};

                    set_cube_obj_matrix(model, x, y, z, obj_mat);
                    fill_cube_tex_colors(model, x, y, z, colors);

                    GLfloat nearest_face;
                    bool intersects = cube_intersects(near_x, near_y, near_z,
                                                      model, obj_mat, cam_mat,
                                                      &nearest_face);

                    glUseProgram(program.prog);
                    glBindVertexArray(program.vao);
                    glActiveTexture((GL_TEXTURE0) + index);

                    if (intersects) {
                        glUniform1f(program.info.ix_face_loc, nearest_face);
                    } else {
                        glUniform1f(program.info.ix_face_loc, -1.0f);
                    }

                    // TODO(bhester): consider using glUinform
                    glUniformMatrix4fv(program.info.cam_loc, 1, GL_TRUE, cam_mat);
                    glUniformMatrix4fv(program.info.per_loc, 1, GL_TRUE, perspective_mat);
                    glUniformMatrix4fv(program.info.obj_loc, 1, GL_TRUE, obj_mat);

                    glBindTexture(GL_TEXTURE_1D, program.texture);
                    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB, 6, 0, GL_RGB,
                                 GL_UNSIGNED_BYTE, colors);

                    glDrawArrays(GL_TRIANGLES, 0, TRIANGLE_VERT_COUNT);
                }
            }
        }


        glfwSwapBuffers(window);
        glfwPollEvents();

        double next_time = glfwGetTime();
        double delta_time = next_time - cur_time;

        if (delta_time < target_time) {
            double sleep_time = target_time - delta_time;
            double MICROSECONDS = 1000000;
            usleep(sleep_time * MICROSECONDS);
        }

        cur_time = next_time;

        int key_q = glfwGetKey(window, GLFW_KEY_Q);
        if (key_q) glfwSetWindowShouldClose(window, 1);

        int key_w = glfwGetKey(window, GLFW_KEY_W);
        int key_a = glfwGetKey(window, GLFW_KEY_A);
        int key_s = glfwGetKey(window, GLFW_KEY_S);
        int key_d = glfwGetKey(window, GLFW_KEY_D);
        int key_i = glfwGetKey(window, GLFW_KEY_I);
        int key_o = glfwGetKey(window, GLFW_KEY_O);

        int key_la = glfwGetKey(window, GLFW_KEY_LEFT);
        int key_ra = glfwGetKey(window, GLFW_KEY_RIGHT);
        int key_ua = glfwGetKey(window, GLFW_KEY_UP);
        int key_da = glfwGetKey(window, GLFW_KEY_DOWN);
        int key_pu = glfwGetKey(window, GLFW_KEY_PAGE_UP);
        int key_pd = glfwGetKey(window, GLFW_KEY_PAGE_DOWN);

        int key_z = glfwGetKey(window, GLFW_KEY_Z);
        int key_x = glfwGetKey(window, GLFW_KEY_X);
        int key_c = glfwGetKey(window, GLFW_KEY_C);

        int key_1 = glfwGetKey(window, GLFW_KEY_1);
        int key_2 = glfwGetKey(window, GLFW_KEY_2);
        int key_3 = glfwGetKey(window, GLFW_KEY_3);

        double speed = 1.0;

        if (key_w) cam.ty += speed * delta_time;
        if (key_a) cam.tx -= speed * delta_time;
        if (key_s) cam.ty -= speed * delta_time;
        if (key_d) cam.tx += speed * delta_time;
        if (key_i) cam.tz -= speed * delta_time;
        if (key_o) cam.tz += speed * delta_time;

        if (key_ua) cam.rotx += (speed * 0.125f) * delta_time;
        if (key_la) cam.roty += (speed * 0.125f) * delta_time;
        if (key_da) cam.rotx -= (speed * 0.125f) * delta_time;
        if (key_ra) cam.roty -= (speed * 0.125f) * delta_time;
        if (key_pu) cam.rotz -= (speed * 0.125f) * delta_time;
        if (key_pd) cam.rotz += (speed * 0.125f) * delta_time;

        if (key_1) idx = 0;
        if (key_2) idx = 1;
        if (key_3) idx = 2;

        if (key_z) model->xrot[idx] += speed * delta_time;
        if (key_x) model->yrot[idx] += speed * delta_time;
        if (key_c) model->zrot[idx] += speed * delta_time;

        get_camera(cam_mat, &cam);
    }

    for (GLuint i = 0; i < 27; ++i) {
        cleanup_cube_program(programs[i]);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
