#include "common.hh"
#include "gl_functions.hh"
#include "math.hh"

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

#define DEBUG 1
#define DEBUG_INSTANCE_COUNT 9

struct KeyEvent {
    int key;
    int action;
    int mods;
};

struct MouseButtonEvent {
    int button;
    int action;
    int mods;
};

struct CursorPosEvent {
    double x;
    double y;
};

struct Event {
    enum Kind {
        Kind_None,
        Kind_Key,
        Kind_MouseButton,
        Kind_CursorPos,
    };

    Kind kind;
    union {
        KeyEvent key_event;
        MouseButtonEvent mouse_button_event;
        CursorPosEvent cursor_pos_event;
    };
};

struct Context {
    Event events[EVENT_COUNT];
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

void pushEvent(Context *context, Event event) {
    unsigned short next_event = NEXT_EVENT(context->next_event);
    if (next_event != context->last_event) {
        context->events[context->next_event] = event;
        context->next_event = next_event;
    }
}

void glfwHandleKey(GLFWwindow *window, int key, int scancode, int action,
                   int mods) {
    (void)scancode;

    Context *context = (Context *)glfwGetWindowUserPointer(window);

    KeyEvent key_event = {};
    key_event.key = key;
    key_event.action = action;
    key_event.mods = mods;

    Event event = {};
    event.kind = Event::Kind_Key;
    event.key_event = key_event;

    pushEvent(context, event);
}

void glfwHandleMouseButton(GLFWwindow *window, int button, int action,
                           int mods) {
    Context *context = (Context *)glfwGetWindowUserPointer(window);

    MouseButtonEvent mouse_button_event = {};
    mouse_button_event.button = button;
    mouse_button_event.action = action;
    mouse_button_event.mods = mods;

    Event event = {};
    event.kind = Event::Kind_MouseButton;
    event.mouse_button_event = mouse_button_event;

    pushEvent(context, event);
}

void glfwHandleCursorPos(GLFWwindow *window, double xpos, double ypos) {
    Context *context = (Context *)glfwGetWindowUserPointer(window);

    CursorPosEvent cursor_pos_event = {};
    cursor_pos_event.x = xpos;
    cursor_pos_event.y = ypos;

    Event event = {};
    event.kind = Event::Kind_CursorPos;
    event.cursor_pos_event = cursor_pos_event;

    pushEvent(context, event);
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

bool compileShader(GL &gl, GLuint handle) {
    bool successful = true;

    gl.compileShader(handle);

    GLint status = {};
    gl.getShaderiv(handle, GL_COMPILE_STATUS, &status);

    if (status != GL_TRUE) {
        GLint log_length = 0;
        gl.getShaderiv(handle, GL_INFO_LOG_LENGTH, &log_length);

        GLchar *log = (GLchar *)malloc(log_length);
        assert(log != nullptr);

        gl.getShaderInfoLog(handle, log_length, nullptr, log);

        fprintf(stderr, "Compile error: %.*s\n", log_length, log);

        gl.deleteShader(handle);
        successful = false;

        free(log);
    }

    return successful;
}

Shader loadShader(GL &gl, GLenum shaderKind, Slice<char const *> filenames) {
    Shader shader = {};

    GLuint handle = gl.createShader(shaderKind);
    if (handle) {
        bool successful = true;

        DList<char *> sources = {};
        sources.ensureSize(filenames.len);

        for (char const *filename : filenames) {
            char *contents = readFile(filename);
            if (contents != nullptr) {
                sources.push(contents);
            } else {
                // cleanup
                for (char *contents : sources.items()) {
                    free(contents);
                }
                sources.erase();

                successful = false;
            }
        }

        if (successful) {
            gl.shaderSource(handle, sources.len, sources.dat, nullptr);
            compileShader(gl, handle);

            for (char *contents : sources.items()) {
                free(contents);
            }
            sources.erase();

            successful = compileShader(gl, handle);
            if (successful) {
                shader.handle = handle;
            }
        }
    }

    return shader;
}

Shader loadShader(GL &gl, GLenum shaderKind, char const *filename) {
    Shader shader =
        loadShader(gl, shaderKind, Slice<char const *>{1, &filename});

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

struct Vertex {
    Vector3 offset;
    float face_id;
};

#define RAD 0.5f
#define VERTEX_COUNT (LEN(cube_vertices))
Vertex cube_vertices[] = {
    // bottom
    {{-RAD, -RAD, -RAD}, 0.0f}, //
    {{-RAD, +RAD, -RAD}, 0.0f}, //
    {{+RAD, +RAD, -RAD}, 0.0f}, //

    {{-RAD, -RAD, -RAD}, 0.0f}, //
    {{+RAD, +RAD, -RAD}, 0.0f}, //
    {{+RAD, -RAD, -RAD}, 0.0f}, //

    // front
    {{+RAD, -RAD, -RAD}, 1.0f}, //
    {{+RAD, +RAD, -RAD}, 1.0f}, //
    {{+RAD, +RAD, +RAD}, 1.0f}, //

    {{+RAD, -RAD, -RAD}, 1.0f}, //
    {{+RAD, +RAD, +RAD}, 1.0f}, //
    {{+RAD, -RAD, +RAD}, 1.0f}, //

    // right
    {{+RAD, +RAD, -RAD}, 2.0f}, //
    {{-RAD, +RAD, -RAD}, 2.0f}, //
    {{-RAD, +RAD, +RAD}, 2.0f}, //

    {{+RAD, +RAD, -RAD}, 2.0f}, //
    {{-RAD, +RAD, +RAD}, 2.0f}, //
    {{+RAD, +RAD, +RAD}, 2.0f}, //

    // back
    {{-RAD, +RAD, -RAD}, 3.0f}, //
    {{-RAD, -RAD, -RAD}, 3.0f}, //
    {{-RAD, -RAD, +RAD}, 3.0f}, //

    {{-RAD, +RAD, -RAD}, 3.0f}, //
    {{-RAD, -RAD, +RAD}, 3.0f}, //
    {{-RAD, +RAD, +RAD}, 3.0f}, //

    // left
    {{-RAD, -RAD, -RAD}, 4.0f}, //
    {{+RAD, -RAD, -RAD}, 4.0f}, //
    {{+RAD, -RAD, +RAD}, 4.0f}, //

    {{-RAD, -RAD, -RAD}, 4.0f}, //
    {{+RAD, -RAD, +RAD}, 4.0f}, //
    {{-RAD, -RAD, +RAD}, 4.0f}, //

    // top
    {{+RAD, -RAD, +RAD}, 5.0f}, //
    {{+RAD, +RAD, +RAD}, 5.0f}, //
    {{-RAD, +RAD, +RAD}, 5.0f}, //

    {{+RAD, -RAD, +RAD}, 5.0f}, //
    {{-RAD, +RAD, +RAD}, 5.0f}, //
    {{-RAD, -RAD, +RAD}, 5.0f}, //
};

struct Cube {
    Vector3 position;
    Quaternion rotation;
    Vector3 velocity; // dPosition
    Vector3 omega;    // dRotation
};

struct Joint {
    Cube *cube;
    Vector3 cube_r;
    float len;
};

Vector3 mapCubePoint(Cube const &cube, Vector3 point) {
    Matrix4 rot = quaternionRotation(cube.rotation);
    Matrix4 tx = translationMatr(cube.position);

    Vector<4> world = tx * rot * point.hom();
    assert(world.vals[3] == 1.0);

    Vector3 res = {
        world.vals[0],
        world.vals[1],
        world.vals[2],
    };
    return res;
}

Vector3 triangleIntersection(Vector3 unit_ray, Vector3 a, Vector3 b, Vector3 c,
                             bool *found) {
    Vector3 intersection = {};

    Vector3 ba = b - a;
    Vector3 ca = c - a;
    Vector3 tri_unit_normal = normalize(cross(ba, ca));

    float normal_to_plane = dot(a, tri_unit_normal);
    float normal_ray = dot(unit_ray, tri_unit_normal);

    if (normal_ray > 0.0) {
        Vector3 ray_on_plane = (normal_to_plane / normal_ray) * unit_ray;
        Vector3 ra = ray_on_plane - a;

        Vector3 unit_ba = normalize(ba);
        Vector3 ba_perp = normalize(ca - dot(ca, unit_ba) * unit_ba);

        float normal_to_c = dot(ca, ba_perp);
        float normal_ra = dot(ra, ba_perp);

        float c_factor = normal_ra / normal_to_c;
        if (c_factor >= 0) {
            Vector3 ray_to_c = c_factor * ca;
            Vector3 left_in_b = ra - ray_to_c;

            float parallel_to_b = dot(ba, unit_ba);
            float parallel_ra = dot(left_in_b, unit_ba);

            float b_factor = parallel_ra / parallel_to_b;
            if (b_factor >= 0) {
                if (b_factor + c_factor <= 1) {
                    intersection = ray_on_plane;
                    *found = true;
                }
            }
        }
    }

    return intersection;
}

Vector3 cubeIntersectionInDirection(Vector3 direction, bool *found) {
    Vector3 intersection = {};

    direction = normalize(direction);

    for (unsigned int i = 0; i < VERTEX_COUNT; i += 3) {
        Vector3 a = cube_vertices[i + 0].offset;
        Vector3 b = cube_vertices[i + 1].offset;
        Vector3 c = cube_vertices[i + 2].offset;

        bool triangle_found = false;
        Vector3 triangle_intersection =
            triangleIntersection(direction, a, b, c, &triangle_found);

        if (triangle_found) {
            intersection = triangle_intersection;
            *found = true;

            break;
        }
    }

    return intersection;
}

#define INSTANCE_COUNT 27
Slice<Cube> generateCubes() {
    Cube *cubes = (Cube *)malloc(INSTANCE_COUNT * sizeof(Cube));
    assert(cubes != nullptr);

    int side_count = 3;

    float side_len = 2.0f * RAD;
    float padding = 0.1f;

    float width = side_count * side_len + (side_count - 1) * padding;
    float base = -width / 2.0f;

    for (int x = 0; x < side_count; ++x) {
        for (int y = 0; y < side_count; ++y) {
            for (int z = 0; z < side_count; ++z) {
                Cube &c = cubes[9 * x + 3 * y + z];

                Vector3 position = {
                    base + (side_len + padding) * x + RAD,
                    base + (side_len + padding) * y + RAD,
                    base + (side_len + padding) * z + RAD,
                };

                c = {};
                c.position = position;
                c.rotation = {1, 0, 0, 0};
            }
        }
    }

    return {INSTANCE_COUNT, cubes};
}

DList<Joint> generateJoints(Slice<Cube> cubes) {
    DList<Joint> joints = {};
    joints.ensureSize(cubes.len);

    for (unsigned int i = 0; i < cubes.len; ++i) {
        Cube &cube = cubes[i];

        bool found = false;

        Vector3 position = cube.position;
        if (lengthSq(position) == 0) {
            // skip the center cube
            continue;
        }

        Vector3 intersection = cubeIntersectionInDirection(-position, &found);

#if 0
        if (!found) {
            // call again to debug
            cubeIntersectionInDirection(-position, &found);
        }
#else
        assert(found == true);
#endif

        Joint joint = {};
        joint.cube = &cube;
        joint.cube_r = intersection;
        joint.len = sqrtf(lengthSq(intersection));

        joints.push(joint);
    }

    return joints;
}

GLubyte *generateCubeTexture() {
    unsigned int size = INSTANCE_COUNT * 4 * 6 * sizeof(GLubyte);
    GLubyte *vals = (GLubyte *)malloc(size);
    assert(vals != nullptr);

    memset(vals, 0x18, size);

    unsigned int stride = 4 * 6 * sizeof(GLubyte);
    GLubyte source[4 * 6] = {
        0xFF, 0xFF, 0x00, 0xFF, // yellow
        0xFF, 0x00, 0x00, 0xFF, // red
        0x00, 0x00, 0xFF, 0xFF, // blue
        0xFF, 0xA5, 0x00, 0xFF, // orange
        0x00, 0xFF, 0x00, 0xFF, // green
        0xFF, 0xFF, 0xFF, 0xFF, // white
    };

    for (char x = 0; x < 3; ++x) {
        for (char y = 0; y < 3; ++y) {
            for (char z = 0; z < 3; ++z) {
                GLubyte *dest = vals + stride * (9 * x + 3 * y + z);

                if (z == 0) {
                    memcpy(dest + 4 * 0, source + 4 * 0, 4);
                }
                if (x == 2) {
                    memcpy(dest + 4 * 1, source + 4 * 1, 4);
                }
                if (y == 2) {
                    memcpy(dest + 4 * 2, source + 4 * 2, 4);
                }
                if (x == 0) {
                    memcpy(dest + 4 * 3, source + 4 * 3, 4);
                }
                if (y == 0) {
                    memcpy(dest + 4 * 4, source + 4 * 4, 4);
                }
                if (z == 2) {
                    memcpy(dest + 4 * 5, source + 4 * 5, 4);
                }

                // ensure alpha
                dest[3] = 0xFF;
            }
        }
    }

    return vals;
}

struct Collision {
    Cube *a;
    Cube *b;
    Vector3 ra;
    Vector3 rb;
    float depth;
    float sum_lambda;
};

void checkCollisions(DList<Collision> &collisions, Cube &a, Cube &b) {
    // TODO(bhester): check collisions
    Collision c = {&a, &b, {}, {}, 0.0, 0.0};
    collisions.push(c);
}

void preStepCollision(Collision &collision) {
    // TODO(bhester): pre step. should technically do nothing right now?
}

void stepCollision(Collision &collision, double inv_dt) {
    // TODO(bhester): step collision
}

void stepJointPosition(Joint &joint, double inv_dt) {}

void stepJointRotation(Joint &joint, double inv_dt) {
    Vector3 normal_tether_dir = normalize(joint.cube->position);
    Vector3 point_in_space_dir =
        normalize(mapCubePoint(*joint.cube, joint.cube_r));

    float cos_between = dot(normal_tether_dir, point_in_space_dir);
    float angle = acos(cos_between);
}

void stepJoint(Joint &joint, double inv_dt) {
    stepJointPosition(joint, inv_dt);
    stepJointRotation(joint, inv_dt);
}

Quaternion quatExp(Vector3 omega, double delta_time_seconds) {
    float theta = sqrtf(lengthSq((float)delta_time_seconds * omega));
    float half_theta = theta / 2.0;

    float c = cosf(half_theta);
    float s = sinf(half_theta);

    Quaternion r = {
        c,
        s * omega.x(),
        s * omega.y(),
        s * omega.z(),
    };
    return r;
}

#define NUM_ITERATIONS 10
void runPhysics(DList<Collision> &collisions, Slice<Cube> cubes,
                Slice<Joint> joints, double delta_time_seconds) {
    double inv_dt = delta_time_seconds > 0.0 ? 1.0 / delta_time_seconds : 0.0;

    collisions.ensureSize(cubes.len * 4); // TODO(bhester): tune
    collisions.clearRetainCapacity();

    for (unsigned int i = 0; i < cubes.len; ++i) {
        for (unsigned int j = i + 1; j < cubes.len; ++j) {
            Cube &a = cubes[i];
            Cube &b = cubes[j];

            checkCollisions(collisions, a, b);
        }
    }

    for (Collision &collision : collisions.items()) {
        preStepCollision(collision);
    }

    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        for (Collision &collision : collisions.items()) {
            stepCollision(collision, inv_dt);
        }

        for (Joint &joint : joints) {
            stepJoint(joint, inv_dt);
        }
    }

    for (Cube &cube : cubes) {
        cube.position = cube.position + delta_time_seconds * cube.velocity;
        cube.rotation = quatExp(cube.omega, delta_time_seconds) * cube.rotation;
    }
}

struct MainProgram {
    Program program;
    VertexArray vao;
    Buffer verts_buf;
    Buffer cube_state_buf;
    Texture color_tex;

    // vertex attributes
    GLint a_vert_position_attrib_pos;
    GLint a_vert_tex_coord_attrib_pos;
    GLint a_cube_offset_attrib_pos;
    GLint a_cube_rotation_attrib_pos;

    // uniforms
    GLint camera_translate_loc;
    GLint camera_rotation_loc;
    GLint perspective_loc;
    GLint color_tex_loc;
};

MainProgram loadMainProgram(GL &gl, Slice<Shader> shaders) {
    MainProgram mp = {};

    Program program = compileProgram(gl, shaders);
    VertexArray vao = genVertexArray(gl);
    Buffer verts_buf = genBuffer(gl);
    Buffer cube_state_buf = genBuffer(gl);
    Texture color_tex = genTexture(gl);

    if (program.handle == 0 || vao.handle == 0 || verts_buf.handle == 0 ||
        cube_state_buf.handle == 0 || color_tex.handle == 0) {
        goto cleanup;
    }

    mp.program = program;
    mp.vao = vao;
    mp.verts_buf = verts_buf;
    mp.cube_state_buf = cube_state_buf;
    mp.color_tex = color_tex;

    mp.a_vert_position_attrib_pos =
        gl.getAttribLocation(mp.program.handle, "a_vert_position");
    mp.a_vert_tex_coord_attrib_pos =
        gl.getAttribLocation(mp.program.handle, "a_vert_tex_coord");
    mp.a_cube_offset_attrib_pos =
        gl.getAttribLocation(mp.program.handle, "a_cube_offset");
    mp.a_cube_rotation_attrib_pos =
        gl.getAttribLocation(mp.program.handle, "a_cube_rotation");

    mp.camera_translate_loc =
        gl.getUniformLocation(mp.program.handle, "camera_translate");
    mp.camera_rotation_loc =
        gl.getUniformLocation(mp.program.handle, "camera_rotation");

    mp.perspective_loc =
        gl.getUniformLocation(mp.program.handle, "perspective");

    mp.color_tex_loc = gl.getUniformLocation(mp.program.handle, "color_tex");

end:
    return mp;

cleanup:
    deleteTexture(gl, color_tex);
    deleteBuffer(gl, cube_state_buf);
    deleteBuffer(gl, verts_buf);
    deleteVertexArray(gl, vao);
    deleteProgram(gl, program);

    goto end;
}

void useMainProgram(GL &gl, MainProgram mp) {
    gl.useProgram(mp.program.handle);
    gl.bindVertexArray(mp.vao.handle);
}

void mainProgramSetAttributesAndInitialData(GL &gl, MainProgram &mp,
                                            Slice<Cube> cubes,
                                            GLubyte *color_tex_data) {
    gl.activeTexture(GL_TEXTURE0);
    gl.uniform1i(mp.color_tex_loc, 0);

    gl.bindTexture(GL_TEXTURE_1D_ARRAY, mp.color_tex.handle);
    gl.texImage2D(GL_TEXTURE_1D_ARRAY, 0, GL_RGBA8, 6, INSTANCE_COUNT, 0,
                  GL_RGBA, GL_UNSIGNED_BYTE, color_tex_data);

    gl.texParameteri(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.texParameteri(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl.texParameteri(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_WRAP_S,
                     GL_CLAMP_TO_BORDER);

    // cube vertex data

    gl.bindBuffer(GL_ARRAY_BUFFER, mp.verts_buf.handle);
    gl.bufferData(GL_ARRAY_BUFFER, LEN(cube_vertices) * sizeof(Vertex),
                  (void *)cube_vertices, GL_STATIC_DRAW);

    gl.enableVertexAttribArray(mp.a_vert_position_attrib_pos);
    gl.enableVertexAttribArray(mp.a_vert_tex_coord_attrib_pos);

    assert(sizeof(((Vertex *)0)->offset) == 3 * sizeof(float));
    gl.vertexAttribPointer(mp.a_vert_position_attrib_pos, 3, GL_FLOAT, GL_FALSE,
                           sizeof(Vertex), (void *)offsetof(Vertex, offset));
    assert(sizeof(((Vertex *)0)->face_id) == 1 * sizeof(float));
    gl.vertexAttribPointer(mp.a_vert_tex_coord_attrib_pos, 1, GL_FLOAT,
                           GL_FALSE, sizeof(Vertex),
                           (void *)offsetof(Vertex, face_id));

    // cube instance data

    gl.bindBuffer(GL_ARRAY_BUFFER, mp.cube_state_buf.handle);
    gl.bufferData(GL_ARRAY_BUFFER, INSTANCE_COUNT * sizeof(Cube),
                  (void *)cubes.dat, GL_STATIC_DRAW);

    gl.enableVertexAttribArray(mp.a_cube_offset_attrib_pos);
    gl.enableVertexAttribArray(mp.a_cube_rotation_attrib_pos);

    assert(sizeof(((Cube *)0)->position) == 3 * sizeof(float));
    gl.vertexAttribPointer(mp.a_cube_offset_attrib_pos, 3, GL_FLOAT, GL_FALSE,
                           sizeof(Cube), (void *)offsetof(Cube, position));
    assert(sizeof(((Cube *)0)->rotation) == 4 * sizeof(float));
    gl.vertexAttribPointer(mp.a_cube_rotation_attrib_pos, 4, GL_FLOAT, GL_FALSE,
                           sizeof(Cube), (void *)offsetof(Cube, rotation));

    gl.vertexAttribDivisor(mp.a_cube_offset_attrib_pos, 1);
    gl.vertexAttribDivisor(mp.a_cube_rotation_attrib_pos, 1);
}

void destroyMainProgram(GL &gl, MainProgram &mp) {
    deleteTexture(gl, mp.color_tex);
    deleteBuffer(gl, mp.cube_state_buf);
    deleteBuffer(gl, mp.verts_buf);
    deleteVertexArray(gl, mp.vao);
    deleteProgram(gl, mp.program);

    mp = {};
}

struct DebugProgram {
    Program program;
    VertexArray vao;
    Buffer line_buf;

    // vertex attributes
    GLint a_vert_position_attrib_pos;
    GLint a_cube_offset_attrib_pos;
    GLint a_cube_rotation_attrib_pos;
    GLint a_color_attrib_pos;

    // uniforms
    GLint perspective_loc;
    GLint camera_translate_loc;
    GLint camera_rotation_loc;
};

DebugProgram loadDebugProgram(GL &gl, Slice<Shader> shaders) {
    DebugProgram dp = {};

    Program program = compileProgram(gl, shaders);
    VertexArray vao = genVertexArray(gl);
    Buffer line_buf = genBuffer(gl);

    if (program.handle == 0 || vao.handle == 0 || line_buf.handle == 0) {
        goto cleanup;
    }

    dp.program = program;
    dp.vao = vao;
    dp.line_buf = line_buf;

    dp.a_vert_position_attrib_pos =
        gl.getAttribLocation(dp.program.handle, "a_vert_position");
    dp.a_cube_offset_attrib_pos =
        gl.getAttribLocation(dp.program.handle, "a_cube_offset");
    dp.a_cube_rotation_attrib_pos =
        gl.getAttribLocation(dp.program.handle, "a_cube_rotation");
    dp.a_color_attrib_pos = gl.getAttribLocation(dp.program.handle, "a_color");

    dp.camera_translate_loc =
        gl.getUniformLocation(dp.program.handle, "camera_translate");
    dp.camera_rotation_loc =
        gl.getUniformLocation(dp.program.handle, "camera_rotation");

    dp.perspective_loc =
        gl.getUniformLocation(dp.program.handle, "perspective");

end:
    return dp;

cleanup:
    deleteBuffer(gl, line_buf);
    deleteVertexArray(gl, vao);
    deleteProgram(gl, program);

    goto end;
}

void useDebugProgram(GL &gl, DebugProgram dp) {
    gl.useProgram(dp.program.handle);
    gl.bindVertexArray(dp.vao.handle);
}

struct DebugVertex {
    Vector3 point;
    Vector3 offset;
    Quaternion rotation;
    Vector3 color;
};

DebugVertex debugVertex(Vector3 point, Cube const *cube = nullptr,
                        Vector3 color = {1.0, 1.0, 1.0}) {
    DebugVertex v = {};

    v.point = point;
    if (cube != nullptr) {
        v.offset = cube->position;
        v.rotation = cube->rotation;
    }
    v.color = color;

    return v;
}

void debugProgramSetAttributesAndInitialData(GL &gl, DebugProgram &dp) {
    // line vertex data

    gl.bindBuffer(GL_ARRAY_BUFFER, dp.line_buf.handle);

    gl.enableVertexAttribArray(dp.a_vert_position_attrib_pos);
    gl.enableVertexAttribArray(dp.a_cube_offset_attrib_pos);
    gl.enableVertexAttribArray(dp.a_cube_rotation_attrib_pos);
    gl.enableVertexAttribArray(dp.a_color_attrib_pos);

    assert(sizeof(((DebugVertex *)0)->point) == 3 * sizeof(float));
    gl.vertexAttribPointer(dp.a_vert_position_attrib_pos, 3, GL_FLOAT, GL_FALSE,
                           sizeof(DebugVertex),
                           (void *)offsetof(DebugVertex, point));
    assert(sizeof(((DebugVertex *)0)->offset) == 3 * sizeof(float));
    gl.vertexAttribPointer(dp.a_cube_offset_attrib_pos, 3, GL_FLOAT, GL_FALSE,
                           sizeof(DebugVertex),
                           (void *)offsetof(DebugVertex, offset));
    assert(sizeof(((DebugVertex *)0)->rotation) == 4 * sizeof(float));
    gl.vertexAttribPointer(dp.a_cube_rotation_attrib_pos, 4, GL_FLOAT, GL_FALSE,
                           sizeof(DebugVertex),
                           (void *)offsetof(DebugVertex, rotation));
    assert(sizeof(((DebugVertex *)0)->color) == 3 * sizeof(float));
    gl.vertexAttribPointer(dp.a_color_attrib_pos, 3, GL_FLOAT, GL_FALSE,
                           sizeof(DebugVertex),
                           (void *)offsetof(DebugVertex, color));
}

void destroyDebugProgram(GL &gl, DebugProgram &dp) {
    deleteBuffer(gl, dp.line_buf);
    deleteVertexArray(gl, dp.vao);
    deleteProgram(gl, dp.program);

    dp = {};
}

int main() {
    GL gl = {};
    GLFWwindow *window = {};
    Context context = {};

    DList<Collision> collisions = {};
    Slice<Cube> cubes = generateCubes();
    DList<Joint> joints = generateJoints(cubes);
    GLubyte *color_tex_data = generateCubeTexture();

    glfwSetErrorCallback(glfwError);

    if (!glfwInit()) {
        fprintf(stderr, "Failed to init glfw\n");
        return -1;
    }

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

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
    glfwSetMouseButtonCallback(window, glfwHandleMouseButton);
    glfwSetCursorPosCallback(window, glfwHandleCursorPos);

    gl.enable(GL_BLEND);
    gl.enable(GL_CULL_FACE);
    gl.enable(GL_DEPTH_TEST);

    gl.frontFace(GL_CCW);
    gl.cullFace(GL_BACK);

    gl.blendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gl.clearColor(0.0f, 0.0f, 0.0f, 0.0f);

    char const *cube_vert = "shaders/cube_vert.glsl";
    char const *line_vert = "shaders/line_vert.glsl";
    char const *cube_frag = "shaders/cube_frag.glsl";
    char const *line_frag = "shaders/line_frag.glsl";
    char const *common = "shaders/common.glsl";

    char const *cube_vert_sources[] = {cube_vert, common};
    char const *debug_vert_sources[] = {line_vert, common};

    Shader vert = loadShader(gl, GL_VERTEX_SHADER,
                             SLICE(char const *, cube_vert_sources));
    Shader debug_vert = loadShader(gl, GL_VERTEX_SHADER,
                                   SLICE(char const *, debug_vert_sources));
    Shader frag = loadShader(gl, GL_FRAGMENT_SHADER, cube_frag);
    Shader debug_frag = loadShader(gl, GL_FRAGMENT_SHADER, line_frag);

    Shader shaders[] = {vert, frag};
    Shader debug_shaders[] = {debug_vert, debug_frag};

    MainProgram mp = loadMainProgram(gl, SLICE(Shader, shaders));
    DebugProgram dp = loadDebugProgram(gl, SLICE(Shader, debug_shaders));

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

    useMainProgram(gl, mp);
    mainProgramSetAttributesAndInitialData(gl, mp, cubes, color_tex_data);
    gl.uniformMatrix4fv(mp.perspective_loc, 1, GL_TRUE, perspective.vals);

#if DEBUG
    useDebugProgram(gl, dp);
    debugProgramSetAttributesAndInitialData(gl, dp);
    gl.uniformMatrix4fv(dp.perspective_loc, 1, GL_TRUE, perspective.vals);
    useMainProgram(gl, mp);
#endif

    bool should_quit = false;
    bool right_click_down = false;

    bool w_down = false;
    bool a_down = false;
    bool s_down = false;
    bool d_down = false;

    bool up_down = false;
    bool left_down = false;
    bool down_down = false;
    bool right_down = false;

    double last_time_seconds = glfwGetTime();
    double cur_mouse_x = 0.0;
    double cur_mouse_y = 0.0;

    double right_click_drag_x = 0.0;
    double right_click_drag_y = 0.0;

    glfwGetCursorPos(window, &cur_mouse_x, &cur_mouse_y);

    while (!should_quit && !glfwWindowShouldClose(window)) {
        double cur_time_seconds = glfwGetTime();
        double delta_time_seconds = cur_time_seconds - last_time_seconds;

        right_click_drag_x = 0.0;
        right_click_drag_y = 0.0;

        // handle inputs

        while (context.last_event != context.next_event) {
            Event event = context.events[context.last_event];
            switch (event.kind) {
            case Event::Kind_None: {
            } break;
            case Event::Kind_Key: {
                KeyEvent ke = event.key_event;

                if (ke.action == GLFW_PRESS && ke.key == GLFW_KEY_Q) {
                    should_quit = true;
                }

                if (ke.action == GLFW_PRESS || ke.action == GLFW_RELEASE) {
                    switch (ke.key) {
#define CHECK(KEY, key)                                                        \
    case KEY: {                                                                \
        key = ke.action == GLFW_PRESS;                                         \
    } break
                        CHECK(GLFW_KEY_W, w_down);
                        CHECK(GLFW_KEY_A, a_down);
                        CHECK(GLFW_KEY_S, s_down);
                        CHECK(GLFW_KEY_D, d_down);

                        CHECK(GLFW_KEY_UP, up_down);
                        CHECK(GLFW_KEY_LEFT, left_down);
                        CHECK(GLFW_KEY_DOWN, down_down);
                        CHECK(GLFW_KEY_RIGHT, right_down);
                    }
#undef CHECK
                }
            } break;
            case Event::Kind_MouseButton: {
                MouseButtonEvent mbe = event.mouse_button_event;

                if (mbe.button == GLFW_MOUSE_BUTTON_RIGHT) {
                    right_click_down = mbe.action == GLFW_PRESS;
                }
            } break;
            case Event::Kind_CursorPos: {
                CursorPosEvent cpe = event.cursor_pos_event;

                double delta_x = cpe.x - cur_mouse_x;
                double delta_y = cpe.y - cur_mouse_y;

                if (right_click_down) {
                    right_click_drag_x += delta_x;
                    right_click_drag_y += delta_y;
                }

                cur_mouse_x = cpe.x;
                cur_mouse_y = cpe.y;
            } break;
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

        camera_x_vel -= float(right_click_drag_x);
        camera_y_vel -= float(right_click_drag_y);

        Matrix4 x_rot = yAxisRotation(camera_x_vel * delta_time_seconds);
        Matrix4 y_rot = xAxisRotation(camera_y_vel * delta_time_seconds);

        camera_rotation = y_rot * x_rot * camera_rotation;

        gl.uniform1f(mp.camera_translate_loc, camera_translate);
        gl.uniformMatrix4fv(mp.camera_rotation_loc, 1, GL_TRUE,
                            camera_rotation.vals);

#if DEBUG
        useDebugProgram(gl, dp);
        gl.uniform1f(dp.camera_translate_loc, camera_translate);
        gl.uniformMatrix4fv(dp.camera_rotation_loc, 1, GL_TRUE,
                            camera_rotation.vals);
        useMainProgram(gl, mp);
#endif

        float cube_x_vel = 0.0f;
        float cube_y_vel = 0.0f;

        if (left_down) {
            cube_x_vel += 1.0f;
        }
        if (right_down) {
            cube_x_vel -= 1.0f;
        }
        if (up_down) {
            cube_y_vel += 1.0f;
        }
        if (down_down) {
            cube_y_vel -= 1.0f;
        }

        cubes[0].velocity.z() += cube_x_vel * delta_time_seconds;
        cubes[0].velocity.y() += cube_y_vel * delta_time_seconds;

        // handle physics

        runPhysics(collisions, cubes, joints.items(), delta_time_seconds);

        gl.bindBuffer(GL_ARRAY_BUFFER, mp.cube_state_buf.handle);
        gl.bufferData(GL_ARRAY_BUFFER, INSTANCE_COUNT * sizeof(Cube),
                      (void *)cubes.dat, GL_STATIC_DRAW);

        // render

        gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

#if DEBUG
        gl.drawArraysInstanced(GL_TRIANGLES, 0, VERTEX_COUNT,
                               DEBUG_INSTANCE_COUNT);
#else
        gl.drawArraysInstanced(GL_TRIANGLES, 0, VERTEX_COUNT, INSTANCE_COUNT);
#endif

#if DEBUG
        useDebugProgram(gl, dp);

        DebugVertex line_verts[2 * DEBUG_INSTANCE_COUNT] = {};
        for (int i = 0; i < DEBUG_INSTANCE_COUNT; ++i) {
            Joint const &j = joints[i];

#if 0
            line_verts[2 * i + 0] = debugVertex({}, nullptr, {0, 1.0, 0});
            line_verts[2 * i + 1] =
                debugVertex(j.cube->position, nullptr, {0, 0, 1.0});
#else
            line_verts[2 * i + 0] = debugVertex({});
            line_verts[2 * i + 1] = debugVertex(j.cube_r, j.cube, {1.0, 0, 0});
#endif
        }

        gl.bindBuffer(GL_ARRAY_BUFFER, dp.line_buf.handle);
        gl.bufferData(GL_ARRAY_BUFFER, LEN(line_verts) * sizeof(DebugVertex),
                      (void *)line_verts, GL_STATIC_DRAW);

        gl.drawArrays(GL_LINES, 0, LEN(line_verts));

        useMainProgram(gl, mp);
#endif

        glfwSwapBuffers(window);
        glfwPollEvents();

        last_time_seconds = cur_time_seconds;
    }

    destroyDebugProgram(gl, dp);
    destroyMainProgram(gl, mp);

    deleteShader(gl, frag);
    deleteShader(gl, vert);

    free(color_tex_data);
    joints.erase();
    free(cubes.dat);
    collisions.erase();

    glfwTerminate();
    return 0;
}

#include "math.cc"
