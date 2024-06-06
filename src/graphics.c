#include "graphics.h"

#include <stdio.h>

#include "my_math.h"

#define _DEBUG

#define CLEANUP_IF(check, ...)                                                 \
    do {                                                                       \
        if (check) {                                                           \
            CLEANUP(__VA_ARGS__);                                              \
        }                                                                      \
    } while (0)

#define CLEANUP(...) _CLEANUP(__COUNTER__ + 1, __VA_ARGS__)
#define _CLEANUP(n, ...)                                                       \
    do {                                                                       \
        fprintf(stderr, __VA_ARGS__);                                          \
        ret = n;                                                               \
        goto cleanup;                                                          \
    } while (0)

static int print_a_thing = 0;

#define FRAMES 60.0
static double target_mspf = 1000.0 / FRAMES;

#define FACES 6
static int *indices;
static int const square_indices_per_face = 4;
static int const num_indices = FACES * VERTEX_COUNT_TO_TRIANGLE_COUNT(4);

static Camera get_initial_camera(void) {
    return (Camera){
        .rho = 2.0 * CAMERA_SCREEN_DIST,
        .theta = 0.0,
        .phi = PI_2,
    };
}

static State get_initial_state(Cube *cube) {
    return (State){
        .theta_rot_dir = 1.0,
        .phi_rot_dir = 1.0,

        .should_rotate = 0,
        .should_close = 0,
        .camera = get_initial_camera(),
        .cube = cube,
    };
}

#define INIT_WIDTH 680
#define INIT_HEIGHT 480

static int sdl_init(SDL_Window **window) {
    int ret = 0;

    CLEANUP_IF(SDL_Init(SDL_INIT_VIDEO) < 0, "Failed to init SDL\n");

    // use OpenGL 4.2 Core profile
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_CORE);

    // Turn on double buffering with 24-bit z buffer
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    *window = SDL_CreateWindow("Rubik's Cube", SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED, INIT_WIDTH, INIT_HEIGHT,
                               SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

    CLEANUP_IF(*window == NULL, "Failed to create window\n");

cleanup:
    if (ret != 0) {
        if (*window != NULL) {
            SDL_DestroyWindow(*window);
        }

        SDL_Quit();
    }

    return ret;
}

static int load_file(char const *filename, long *size, char **p_contents) {
    int ret = 0;
    FILE *f = NULL;
    char *contents = NULL;
    long f_len;
    unsigned long n_read;

    f = fopen(filename, "r");

    fseek(f, 0, SEEK_END);
    f_len = ftell(f);
    fseek(f, 0, SEEK_SET);

    contents = (char *)malloc((sizeof(char) * f_len) + 1);

    CLEANUP_IF(contents == NULL,
               "Failed to alloc buffer for file contents. Wanted %lu bytes.\n",
               f_len);

    *p_contents = contents;
    *size = f_len;

    // read a single block of file length because we need the whole file
    n_read = fread(contents, f_len, 1, f);

    CLEANUP_IF(n_read != 1, "Failed to read file contents\n");

    contents[f_len] = '\0';

cleanup:
    if (ret != 0) {
        if (contents != NULL) {
            free(contents);
        }

        if (f != NULL) {
            fclose(f);
        }
    }

    return ret;
}

static int gl_load_shader(GLuint *ui_shader, GLenum shader_type,
                          GLchar const *c_shader) {
    GLint int_test_return;

    *ui_shader = glCreateShader(shader_type);
    glShaderSource(*ui_shader, 1, &c_shader, NULL);
    glCompileShader(*ui_shader);

    glGetShaderiv(*ui_shader, GL_COMPILE_STATUS, &int_test_return);
    if (int_test_return == GL_FALSE) {
        GLchar p_c_info_log[1024];
        int32_t i_error_length;

        glGetShaderInfoLog(*ui_shader, 1024, &i_error_length, p_c_info_log);
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION,
                        "Failed to compile shader: %s\n", p_c_info_log);
        glDeleteShader(*ui_shader);
        return -1;
    }

    return 0;
}

