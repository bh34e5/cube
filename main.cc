#include "common.hh"
#include "gl_functions.hh"

#include <GLFW/glfw3.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define GL_FN_DECL(glName, shortName) decltype(&glName) shortName;
struct GL {
    FOR_GL_FUNCTIONS(GL_FN_DECL)
};
#undef GL_FN_DECL

void glfwError(int error_code, char const *description) {
    fprintf(stderr, "GLFW Error(%d): %s\n", error_code, description);
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

struct Matrix4 {
    float vals[16];
};

Matrix4 identityMatrix4() {
    return {
        1.0f, 0.0f, 0.0f, 0.0f, //
        0.0f, 1.0f, 0.0f, 0.0f, //
        0.0f, 0.0f, 1.0f, 0.0f, //
        0.0f, 0.0f, 0.0f, 1.0f, //
    };
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

float cube_vertices[] = {
    // bottom
    -0.5f, -0.5f, -0.5f, 0.0f, //
    +0.5f, +0.5f, -0.5f, 0.0f, //
    -0.5f, +0.5f, -0.5f, 0.0f, //

    -0.5f, -0.5f, -0.5f, 0.0f, //
    +0.5f, -0.5f, -0.5f, 0.0f, //
    +0.5f, +0.5f, -0.5f, 0.0f, //

    // front
    +0.5f, -0.5f, -0.5f, 1.0f, //
    +0.5f, +0.5f, -0.5f, 1.0f, //
    +0.5f, +0.5f, -0.5f, 1.0f, //

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

float cubes[] = {
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, //
    0.2f, 0.2f, 0.2f, 0.0f, 0.0f, 0.0f, //
};

int main() {
    GL gl = {};
    GLFWwindow *window = {};

    glfwSetErrorCallback(glfwError);

    if (!glfwInit()) {
        fprintf(stderr, "Failed to init glfw\n");
        return -1;
    }

#if defined(__linux__)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
#endif

    window = glfwCreateWindow(640, 480, "Hello, world", NULL, NULL);
    if (!window) {
        fprintf(stderr, "Failed to create window\n");
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    if (loadFunctions(gl) < 0) {
        fprintf(stderr, "Failed to load functions\n");
        glfwTerminate();
        return -1;
    }

    gl.enable(GL_CULL_FACE);
    gl.enable(GL_DEPTH_TEST);

    gl.frontFace(GL_CCW);
    gl.cullFace(GL_BACK);

    Shader vert = loadShader(gl, GL_VERTEX_SHADER, "shaders/cube_vert.glsl");
    Shader frag = loadShader(gl, GL_FRAGMENT_SHADER, "shaders/cube_frag.glsl");

    Shader shaders[] = {vert, frag};
    Program cubeProg = compileProgram(gl, SLICE(Shader, shaders));

    VertexArray vao = genVertexArray(gl);

    gl.useProgram(cubeProg.handle);
    gl.bindVertexArray(vao.handle);

    Buffer verts_buf = genBuffer(gl);
    Buffer cube_state_buf = genBuffer(gl);

    {
        gl.bindBuffer(GL_ARRAY_BUFFER, verts_buf.handle);
        gl.bufferData(GL_ARRAY_BUFFER, LEN(cube_vertices) * sizeof(float),
                      (void *)cube_vertices, GL_STATIC_DRAW);

        GLint a_vert_position_attrib_pos =
            gl.getAttribLocation(cubeProg.handle, "a_vert_position");
        GLint a_vert_tex_coord_attrib_pos =
            gl.getAttribLocation(cubeProg.handle, "a_vert_tex_coord");

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
            gl.getAttribLocation(cubeProg.handle, "a_cube_offset");
        GLint a_cube_rotation_attrib_pos =
            gl.getAttribLocation(cubeProg.handle, "a_cube_rotation");

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

    Matrix4 camera = identityMatrix4();

    GLint camera_loc = gl.getUniformLocation(cubeProg.handle, "camera");
    gl.uniformMatrix4fv(camera_loc, 1, GL_TRUE, camera.vals);

    // glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    // glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAX_FILTER, GL_NEAREST);
    // glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMPTO_BORDER);

    while (!glfwWindowShouldClose(window)) {
        gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        unsigned int vert_count = LEN(cube_vertices) / 4;
        unsigned int instance_count = LEN(cubes) / 6;

        gl.drawArraysInstanced(GL_TRIANGLES, 0, vert_count, instance_count);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    deleteProgram(gl, cubeProg);
    deleteShader(gl, frag);
    deleteShader(gl, vert);

    glfwTerminate();
    return 0;
}
