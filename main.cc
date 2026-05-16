#include "common.hh"
#include "gl_functions.hh"

#include <GLFW/glfw3.h>

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GL_FN_DECL(glName, shortName) decltype(&glName) shortName;
struct GL {
    FOR_GL_FUNCTIONS(GL_FN_DECL)
};
#undef GL_FN_DECL

#define EVENT_COUNT 256
#define NEXT_EVENT(n) (((n) + 1) % EVENT_COUNT)

struct KeyEvent {
    int key;
    int action;
    int mods;
};

struct Context {
    KeyEvent events[EVENT_COUNT];
    unsigned short next_event;
    unsigned short last_event;
};

void glfwError(int error_code, char const *description) {
    fprintf(stderr, "GLFW Error(%d): %s\n", error_code, description);
}

void checkError(GL &gl, int line) {
    GLenum err = gl.getError();
    if (err != GL_NO_ERROR) {
        fprintf(stderr, "GL Error(%d): %d (0x%x)\n", line, err, err);
    }
}

void glfwHandleKey(GLFWwindow *window, int key, int scancode, int action,
                   int mods) {
    (void)scancode;

    Context *context = (Context *)glfwGetWindowUserPointer(window);

    KeyEvent event = {};
    event.key = key;
    event.action = action;
    event.mods = mods;

    unsigned short next_event = NEXT_EVENT(context->next_event);
    if (next_event != context->last_event) {
        context->events[context->next_event] = event;
        context->next_event = next_event;
    }
}