static int gl_link_program(GLuint *program, GLuint vertex_shader,
                           GLuint geometry_shader, GLuint fragment_shader) {
    GLint int_test_return;

    *program = glCreateProgram();
    glAttachShader(*program, vertex_shader);
    glAttachShader(*program, geometry_shader);
    glAttachShader(*program, fragment_shader);
    glLinkProgram(*program);

    glGetProgramiv(*program, GL_LINK_STATUS, &int_test_return);
    if (int_test_return == GL_FALSE) {
        GLchar p_c_info_log[1024];
        int32_t i_error_length;

        glGetShaderInfoLog(*program, 1024, &i_error_length, p_c_info_log);
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION,
                        "Failed to link shaders: %s\n", p_c_info_log);
        glDeleteProgram(*program);
        return -1;
    }

    return 0;
}

#ifdef _DEBUG
static char *p_severity[] = {"High", "Medium", "Low", "Notification"};
static char *p_type[] = {"Error",       "Deprecated",  "Undefined",
                         "Portability", "Performance", "Other"};
static char *p_source[] = {"OpenGL",    "OS",          "GLSL Compiler",
                           "3rd Party", "Application", "Other"};

static void debug_callback(uint32_t source, uint32_t type, uint32_t id,
                           uint32_t severity, int32_t length,
                           char const *p_message, void *p_user_param) {
    uint32_t sev_id = 3;
    uint32_t type_id = 5;
    uint32_t source_id = 5;

    // Get the severity
    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:
        sev_id = 0;
        break;
    case GL_DEBUG_SEVERITY_MEDIUM:
        sev_id = 1;
        break;
    case GL_DEBUG_SEVERITY_LOW:
        sev_id = 2;
        break;
    case GL_DEBUG_SEVERITY_NOTIFICATION:
    default:
        sev_id = 3;
        break;
    }

    // Get the type
    switch (type) {
    case GL_DEBUG_TYPE_ERROR:
        type_id = 0;
        break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        type_id = 1;
        break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        type_id = 2;
        break;
    case GL_DEBUG_TYPE_PORTABILITY:
        type_id = 3;
        break;
    case GL_DEBUG_TYPE_PERFORMANCE:
        type_id = 4;
        break;
    case GL_DEBUG_TYPE_OTHER:
    default:
        type_id = 5;
        break;
    }

    // Get the source
    switch (source) {
    case GL_DEBUG_SOURCE_API:
        source_id = 0;
        break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
        source_id = 1;
        break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER:
        source_id = 2;
        break;
    case GL_DEBUG_SOURCE_THIRD_PARTY:
        source_id = 3;
        break;
    case GL_DEBUG_SOURCE_APPLICATION:
        source_id = 4;
        break;
    case GL_DEBUG_SOURCE_OTHER:
    default:
        source_id = 5;
        break;
    }

    // Output to the Log
    SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION,
                    "OpenGL Debug: Severity=%s, Type=%s, Source=%s - %s",
                    p_severity[sev_id], p_type[type_id], p_source[source_id],
                    p_message);

    if (severity == GL_DEBUG_SEVERITY_HIGH) {
        // This a serious error so we need to shutdown the program
        SDL_Event event;
        event.type = SDL_QUIT;
        SDL_PushEvent(&event);
    }
}

static void gl_debug_init(void) {
    uint32_t unused_ids = 0;

    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

    glDebugMessageCallback((GLDEBUGPROC)&debug_callback, NULL);

    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0,
                          &unused_ids, GL_TRUE);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                          GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, GL_FALSE);
}
#endif

#define VERTEX_SHADER_NAME "./shaders/cube.vert"
#define GEOMETRY_SHADER_NAME "./shaders/cube.geom"
#define FRAGMENT_SHADER_NAME "./shaders/cube.frag"

