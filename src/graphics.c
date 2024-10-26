#include "graphics.h"

#include <stdio.h>

#include "common.h"

#define _DEBUG

#ifdef _DEBUG
#include "debug_graphics.c"
#endif

#define INIT_WIDTH 680
#define INIT_HEIGHT 480

#define VERTEX_SHADER_NAME "./shaders/cube.vert"
#define FRAGMENT_SHADER_NAME "./shaders/cube.frag"

#define RHO_PIXELS_PER_SEC 20.0
#define THETA_PIXELS_PER_SEC (4.0 * PI / 5.0)
#define PHI_PIXELS_PER_SEC (-4.0 * PI / 5.0)

#define DANGEROUS_MAX(a, b) ((a) > (b) ? (a) : (b))
#define DANGEROUS_MIN(a, b) ((a) < (b) ? (a) : (b))

#define DANGEROUS_CLAMP(min, n, max) (DANGEROUS_MIN(max, DANGEROUS_MAX(min, n)))

#define FRAMES 60.0
static double target_mspf = 1000.0 / FRAMES;

static int print_a_thing = 0;

static Camera get_initial_camera(void) {
    return (Camera){
        .rho = 2.0 * CAMERA_SCREEN_DIST,
        .theta = 0.0,
        .phi = PI_2,
    };
}

static State get_initial_state(void) {
    return (State){
        .theta_rot_dir = 1.0,
        .phi_rot_dir = 1.0,

        .rotate_depth = 0,

        .should_rotate = 0,
        .should_close = 0,
        .mouse_held = 0,
        .mouse_clicked_cube = 0,
        .cube_intersection_found = 0,

        .screen_mouse_x = 0,
        .screen_mouse_y = 0,

        .window_width = INIT_WIDTH,
        .window_height = INIT_HEIGHT,

        .hover_info = {0},
        .click_info = {{0}},

        .mouse = {.x = 0, .y = 0},
        .camera = get_initial_camera(),

        .basis_info = {{{{0}}}},
    };
}

static int sdl_init(SDL_Window **window) {
    int ret = 0;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "Failed to init SDL\n");
        goto init_fail;
    }

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
                               SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN |
                                   SDL_WINDOW_RESIZABLE);

    if (*window == NULL) {
        fprintf(stderr, "Failed to create window\n");
        goto create_fail;
    }

    return ret;

create_fail:
    ret += 1;
    SDL_DestroyWindow(*window);
    SDL_Quit();

init_fail:
    ret += 1;
    return ret;
}