int loadFunctions(GL &gl) {
#define LOAD_FN(glName, shortName)                                             \
    decltype(&glName) _##glName = nullptr;                                     \
    do {                                                                       \
        _##glName =                                                            \
            (decltype(&glName))((void *)(glfwGetProcAddress(#glName)));        \
        if (_##glName == nullptr) {                                            \
            return -1;                                                         \
        }                                                                      \
    } while (0);

#define STORE_FN(glName, shortName) gl.shortName = _##glName;

    FOR_GL_FUNCTIONS(LOAD_FN);
    FOR_GL_FUNCTIONS(STORE_FN);

    return 0;

#undef STORE_FN
#undef LOAD_FN
}

struct Shader {
    GLuint handle;
};

char *readFile(char const *filename) {
    char *contents = nullptr;

    FILE *file = fopen(filename, "r");
    if (file != nullptr) {
        long int file_len = 0;

        fseek(file, 0, SEEK_END);
        file_len = ftell(file);
        fseek(file, 0, SEEK_SET);

        assert(file_len == (long int)(unsigned int)file_len);

        contents = (char *)malloc(file_len + 1);

        assert(contents != nullptr);

        unsigned long nread = fread(contents, file_len, 1, file);
        assert(nread == 1);

        contents[file_len] = '\0';

        fclose(file);
    }

    return contents;
}

Shader loadShaderContents(GL &gl, GLenum shaderKind, char const *contents) {
    Shader shader = {};

    GLuint handle = gl.createShader(shaderKind);
    if (handle) {
        gl.shaderSource(handle, 1, &contents, nullptr);
        gl.compileShader(handle);

        GLint status = {};
        gl.getShaderiv(handle, GL_COMPILE_STATUS, &status);

        if (status == GL_TRUE) {
            shader.handle = handle;
        } else {
            GLint log_length = 0;
            gl.getShaderiv(handle, GL_INFO_LOG_LENGTH, &log_length);

            GLchar *log = (GLchar *)malloc(log_length);
            assert(log != nullptr);

            gl.getShaderInfoLog(handle, log_length, nullptr, log);

            fprintf(stderr, "Compile error: %.*s\n", log_length, log);

            gl.deleteShader(handle);
            free(log);
        }
    }

    return shader;
}

Shader loadShader(GL &gl, GLenum shaderKind, char const *filename) {
    Shader shader = {};

    char *contents = readFile(filename);
    if (contents != nullptr) {
        shader = loadShaderContents(gl, shaderKind, contents);

        free(contents);
    }

    return shader;
}

void deleteShader(GL &gl, Shader &shader) {
    gl.deleteShader(shader.handle);
    shader.handle = 0;
}

struct Program {
    GLuint handle;
};

Program compileProgram(GL &gl, Slice<Shader> shaders) {
    Program prog = {};

    GLuint handle = gl.createProgram();
    if (handle) {
        for (Shader const &shader : shaders) {
            if (shader.handle) {
                gl.attachShader(handle, shader.handle);
            }
        }

        gl.linkProgram(handle);

        GLint status = {};
        gl.getProgramiv(handle, GL_LINK_STATUS, &status);

        if (status == GL_TRUE) {
            prog.handle = handle;
        } else {
            GLint log_length = 0;
            gl.getProgramiv(handle, GL_INFO_LOG_LENGTH, &log_length);

            GLchar *log = (GLchar *)malloc(log_length);
            assert(log != nullptr);

            gl.getProgramInfoLog(handle, log_length, nullptr, log);

            fprintf(stderr, "Link error: %.*s\n", log_length, log);

            gl.deleteProgram(handle);
            free(log);
        }
    }

    return prog;
}

void deleteProgram(GL &gl, Program &program) {
    gl.deleteProgram(program.handle);
    program.handle = 0;
}

struct VertexArray {
    GLuint handle;
};

void genVertexArrays(GL &gl, Slice<VertexArray> vaos) {
    assert(sizeof(VertexArray) == sizeof(GLuint));
    gl.genVertexArrays(vaos.len, (GLuint *)vaos.dat);
}

VertexArray genVertexArray(GL &gl) {
    VertexArray vao = {};

    gl.genVertexArrays(1, &vao.handle);

    return vao;
}

void deleteVertexArray(GL &gl, VertexArray &vao) {
    gl.deleteVertexArrays(1, &vao.handle);
    vao.handle = 0;
}

struct Buffer {
    GLuint handle;
};

void genBuffers(GL &gl, Slice<Buffer> buffers) {
    assert(sizeof(Buffer) == sizeof(GLuint));
    gl.genVertexArrays(buffers.len, (GLuint *)buffers.dat);
}

Buffer genBuffer(GL &gl) {
    Buffer buffer = {};

    gl.genBuffers(1, &buffer.handle);

    return buffer;
}

void deleteBuffer(GL &gl, Buffer &buffer) {
    gl.deleteBuffers(1, &buffer.handle);
    buffer.handle = 0;
}

struct Texture {
    GLuint handle;
};

Texture genTexture(GL &gl) {
    Texture tex = {};

    gl.genTextures(1, &tex.handle);

    return tex;
}

void deleteTexture(GL &gl, Texture &tex) {
    gl.deleteTextures(1, &tex.handle);
    tex.handle = 0;
}

struct Matrix4 {
    float vals[16];
};

#define MAT4(x, y) ((x) * 4 + (y))

Matrix4 identityMatrix4() {
    return {
        1.0f, 0.0f, 0.0f, 0.0f, //
        0.0f, 1.0f, 0.0f, 0.0f, //
        0.0f, 0.0f, 1.0f, 0.0f, //
        0.0f, 0.0f, 0.0f, 1.0f, //
    };
}

Matrix4 perspectiveMatrix(float near, float far, float width, float height) {
    // float perspective_mat[16] = {
    //     2.0*n/w, 0.0,      0.0,          0.0,
    //     0.0,     2.0*n/h,  0.0,          0.0,
    //     0.0,     0.0,     -(f+n)/(f-n), -2.0*f*n/(f-n),
    //     0.0,     0.0,     -1.0,          0.0,
    // };
    //
    // in the limit far -> +Inf, we get
    // float perspective_mat[16] = {
    //     2.0*n/w, 0.0,      0.0,  0.0,
    //     0.0,     2.0*n/h,  0.0,  0.0,
    //     0.0,     0.0,     -1.0, -2.0*n,
    //     0.0,     0.0,     -1.0,  0.0,
    // };

    Matrix4 mat = {};

    mat.vals[MAT4(0, 0)] = 2.0f * near / width;
    mat.vals[MAT4(1, 1)] = 2.0f * near / height;
    mat.vals[MAT4(2, 2)] = -(far + near) / (far - near);
    mat.vals[MAT4(2, 3)] = -2.0f * far * near / (far - near);
    mat.vals[MAT4(3, 2)] = -1.0f;

    return mat;
}

Matrix4 xAxisRotation(float theta) {
    Matrix4 mat = identityMatrix4();

    float ct = cos(theta);
    float st = sin(theta);

    mat.vals[MAT4(1, 1)] = +ct;
    mat.vals[MAT4(1, 2)] = +st;
    mat.vals[MAT4(2, 1)] = -st;
    mat.vals[MAT4(2, 2)] = +ct;

    return mat;
}

Matrix4 yAxisRotation(float theta) {
    Matrix4 mat = identityMatrix4();

    float ct = cos(theta);
    float st = sin(theta);

    mat.vals[MAT4(0, 0)] = +ct;
    mat.vals[MAT4(0, 2)] = -st;
    mat.vals[MAT4(2, 0)] = +st;
    mat.vals[MAT4(2, 2)] = +ct;

    return mat;
}

Matrix4 operator*(Matrix4 const &lhs, Matrix4 const &rhs) {
    Matrix4 res; // no clear because we are setting every value

    for (char i = 0; i < 4; ++i) {
        for (char j = 0; j < 4; ++j) {
            float r = 0.0f;
            for (char k = 0; k < 4; ++k) {
                r += lhs.vals[MAT4(i, k)] * rhs.vals[MAT4(k, j)];
            }

            res.vals[MAT4(i, j)] = r;
        }
    }

    return res;
}

struct Vector3 {
    float x;
    float y;
    float z;
};

struct Cubie {
    Vector3 position;
    Vector3 rotation;
    float colors[6];
};

#define VERTEX_COUNT (LEN(cube_vertices) / 4)
float cube_vertices[] = {
    // bottom
    -0.5f, -0.5f, -0.5f, 0.0f, //
    -0.5f, +0.5f, -0.5f, 0.0f, //
    +0.5f, +0.5f, -0.5f, 0.0f, //

    -0.5f, -0.5f, -0.5f, 0.0f, //
    +0.5f, +0.5f, -0.5f, 0.0f, //
    +0.5f, -0.5f, -0.5f, 0.0f, //

    // front
    +0.5f, -0.5f, -0.5f, 1.0f, //
    +0.5f, +0.5f, -0.5f, 1.0f, //
    +0.5f, +0.5f, +0.5f, 1.0f, //

    +0.5f, -0.5f, -0.5f, 1.0f, //
    +0.5f, +0.5f, +0.5f, 1.0f, //
    +0.5f, -0.5f, +0.5f, 1.0f, //

    // right
    +0.5f, +0.5f, -0.5f, 2.0f, //
    -0.5f, +0.5f, -0.5f, 2.0f, //
    -0.5f, +0.5f, +0.5f, 2.0f, //

    +0.5f, +0.5f, -0.5f, 2.0f, //
    -0.5f, +0.5f, +0.5f, 2.0f, //
    +0.5f, +0.5f, +0.5f, 2.0f, //

    // back
    -0.5f, +0.5f, -0.5f, 3.0f, //
    -0.5f, -0.5f, -0.5f, 3.0f, //
    -0.5f, -0.5f, +0.5f, 3.0f, //

    -0.5f, +0.5f, -0.5f, 3.0f, //
    -0.5f, -0.5f, +0.5f, 3.0f, //
    -0.5f, +0.5f, +0.5f, 3.0f, //

    // left
    -0.5f, -0.5f, -0.5f, 4.0f, //
    +0.5f, -0.5f, -0.5f, 4.0f, //
    +0.5f, -0.5f, +0.5f, 4.0f, //

    -0.5f, -0.5f, -0.5f, 4.0f, //
    +0.5f, -0.5f, +0.5f, 4.0f, //
    -0.5f, -0.5f, +0.5f, 4.0f, //

    // top
    +0.5f, -0.5f, +0.5f, 5.0f, //
    +0.5f, +0.5f, +0.5f, 5.0f, //
    -0.5f, +0.5f, +0.5f, 5.0f, //

    +0.5f, -0.5f, +0.5f, 5.0f, //
    -0.5f, +0.5f, +0.5f, 5.0f, //
    -0.5f, -0.5f, +0.5f, 5.0f, //
};

#define INSTANCE_COUNT (LEN(cubes) / 6)
float cubes[] = {
    /* pos */ -1.1f, -1.1f, -1.1f, /* rot */ +0.0f, +0.0f, +0.0f, //
    /* pos */ -1.1f, -1.1f, +0.0f, /* rot */ +0.0f, +0.0f, +0.0f, //
    /* pos */ -1.1f, -1.1f, +1.1f, /* rot */ +0.0f, +0.0f, +0.0f, //
    /* pos */ -1.1f, +0.0f, -1.1f, /* rot */ +0.0f, +0.0f, +0.0f, //
    /* pos */ -1.1f, +0.0f, +0.0f, /* rot */ +0.0f, +0.0f, +0.0f, //
    /* pos */ -1.1f, +0.0f, +1.1f, /* rot */ +0.0f, +0.0f, +0.0f, //
    /* pos */ -1.1f, +1.1f, -1.1f, /* rot */ +0.0f, +0.0f, +0.0f, //
    /* pos */ -1.1f, +1.1f, +0.0f, /* rot */ +0.0f, +0.0f, +0.0f, //
    /* pos */ -1.1f, +1.1f, +1.1f, /* rot */ +0.0f, +0.0f, +0.0f, //
    /* pos */ +0.0f, -1.1f, -1.1f, /* rot */ +0.0f, +0.0f, +0.0f, //
    /* pos */ +0.0f, -1.1f, +0.0f, /* rot */ +0.0f, +0.0f, +0.0f, //
    /* pos */ +0.0f, -1.1f, +1.1f, /* rot */ +0.0f, +0.0f, +0.0f, //
    /* pos */ +0.0f, +0.0f, -1.1f, /* rot */ +0.0f, +0.0f, +0.0f, //
    /* pos */ +0.0f, +0.0f, +0.0f, /* rot */ +0.0f, +0.0f, +0.0f, //
    /* pos */ +0.0f, +0.0f, +1.1f, /* rot */ +0.0f, +0.0f, +0.0f, //
    /* pos */ +0.0f, +1.1f, -1.1f, /* rot */ +0.0f, +0.0f, +0.0f, //
    /* pos */ +0.0f, +1.1f, +0.0f, /* rot */ +0.0f, +0.0f, +0.0f, //
    /* pos */ +0.0f, +1.1f, +1.1f, /* rot */ +0.0f, +0.0f, +0.0f, //
    /* pos */ +1.1f, -1.1f, -1.1f, /* rot */ +0.0f, +0.0f, +0.0f, //
    /* pos */ +1.1f, -1.1f, +0.0f, /* rot */ +0.0f, +0.0f, +0.0f, //
    /* pos */ +1.1f, -1.1f, +1.1f, /* rot */ +0.0f, +0.0f, +0.0f, //
    /* pos */ +1.1f, +0.0f, -1.1f, /* rot */ +0.0f, +0.0f, +0.0f, //
    /* pos */ +1.1f, +0.0f, +0.0f, /* rot */ +0.0f, +0.0f, +0.0f, //
    /* pos */ +1.1f, +0.0f, +1.1f, /* rot */ +0.0f, +0.0f, +0.0f, //
    /* pos */ +1.1f, +1.1f, -1.1f, /* rot */ +0.0f, +0.0f, +0.0f, //
    /* pos */ +1.1f, +1.1f, +0.0f, /* rot */ +0.0f, +0.0f, +0.0f, //
    /* pos */ +1.1f, +1.1f, +1.1f, /* rot */ +0.0f, +0.0f, +0.0f, //
};

GLubyte *generateCubeTexture() {
    GLubyte *vals = (GLubyte *)malloc(INSTANCE_COUNT * 4 * 6 * sizeof(GLubyte));

    unsigned int stride = 4 * 6 * sizeof(GLubyte);
    GLubyte source[4 * 6] = {
        0xFF, 0xFF, 0x00, 0xFF, // yellow
        0xFF, 0x00, 0x00, 0xFF, // red
        0x00, 0x00, 0xFF, 0xFF, // blue
        0xFF, 0xA5, 0x00, 0xFF, // orange
        0x00, 0xFF, 0x00, 0xFF, // green
        0xFF, 0xFF, 0xFF, 0xFF, // white
    };

    for (unsigned int i = 0; i < INSTANCE_COUNT; ++i) {
        memcpy(vals + stride * i, source, stride);
    }

    return vals;
}

int main() {
    GL gl = {};
    GLFWwindow *window = {};
    Context context = {};

    GLubyte *color_tex_data = generateCubeTexture();

    glfwSetErrorCallback(glfwError);

    if (!glfwInit()) {
        fprintf(stderr, "Failed to init glfw\n");
        return -1;
    }

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
#if defined(__linux__)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
#endif

    window = glfwCreateWindow(1280, 960, "Hello, world", NULL, NULL);
    if (!window) {
        fprintf(stderr, "Failed to create window\n");
        glfwTerminate();
        return -1;
    }

    glfwSetWindowPos(window, 100, 100);
    glfwShowWindow(window);
    glfwMakeContextCurrent(window);

    if (loadFunctions(gl) < 0) {
        fprintf(stderr, "Failed to load functions\n");
        glfwTerminate();
        return -1;
    }

    glfwSetWindowUserPointer(window, &context);
    glfwSetKeyCallback(window, glfwHandleKey);

    gl.enable(GL_CULL_FACE);
    gl.enable(GL_DEPTH_TEST);

    gl.frontFace(GL_CCW);
    gl.cullFace(GL_BACK);

    Shader vert = loadShader(gl, GL_VERTEX_SHADER, "shaders/cube_vert.glsl");
    Shader frag = loadShader(gl, GL_FRAGMENT_SHADER, "shaders/cube_frag.glsl");

    Shader shaders[] = {vert, frag};
    Program cube_prog = compileProgram(gl, SLICE(Shader, shaders));

    VertexArray vao = genVertexArray(gl);

    Buffer verts_buf = genBuffer(gl);
    Buffer cube_state_buf = genBuffer(gl);

    Texture color_tex = genTexture(gl);

    gl.useProgram(cube_prog.handle);
    gl.bindVertexArray(vao.handle);

    gl.activeTexture(GL_TEXTURE0);
    gl.bindTexture(GL_TEXTURE_1D_ARRAY, color_tex.handle);
    gl.texImage2D(GL_TEXTURE_1D_ARRAY, 0, GL_RGBA8, 6, INSTANCE_COUNT, 0,
                  GL_RGBA, GL_UNSIGNED_BYTE, color_tex_data);

    GLint color_tex_loc = gl.getUniformLocation(cube_prog.handle, "color_tex");
    gl.uniform1i(color_tex_loc, 0);

    {
        gl.bindBuffer(GL_ARRAY_BUFFER, verts_buf.handle);
        gl.bufferData(GL_ARRAY_BUFFER, LEN(cube_vertices) * sizeof(float),
                      (void *)cube_vertices, GL_STATIC_DRAW);

        GLint a_vert_position_attrib_pos =
            gl.getAttribLocation(cube_prog.handle, "a_vert_position");
        GLint a_vert_tex_coord_attrib_pos =
            gl.getAttribLocation(cube_prog.handle, "a_vert_tex_coord");

        gl.enableVertexAttribArray(a_vert_position_attrib_pos);
        gl.enableVertexAttribArray(a_vert_tex_coord_attrib_pos);

        GLsizei stride = 4 * sizeof(float);
        gl.vertexAttribPointer(a_vert_position_attrib_pos, 3, GL_FLOAT,
                               GL_FALSE, stride, (void *)0);
        gl.vertexAttribPointer(a_vert_tex_coord_attrib_pos, 1, GL_FLOAT,
                               GL_FALSE, stride, (void *)(3 * sizeof(float)));
    }

    {
        gl.bindBuffer(GL_ARRAY_BUFFER, cube_state_buf.handle);
        gl.bufferData(GL_ARRAY_BUFFER, LEN(cubes) * sizeof(float),
                      (void *)cubes, GL_STATIC_DRAW);

        GLint a_cube_offset_attrib_pos =
            gl.getAttribLocation(cube_prog.handle, "a_cube_offset");
        GLint a_cube_rotation_attrib_pos =
            gl.getAttribLocation(cube_prog.handle, "a_cube_rotation");

        gl.enableVertexAttribArray(a_cube_offset_attrib_pos);
        gl.enableVertexAttribArray(a_cube_rotation_attrib_pos);

        GLsizei stride = 6 * sizeof(float);
        gl.vertexAttribPointer(a_cube_offset_attrib_pos, 3, GL_FLOAT, GL_FALSE,
                               stride, (void *)0);
        gl.vertexAttribPointer(a_cube_rotation_attrib_pos, 3, GL_FLOAT,
                               GL_FALSE, stride, (void *)(3 * sizeof(float)));

        gl.vertexAttribDivisor(a_cube_offset_attrib_pos, 1);
        gl.vertexAttribDivisor(a_cube_rotation_attrib_pos, 1);
    }

    gl.clearColor(0.0f, 0.0f, 0.0f, 0.0f);

    int i_width = {};
    int i_height = {};
    glfwGetWindowSize(window, &i_width, &i_height);

    float near = 12.0f;
    float far = 24.0f;
    float width = float(i_width) / 320.0f;
    float height = float(i_height) / 320.0f;

    float camera_translate = 20.0f;
    Matrix4 camera_rotation = identityMatrix4();
    Matrix4 perspective = perspectiveMatrix(near, far, width, height);

    GLint camera_translate_loc =
        gl.getUniformLocation(cube_prog.handle, "camera_translate");
    GLint camera_rotation_loc =
        gl.getUniformLocation(cube_prog.handle, "camera_rotation");

    GLint perspective_loc =
        gl.getUniformLocation(cube_prog.handle, "perspective");
    gl.uniformMatrix4fv(perspective_loc, 1, GL_TRUE, perspective.vals);

    gl.texParameteri(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.texParameteri(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl.texParameteri(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_WRAP_S,
                     GL_CLAMP_TO_BORDER);

    bool w_down = false;
    bool a_down = false;
    bool s_down = false;
    bool d_down = false;

    double last_time_seconds = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        double cur_time_seconds = glfwGetTime();
        double delta_time_seconds = cur_time_seconds - last_time_seconds;

        // handle inputs

        while (context.last_event != context.next_event) {
            KeyEvent event = context.events[context.last_event];

            if (event.action == GLFW_PRESS || event.action == GLFW_RELEASE) {
                switch (event.key) {
                case GLFW_KEY_W: {
                    w_down = event.action == GLFW_PRESS;
                } break;
                case GLFW_KEY_A: {
                    a_down = event.action == GLFW_PRESS;
                } break;
                case GLFW_KEY_S: {
                    s_down = event.action == GLFW_PRESS;
                } break;
                case GLFW_KEY_D: {
                    d_down = event.action == GLFW_PRESS;
                } break;
                }
            }

            context.last_event = NEXT_EVENT(context.last_event);
        }

        float camera_x_vel = 0.0f;
        float camera_y_vel = 0.0f;

        if (a_down) {
            camera_x_vel -= 1.0f;
        }
        if (d_down) {
            camera_x_vel += 1.0f;
        }
        if (w_down) {
            camera_y_vel -= 1.0f;
        }
        if (s_down) {
            camera_y_vel += 1.0f;
        }

        camera_rotation =
            yAxisRotation(camera_x_vel * delta_time_seconds) * camera_rotation;
        camera_rotation =
            xAxisRotation(camera_y_vel * delta_time_seconds) * camera_rotation;

        gl.uniform1f(camera_translate_loc, camera_translate);
        gl.uniformMatrix4fv(camera_rotation_loc, 1, GL_TRUE,
                            camera_rotation.vals);

        // render

        gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        gl.drawArraysInstanced(GL_TRIANGLES, 0, VERTEX_COUNT, INSTANCE_COUNT);

        glfwSwapBuffers(window);
        glfwPollEvents();

        last_time_seconds = cur_time_seconds;
    }

    deleteTexture(gl, color_tex);

    deleteBuffer(gl, cube_state_buf);
    deleteBuffer(gl, verts_buf);

    deleteVertexArray(gl, vao);

    deleteProgram(gl, cube_prog);
    deleteShader(gl, frag);
    deleteShader(gl, vert);

    free(color_tex_data);

    glfwTerminate();
    return 0;
}