static int gl_init(SDL_Window *window, Application_GL_Info *gl_info) {
    int ret = 0;
    SDL_GLContext context = NULL;
    GLenum glew_error;
    GLuint vert_shader, geom_shader, frag_shader, gl_program;

    char *vert_contents = NULL;
    char *geom_contents = NULL;
    char *frag_contents = NULL;
    long vert_size, geom_size, frag_size;

    GLuint vao, vbo[2], ebo, texture;

    context = SDL_GL_CreateContext(window);
    CLEANUP_IF(context == NULL, "Could not create OpenGL context\n");

    CLEANUP_IF((glew_error = glewInit()) != GLEW_OK,
               "Failed to initialize GLEW: %s\n",
               glewGetErrorString(glew_error));

    // TODO: figure out if this should be +1 or -1
    SDL_GL_SetSwapInterval(-1);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glFrontFace(GL_CW);
    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);    // TODO: what is this?
    glDisable(GL_STENCIL_TEST); // TODO: what is this?

#ifdef _DEBUG
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
    gl_debug_init();
#endif

    CLEANUP_IF(load_file(VERTEX_SHADER_NAME, &vert_size, &vert_contents) != 0,
               "Failed to load vertex shader contents\n");

    CLEANUP_IF(load_file(GEOMETRY_SHADER_NAME, &geom_size, &geom_contents) != 0,
               "Failed to load geometry shader contents\n");

    CLEANUP_IF(load_file(FRAGMENT_SHADER_NAME, &frag_size, &frag_contents) != 0,
               "Failed to load fragment shader contents\n");

    CLEANUP_IF(gl_load_shader(&vert_shader, GL_VERTEX_SHADER,
                              (GLchar const *)vert_contents) < 0,
               "Failed to load vertex shader\n");

    CLEANUP_IF(gl_load_shader(&geom_shader, GL_GEOMETRY_SHADER,
                              (GLchar const *)geom_contents) < 0,
               "Failed to load geometry shader\n");

    CLEANUP_IF(gl_load_shader(&frag_shader, GL_FRAGMENT_SHADER,
                              (GLchar const *)frag_contents) < 0,
               "Failed to load fragment shader\n");

    CLEANUP_IF(
        gl_link_program(&gl_program, vert_shader, geom_shader, frag_shader) < 0,
        "Failed to link program\n");

    // after linking, it's okay to delete these
    free(vert_contents);
    free(geom_contents);
    free(frag_contents);
    glDeleteShader(vert_shader);
    glDeleteShader(geom_shader);
    glDeleteShader(frag_shader);

    glUseProgram(gl_program);

    glGenVertexArrays(1, &vao);
    glGenBuffers(2, &vbo[0]);
    glGenBuffers(1, &ebo);
    glGenTextures(1, &texture);

    glBindVertexArray(vao);

    *gl_info = (Application_GL_Info){
        .gl_context = context,
        .gl_program = gl_program,

        .vao = 0,
        .vbo = {0},
        .ebo = 0,
        .texture = 0,
    };
    gl_info->vao = vao;
    gl_info->vbo[0] = vbo[0];
    gl_info->vbo[1] = vbo[1];
    gl_info->ebo = ebo;
    gl_info->texture = texture;

cleanup:
    if (ret != 0) {
        if (texture != 0) {
            glDeleteTextures(1, &texture);
        }

        if (ebo != 0) {
            glDeleteBuffers(1, &ebo);
        }

        if (vbo[0] != 0) {
            glDeleteBuffers(2, &vbo[0]);
        }

        if (vao != 0) {
            glDeleteVertexArrays(1, &vao);
        }

        if (gl_program != 0) {
            glDeleteProgram(gl_program);
        }

        if (frag_contents != NULL) {
            free(frag_contents);
        }

        if (vert_contents != NULL) {
            free(vert_contents);
        }

        if (context != NULL) {
            SDL_GL_DeleteContext(context);
        }
    }

    return ret;
}

