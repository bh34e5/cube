#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GLFW/glfw3.h>

#include <assert.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>

#define unreachable _unreachable(__FILE__, __LINE__)
#define _unreachable(f, l) __unreachable(f, l)
#define __unreachable(f, l) assert(0 && ("Unreachable at " #f #l))

struct Context {
    double xpos, ypos;
};

Context *get_context(GLFWwindow *window) {
    assert(window != nullptr);
    Context *ctx = static_cast<Context *>(glfwGetWindowUserPointer(window));

    assert(ctx != nullptr);
    return ctx;
}

void handle_error(int error, char const *description) {
    fprintf(stderr, "GLFW Error (%d)): %s\n", error, description);
}

void _handle_framebuffer_size(GLFWwindow *, int width, int height) {
    glViewport(0, 0, width, height);
}

void _handle_cursor_pos(GLFWwindow* window, double xpos, double ypos) {
    Context *ctx = get_context(window);

    ctx->xpos = xpos;
    ctx->ypos = ypos;
}

GLFWframebuffersizefun handle_framebuffer_size = &_handle_framebuffer_size;
GLFWcursorposfun handle_cursor_pos = &_handle_cursor_pos;

#define FACE_COUNT (6)
#define TRIANGLE_VERT_COUNT (FACE_COUNT * (2 * 3))

enum Face {
    WHITE,
    RED,
    BLUE,
    ORANGE,
    GREEN,
    YELLOW,
};

struct FaceIter {
    bool done;
    Face face;
};

inline FaceIter begin_iter_faces() {
    return FaceIter{false, WHITE};
}