static int load_file(char const *filename, long *size, char **p_contents) {
    int ret = 0;
    FILE *f = NULL;
    char *contents = NULL;
    long f_len;
    unsigned long n_read;

    f = fopen(filename, "r");
    if (f == NULL) {
        fprintf(stderr, "Failed to open file for reading\n");
        goto open_fail;
    }

    // TODO: maybe these can fail too? they return int...
    fseek(f, 0, SEEK_END);
    f_len = ftell(f);
    fseek(f, 0, SEEK_SET);

    contents = (char *)malloc((sizeof(char) * f_len) + 1);

    if (contents == NULL) {
        fprintf(stderr,
                "Failed to alloc buffer for file contents. Wanted %lu bytes.\n",
                f_len);
        goto contents_malloc_fail;
    }

    *p_contents = contents;
    *size = f_len;

    // read a single block of file length because we need the whole file
    n_read = fread(contents, f_len, 1, f);

    if (n_read != 1) {
        fprintf(stderr, "Failed to read file contents\n");
        goto read_fail;
    }

    contents[f_len] = '\0';

    return ret;

read_fail:
    ret += 1;
    free(contents);

contents_malloc_fail:
    ret += 1;
    fclose(f);

open_fail:
    ret += 1;
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
                           GLuint fragment_shader) {
    GLint int_test_return;

    *program = glCreateProgram();
    glAttachShader(*program, vertex_shader);
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

static int load_cube_shader_programs(GLuint *gl_program) {
    int ret = 0;
    GLuint vert_shader, frag_shader;

    char *vert_contents = NULL;
    char *frag_contents = NULL;
    long vert_size, frag_size;

    if (load_file(VERTEX_SHADER_NAME, &vert_size, &vert_contents) != 0) {
        fprintf(stderr, "Failed to load vertex shader contents\n");
        goto vert_load_fail;
    }

    if (load_file(FRAGMENT_SHADER_NAME, &frag_size, &frag_contents) != 0) {
        fprintf(stderr, "Failed to load fragment shader contents\n");
        goto frag_load_fail;
    }

    if (gl_load_shader(&vert_shader, GL_VERTEX_SHADER,
                       (GLchar const *)vert_contents) < 0) {
        fprintf(stderr, "Failed to load vertex shader\n");
        goto vert_load_fail_2;
    }

    if (gl_load_shader(&frag_shader, GL_FRAGMENT_SHADER,
                       (GLchar const *)frag_contents) < 0) {
        fprintf(stderr, "Failed to load fragment shader\n");
        goto frag_load_fail_2;
    }

    if (gl_link_program(gl_program, vert_shader, frag_shader) < 0) {
        fprintf(stderr, "Failed to link program\n");
        goto link_fail;
    }

    // after linking, it's okay to delete these
    glDeleteShader(vert_shader);
    glDeleteShader(frag_shader);

    return ret;

link_fail:
    ret += 1;

frag_load_fail_2:
    ret += 1;

vert_load_fail_2:
    ret += 1;
    free(frag_contents);

frag_load_fail:
    ret += 1;
    free(vert_contents);

vert_load_fail:
    ret += 1;
    return ret;
}

static int gl_init(SDL_Window *window, SDL_GLContext **p_gl_context,
                   GLuint *p_gl_program, GraphicsCube *cube) {
    int ret = 0;
    SDL_GLContext context = NULL;
    GLenum glew_error;
    GLuint gl_program;

    context = SDL_GL_CreateContext(window);
    if (context == NULL) {
        fprintf(stderr, "Could not create OpenGL context\n");
        goto context_fail;
    }

    if ((glew_error = glewInit()) != GLEW_OK) {
        fprintf(stderr, "Failed to initialize GLEW: %s\n",
                glewGetErrorString(glew_error));
        goto glew_init_fail;
    }

    // TODO: figure out if this should be +1 or -1
    SDL_GL_SetSwapInterval(-1);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

#ifdef _DEBUG
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
    gl_debug_init();
#endif

    if (load_cube_shader_programs(&gl_program)) {
        fprintf(stderr, "Failed to link cube shader program\n");
        goto shader_load_fail;
    }

    glUseProgram(gl_program);

    glGenVertexArrays(1, &cube->vao);
    glGenBuffers(2, cube->vbo);
    glGenBuffers(1, &cube->ebo);
    glGenTextures(1, &cube->texture);

    *p_gl_context = context;
    *p_gl_program = gl_program;

    return ret;

    // I suppose there's nothing that can fail between these getting created and
    // returning from the function...

    if (cube->texture != 0) {
        glDeleteTextures(1, &cube->texture);
    }

    if (cube->ebo != 0) {
        glDeleteBuffers(1, &cube->ebo);
    }

    if (cube->vbo[0] != 0) {
        glDeleteBuffers(2, cube->vbo);
    }

    if (cube->vao != 0) {
        glDeleteVertexArrays(1, &cube->vao);
    }

    if (gl_program != 0) {
        glDeleteProgram(gl_program);
    }

shader_load_fail:
    ret += 1;

glew_init_fail:
    ret += 1;
    SDL_GL_DeleteContext(context);

context_fail:
    ret += 1;
    return ret;
}

static void app_cleanup(Application *app) {
    if (app->cube.texture != 0) {
        glDeleteTextures(1, &app->cube.texture);
    }

    if (app->cube.ebo != 0) {
        glDeleteBuffers(1, &app->cube.ebo);
    }

    if (app->cube.vbo[0] != 0) {
        glDeleteBuffers(2, app->cube.vbo);
    }

    if (app->cube.vao != 0) {
        glDeleteVertexArrays(1, &app->cube.vao);
    }

    if (app->gl_program != 0) {
        glDeleteProgram(app->gl_program);
    }

    if (app->gl_context != NULL) {
        SDL_GL_DeleteContext(app->gl_context);
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
        case SDL_WINDOWEVENT: {
            SDL_WindowEvent w_event = event.window;
            switch (w_event.event) {
            case SDL_WINDOWEVENT_RESIZED: {
                s_update.window_width = w_event.data1;
                s_update.window_height = w_event.data2;
                s_update.window_resized = 1;
            } break;
            }
        } break;
        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEBUTTONDOWN: {
            s_update.toggle_mouse_click = 1;
        } break;
        case SDL_MOUSEMOTION: {
            s_update.mouse_x = event.motion.x;
            s_update.mouse_y = event.motion.y;
            s_update.mouse_moved = 1;
        } break;
        case SDL_KEYDOWN: {
            Uint8 const *keys = SDL_GetKeyboardState(NULL);

            // quit the application on Q
            if (keys[SDL_SCANCODE_Q] == 1) {
                s_update.toggle_close = 1;
            }

            // quit the application on Q
            if (keys[SDL_SCANCODE_R] == 1) {
                s_update.rotate_front = 1;
            }

            // print the current status
            if (keys[SDL_SCANCODE_P] == 1) {
                print_a_thing = 1;
            }

            // update the facing direction or the rotation depth depending on
            // the S key being pressed
            if (keys[SDL_SCANCODE_S] == 1) {
                if (keys[SDL_SCANCODE_0] == 1) {
                    s_update.set_face = 1;
                    s_update.target_face = 0;
                } else if (keys[SDL_SCANCODE_1] == 1) {
                    s_update.set_face = 1;
                    s_update.target_face = 1;
                } else if (keys[SDL_SCANCODE_2] == 1) {
                    s_update.set_face = 1;
                    s_update.target_face = 2;
                } else if (keys[SDL_SCANCODE_3] == 1) {
                    s_update.set_face = 1;
                    s_update.target_face = 3;
                } else if (keys[SDL_SCANCODE_4] == 1) {
                    s_update.set_face = 1;
                    s_update.target_face = 4;
                } else if (keys[SDL_SCANCODE_5] == 1) {
                    s_update.set_face = 1;
                    s_update.target_face = 5;
                }
            } else {
                if (keys[SDL_SCANCODE_1] == 1) {
                    s_update.set_depth = 1;
                    s_update.rotate_depth = 1;
                } else if (keys[SDL_SCANCODE_2] == 1) {
                    s_update.set_depth = 1;
                    s_update.rotate_depth = 2;
                } else if (keys[SDL_SCANCODE_3] == 1) {
                    s_update.set_depth = 1;
                    s_update.rotate_depth = 3;
                } else if (keys[SDL_SCANCODE_4] == 1) {
                    s_update.set_depth = 1;
                    s_update.rotate_depth = 4;
                } else if (keys[SDL_SCANCODE_5] == 1) {
                    s_update.set_depth = 1;
                    s_update.rotate_depth = 5;
                } else if (keys[SDL_SCANCODE_6] == 1) {
                    s_update.set_depth = 1;
                    s_update.rotate_depth = 6;
                } else if (keys[SDL_SCANCODE_7] == 1) {
                    s_update.set_depth = 1;
                    s_update.rotate_depth = 7;
                } else if (keys[SDL_SCANCODE_8] == 1) {
                    s_update.set_depth = 1;
                    s_update.rotate_depth = 8;
                } else if (keys[SDL_SCANCODE_9] == 1) {
                    s_update.set_depth = 1;
                    s_update.rotate_depth = 9;
                } else if (keys[SDL_SCANCODE_0] == 1) {
                    s_update.set_depth = 1;
                    s_update.rotate_depth = 0;
                }
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
                s_update.camera_phi_dir = 1;
            } else if (keys[SDL_SCANCODE_UP] == 1) {
                s_update.camera_phi_dir = -1;
            }

            // move the camera left and right
            if (keys[SDL_SCANCODE_LEFT] == 1) {
                s_update.camera_theta_dir = 1;
            } else if (keys[SDL_SCANCODE_RIGHT] == 1) {
                s_update.camera_theta_dir = -1;
            }

            if (keys[SDL_SCANCODE_C] == 1) {
                s_update.checkerboard = 1;
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

// assumes the point is in the plane of the triangle
static int inside_triangle(V3 point, V3 ab, V3 ac) {
    V3 bc_perp, cb_perp;
    V3 ab_perp, ac_perp;

    float x;
    float y;

    decompose(ab, as_unit(ac), &bc_perp);
    decompose(ac, as_unit(ab), &cb_perp);

    ab_perp = scale3(bc_perp, 1.0f / dot(bc_perp, ab));
    ac_perp = scale3(cb_perp, 1.0f / dot(cb_perp, ac));

    x = dot(point, ab_perp);
    y = dot(point, ac_perp);

    if ((0.0f <= x && x <= 1.0f) && (0.0f <= y && y <= 1.0f) &&
        (x + y <= 1.0f)) {
        return 1;
    }

    return 0;
}

static int check_intersection(V3 camera, V3 mouse_direction, V3 a, V3 b, V3 c,
                              float *res_t) {
    int intersects = 0;

    V3 minus_a = scale3(a, -1.0f);
    V3 ab = add3(b, minus_a);
    V3 ac = add3(c, minus_a);

    V3 perp = cross(ab, ac);

    float t = (dot(a, perp) - dot(camera, perp)) / dot(mouse_direction, perp);

    V3 point = add3(camera, scale3(mouse_direction, t));

    if (inside_triangle(add3(point, minus_a), ab, ac)) {
        *res_t = t;
        intersects = 1;

        if (print_a_thing) {
            printf("found intersection\n");
            printf("  cam = {.x = %f, .y = %f, .z = %f},\n   md = {.x = %f, .y "
                   "= %f, .z = %f},\npoint = {.x = %f, .y = %f, .z = %f},\n    "
                   "a = {.x = %f, .y = "
                   "%f, .z = %f},\n    b = {.x = %f, .y = %f, "
                   ".z = %f},\n    c = {.x = %f, .y = %f, .z = %f}\n\n",
                   camera.x, camera.y, camera.z, mouse_direction.x,
                   mouse_direction.y, mouse_direction.z, point.x, point.y,
                   point.z, a.x, a.y, a.z, b.x, b.y, b.z, c.x, c.y, c.z);
        }
    }

    return intersects;
}

static V2 to_screen_coords(V3 target, V3 x_dir, V3 y_dir) {
    V3 yz_comp;
    float x_comp = dot(x_dir, decompose(target, x_dir, &yz_comp));
    float y_comp = dot(y_dir, decompose(yz_comp, y_dir, NULL));

    return (V2){
        .x = x_comp,
        .y = y_comp,
    };
}

static V3 const UNIT_X_AXIS = {.x = 1.0f, .y = 0.0f, .z = 0.0f};
static V3 const UNIT_Y_AXIS = {.x = 0.0f, .y = 1.0f, .z = 0.0f};
static V3 const UNIT_Z_AXIS = {.x = 0.0f, .y = 0.0f, .z = 1.0f};

static BasisInformation get_basis_information(V3 camera_pos) {
    V3 minus_center = polar_to_rectangular(camera_pos);
    V3 cube_center = scale3(minus_center, -1.0);
    V3 unit_center = as_unit(cube_center);
    V3 y_dir_polar = {
        .rho = 1,
        .theta = camera_pos.phi >= PI_2 ? camera_pos.theta       //
                                        : camera_pos.theta + PI, //
        .phi = camera_pos.phi >= PI_2 ? camera_pos.phi - PI_2    //
                                      : PI_2 - camera_pos.phi,   //
    };
    V3 y_dir = polar_to_rectangular(y_dir_polar);
    V3 x_dir = cross(unit_center, y_dir);

    V3 x_loc = add3(UNIT_X_AXIS, cube_center);
    V3 y_loc = add3(UNIT_Y_AXIS, cube_center);
    V3 z_loc = add3(UNIT_Z_AXIS, cube_center);

    return (BasisInformation){
        .x_dir = x_dir,
        .y_dir = y_dir,
        .z_dir = unit_center,
        .screen_x_dir = to_screen_coords(x_loc, x_dir, y_dir),
        .screen_y_dir = to_screen_coords(y_loc, x_dir, y_dir),
        .screen_z_dir = to_screen_coords(z_loc, x_dir, y_dir),
    };
}

static inline V2 pixel_to_screen(V2 pixel, V2 dims) {
    float mx = pixel.x;
    float my = pixel.y;
    float ww = dims.x;
    float wh = dims.y;
    float factor = sqrt(ww * ww + wh * wh);

    return (V2){
        .x = (-1.0f * ww + 2.0f * mx) / factor,
        .y = (+1.0f * wh - 2.0f * my) / factor,
    };
}

static void update_from_user_input(State *state, Cube *cube,
                                   StateUpdate s_update) {
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

        new_theta +=
            0.25f * state->theta_rot_dir * THETA_PIXELS_PER_SEC * delta_time;
        new_phi -= 0.25f * state->phi_rot_dir * PHI_PIXELS_PER_SEC * delta_time;
    }

    // For now, these numbers are made up... they should be adjusted later
    state->camera.rho =
        DANGEROUS_CLAMP(1.0625 * CAMERA_SCREEN_DIST, new_rho, 1000.0);
    state->camera.theta = new_theta; // TODO: mod by 2pi
    state->camera.phi = DANGEROUS_CLAMP(0.0, new_phi, PI);

    if (s_update.toggle_close) {
        state->should_close = 1 - state->should_close;
    }

    if (s_update.toggle_rotate) {
        state->should_rotate = 1 - state->should_rotate;
    }

    if (s_update.set_depth) {
        state->rotate_depth = s_update.rotate_depth;
    }

    if (s_update.rotate_front) {
        rotate_front(cube, state->rotate_depth, 1);
    }

    if (s_update.checkerboard) {
        checkerboard(cube);
    }

    if (s_update.set_face) {
        FaceColor clamped_fc =
            DANGEROUS_CLAMP(0, s_update.target_face, FC_Count - 1);

        set_facing_side(cube, clamped_fc);
    }

    if (s_update.window_resized) {
        state->window_width = s_update.window_width;
        state->window_height = s_update.window_height;
    }

    if (s_update.toggle_mouse_click) {
        state->mouse_held = 1 - state->mouse_held;
    }

    if (s_update.mouse_moved) {
        state->screen_mouse_x = s_update.mouse_x;
        state->screen_mouse_y = s_update.mouse_y;
    }

    if (s_update.window_resized || s_update.mouse_moved) {
        V2 pixel = {.x = state->screen_mouse_x, .y = state->screen_mouse_y};
        V2 dims = {.x = state->window_width, .y = state->window_height};

        state->mouse = pixel_to_screen(pixel, dims);
    }
}

static inline void force_point_to_cube_edge(V3 *point) {
    float epsilon = 1e-6f;

    if (fabsf(point->x - 1.0f) < epsilon) {
        point->x = +1.0f;
    }
    if (fabsf(point->x + 1.0f) < epsilon) {
        point->x = -1.0f;
    }
    if (fabsf(point->y - 1.0f) < epsilon) {
        point->y = +1.0f;
    }
    if (fabsf(point->y + 1.0f) < epsilon) {
        point->y = -1.0f;
    }
    if (fabsf(point->z - 1.0f) < epsilon) {
        point->z = +1.0f;
    }
    if (fabsf(point->z + 1.0f) < epsilon) {
        point->z = -1.0f;
    }
}

static int find_intersection(V3 camera_pos, V3 mouse_3,
                             BasisInformation basis_info, GraphicsCube *cube,
                             HoverInformation *p_hover_info) {
    int found = 0;
    float closest_t = INFINITY;
    HoverInformation hover_info = {0};

    V3 minus_center = polar_to_rectangular(camera_pos);
    V3 mouse_in_world =
        compose(mouse_3, basis_info.x_dir, basis_info.y_dir, basis_info.z_dir);

    for (uint32_t ind = 0; ind < cube->index_count; ind += 3) {
        float res_t;
        int ind0 = cube->indices[ind + 0];
        int ind1 = cube->indices[ind + 1];
        int ind2 = cube->indices[ind + 2];

        int is_intersecting_triangle = check_intersection(
            minus_center, mouse_in_world, cube->info[ind0].position,
            cube->info[ind1].position, cube->info[ind2].position, &res_t);

        if (is_intersecting_triangle) {
            if (!found || res_t < closest_t) {
                found = 1;
                closest_t = res_t;
                hover_info.hover_face = (FaceColor)cube->info[ind0].face_num;
                hover_info.cube_intersection =
                    add3(minus_center, scale3(mouse_in_world, closest_t));
            }
        }
    }

    if (found) {
        *p_hover_info = hover_info;
    }

    return found;
}

static int matching_direction(BasisInformation basis_info, V3 intersection,
                              V2 screen_diff, float *r_mag, V3 *res) {
    static float thresh = 1e-3f;

    float mag = sqrtf(dot2(screen_diff, screen_diff));
    float inv_mag;

    V2 unit_test;
    V2 t_one;
    V2 t_two;

    V3 r_one;
    V3 r_two;

    float match_one;
    float match_two;
    float r_match;

    if (fabsf(mag) < thresh) {
        return 0;
    }

    inv_mag = 1.0f / mag;
    unit_test = (V2){
        .x = screen_diff.x * inv_mag,
        .y = screen_diff.y * inv_mag,
    };

    if (intersection.x == +1.0f || intersection.x == -1.0f) {
        t_one = basis_info.screen_y_dir;
        t_two = basis_info.screen_z_dir;

        r_one = UNIT_Y_AXIS;
        r_two = UNIT_Z_AXIS;
    } else if (intersection.y == +1.0f || intersection.y == -1.0f) {
        t_one = basis_info.screen_z_dir;
        t_two = basis_info.screen_x_dir;

        r_one = UNIT_Z_AXIS;
        r_two = UNIT_X_AXIS;
    } else if (intersection.z == +1.0f || intersection.z == -1.0f) {
        t_one = basis_info.screen_x_dir;
        t_two = basis_info.screen_y_dir;

        r_one = UNIT_X_AXIS;
        r_two = UNIT_Y_AXIS;
    } else {
        printf("The intersection was not on the cube somehow...\n");
        return 0;
    }

    match_one = dot2(t_one, unit_test) / sqrtf(dot2(t_one, t_one));
    match_two = dot2(t_two, unit_test) / sqrtf(dot2(t_two, t_two));

    if (fabsf(match_one) > fabsf(match_two)) {
        *res = r_one;
        r_match = match_one;
    } else {
        *res = r_two;
        r_match = match_two;
    }

    if (r_match > 0.0f) {
        *r_mag = +1.0f;
    } else {
        *r_mag = -1.0f;
    }

    return 1;
}

static int get_rotation_depth(FaceColor rotation_face, V3 intersection,
                              size_t cube_size) {
    float target_coord;

    switch (rotation_face) {
    case FC_White: {
        target_coord = -intersection.z;
    } break;
    case FC_Red: {
        target_coord = -intersection.x;
    } break;
    case FC_Blue: {
        target_coord = -intersection.y;
    } break;
    case FC_Orange: {
        target_coord = +intersection.x;
    } break;
    case FC_Green: {
        target_coord = +intersection.y;
    } break;
    case FC_Yellow: {
        target_coord = +intersection.z;
    } break;
    default:
        return 0;
    }

    target_coord = (float)cube_size * 0.5f * (target_coord + 1.0f);
    return (int)(target_coord >= cube_size ? cube_size - 1 : target_coord);
}

static void update_intersection_info(State *state, GraphicsCube *cube,
                                     uint32_t toggled_click) {
    V3 camera_pos = {
        .rho = state->camera.rho,
        .theta = state->camera.theta,
        .phi = state->camera.phi,
    };

    V3 mouse_3 = {
        .x = state->mouse.x,
        .y = state->mouse.y,
        .z = CAMERA_SCREEN_DIST,
    };

    BasisInformation basis_info = get_basis_information(camera_pos);
    int found = 0;

    state->basis_info = basis_info;

    if (toggled_click || !state->mouse_held) {
        HoverInformation hover_info;
        found = find_intersection(camera_pos, mouse_3, basis_info, cube,
                                  &hover_info);

        if (found) {
            state->cube_intersection_found = 1;
            state->hover_info = hover_info;
        } else {
            state->cube_intersection_found = 0;
        }
    }

    if (toggled_click) {
        if (state->mouse_held) {
            state->click_info = (ClickInformation){
                .hover_info = state->hover_info,
                .screen_x = state->screen_mouse_x,
                .screen_y = state->screen_mouse_y,
            };
        } else {
            V3 intersection = state->click_info.hover_info.cube_intersection;
            V2 diff_vec = {
                .x = (float)state->screen_mouse_x -
                     (float)state->click_info.screen_x +
                     0.5f * (float)state->window_width,
                .y = (float)state->screen_mouse_y -
                     (float)state->click_info.screen_y +
                     0.5f * (float)state->window_height,
            };
            V2 dims = {.x = state->window_width, .y = state->window_height};

            V2 screen_diff = pixel_to_screen(diff_vec, dims);

            V3 matched_dir;
            float matched_mag;
            int matched;

            force_point_to_cube_edge(&intersection);
            matched = matching_direction(basis_info, intersection, screen_diff,
                                         &matched_mag, &matched_dir);

            if (matched) {
                V3 face_center = point_to_face_center(intersection);
                // FaceColor intersected_face = get_cube_face(face_center);

                V3 matched_dir_mult = scale3(matched_dir, matched_mag);
                V3 rotation_axis = cross(face_center, matched_dir_mult);
                FaceColor rotation_face = get_cube_face(rotation_axis);

                int rotation_depth = get_rotation_depth(
                    rotation_face, intersection, get_side_count(cube->cube));
                set_facing_side(cube->cube, rotation_face);
                rotate_front(cube->cube, rotation_depth, 0);
            }
        }

        if (found) {
            state->mouse_clicked_cube = 1;
        } else {
            state->mouse_clicked_cube = 0;
        }
    }
}

static void update(Application *app, StateUpdate s_update) {
    State *state = &app->state;

    update_from_user_input(state, app->cube.cube, s_update);
    update_intersection_info(state, &app->cube, s_update.toggle_mouse_click);

    if (print_a_thing) {
        printf(
            "\nScreen dirs:\n"
            "x_axis = {.x = %f, .y = %f}\n"
            "y_axis = {.x = %f, .y = %f}\n"
            "z_axis = {.x = %f, .y = %f}\n",
            state->basis_info.screen_x_dir.x, state->basis_info.screen_x_dir.y,
            state->basis_info.screen_y_dir.x, state->basis_info.screen_y_dir.y,
            state->basis_info.screen_z_dir.x, state->basis_info.screen_z_dir.y);
    }

    app->last_ticks = s_update.ticks;
}

typedef struct {
    unsigned char r, g, b;
} Color;

static void write_color_for_face(void *v_buf, FaceColor fc) {
    Color *buf = (Color *)v_buf;

    static Color fc_color[FC_Count] = {
        [FC_White] = {.r = 0xFF, .g = 0xFF, .b = 0xFF},
        [FC_Green] = {.r = 0x00, .g = 0xFF, .b = 0x00},
        [FC_Red] = {.r = 0xFF, .g = 0x00, .b = 0x00},
        [FC_Blue] = {.r = 0x00, .g = 0x00, .b = 0xFF},
        [FC_Orange] = {.r = 0xFF, .g = 0xA5, .b = 0x00},
        [FC_Yellow] = {.r = 0xFF, .g = 0xFF, .b = 0x00},
    };

    *buf = fc_color[fc];
}

static void create_texture_from_cube(Application const *app, Color **p_texture,
                                     uint32_t *p_width, uint32_t *p_height) {
    uint32_t side_count = get_side_count(app->cube.cube);

    uint32_t item_size = sizeof(Color);
    uint32_t stride = 4 * side_count;
    uint32_t height = 4 * side_count;
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

    generic_write_cube(app->cube.cube, (void *)texture, spacing,
                       write_color_for_face);

    *p_texture = texture;
    *p_width = stride;
    *p_height = height;
}

static void set_vec2_uniform(GLuint gl_program, char const *uniform_name,
                             uint32_t vec_count, float *floats) {
    GLuint uniform_index = glGetUniformLocation(gl_program, uniform_name);
    glUniform2fv(uniform_index, vec_count, (GLfloat const *)floats);
}

static void set_vec3_uniform(GLuint gl_program, char const *uniform_name,
                             uint32_t vec_count, float *floats) {
    GLuint uniform_index = glGetUniformLocation(gl_program, uniform_name);
    glUniform3fv(uniform_index, vec_count, (GLfloat const *)floats);
}

static void set_float_uniform(GLuint gl_program, char const *uniform_name,
                              float f) {
    GLuint uniform_index = glGetUniformLocation(gl_program, uniform_name);
    glUniform1f(uniform_index, f);
}

static void set_cube_uniforms(GLuint gl_program, V3 x_dir, V3 y_dir,
                              V3 cube_center, uint32_t side_count,
                              float screen_cube_ratio, V2 dim_vec) {
    set_vec3_uniform(gl_program, "view_information.x_dir", 1, x_dir.xyz);
    set_vec3_uniform(gl_program, "view_information.y_dir", 1, y_dir.xyz);
    set_vec3_uniform(gl_program, "view_information.cube_center", 1,
                     cube_center.xyz);
    set_float_uniform(gl_program, "view_information.screen_cube_ratio",
                      screen_cube_ratio);
    set_vec2_uniform(gl_program, "view_information.screen_dims", 1, dim_vec.xy);
    set_float_uniform(gl_program, "side_count", (float)side_count);
}

static void get_dirs(V3 x_dir, V3 y_dir, V3 z_dir, V2 *screen_x, V2 *screen_y,
                     V2 *screen_z) {
    V3 x = {.x = 1.0f, .y = 0.0f, .z = 0.0f};
    V3 y = {.x = 0.0f, .y = 1.0f, .z = 0.0f};
    V3 z = {.x = 0.0f, .y = 0.0f, .z = 1.0f};

    V3 x_prime = complete_decomp(x, x_dir, y_dir, z_dir);
    V3 y_prime = complete_decomp(y, x_dir, y_dir, z_dir);
    V3 z_prime = complete_decomp(z, x_dir, y_dir, z_dir);

    *screen_x = x_prime.xy;
    *screen_y = y_prime.xy;
    *screen_z = z_prime.xy;
}

static int render_cube(Application const *app, V2 dim_vec) {
    int ret = 0;

    GLuint gl_program = app->gl_program;
    uint32_t side_count = get_side_count(app->cube.cube);

    GraphicsCube const *cube = &app->cube;

    Camera const *camera = &app->state.camera;
    V3 camera_pos = {
        .rho = camera->rho,
        .theta = camera->theta,
        .phi = camera->phi,
    };

    V3 minus_center = polar_to_rectangular(camera_pos);
    V3 cube_center = scale3(minus_center, -1.0);
    V3 unit_center = as_unit(cube_center);
    float screen_cube_ratio = CAMERA_SCREEN_DIST / camera_pos.rho;

    V3 y_dir_polar = {
        .rho = 1,
        .theta =
            camera_pos.phi >= PI_2 ? camera_pos.theta : camera_pos.theta + PI,
        .phi = camera_pos.phi >= PI_2 ? camera_pos.phi - PI_2
                                      : PI_2 - camera_pos.phi,
    };
    V3 y_dir = polar_to_rectangular(y_dir_polar);
    V3 x_dir = cross(unit_center, y_dir);

    Color *texture = NULL;
    uint32_t tex_width, tex_height;

    GLuint uniform_index;

    V3 mouse_3 = {
        .x = app->state.mouse.x,
        .y = app->state.mouse.y,
        .z = CAMERA_SCREEN_DIST,
    };
    V3 mouse_in_world = compose(mouse_3, x_dir, y_dir, unit_center);

    V2 screen_x, screen_y, screen_z;
    get_dirs(x_dir, y_dir, unit_center, &screen_x, &screen_y, &screen_z);

    VertexAttributes *attributes =
        ARENA_PUSH_N(VertexAttributes, app->arena, cube->vertex_count);
    if (attributes == NULL) {
        fprintf(stderr, "Couldn't allocate space for vertex attribute array\n");
        goto vertex_alloc_fail;
    }

    if (print_a_thing) {
        printf("mouse_2 = {.x = %f, .y = %f},\nmouse_3 = {.x = %f, .y = %f, .z "
               "= %f}\nmouse_in_world = {.x = "
               "%f, .y = %f, .z = %f}\n\n",
               app->state.mouse.x, app->state.mouse.y, mouse_3.x, mouse_3.y,
               mouse_3.z, mouse_in_world.x, mouse_in_world.y, mouse_in_world.z);
    }

    // TODO: figure out if there is a better way to clear this, since these are
    // all floats... try to figure out how to pass integers into the buffer
    // without breaking everything
    for (uint32_t ind = 0; ind < cube->vertex_count; ++ind) {
        attributes[ind].intersecting = 0.0f;
    }

    if (app->state.cube_intersection_found) {
        for (uint32_t ind = 0; ind < cube->index_count; ind += 6) {
            int ind00 = cube->indices[ind + 0];
            int ind01 = cube->indices[ind + 1];
            int ind02 = cube->indices[ind + 2];
            int ind10 = cube->indices[ind + 3];
            int ind11 = cube->indices[ind + 4];
            int ind12 = cube->indices[ind + 5];

            if ((FaceColor)cube->info[ind00].face_num !=
                app->state.hover_info.hover_face) {
                // can skip this one because we aren't even looking at the right
                // face
                continue;
            }

            V3 minus_a0 = scale3(cube->info[ind00].position, -1.0f);
            V3 minus_a1 = scale3(cube->info[ind10].position, -1.0f);

            int is_intersecting_triangle0 = inside_triangle(
                add3(app->state.hover_info.cube_intersection, minus_a0),
                add3(cube->info[ind01].position, minus_a0),
                add3(cube->info[ind02].position, minus_a0));

            int is_intersecting_triangle1 = inside_triangle(
                add3(app->state.hover_info.cube_intersection, minus_a1),
                add3(cube->info[ind11].position, minus_a1),
                add3(cube->info[ind12].position, minus_a1));

            if (is_intersecting_triangle0 || is_intersecting_triangle1) {
                V3 a0 = cube->info[ind00].position;
                V3 b0 = cube->info[ind01].position;
                V3 c0 = cube->info[ind02].position;
                V3 a1 = cube->info[ind10].position;
                V3 b1 = cube->info[ind11].position;
                V3 c1 = cube->info[ind12].position;

                if (print_a_thing) {
                    printf(
                        "Intersection with:\n"
                        "closest_intersection = {.x = %f, .y = %f, .z = %f}\n"
                        "a0 = {.x = %f, .y = %f, .z = %f}\nb0 = {.x = %f, .y = "
                        "%f, .z = %f}\nc0 = {.x = %f, .y = %f, .z = %f}\n"
                        "a1 = {.x = %f, .y = %f, .z = %f}\nb1 = {.x = %f, .y = "
                        "%f, .z = %f}\nc1 = {.x = %f, .y = %f, .z = %f}\n\n",
                        app->state.hover_info.cube_intersection.x,
                        app->state.hover_info.cube_intersection.y,
                        app->state.hover_info.cube_intersection.z, a0.x, a0.y,
                        a0.z, b0.x, b0.y, b0.z, c0.x, c0.y, c0.z, a1.x, a1.y,
                        a1.z, b1.x, b1.y, b1.z, c1.x, c1.y, c1.z);
                }

                attributes[ind00].intersecting = 1.0f;
                attributes[ind01].intersecting = 1.0f;
                attributes[ind02].intersecting = 1.0f;
                attributes[ind10].intersecting = 1.0f;
                attributes[ind11].intersecting = 1.0f;
                attributes[ind12].intersecting = 1.0f;
            }
        }
    }

    create_texture_from_cube(app, &texture, &tex_width, &tex_height);
    if (texture == NULL) {
        fprintf(stderr, "Couldn't allocate space for cube texture\n");
        goto texture_alloc_fail;
    }

    glBindVertexArray(cube->vao);

    glActiveTexture(GL_TEXTURE0);
    uniform_index = glGetUniformLocation(gl_program, "cube_texture");
    glUniform1i(uniform_index, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    set_cube_uniforms(gl_program, x_dir, y_dir, cube_center, side_count,
                      screen_cube_ratio, dim_vec);

    glBindTexture(GL_TEXTURE_2D, cube->texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, tex_width, tex_height, 0, GL_RGB,
                 GL_UNSIGNED_BYTE, texture);
    glGenerateMipmap(GL_TEXTURE_2D);

    // fill vertex information buffer
    glBindBuffer(GL_ARRAY_BUFFER, cube->vbo[0]);
    glBufferData(GL_ARRAY_BUFFER, cube->vertex_count * sizeof(*cube->info),
                 (void const *)cube->info, GL_STATIC_DRAW);

    // specify location of data within buffer
    glVertexAttribPointer(
        0, 3, GL_FLOAT, GL_FALSE, sizeof(*cube->info),
        (GLvoid const *)offsetof(VertexInformation, position));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(
        1, 1, GL_FLOAT, GL_FALSE, sizeof(*cube->info),
        (GLvoid const *)offsetof(VertexInformation, face_num));
    glEnableVertexAttribArray(1);

    // fill vertex information buffer
    glBindBuffer(GL_ARRAY_BUFFER, cube->vbo[1]);
    glBufferData(GL_ARRAY_BUFFER, cube->vertex_count * sizeof(*attributes),
                 (void const *)attributes, GL_STATIC_DRAW);

    // specify location of data within buffer
    glVertexAttribPointer(
        2, 1, GL_FLOAT, GL_FALSE, sizeof(*attributes),
        (GLvoid const *)offsetof(VertexAttributes, intersecting));
    glEnableVertexAttribArray(2);

    // fill index buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cube->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 cube->index_count * sizeof(*cube->indices),
                 (void const *)cube->indices, GL_STATIC_DRAW);

    glDrawElements(GL_TRIANGLES, cube->index_count, GL_UNSIGNED_INT,
                   (void const *)0);

    return ret;

texture_alloc_fail:
    ret += 1;

vertex_alloc_fail:
    ret += 1;
    return ret;
}

static void render(Application const *app) {
    // TODO: Get the screen width and height and use a "pixels to meters" type
    // thing like from Handmade Hero?

    V2 dim_vec = {
        .x = (float)app->state.window_width,
        .y = (float)app->state.window_height,
    };

    glViewport(0, 0, app->state.window_width, app->state.window_height);

    if (arena_begin(app->arena) == 0) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // TODO: maybe write some wrappers for this?
        if (render_cube(app, dim_vec) != 0) {
            goto skip_render;
        }

        SDL_GL_SwapWindow(app->window);

    skip_render:
        arena_pop(app->arena);
    }
}

static void define_split_vertices(Arena *arena, uint32_t cube_size,
                                  GraphicsCube *cube) {
    uint32_t split_vertex_count =
        CUBE_FACES * (2 * cube_size) * (2 * cube_size);

    double factor = 1.0 / (double)cube_size;

    cube->vertex_count = split_vertex_count;
    cube->info = ARENA_PUSH_N(VertexInformation, arena, split_vertex_count);

    uint32_t cur_index = 0;
    for (uint32_t face_id = 0; face_id < CUBE_FACES; ++face_id) {
        int const *cube_indices = face_indices + (face_id * SQUARE_CORNERS);

        V3 start = cube_vertices[cube_indices[0]];
        V3 final = cube_vertices[cube_indices[2]];

        V3 target_one = cube_vertices[cube_indices[1]];
        V3 target_two = cube_vertices[cube_indices[3]];

        // bottom left
        cube->info[cur_index++] = (VertexInformation){
            .position = start,
            .face_num = (float)face_id,
        };

        // left side
        for (uint32_t x_i = 1; x_i < cube_size; ++x_i) {
            // two of them to allow for rotation
            cube->info[cur_index++] = (VertexInformation){
                .position = v_lerp(start, x_i * factor, target_one),
                .face_num = (float)face_id,
            };

            // (the second)
            cube->info[cur_index++] = (VertexInformation){
                .position = v_lerp(start, x_i * factor, target_one),
                .face_num = (float)face_id,
            };
        }

        // top left
        cube->info[cur_index++] = (VertexInformation){
            .position = target_one,
            .face_num = (float)face_id,
        };

        for (uint32_t y_i = 1; y_i < cube_size; ++y_i) {
            // two of them to allow for rotation
            {
                V3 inner_start = v_lerp(start, y_i * factor, target_two);
                V3 inner_target_one = v_lerp(target_one, y_i * factor, final);

                cube->info[cur_index++] = (VertexInformation){
                    .position = inner_start,
                    .face_num = (float)face_id,
                };

                for (uint32_t x_i = 1; x_i < cube_size; ++x_i) {
                    // two of them to allow for rotation
                    cube->info[cur_index++] = (VertexInformation){
                        .position =
                            v_lerp(inner_start, x_i * factor, inner_target_one),
                        .face_num = (float)face_id,
                    };

                    // (the second)
                    cube->info[cur_index++] = (VertexInformation){
                        .position =
                            v_lerp(inner_start, x_i * factor, inner_target_one),
                        .face_num = (float)face_id,
                    };
                }

                cube->info[cur_index++] = (VertexInformation){
                    .position = inner_target_one,
                    .face_num = (float)face_id,
                };
            }

            // (the second)
            {
                V3 inner_start = v_lerp(start, y_i * factor, target_two);
                V3 inner_target_one = v_lerp(target_one, y_i * factor, final);

                cube->info[cur_index++] = (VertexInformation){
                    .position = inner_start,
                    .face_num = (float)face_id,
                };

                for (uint32_t x_i = 1; x_i < cube_size; ++x_i) {
                    // two of them to allow for rotation
                    cube->info[cur_index++] = (VertexInformation){
                        .position =
                            v_lerp(inner_start, x_i * factor, inner_target_one),
                        .face_num = (float)face_id,
                    };

                    // (the second)
                    cube->info[cur_index++] = (VertexInformation){
                        .position =
                            v_lerp(inner_start, x_i * factor, inner_target_one),
                        .face_num = (float)face_id,
                    };
                }

                cube->info[cur_index++] = (VertexInformation){
                    .position = inner_target_one,
                    .face_num = (float)face_id,
                };
            }
        }

        // bottom right
        cube->info[cur_index++] = (VertexInformation){
            .position = target_two,
            .face_num = (float)face_id,
        };

        // right side
        for (uint32_t x_i = 1; x_i < cube_size; ++x_i) {
            // two of them to allow for rotation
            cube->info[cur_index++] = (VertexInformation){
                .position = v_lerp(target_two, x_i * factor, final),
                .face_num = (float)face_id,
            };

            // (the second)
            cube->info[cur_index++] = (VertexInformation){
                .position = v_lerp(target_two, x_i * factor, final),
                .face_num = (float)face_id,
            };
        }

        // top right
        cube->info[cur_index++] = (VertexInformation){
            .position = final,
            .face_num = (float)face_id,
        };
    }

    if (cur_index != split_vertex_count) {
        fprintf(stderr, "Wrong vertex count\n");
    }

#if 1
    for (uint32_t i = 0; i < cur_index / CUBE_FACES; ++i) {
        printf("cube->info[%d] = { .position = { .x = %f, .y = %f, .z = %f }, "
               ".face_num = %d }\n",
               i, cube->info[i].position.x, cube->info[i].position.y,
               cube->info[i].position.z, (int)cube->info[i].face_num);
    }
#endif
}

static void define_split_indices(Arena *arena, uint32_t cube_size,
                                 GraphicsCube *cube) {
    uint32_t two_cs_m_one = 2 * cube_size - 1;
    uint32_t expanded_square_vertices =
        VERTEX_COUNT_TO_TRIANGLE_COUNT(SQUARE_CORNERS);
    uint32_t index_count_per_side =
        expanded_square_vertices * (two_cs_m_one * two_cs_m_one);
    uint32_t split_index_count = CUBE_FACES * index_count_per_side;
    uint32_t index_stride = 2 * cube_size;

    cube->index_count = split_index_count;
    cube->indices = ARENA_PUSH_N(int, arena, split_index_count);

    uint32_t count = 0;
    for (uint32_t face_id = 0; face_id < CUBE_FACES; ++face_id) {
        uint32_t index_base = face_id * (index_stride * index_stride);
        int *cur_loc = cube->indices + (face_id * index_count_per_side);

        for (uint32_t col = 0; col < two_cs_m_one; ++col) {
            for (uint32_t row = 0; row < two_cs_m_one; ++row) {

                int face_square_indices[4] = {
                    index_base,                    //
                    index_base + 1,                //
                    index_base + index_stride + 1, //
                    index_base + index_stride,     //
                };

                expand_vertices_to_triangles(face_square_indices, 4, 4,
                                             cur_loc);
                cur_loc += expanded_square_vertices;
                count += expanded_square_vertices;
                index_base += 1;
            }
            index_base += 1;
        }
    }

    if (count != split_index_count) {
        fprintf(stderr, "Wrong index count\n");
    }

#if 1
    for (uint32_t i = 0;
         i < cube->index_count / expanded_square_vertices / CUBE_FACES; ++i) {
        printf("cube->indices[%03d..%03d] = { ", expanded_square_vertices * i,
               expanded_square_vertices * (i + 1));

        printf(" %02d", cube->indices[expanded_square_vertices * i]);
        for (uint32_t j = 1; j < expanded_square_vertices; ++j) {
            printf(", %02d", cube->indices[expanded_square_vertices * i + j]);
        }

        printf(" }\n");
    }
#endif
}

int graphics_main(void) {
    int ret = 0;
    int sdl_init_ret, gl_init_ret;
    uint32_t cube_size = 5;

    Arena *arena = NULL;
    Cube *cube_state;
    SDL_Window *window = NULL;
    SDL_GLContext *gl_context = NULL;
    GLuint gl_program = 0;
    GraphicsCube cube = {0};
    Application app;

    arena = alloc_arena();
    if (arena == NULL) {
        fprintf(stderr, "Unable to allocate arena\n");
        goto arena_alloc_fail;
    }

    if ((sdl_init_ret = sdl_init(&window)) != 0) {
        fprintf(stderr, "Unable to init application with code %d\n",
                sdl_init_ret);
        goto sdl_init_fail;
    }

    if ((gl_init_ret = gl_init(window, &gl_context, &gl_program, &cube)) != 0) {
        fprintf(stderr, "Unable to init GLEW and shaders with code %d\n",
                gl_init_ret);
        goto gl_init_fail;
    }

    if ((cube_state = new_cube(cube_size)) == NULL) {
        fprintf(stderr, "Could not allocate cube\n");
        goto cube_alloc_fail;
    }

    cube.cube = cube_state;

    app = (Application){
        .window = window,
        .last_ticks = SDL_GetTicks(),

        .gl_context = gl_context,
        .gl_program = gl_program,
        .cube = cube,

        .arena = arena,
        .state = get_initial_state(),
    };

    if (arena_begin(arena) == 0) {
        define_split_vertices(arena, cube_size, &app.cube);
        define_split_indices(arena, cube_size, &app.cube);

        while (!app.state.should_close) {
            StateUpdate s_update;

            s_update = get_inputs(&app);
            update(&app, s_update);
            render(&app);
        }

        arena_pop(arena);
    }

    return ret;

cube_alloc_fail:
    ret += 1;

gl_init_fail:
    ret += 1;

sdl_init_fail:
    ret += 1;

arena_alloc_fail:
    ret += 1;
    app_cleanup(&app);
    return ret;
}