static void app_cleanup(Application *app) {
    if (app->gl_info.texture != 0) {
        glDeleteTextures(1, &app->gl_info.texture);
    }

    if (app->gl_info.ebo != 0) {
        glDeleteBuffers(1, &app->gl_info.ebo);
    }

    if (app->gl_info.vbo[0] != 0) {
        glDeleteBuffers(2, &app->gl_info.vbo[0]);
    }

    if (app->gl_info.vao != 0) {
        glDeleteVertexArrays(1, &app->gl_info.vao);
    }

    if (app->gl_info.gl_program != 0) {
        glDeleteProgram(app->gl_info.gl_program);
    }

    if (app->gl_info.gl_context != NULL) {
        SDL_GL_DeleteContext(app->gl_info.gl_context);
    }

    if (app->window != NULL) {
        SDL_DestroyWindow(app->window);
    }

    if (app->arena != NULL) {
        free_arena(app->arena);
    }

    SDL_Quit();
}

static StateUpdate get_inputs(Application const *app) {
    SDL_Event event;
    Uint32 last_ticks, ticks;
    StateUpdate s_update = {0};
    double cur_delta_ms;

    print_a_thing = 0;
    while (SDL_PollEvent(&event) > 0) {
        switch (event.type) {
        case SDL_QUIT: {
            s_update.toggle_close = 1;
        } break;
        case SDL_KEYDOWN: {
            Uint8 const *keys = SDL_GetKeyboardState(NULL);

            // quit the application on Q
            if (keys[SDL_SCANCODE_Q] == 1) {
                s_update.toggle_close = 1;
            }

            // print the current status
            if (keys[SDL_SCANCODE_P] == 1) {
                print_a_thing = 1;
            }

            // start / stop automatic rotation
            if (keys[SDL_SCANCODE_U] == 1) {
                s_update.toggle_rotate = 1;
            }

            // zoom in/out on the cube
            if (keys[SDL_SCANCODE_I] == 1) {
                s_update.camera_rho_dir = -1;
            } else if (keys[SDL_SCANCODE_O] == 1) {
                s_update.camera_rho_dir = 1;
            }

            // move the camera up and down
            if (keys[SDL_SCANCODE_DOWN] == 1) {
                s_update.camera_phi_dir = -1;
            } else if (keys[SDL_SCANCODE_UP] == 1) {
                s_update.camera_phi_dir = 1;
            }

            // move the camera left and right
            if (keys[SDL_SCANCODE_LEFT] == 1) {
                s_update.camera_theta_dir = -1;
            } else if (keys[SDL_SCANCODE_RIGHT] == 1) {
                s_update.camera_theta_dir = 1;
            }
        } break;
        }
    }

    last_ticks = app->last_ticks;
    ticks = SDL_GetTicks();

    cur_delta_ms = (double)(ticks - last_ticks);

    if (cur_delta_ms < target_mspf) {
        double diff = target_mspf - cur_delta_ms;

        SDL_Delay((Uint32)diff);
        cur_delta_ms += diff;
    }

    s_update.delta_time = cur_delta_ms / 1000.0;
    s_update.ticks = ticks;

    return s_update;
}

#define RHO_PIXELS_PER_SEC 10.0
#define THETA_PIXELS_PER_SEC (2.0 * PI / 10.0)
#define PHI_PIXELS_PER_SEC (-2.0 * PI / 10.0)

#define DANGEROUS_MAX(a, b) ((a) > (b) ? (a) : (b))
#define DANGEROUS_MIN(a, b) ((a) < (b) ? (a) : (b))

#define DANGEROUS_CLAMP(min, n, max) (DANGEROUS_MIN(max, DANGEROUS_MAX(min, n)))

