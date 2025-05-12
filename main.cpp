#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GLFW/glfw3.h>

#include <assert.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>

typedef unsigned int uint;

#define unreachable _unreachable(__FILE__, __LINE__)
#define _unreachable(f, l) __unreachable(f, l)
#define __unreachable(f, l) assert(0 && ("Unreachable at " #f #l))

void handle_error(int error, char const *description) {
    fprintf(stderr, "GLFW Error (%d)): %s\n", error, description);
}

void handle_framebuffer_size(GLFWwindow *, int width, int height) {
    glViewport(0, 0, width, height);
}

#define FACE_COUNT (6)
#define CUBE_INDEX_COUNT (FACE_COUNT * 2 * 3)
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
    if (cur.done) {
        return cur;
    }

    switch (cur.face) {
    case WHITE:
        return FaceIter{false, RED};
    case RED:
        return FaceIter{false, BLUE};
    case BLUE:
        return FaceIter{false, ORANGE};
    case ORANGE:
        return FaceIter{false, GREEN};
    case GREEN:
        return FaceIter{false, YELLOW};
    case YELLOW:
        return FaceIter{true, WHITE};
    }

    unreachable;
}

struct CubeModel {
    Face faces[FACE_COUNT][3][3] = {};
    float face_rot[FACE_COUNT] = {};  // looking down at a face, rot CCW
    float inner_rot[FACE_COUNT] = {}; // looking at a face, middle rot up
};

inline uint face_idx(Face face) {
    assert(face >= WHITE && face <= YELLOW);
    switch (face) {
    case WHITE:
        return 0;
    case RED:
        return 1;
    case BLUE:
        return 2;
    case ORANGE:
        return 3;
    case GREEN:
        return 4;
    case YELLOW:
        return 5;
    }

    unreachable;
}

void init_cube(CubeModel *cube) {
    FaceIter iter = begin_iter_faces();
    while (!iter.done) {
        uint idx = face_idx(iter.face);
        for (uint r = 0; r < 3; ++r) {
            for (uint c = 0; c < 3; ++c) {
                cube->faces[idx][r][c] = iter.face;
            }
        }

        iter = next_face(iter);
    }
}

struct Context {};

Context *get_context(GLFWwindow *window) {
    assert(window != nullptr);
    Context *ctx = static_cast<Context *>(glfwGetWindowUserPointer(window));

    assert(ctx != nullptr);
    return ctx;
}

char const *vert_shader_text = R"""(
#version 330 core

in vec3 pos;
out vec3 pos_v;

uniform mat4 object;
uniform mat4 camera;
uniform mat4 perspective;

void main() {
    vec4 pos_hom = vec4(pos, 1.0);

    pos_v = pos;
    gl_Position = perspective * camera * object * pos_hom;
}
)""";

char const *frag_shader_text = R"""(
#version 330 core

in vec3 pos_v;
out vec4 color_out;

void main() {
    vec3 mapped = (pos_v + 1.0) * 0.5;

    int c = 0;
    if (abs(pos_v.x) > 0.4) ++c;
    if (abs(pos_v.y) > 0.4) ++c;
    if (abs(pos_v.z) > 0.4) ++c;

    if (c > 1) color_out = vec4(0.2, 0.2, 0.2, 1.0);
    else color_out = vec4(mapped, 1.0);
}
)""";

struct Camera {
    float tx, ty, tz;
    float rotx, roty, rotz;
};

inline float row_mult(float lhs[16], float rhs[16], uint r, uint c)
{
    float val = 0.0f;
    for (uint i = 0; i < 4; ++i) {
        val += lhs[r * 4 + i] * rhs[i * 4 + c];
    }
    return val;
}

void mat_mult(float lhs[16], float rhs[16], float res[16]) {
    for (uint r = 0; r < 4; ++r) {
        for (uint c = 0 ; c < 4; ++c) {
            res[r * 4 + c] = row_mult(lhs, rhs, r, c);
        }
    }
}

void mat_mult_left(float lhs[16], float rhs[16]) {
    float res[16];
    mat_mult(lhs, rhs, res);

    memmove(rhs, res, sizeof(res));
}

