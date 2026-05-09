#include <GLFW/glfw3.h>

#include <stdio.h>

#include "gl_functions.hh"

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

int main() {
    GL gl = {};
    GLFWwindow *window = {};

    glfwSetErrorCallback(glfwError);

    if (!glfwInit()) {
        fprintf(stderr, "Failed to init glfw\n");
        return -1;
    }

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

    gl.clearColor(1.0f, 1.0f, 1.0f, 0.0f);

    while (!glfwWindowShouldClose(window)) {
        gl.clear(GL_COLOR_BUFFER_BIT);

        gl.begin(GL_TRIANGLES);

        gl.color3f(1.0f, 0.0f, 0.0f);
        gl.vertex2f(-0.5f, -0.5f);

        gl.color3f(0.0f, 1.0f, 0.0f);
        gl.vertex2f(+0.5f, -0.5f);

        gl.color3f(0.0f, 0.0f, 1.0f);
        gl.vertex2f(+0.0f, +0.5f);

        gl.end();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
