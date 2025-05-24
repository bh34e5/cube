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

    if (intersection_face >= -1e-3 && abs(tex_ind_v - intersection_face) < 1e-3) {
        // dim the intersected face
        color_out.rgb = 0.6 * color_out.rgb;
    }
}
)";

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

    glDeleteShader(vert_shader);
    glDeleteShader(frag_shader);

    return program;
}

inline GLfloat sgnf(GLfloat f) {
    if (f < 0) return -1.0f;
    if (f > 0) return +1.0f;
    return 0.0f;
}

enum Axis {
    X,
    Y,
    Z,
};

struct SignedAxis {
    Axis axis;
    bool neg;
};

inline SignedAxis axis_cross(SignedAxis lhs, SignedAxis rhs) {
    switch (lhs.axis) {
    case X: {
        switch (rhs.axis) {
        case X: assert(0 && "Bad cross");
        case Y: { return (SignedAxis){ .axis = Axis::Z, .neg = lhs.neg != rhs.neg }; } break;
        case Z: { return (SignedAxis){ .axis = Axis::Y, .neg = lhs.neg == rhs.neg }; } break;
        }
    } break;
    case Y: {
        switch (rhs.axis) {
        case X: { return (SignedAxis){ .axis = Axis::Z, .neg = lhs.neg == rhs.neg }; } break;
        case Y: assert(0 && "Bad cross");
        case Z: { return (SignedAxis){ .axis = Axis::X, .neg = lhs.neg != rhs.neg }; } break;
        }
    } break;
    case Z: {
        switch (rhs.axis) {
        case X: { return (SignedAxis){ .axis = Axis::Y, .neg = lhs.neg != rhs.neg }; } break;
        case Y: { return (SignedAxis){ .axis = Axis::X, .neg = lhs.neg == rhs.neg }; } break;
        case Z: assert(0 && "Bad cross");
        }
    } break;
    }
    unreachable;
}

template <GLuint N>
inline GLfloat vecdot(GLfloat const lhs[N], GLfloat const rhs[N]) {
    GLfloat res = 0.0f;
    for (GLuint i = 0; i < N; ++i) {
        res += lhs[i] * rhs[i];
    }
    return res;
}

template <GLuint N>
inline GLfloat veclen(GLfloat const vec[N]) {
    return vecdot<N>(vec, vec);
}