void get_camera(float camera[16], Camera const *cam) {
    // init to identity
    for (uint i = 0; i < 16; ++i) {
        camera[i] = 0.0f;
    }

    camera[0 * 4 + 0] = 1.0f;
    camera[1 * 4 + 1] = 1.0f;
    camera[2 * 4 + 2] = 1.0f;
    camera[3 * 4 + 3] = 1.0f;

    // define matrices

    float rotx_mat[16] = {
        1.0f, 0.0f,             0.0f,              0.0f,
        0.0f, cosf(-cam->rotx), -sinf(-cam->rotx), 0.0f,
        0.0f, sinf(-cam->rotx), cosf(-cam->rotx),  0.0f,
        0.0f, 0.0f,             0.0f,              1.0f,
    };

    float roty_mat[16] = {
        cosf(-cam->roty),  0.0f, sinf(-cam->roty), 0.0f,
        0.0f,              1.0f, 0.0f,             0.0f,
        -sinf(-cam->roty), 0.0f, cosf(-cam->roty), 0.0f,
        0.0f,              0.0f, 0.0f,             1.0f,
    };

    float rotz_mat[16] = {
        cosf(-cam->rotz), -sinf(-cam->rotz), 0.0f, 0.0f,
        sinf(-cam->rotz), cosf(-cam->rotz),  0.0f, 0.0f,
        0.0f,             0.0f,              1.0f, 0.0f,
        0.0f,             0.0f,              0.0f, 1.0f,
    };

    float translation_mat[16] = {
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

GLuint compile_program(char const *vert, char const *frag) {
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
    float x, y, z;
};

struct Cube {
    Vec3 center;
    float radius;
};

void bind_cube_pos_data(Cube const *cube, GLuint vbo, GLuint veo) {
    GLfloat verts[8 * 3] = {
        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f, +1.0f,
        -1.0f, +1.0f, -1.0f,
        -1.0f, +1.0f, +1.0f,
        +1.0f, -1.0f, -1.0f,
        +1.0f, -1.0f, +1.0f,
        +1.0f, +1.0f, -1.0f,
        +1.0f, +1.0f, +1.0f,
    };
    GLuint indices[CUBE_INDEX_COUNT] = {
        0, 2, 6,
        0, 6, 4,
        4, 6, 7,
        4, 7, 5,
        6, 2, 3,
        6, 3, 7,
        2, 0, 1,
        2, 1, 3,
        0, 4, 5,
        0, 5, 1,
        5, 7, 3,
        5, 3, 1,
    };

    for (uint i = 0; i < 8; ++i) {
        verts[i * 3 + 0] = cube->radius * verts[i * 3 + 0] + cube->center.x;
        verts[i * 3 + 1] = cube->radius * verts[i * 3 + 1] + cube->center.y;
        verts[i * 3 + 2] = cube->radius * verts[i * 3 + 2] + cube->center.z;
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, veo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
                 GL_STATIC_DRAW);
}

void bind_cube_object_matrix(Cube const* cube, int x, int y, int z, GLint loc) {
    GLfloat mat[16] = {
        1.0f, 0.0f, 0.0f, (x - 1) * 2.0f * cube->radius,
        0.0f, 1.0f, 0.0f, (y - 1) * 2.0f * cube->radius,
        0.0f, 0.0f, 1.0f, (z - 1) * 2.0f * cube->radius,
        0.0f, 0.0f, 0.0f, 1.0f,
    };

    glUniformMatrix4fv(loc, 1, GL_TRUE, mat);
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

    CubeModel c = {};
    init_cube(&c);

    Context ctx = {};
    glfwSetWindowUserPointer(window, &ctx);

    glClearColor(0.0, 0.0, 0.0, 0.0);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // callbacks
    glfwSetFramebufferSizeCallback(window, handle_framebuffer_size);

    GLuint prog = compile_program(vert_shader_text, frag_shader_text);

    glUseProgram(prog);

    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint veo = 0;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &veo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    GLint loc = 0;

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

    loc = glGetUniformLocation(prog, "perspective");
    glUniformMatrix4fv(loc, 1, GL_TRUE, perspective_mat);

    loc = glGetAttribLocation(prog, "pos");
    glVertexAttribPointer(loc, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float),
                          (void const *)0);
    glEnableVertexAttribArray(loc);

    Cube cube = {{0.0f, 0.0f, 0.0f}, 0.5f};
    bind_cube_pos_data(&cube, vbo, veo);

    Camera cam = {};
    float cam_mat[16];

    cam.tz = 10.0f;
    get_camera(cam_mat, &cam);

    double target_time = 1.0 / 16.0 ; // seconds per frame
    double cur_time = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        loc = glGetUniformLocation(prog, "camera");
        glUniformMatrix4fv(loc, 1, GL_TRUE, cam_mat);

        loc = glGetUniformLocation(prog, "object");
        for (int x = 0; x < 3; ++x) {
            for (int y = 0; y < 3; ++y) {
                for (int z = 0; z < 3; ++z) {
                    bind_cube_object_matrix(&cube, x, y, z, loc);
                    glDrawElements(GL_TRIANGLES, CUBE_INDEX_COUNT, GL_UNSIGNED_INT,
                                   (GLvoid const *)0);
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

        get_camera(cam_mat, &cam);
    }

    glDeleteBuffers(1, &veo);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