inline FaceIter next_face(FaceIter cur) {
    if (cur.done) return cur;

    switch (cur.face) {
    case WHITE:  return FaceIter{false, RED};
    case RED:    return FaceIter{false, BLUE};
    case BLUE:   return FaceIter{false, ORANGE};
    case ORANGE: return FaceIter{false, GREEN};
    case GREEN:  return FaceIter{false, YELLOW};
    case YELLOW: return FaceIter{true,  WHITE};
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

inline GLuint face_idx(Face face) {
    assert(face >= WHITE && face <= YELLOW);
    switch (face) {
    case WHITE:  return 0;
    case RED:    return 1;
    case BLUE:   return 2;
    case ORANGE: return 3;
    case GREEN:  return 4;
    case YELLOW: return 5;
    }

    unreachable;
}

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

char const *vert_shader_text = R"""(
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
)""";

char const *frag_shader_text = R"""(
#version 330 core

in vec3 pos_v;
in float tex_ind_v;
out vec4 color_out;

uniform sampler2D face_colors;

void main() {
    vec3 mapped = (pos_v + 1.0) * 0.5;

    int c = 0;
    if (abs(pos_v.x) > 0.4) ++c;
    if (abs(pos_v.y) > 0.4) ++c;
    if (abs(pos_v.z) > 0.4) ++c;

    color_out = (c > 1)
        ? vec4(0.2, 0.2, 0.2, 1.0)
        : texture(face_colors, vec2(tex_ind_v, 0.0));
}
)""";

struct Camera {
    GLfloat tx, ty, tz;
    GLfloat rotx, roty, rotz;
};

inline GLfloat row_mult(GLfloat lhs[16], GLfloat rhs[16], GLuint r, GLuint c)
{
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

void bind_cube_pos_data(GLfloat cube_radius, GLuint vbo, GLuint veo) {
    GLfloat verts[] = {
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

    GLuint total_count = sizeof(verts) / sizeof(*verts);
    GLuint row_count = total_count / 4;

    for (GLuint i = 0; i < row_count; ++i) {
        verts[i * 4 + 0] *= cube_radius;
        verts[i * 4 + 1] *= cube_radius;
        verts[i * 4 + 2] *= cube_radius;
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
}

void bind_cube_object_matrix(CubeModel const* model, int x, int y, int z,
                             GLint loc) {
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

    GLfloat xf = GLfloat(x);
    GLfloat yf = GLfloat(y);
    GLfloat zf = GLfloat(z);

    GLfloat mat[16] = {
        1.0f, 0.0f, 0.0f, (xf - 1.0f) * 2.0f * model->radius,
        0.0f, 1.0f, 0.0f, (yf - 1.0f) * 2.0f * model->radius,
        0.0f, 0.0f, 1.0f, (zf - 1.0f) * 2.0f * model->radius,
        0.0f, 0.0f, 0.0f, 1.0f,
    };

    mat_mult_left(xrot_mat, mat);
    mat_mult_left(yrot_mat, mat);
    mat_mult_left(zrot_mat, mat);

    glUniformMatrix4fv(loc, 1, GL_TRUE, mat);
}

void bind_cube_texture(CubeModel const *model, GLuint x, GLuint y, GLuint z,
                       GLuint texture) {
    GLubyte colors[6 * 3] = {};

    for (GLuint i = 0; i < 6; ++i) {
        colors[3 * i + 0] = 64 * (x + 1) - 1;
        colors[3 * i + 1] = 64 * (y + 1) - 1;
        colors[3 * i + 2] = 64 * (z + 1) - 1;
    }

    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 6, 1, 0, GL_RGB, GL_UNSIGNED_BYTE,
                 colors);
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

    GLFWwindow *window =
        glfwCreateWindow(640, 640, "Hello, world", nullptr, nullptr);

    glfwSetWindowPos(window, 600, 400);
    glfwShowWindow(window);

    if (window == nullptr) {
        fprintf(stderr, "Failed to create window\n");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);

    CubeModel _model = {{}, {}, {}, {}, 0.5f};
    CubeModel *model = &_model;
    init_cube(model);

    Context ctx = {};
    glfwSetWindowUserPointer(window, &ctx);

    glClearColor(0.0, 0.0, 0.0, 0.0);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // callbacks
    glfwSetFramebufferSizeCallback(window, handle_framebuffer_size);
    glfwSetCursorPosCallback(window, _handle_cursor_pos);

    GLuint prog = compile_link_program(vert_shader_text, frag_shader_text);

    glUseProgram(prog);
    GLint obj_loc = glGetUniformLocation(prog, "object");
    GLint cam_loc = glGetUniformLocation(prog, "camera");
    GLint per_loc = glGetUniformLocation(prog, "perspective");
    GLint pos_loc = glGetAttribLocation(prog, "pos");
    GLint tex_ind_loc = glGetAttribLocation(prog, "tex_ind");

    GLuint vaos[27] = {};
    GLuint vbo = 0;
    GLuint veo = 0;
    GLuint textures[27] = {};

    glGenVertexArrays(27, vaos);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &veo);
    glGenTextures(27, textures);

    // TODO(bhester): tune these...
    GLfloat near = 1.0f;
    GLfloat far = 12.0f;
    GLfloat width = 2.0f;
    GLfloat height = 2.0f;
    GLfloat perspective_mat[16] = {
        2.0f * near / width, 0.0f,                 0.0f,                         0.0f,
        0.0f,                2.0f * near / height, 0.0f,                         0.0f,
        0.0f,                0.0f,                 -(far + near) / (far - near), -2.0f * far * near / (far - near),
        0.0f,                0.0f,                 -1.0f,                        0.0f,
    };

    for (GLuint i = 0; i < 27; ++i) {
        GLuint vao = vaos[i];
        GLuint texture = textures[i];

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);

        glUniformMatrix4fv(per_loc, 1, GL_TRUE, perspective_mat);

        glVertexAttribPointer(pos_loc, 3, GL_FLOAT, GL_FALSE,
                              4 * sizeof(GLfloat), (void const *)0);
        glEnableVertexAttribArray(pos_loc);

        glVertexAttribPointer(tex_ind_loc, 1, GL_FLOAT, GL_FALSE,
                              4 * sizeof(GLfloat),
                              (void const *)(3 * sizeof(GLfloat)));
        glEnableVertexAttribArray(tex_ind_loc);

        bind_cube_pos_data(model->radius, vbo, veo);

        glBindTexture(GL_TEXTURE_2D, texture);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    Camera cam = {};
    GLfloat cam_mat[16];

    cam.tz = 4.0f;
    get_camera(cam_mat, &cam);

    int idx = 0;

    double target_time = 1.0 / 16.0 ; // seconds per frame
    double cur_time = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUniformMatrix4fv(cam_loc, 1, GL_TRUE, cam_mat);

        for (int x = 0; x < 3; ++x) {
            for (int y = 0; y < 3; ++y) {
                for (int z = 0; z < 3; ++z) {
                    int index = 3 * (3 * (x) + y) + z;
                    GLuint vao = vaos[index];
                    GLuint texture = textures[index];

                    glBindVertexArray(vao);

                    bind_cube_object_matrix(model, x, y, z, obj_loc);
                    bind_cube_texture(model, x, y, z, texture);
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

        double speed = 0.5;

        if (key_w) cam.ty += speed * delta_time;
        if (key_a) cam.tx -= speed * delta_time;
        if (key_s) cam.ty -= speed * delta_time;
        if (key_d) cam.tx += speed * delta_time;
        if (key_i) cam.tz -= speed * delta_time;
        if (key_o) cam.tz += speed * delta_time;

        if (key_ua) cam.rotx += speed * delta_time;
        if (key_la) cam.roty += speed * delta_time;
        if (key_da) cam.rotx -= speed * delta_time;
        if (key_ra) cam.roty -= speed * delta_time;
        if (key_pu) cam.rotz -= speed * delta_time;
        if (key_pd) cam.rotz += speed * delta_time;

        if (key_1) idx = 0;
        if (key_2) idx = 1;
        if (key_3) idx = 2;

        if (key_z) model->xrot[idx] += speed * delta_time;
        if (key_x) model->yrot[idx] += speed * delta_time;
        if (key_c) model->zrot[idx] += speed * delta_time;

        get_camera(cam_mat, &cam);
    }

    glDeleteTextures(27, textures);
    glDeleteBuffers(1, &veo);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(27, vaos);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