inline void set_identity(GLfloat mat[16]) {
    static GLfloat identity[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    memmove(mat, identity, sizeof(identity));
}

inline GLfloat row_mult(GLfloat const lhs[16], GLfloat const rhs[16], GLuint r,
                        GLuint c) {
    GLfloat val = 0.0f;
    for (GLuint i = 0; i < 4; ++i) {
        val += lhs[r * 4 + i] * rhs[i * 4 + c];
    }
    return val;
}

void mat_mult(GLfloat const lhs[16], GLfloat const rhs[16], GLfloat res[16]) {
    for (GLuint r = 0; r < 4; ++r) {
        for (GLuint c = 0 ; c < 4; ++c) {
            res[r * 4 + c] = row_mult(lhs, rhs, r, c);
        }
    }
}

void mat_mult_left(GLfloat const lhs[16], GLfloat rhs[16]) {
    GLfloat res[16];
    mat_mult(lhs, rhs, res);

    memmove(rhs, res, sizeof(res));
}

template <GLuint N>
inline GLfloat row_apply(GLfloat const lhs[N * N], GLfloat const rhs[N],
                         GLuint res) {
    GLfloat val = 0.0f;
    for (GLuint i = 0; i < N; ++i) {
        val += lhs[res * N + i] * rhs[i];
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

static GLfloat const verts[TRIANGLE_VERT_COUNT * 4] = {
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

struct MouseState {
    double xpos, ypos;
    bool down;
    double down_x, down_y;

    bool indices_saved;
    GLint cube_idx;
    GLfloat face_idx;
    GLfloat map_axis_1[4];
    GLfloat map_axis_2[4];
    GLfloat last_rot[16];
};

struct Context {
    int width, height;
    PerspectiveProps props;
    MouseState mouse;
};

void init_context(Context *ctx, int width, int height) {
    double d_width = width;
    double d_height = height;

    *ctx = (Context){
        .width = width,
        .height = height,
        .props = {
            .near = 12.0f,
            .far = 24.0f,
            .width = GLfloat(d_width * PIX_TO_SCREEN),
            .height = GLfloat(d_height * PIX_TO_SCREEN),
        },
        .mouse = {
            .xpos = {},
            .ypos = {},
            .down = false,
            .down_x = {},
            .down_y = {},
            .indices_saved = false,
            .cube_idx = -1,
            .face_idx = 1.0f,
            .map_axis_1 = {},
            .map_axis_2 = {},
            .last_rot = {},
        },
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
    ctx->mouse.xpos = xpos;
    ctx->mouse.ypos = ypos;
}

void _handle_mouse_button(GLFWwindow *window, int button, int action, int) {
    Context *ctx = get_context(window);
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            ctx->mouse.down = true;
            ctx->mouse.down_x = ctx->mouse.xpos;
            ctx->mouse.down_y = ctx->mouse.ypos;
        } else {
            assert(action == GLFW_RELEASE);
            ctx->mouse.down = false;
        }
    }
}

GLFWframebuffersizefun handle_framebuffer_size = &_handle_framebuffer_size;
GLFWcursorposfun handle_cursor_pos = &_handle_cursor_pos;
GLFWmousebuttonfun handle_mouse_button = &_handle_mouse_button;

SignedAxis get_normal(GLfloat face_idx) {
    GLint casted = GLint(face_idx);
    switch (casted) {
    case 0: return (SignedAxis){ .axis = Axis::Z, .neg = true };
    case 1: return (SignedAxis){ .axis = Axis::X, .neg = false };
    case 2: return (SignedAxis){ .axis = Axis::Y, .neg = false };
    case 3: return (SignedAxis){ .axis = Axis::X, .neg = true };
    case 4: return (SignedAxis){ .axis = Axis::Y, .neg = true };
    case 5: return (SignedAxis){ .axis = Axis::Z, .neg = false };
    }
    unreachable;
}

SignedAxis get_drag_axis_1(GLfloat face_idx) {
    GLint casted = GLint(face_idx);
    switch (casted) {
    case 0: return (SignedAxis){ .axis = Axis::Y, .neg = false };
    case 1: return (SignedAxis){ .axis = Axis::Z, .neg = true };
    case 2: return (SignedAxis){ .axis = Axis::Z, .neg = false };
    case 3: return (SignedAxis){ .axis = Axis::Z, .neg = false };
    case 4: return (SignedAxis){ .axis = Axis::Z, .neg = true };
    case 5: return (SignedAxis){ .axis = Axis::Y, .neg = true };
    }
    unreachable;
}

SignedAxis get_drag_axis_2(GLfloat face_idx) {
    GLint casted = GLint(face_idx);
    switch (casted) {
    case 0: return (SignedAxis){ .axis = Axis::X, .neg = true };
    case 1: return (SignedAxis){ .axis = Axis::Y, .neg = false };
    case 2: return (SignedAxis){ .axis = Axis::X, .neg = true };
    case 3: return (SignedAxis){ .axis = Axis::Y, .neg = true };
    case 4: return (SignedAxis){ .axis = Axis::X, .neg = false };
    case 5: return (SignedAxis){ .axis = Axis::X, .neg = false };
    }
    unreachable;
}

void fill_axis(SignedAxis axis, GLfloat vec[3]) {
    GLfloat s_one = axis.neg ? -1.0f : +1.0f;

    switch (axis.axis) {
    case X: { vec[0] = s_one; vec[1] = +0.0f; vec[2] = +0.0f; } return;
    case Y: { vec[0] = +0.0f; vec[1] = s_one; vec[2] = +0.0f; } return;
    case Z: { vec[0] = +0.0f; vec[1] = +0.0f; vec[2] = s_one; } return;
    }
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

struct CubePart {
    GLubyte colors[6][3];
    CubeProgram program;

    GLfloat tx_mat[16];
    GLfloat rx_mat[16];
};

struct CubeModel {
    CubePart parts[27];
    GLfloat radius;
};

enum Color {
    YELLOW = 0,
    RED    = 1,
    BLUE   = 2,
    ORANGE = 3,
    GREEN  = 4,
    WHITE  = 5,
};

void set_color(GLubyte dest[3], Color face) {
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

void init_cube_part(CubePart *part, GLuint x, GLuint y, GLuint z,
                    GLfloat radius) {
    GLfloat translation_mat[16] = {
        1.0f, 0.0f, 0.0f, (GLfloat(x) - 1.0f) * 2.0f * radius,
        0.0f, 1.0f, 0.0f, (GLfloat(y) - 1.0f) * 2.0f * radius,
        0.0f, 0.0f, 1.0f, (GLfloat(z) - 1.0f) * 2.0f * radius,
        0.0f, 0.0f, 0.0f, 1.0f,
    };

    switch (x) {
    case 0: {
        set_color(part->colors[3], Color::ORANGE);
    } break;
    case 1: {} break; // do nothing
    case 2: {
        set_color(part->colors[1], Color::RED);
    } break;
    default:
        unreachable;
    }

    switch (y) {
    case 0: {
        set_color(part->colors[4], Color::GREEN);
    } break;
    case 1: {} break; // do nothing
    case 2: {
        set_color(part->colors[2], Color::BLUE);
    } break;
    default:
        unreachable;
    }

    switch (z) {
    case 0: {
        set_color(part->colors[0], Color::YELLOW);
    } break;
    case 1: {} break; // do nothing
    case 2: {
        set_color(part->colors[5], Color::WHITE);
    } break;
    default:
        unreachable;
    }

    part->program = get_cube_program();
    memmove(part->tx_mat, translation_mat, sizeof(translation_mat));
    set_identity(part->rx_mat);
}

void init_cube(CubeModel *model) {
    for (GLuint x = 0; x < 3; ++x) {
        for (GLuint y = 0; y < 3; ++y) {
            for (GLuint z = 0; z < 3; ++z) {
                GLuint idx = 3 * 3 * x + 3 * y + z;
                init_cube_part(model->parts + idx, x, y, z, model->radius);
            }
        }
    }
}

void rotate_in_x(GLfloat mat[16], GLfloat delta) {
    GLfloat rotx_mat[16] = {
        1.0f, 0.0f,        0.0f,         0.0f,
        0.0f, cosf(delta), -sinf(delta), 0.0f,
        0.0f, sinf(delta), cosf(delta),  0.0f,
        0.0f, 0.0f,        0.0f,         1.0f,
    };

    mat_mult_left(mat, rotx_mat);
    memmove(mat, rotx_mat, sizeof(rotx_mat));
}

void rotate_in_y(GLfloat mat[16], GLfloat delta) {
    GLfloat roty_mat[16] = {
        cosf(delta),  0.0f, sinf(delta), 0.0f,
        0.0f,         1.0f, 0.0f,        0.0f,
        -sinf(delta), 0.0f, cosf(delta), 0.0f,
        0.0f,         0.0f, 0.0f,        1.0f,
    };

    mat_mult_left(mat, roty_mat);
    memmove(mat, roty_mat, sizeof(roty_mat));
}

void rotate_in_z(GLfloat mat[16], GLfloat delta) {
    GLfloat rotz_mat[16] = {
        cosf(delta), -sinf(delta), 0.0f, 0.0f,
        sinf(delta), cosf(delta),  0.0f, 0.0f,
        0.0f,        0.0f,         1.0f, 0.0f,
        0.0f,        0.0f,         0.0f, 1.0f,
    };

    mat_mult_left(mat, rotz_mat);
    memmove(mat, rotz_mat, sizeof(rotz_mat));
}

struct Camera {
    GLfloat tx, ty, tz;
    GLfloat rotx, roty, rotz;
};

void get_camera(GLfloat camera[16], Camera const *cam) {
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
    set_identity(camera);
    mat_mult_left(translation_mat, camera);
    mat_mult_left(rotz_mat, camera);
    mat_mult_left(roty_mat, camera);
    mat_mult_left(rotx_mat, camera);
}

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

void set_cube_obj_matrix(GLfloat const tx_mat[16], GLfloat const rx_mat[16],
                         GLfloat mat[16]) {
    set_identity(mat);
    mat_mult_left(tx_mat, mat);
    mat_mult_left(rx_mat, mat);
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

inline GLfloat screen_to_clip_x(double x, double screen_width) {
    return GLfloat(x / screen_width * 2.0 - 1.0);
}

inline GLfloat screen_to_clip_y(double y, double screen_height) {
    double inv_y = screen_height - y;
    return GLfloat(inv_y / screen_height * 2.0 - 1.0);
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
                     GLfloat obj_mat[16], GLfloat cam_mat[16], GLfloat *face,
                     GLfloat ix[3]) {
    bool found = false;

    GLfloat nearest_dist = FLT_MAX;
    GLfloat nearest_point[3] = {};
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
                memmove(nearest_point, tri_ix, sizeof(tri_ix));
                nearest_face = ca_face;
            }
            found = true;
        }
    }

    if (found) {
        memmove(ix, nearest_point, sizeof(nearest_point));
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
    CubeModel _model = {{}, cube_radius};
    CubeModel *model = &_model;
    init_cube(model);

    Context _ctx = {};
    Context *ctx = &_ctx;
    init_context(ctx, init_width, init_height);

    glfwSetWindowUserPointer(window, ctx);

    // callbacks
    glfwSetFramebufferSizeCallback(window, handle_framebuffer_size);
    glfwSetCursorPosCallback(window, handle_cursor_pos);
    glfwSetMouseButtonCallback(window, handle_mouse_button);

    // TODO(bhester): tune these...
    GLfloat perspective_mat[16] = {};
    init_perspective_mat(ctx->props, perspective_mat);

    for (GLuint i = 0; i < 27; ++i) {
        CubeProgram *program = &model->parts[i].program;

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

    double target_time = 1.0 / 16.0 ; // seconds per frame
    double cur_time = glfwGetTime();

    char const *pass_vert = R"(
#version 330 core

in vec3 point;

void main() {
    gl_Position = vec4(point, 1.0);
}
)";

    char const *white_frag = R"(
#version 330 core

out vec4 color_out;

uniform float first;

void main() {
    vec3 part = (first > 0.0) ? vec3(1.0) : vec3(1.0, 0.0, 0.0);
    color_out = vec4(part, 1.0);
}
)";

    GLuint debug_prog = compile_link_program(pass_vert, white_frag);
    GLuint debug_vao, debug_vbo;
    glCreateVertexArrays(1, &debug_vao);
    glCreateBuffers(1, &debug_vbo);

    glUseProgram(debug_prog);
    GLint debug_first_loc = glGetUniformLocation(debug_prog, "first");
    GLint debug_point_loc = glGetAttribLocation(debug_prog, "point");

    glLineWidth(20.0f);
    glBindVertexArray(debug_vao);
    glBindBuffer(GL_ARRAY_BUFFER, debug_vbo);
    glVertexAttribPointer(debug_point_loc, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (void const *)0);
    glEnableVertexAttribArray(debug_point_loc);

    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // FIXME(bhester): do I want to have some flag that tells me when I need
        // to do this? Or just do it anyway since its like 5 numbers
        init_perspective_mat(ctx->props, perspective_mat);

        GLfloat near_x = screen_to_near_x(ctx->mouse.xpos, ctx->width, ctx->props);
        GLfloat near_y = screen_to_near_y(ctx->mouse.ypos, ctx->height, ctx->props);
        GLfloat near_z = -ctx->props.near;

        GLfloat nearest_dist = FLT_MAX;
        GLfloat nearest_point[3] = {};
        GLfloat nearest_face = {};
        GLint intersection_cube = -1;

        if (ctx->mouse.down && ctx->mouse.indices_saved) {
            CubePart *part = model->parts + ctx->mouse.cube_idx;

            GLfloat obj_mat[16] = {};
            set_cube_obj_matrix(part->tx_mat, ctx->mouse.last_rot, obj_mat);

            GLfloat clip_x = screen_to_clip_x(ctx->mouse.xpos, ctx->width);
            GLfloat base_x = screen_to_clip_x(ctx->mouse.down_x, ctx->width);

            GLfloat clip_y = screen_to_clip_y(ctx->mouse.ypos, ctx->height);
            GLfloat base_y = screen_to_clip_y(ctx->mouse.down_y, ctx->height);

            GLfloat origin[4] = {0.0f, 0.0f, 0.0f, 1.0f};

            GLfloat *map_axis_1 = ctx->mouse.map_axis_1;
            GLfloat *map_axis_2 = ctx->mouse.map_axis_2;

            mat_apply_left<4>(obj_mat, origin);
            mat_apply_left<4>(cam_mat, origin);
            mat_apply_left<4>(perspective_mat, origin);

            GLfloat clip_diff[2] = {clip_x - base_x, clip_y - base_y};

            glUseProgram(debug_prog);
            glBindVertexArray(debug_vao);
            glBindBuffer(GL_ARRAY_BUFFER, debug_vbo);

            GLfloat fix_origin = 1.0f / origin[3];

            origin[0] *= fix_origin;
            origin[1] *= fix_origin;
            origin[2] *= fix_origin;
            origin[3] *= fix_origin;

            GLfloat points[4 * 3] = {
                origin[0],     origin[1],     origin[2],
                map_axis_1[0], map_axis_1[1], map_axis_1[2],
                origin[0],     origin[1],     origin[2],
                map_axis_2[0], map_axis_2[1], map_axis_2[2],
            };

            glBufferData(GL_ARRAY_BUFFER, sizeof(points), points, GL_STATIC_DRAW);
            glUniform1f(debug_first_loc, +1.0f);
            glDrawArrays(GL_LINES, 0, 2);
            glUniform1f(debug_first_loc, -1.0f);
            glDrawArrays(GL_LINES, 2, 2);

            GLfloat axis_1_dir = vecdot<2>(clip_diff, map_axis_1);
            GLfloat inv_axis_1_len = 1.0f / veclen<2>(map_axis_1);

            GLfloat axis_2_dir = vecdot<2>(clip_diff, map_axis_2);
            GLfloat inv_axis_2_len = 1.0f / veclen<2>(map_axis_2);

            SignedAxis drag_axis;
            SignedAxis non_drag_axis;
            GLfloat sign_dir;
            GLfloat mag;

            if (fabsf(axis_1_dir * inv_axis_1_len) > fabsf(axis_2_dir * inv_axis_2_len)) {
                // stronger in axis 1, so we drag in that direction
                drag_axis = get_drag_axis_1(ctx->mouse.face_idx);
                non_drag_axis = get_drag_axis_2(ctx->mouse.face_idx);
                sign_dir = sgnf(axis_1_dir);
                mag = veclen<2>(clip_diff) * inv_axis_1_len;
            } else {
                // stronger in axis 2, so we drag in that direction
                drag_axis = get_drag_axis_2(ctx->mouse.face_idx);
                non_drag_axis = get_drag_axis_1(ctx->mouse.face_idx);
                sign_dir = sgnf(axis_2_dir);
                mag = veclen<2>(clip_diff) * inv_axis_2_len;
            }

            SignedAxis face_normal = get_normal(ctx->mouse.face_idx);
            SignedAxis rot_axis = axis_cross(face_normal, drag_axis);

            GLfloat sign_rot = rot_axis.neg ? -1.0f : 1.0f;

            void (*rotate_in_axis)(GLfloat mat[16], GLfloat delta);
            switch (rot_axis.axis) {
            case X: {rotate_in_axis = &rotate_in_x;} break;
            case Y: {rotate_in_axis = &rotate_in_y;} break;
            case Z: {rotate_in_axis = &rotate_in_z;} break;
            }

            memmove(part->rx_mat, ctx->mouse.last_rot, sizeof(ctx->mouse.last_rot));
            rotate_in_axis(part->rx_mat, sign_rot * sign_dir * mag);
        }

        for (int x = 0; x < 3; ++x) {
            for (int y = 0; y < 3; ++y) {
                for (int z = 0; z < 3; ++z) {
                    int idx = 3 * 3 * x + 3 * y + z;
                    CubePart const *part = model->parts + idx;
                    CubeProgram program = part->program;

                    GLfloat obj_mat[16] = {};
                    set_cube_obj_matrix(part->tx_mat, part->rx_mat, obj_mat);

                    GLfloat cube_ix[3] = {};
                    GLfloat cube_face = {};
                    bool intersects = cube_intersects(near_x, near_y, near_z,
                                                      model, obj_mat, cam_mat,
                                                      &cube_face, cube_ix);

                    GLfloat lensq = veclen<3>(cube_ix);
                    if (intersects && lensq < nearest_dist) {
                        nearest_dist = lensq;
                        memmove(nearest_point, cube_ix, sizeof(cube_ix));
                        nearest_face = cube_face;
                        intersection_cube = idx;
                    }

                    glUseProgram(program.prog);
                    glBindVertexArray(program.vao);
                    glActiveTexture((GL_TEXTURE0) + idx);

                    if (intersects) {
                        glUniform1f(program.info.ix_face_loc, cube_face);
                    } else {
                        glUniform1f(program.info.ix_face_loc, -1.0f);
                    }

                    // TODO(bhester): consider using glUinform
                    glUniformMatrix4fv(program.info.cam_loc, 1, GL_TRUE, cam_mat);
                    glUniformMatrix4fv(program.info.per_loc, 1, GL_TRUE, perspective_mat);
                    glUniformMatrix4fv(program.info.obj_loc, 1, GL_TRUE, obj_mat);

                    glBindTexture(GL_TEXTURE_1D, program.texture);
                    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB, 6, 0, GL_RGB,
                                 GL_UNSIGNED_BYTE, part->colors);

                    glDrawArrays(GL_TRIANGLES, 0, TRIANGLE_VERT_COUNT);
                }
            }
        }

        if (intersection_cube >= 0 && ctx->mouse.down && !ctx->mouse.indices_saved) {
            ctx->mouse.cube_idx = intersection_cube;
            ctx->mouse.face_idx = nearest_face;

            CubePart const *part = model->parts + intersection_cube;
            memmove(ctx->mouse.last_rot, part->rx_mat, sizeof(part->rx_mat));

            GLfloat obj_mat[16] = {};
            set_cube_obj_matrix(part->tx_mat, part->rx_mat, obj_mat);

            SignedAxis axis_1 = get_drag_axis_1(nearest_face);
            SignedAxis axis_2 = get_drag_axis_2(nearest_face);

            GLfloat axis_1_vec[4] = {0.0f, 0.0f, 0.0f, 1.0f};
            GLfloat axis_2_vec[4] = {0.0f, 0.0f, 0.0f, 1.0f};

            fill_axis(axis_1, axis_1_vec);
            fill_axis(axis_2, axis_2_vec);

            GLfloat map_axis_1[4];
            GLfloat map_axis_2[4];

            memmove(map_axis_1, axis_1_vec, sizeof(axis_1_vec));
            memmove(map_axis_2, axis_2_vec, sizeof(axis_2_vec));

            mat_apply_left<4>(obj_mat, map_axis_1);
            mat_apply_left<4>(obj_mat, map_axis_2);

            mat_apply_left<4>(cam_mat, map_axis_1);
            mat_apply_left<4>(cam_mat, map_axis_2);

            mat_apply_left<4>(perspective_mat, map_axis_1);
            mat_apply_left<4>(perspective_mat, map_axis_2);

            GLfloat fix_axis_1 = 1.0f / map_axis_1[3];
            GLfloat fix_axis_2 = 1.0f / map_axis_2[3];

            map_axis_1[0] *= fix_axis_1;
            map_axis_1[1] *= fix_axis_1;
            map_axis_1[2] *= fix_axis_1;
            map_axis_1[3] *= fix_axis_1;

            map_axis_2[0] *= fix_axis_2;
            map_axis_2[1] *= fix_axis_2;
            map_axis_2[2] *= fix_axis_2;
            map_axis_2[3] *= fix_axis_2;

            memmove(ctx->mouse.map_axis_1, map_axis_1, sizeof(map_axis_1));
            memmove(ctx->mouse.map_axis_2, map_axis_2, sizeof(map_axis_2));

            ctx->mouse.indices_saved = true;
        } else if (!ctx->mouse.down) {
            ctx->mouse.indices_saved = false;
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

        get_camera(cam_mat, &cam);
    }

    for (GLuint i = 0; i < 27; ++i) {
        cleanup_cube_program(model->parts[i].program);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