static void update(Application *app, StateUpdate s_update) {
    State *state = &app->state;
    double delta_time = s_update.delta_time;
    double new_rho, new_theta, new_phi;

    new_rho = state->camera.rho +
              s_update.camera_rho_dir * delta_time * RHO_PIXELS_PER_SEC;
    new_theta = state->camera.theta +
                s_update.camera_theta_dir * delta_time * THETA_PIXELS_PER_SEC;
    new_phi = state->camera.phi +
              s_update.camera_phi_dir * delta_time * PHI_PIXELS_PER_SEC;

    if (state->should_rotate) {
        if ((state->camera.theta <= 0.0 + 0.001 && state->theta_rot_dir < 0) ||
            (state->camera.theta >= 2 * PI - 0.001 &&
             state->theta_rot_dir > 0)) {
            state->theta_rot_dir *= -1.0;
        }

        if ((state->camera.phi <= 0.0 + 0.001 && state->phi_rot_dir < 0) ||
            (state->camera.phi >= PI - 0.001 && state->phi_rot_dir > 0)) {
            state->phi_rot_dir *= -1.0;
        }

        new_theta += state->theta_rot_dir * THETA_PIXELS_PER_SEC * delta_time;
        new_phi -= state->phi_rot_dir * PHI_PIXELS_PER_SEC * delta_time;
    }

    // For now, these numbers are made up... they should be adjusted later
    state->camera.rho =
        DANGEROUS_CLAMP(1.1 * CAMERA_SCREEN_DIST, new_rho, 1000.0);
    state->camera.theta = new_theta; // TODO: mod by 2pi
    state->camera.phi = DANGEROUS_CLAMP(0.0, new_phi, PI);

    if (s_update.toggle_close) {
        state->should_close = 1 - state->should_close;
    }

    if (s_update.toggle_rotate) {
        state->should_rotate = 1 - state->should_rotate;
    }

    app->last_ticks = s_update.ticks;
}

typedef struct {
    unsigned char r, g, b;
} Color;

static void write_color_for_face(void *v_buf, FaceColor fc) {
    Color *buf = (Color *)v_buf;

    Color fc_color[FC_Count] = {
        [FC_White] = {.r = 0xFF, .g = 0xFF, .b = 0xFF},
        [FC_Green] = {.r = 0x00, .g = 0xFF, .b = 0x00},
        [FC_Red] = {.r = 0xFF, .g = 0x00, .b = 0x00},
        [FC_Blue] = {.r = 0x00, .g = 0x00, .b = 0xFF},
        [FC_Orange] = {.r = 0x80, .g = 0x80, .b = 0x80},
        [FC_Yellow] = {.r = 0xFF, .g = 0xFF, .b = 0x00},
    };

    *buf = fc_color[fc];
}

static void create_texture_from_cube(Application const *app, Color **p_texture,
                                     uint32_t *p_width, uint32_t *p_height) {
    uint32_t side_count = get_side_count(app->state.cube);

    uint32_t item_size = sizeof(Color);
    uint32_t stride = 4 * side_count;
    uint32_t height = 3 * side_count;
    uint32_t rect_size = stride * height;

    Spacing spacing = {
        .item_size = item_size,
        .hgap = 0,
        .vgap = 0,
        .trailing_v = 0,
    };

    Color clear_color = {.r = 0xFF, .g = 0x00, .b = 0xFF};
    Color *texture = ARENA_PUSH_N(Color, app->arena, rect_size * item_size);

    if (texture == NULL) {
        return;
    }

    for (uint32_t i = 0; i < rect_size; ++i) {
        texture[i] = clear_color;
    }

    generic_write_cube(app->state.cube, (void *)texture, spacing,
                       write_color_for_face);

    *p_texture = texture;
    *p_width = stride;
    *p_height = height;
}

static void render(Application const *app) {
    // TODO: Get the screen width and height and use a "pixels to meters" type
    // thing like from Handmade Hero?

    Camera const *camera = &app->state.camera;
    V3 camera_pos = {
        .rho = camera->rho, .theta = camera->theta, .phi = camera->phi};

    V3 minus_center = polar_to_rectangular(camera_pos);
    V3 cube_center = scale(minus_center, -1.0);
    float screen_cube_ratio = CAMERA_SCREEN_DIST / camera_pos.rho;

    V3 y_dir_polar = {
        .rho = 1,
        .theta =
            camera_pos.phi >= PI_2 ? camera_pos.theta : camera_pos.theta + PI,
        .phi = camera_pos.phi >= PI_2 ? camera_pos.phi - PI_2
                                      : PI_2 - camera_pos.phi,
    };
    V3 y_dir = polar_to_rectangular(y_dir_polar);
    V3 x_dir = cross(as_unit(cube_center), y_dir);

    Color *texture = NULL;
    uint32_t tex_width, tex_height;

    if (arena_begin(app->arena) == 0) {
        GLint uniform_index;

        create_texture_from_cube(app, &texture, &tex_width, &tex_height);
        if (texture == NULL) {
            goto skip_render;
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glActiveTexture(GL_TEXTURE0);
        uniform_index =
            glGetUniformLocation(app->gl_info.gl_program, "cube_texture");
        glUniform1i(uniform_index, 0);

        // TODO: maybe write some wrappers for this?

        uniform_index = glGetUniformLocation(app->gl_info.gl_program,
                                             "view_information.x_dir");
        glUniform3fv(uniform_index, 1, (GLfloat const *)x_dir.xyz);

        uniform_index = glGetUniformLocation(app->gl_info.gl_program,
                                             "view_information.y_dir");
        glUniform3fv(uniform_index, 1, (GLfloat const *)y_dir.xyz);

        uniform_index = glGetUniformLocation(app->gl_info.gl_program,
                                             "view_information.cube_center");
        glUniform3fv(uniform_index, 1, (GLfloat const *)cube_center.xyz);

        uniform_index = glGetUniformLocation(
            app->gl_info.gl_program, "view_information.screen_cube_ratio");
        glUniform1f(uniform_index, screen_cube_ratio);

        glBindVertexArray(app->gl_info.vao);

        glBindTexture(GL_TEXTURE_2D, app->gl_info.texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, tex_width, tex_height, 0, GL_RGB,
                     GL_UNSIGNED_BYTE, texture);
        glGenerateMipmap(GL_TEXTURE_2D);

        // fill vertex buffer
        glBindBuffer(GL_ARRAY_BUFFER, app->gl_info.vbo[0]);
        glBufferData(GL_ARRAY_BUFFER, cube_vert_count * sizeof(*cube_vertices),
                     (void const *)cube_vertices, GL_STATIC_DRAW);

        // fill index buffer
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app->gl_info.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, num_indices * sizeof(*indices),
                     (void const *)indices, GL_STATIC_DRAW);

        // specify location of data within buffer
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(*cube_vertices),
                              (GLvoid const *)(0 * sizeof(float)));
        glEnableVertexAttribArray(0);

        glDrawElements(GL_TRIANGLES, num_indices, GL_UNSIGNED_INT,
                       (void const *)0);

        SDL_GL_SwapWindow(app->window);

    skip_render:
        arena_pop(app->arena);
    }
}

int graphics_main(void) {
    int ret = 0;
    int sdl_init_ret, gl_init_ret;

    Arena *arena = NULL;
    Cube *cube;
    SDL_Window *window = NULL;
    Application_GL_Info gl_info;
    Application app;

    arena = alloc_arena();
    CLEANUP_IF(arena == NULL, "Unable to allocate arena\n");

    CLEANUP_IF((sdl_init_ret = sdl_init(&window)) != 0,
               "Unable to init application with code %d\n", sdl_init_ret);

    CLEANUP_IF((gl_init_ret = gl_init(window, &gl_info)) != 0,
               "Unable to init GLEW and shaders with code %d\n", gl_init_ret);

    CLEANUP_IF((cube = new_cube(3)) == NULL, "Could not allocate cube\n");

    app = (Application){
        .window = window,
        .gl_info = gl_info,
        .last_ticks = SDL_GetTicks(),

        .arena = arena,
        .state = get_initial_state(cube),
    };

    if (arena_begin(arena) == 0) {
        indices = ARENA_PUSH_N(int, arena, num_indices);

        expand_vertices_to_triangles(face_indices, face_index_count,
                                     square_indices_per_face, indices);

        while (!app.state.should_close) {
            StateUpdate s_update;

            s_update = get_inputs(&app);
            update(&app, s_update);
            render(&app);
        }

        arena_pop(arena);
    }

cleanup:
    app_cleanup(&app);

    return ret;
}
