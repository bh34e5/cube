#include "graphics.h"

#include <stdio.h>

#include "common.h"

#define _DEBUG

#ifdef _DEBUG
#include "graphics_debug.c"
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

static State get_initial_state(Cube *cube) {
    return (State){
        .theta_rot_dir = 1.0,
        .phi_rot_dir = 1.0,

        .rotate_depth = 1,

        .should_rotate = 0,
        .should_close = 0,
        .camera = get_initial_camera(),
        .cube = cube,
    };
}

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

static int gl_init(SDL_Window *window, Application_GL_Info *gl_info) {
    int ret = 0;
    SDL_GLContext context = NULL;
    GLenum glew_error;
    GLuint vert_shader, frag_shader, gl_program;

    char *vert_contents = NULL;
    char *frag_contents = NULL;
    long vert_size, frag_size;

    CubeModel cube_model = {0};

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

    CLEANUP_IF(load_file(FRAGMENT_SHADER_NAME, &frag_size, &frag_contents) != 0,
               "Failed to load fragment shader contents\n");

    CLEANUP_IF(gl_load_shader(&vert_shader, GL_VERTEX_SHADER,
                              (GLchar const *)vert_contents) < 0,
               "Failed to load vertex shader\n");

    CLEANUP_IF(gl_load_shader(&frag_shader, GL_FRAGMENT_SHADER,
                              (GLchar const *)frag_contents) < 0,
               "Failed to load fragment shader\n");

    CLEANUP_IF(gl_link_program(&gl_program, vert_shader, frag_shader) < 0,
               "Failed to link program\n");

    // after linking, it's okay to delete these
    free(vert_contents);
    free(frag_contents);
    glDeleteShader(vert_shader);
    glDeleteShader(frag_shader);

    glUseProgram(gl_program);

    glGenVertexArrays(1, &cube_model.vao);
    glGenBuffers(1, &cube_model.vbo);
    glGenBuffers(1, &cube_model.ebo);
    glGenTextures(1, &cube_model.texture);

    glBindVertexArray(cube_model.vao);

    *gl_info = (Application_GL_Info){
        .gl_context = context,
        .gl_program = gl_program,
        .cube_model = cube_model,
    };

cleanup:
    if (ret != 0) {
        if (cube_model.texture != 0) {
            glDeleteTextures(1, &cube_model.texture);
        }

        if (cube_model.ebo != 0) {
            glDeleteBuffers(1, &cube_model.ebo);
        }

        if (cube_model.vbo != 0) {
            glDeleteBuffers(1, &cube_model.vbo);
        }

        if (cube_model.vao != 0) {
            glDeleteVertexArrays(1, &cube_model.vao);
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
    if (app->gl_info.cube_model.texture != 0) {
        glDeleteTextures(1, &app->gl_info.cube_model.texture);
    }

    if (app->gl_info.cube_model.ebo != 0) {
        glDeleteBuffers(1, &app->gl_info.cube_model.ebo);
    }

    if (app->gl_info.cube_model.vbo != 0) {
        glDeleteBuffers(1, &app->gl_info.cube_model.vbo);
    }

    if (app->gl_info.cube_model.vao != 0) {
        glDeleteVertexArrays(1, &app->gl_info.cube_model.vao);
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
        app->state.rotate_depth = s_update.rotate_depth;
    }

    if (s_update.rotate_front) {
        rotate_front(app->state.cube, app->state.rotate_depth - 1, 1);
    }

    if (s_update.set_face) {
        FaceColor clamped_fc =
            DANGEROUS_CLAMP(0, s_update.target_face, FC_Count - 1);

        set_facing_side(app->state.cube, clamped_fc);
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
        [FC_Orange] = {.r = 0xFF, .g = 0xA5, .b = 0x00},
        [FC_Yellow] = {.r = 0xFF, .g = 0xFF, .b = 0x00},
    };

    *buf = fc_color[fc];
}

static void create_texture_from_cube(Application const *app, Color **p_texture,
                                     uint32_t *p_width, uint32_t *p_height) {
    uint32_t side_count = get_side_count(app->state.cube);

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

    generic_write_cube(app->state.cube, (void *)texture, spacing,
                       write_color_for_face);

    *p_texture = texture;
    *p_width = stride;
    *p_height = height;
}

static void render(Application const *app) {
    // TODO: Get the screen width and height and use a "pixels to meters" type
    // thing like from Handmade Hero?

    GLuint gl_program = app->gl_info.gl_program;
    CubeModel const *cube_model = &app->gl_info.cube_model;

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
        uniform_index = glGetUniformLocation(gl_program, "cube_texture");
        glUniform1i(uniform_index, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        // TODO: maybe write some wrappers for this?

        uniform_index =
            glGetUniformLocation(gl_program, "view_information.x_dir");
        glUniform3fv(uniform_index, 1, (GLfloat const *)x_dir.xyz);

        uniform_index =
            glGetUniformLocation(gl_program, "view_information.y_dir");
        glUniform3fv(uniform_index, 1, (GLfloat const *)y_dir.xyz);

        uniform_index =
            glGetUniformLocation(gl_program, "view_information.cube_center");
        glUniform3fv(uniform_index, 1, (GLfloat const *)cube_center.xyz);

        uniform_index = glGetUniformLocation(
            gl_program, "view_information.screen_cube_ratio");
        glUniform1f(uniform_index, screen_cube_ratio);

        glBindVertexArray(cube_model->vao);

        glBindTexture(GL_TEXTURE_2D, cube_model->texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, tex_width, tex_height, 0, GL_RGB,
                     GL_UNSIGNED_BYTE, texture);
        glGenerateMipmap(GL_TEXTURE_2D);

        // fill vertex buffer
        glBindBuffer(GL_ARRAY_BUFFER, cube_model->vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     cube_model->vertex_count * sizeof(*cube_model->info),
                     (void const *)cube_model->info, GL_STATIC_DRAW);

        // fill index buffer
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cube_model->ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     cube_model->index_count * sizeof(*cube_model->indices),
                     (void const *)cube_model->indices, GL_STATIC_DRAW);

        // specify location of data within buffer
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                              sizeof(*cube_model->info),
                              (GLvoid const *)(0 * sizeof(float)));
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE,
                              sizeof(*cube_model->info),
                              (GLvoid const *)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glDrawElements(GL_TRIANGLES, cube_model->index_count, GL_UNSIGNED_INT,
                       (void const *)0);

        SDL_GL_SwapWindow(app->window);

    skip_render:
        arena_pop(app->arena);
    }
}

static void define_split_vertices(Arena *arena, uint32_t cube_size,
                                  CubeModel *cube_model) {
    uint32_t split_vertex_count =
        CUBE_FACES * (cube_size + 1) * (cube_size + 1);

    double factor = 1.0 / (double)cube_size;

    cube_model->vertex_count = split_vertex_count;
    cube_model->info =
        ARENA_PUSH_N(VertexInformation, arena, split_vertex_count);

    uint32_t cur_index = 0;
    for (uint32_t face_id = 0; face_id < CUBE_FACES; ++face_id) {
        int const *cube_indices = face_indices + (face_id * SQUARE_CORNERS);

        V3 start = cube_vertices[cube_indices[0]];
        V3 final = cube_vertices[cube_indices[2]];

        V3 target_one = cube_vertices[cube_indices[1]];
        V3 target_two = cube_vertices[cube_indices[3]];

        cube_model->info[cur_index++] = (VertexInformation){
            .position = start,
            .face_num = (float)face_id,
        };

        for (uint32_t x_i = 1; x_i < cube_size; ++x_i) {
            cube_model->info[cur_index++] = (VertexInformation){
                .position = v_lerp(start, x_i * factor, target_one),
                .face_num = (float)face_id,
            };
        }

        cube_model->info[cur_index++] = (VertexInformation){
            .position = target_one,
            .face_num = (float)face_id,
        };

        for (uint32_t y_i = 1; y_i < cube_size; ++y_i) {
            V3 inner_start = v_lerp(start, y_i * factor, target_two);
            V3 inner_target_one = v_lerp(target_one, y_i * factor, final);

            cube_model->info[cur_index++] = (VertexInformation){
                .position = inner_start,
                .face_num = (float)face_id,
            };

            for (uint32_t x_i = 1; x_i < cube_size; ++x_i) {
                cube_model->info[cur_index++] = (VertexInformation){
                    .position =
                        v_lerp(inner_start, x_i * factor, inner_target_one),
                    .face_num = (float)face_id,
                };
            }

            cube_model->info[cur_index++] = (VertexInformation){
                .position = inner_target_one,
                .face_num = (float)face_id,
            };
        }

        cube_model->info[cur_index++] = (VertexInformation){
            .position = target_two,
            .face_num = (float)face_id,
        };

        for (uint32_t x_i = 1; x_i < cube_size; ++x_i) {
            cube_model->info[cur_index++] = (VertexInformation){
                .position = v_lerp(target_two, x_i * factor, final),
                .face_num = (float)face_id,
            };
        }

        cube_model->info[cur_index++] = (VertexInformation){
            .position = final,
            .face_num = (float)face_id,
        };
    }
}

static void define_split_indices(Arena *arena, uint32_t cube_size,
                                 CubeModel *cube_model) {
    uint32_t expanded_square_vertices =
        VERTEX_COUNT_TO_TRIANGLE_COUNT(SQUARE_CORNERS);
    uint32_t index_count_per_side =
        expanded_square_vertices * (cube_size * cube_size);
    uint32_t split_index_count = CUBE_FACES * index_count_per_side;
    uint32_t index_stride = cube_size + 1;

    cube_model->index_count = split_index_count;
    cube_model->indices = ARENA_PUSH_N(int, arena, split_index_count);

    for (uint32_t face_id = 0; face_id < CUBE_FACES; ++face_id) {
        uint32_t index_base = face_id * (index_stride * index_stride);
        int *cur_loc = cube_model->indices + (face_id * index_count_per_side);

        for (uint32_t col = 0; col < cube_size; ++col) {
            for (uint32_t row = 0; row < cube_size; ++row) {

                int face_square_indices[4] = {
                    index_base,                    //
                    index_base + 1,                //
                    index_base + index_stride + 1, //
                    index_base + index_stride,     //
                };

                expand_vertices_to_triangles(face_square_indices, 4, 4,
                                             cur_loc);
                cur_loc += expanded_square_vertices;
                index_base += 1;
            }
            index_base += 1;
        }
    }
}

int graphics_main(void) {
    int ret = 0;
    int sdl_init_ret, gl_init_ret;
    uint32_t cube_size = 5;

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

    CLEANUP_IF((cube = new_cube(cube_size)) == NULL,
               "Could not allocate cube\n");

    app = (Application){
        .window = window,
        .gl_info = gl_info,
        .last_ticks = SDL_GetTicks(),

        .arena = arena,
        .state = get_initial_state(cube),
    };

    if (arena_begin(arena) == 0) {
        define_split_vertices(arena, cube_size, &app.gl_info.cube_model);
        define_split_indices(arena, cube_size, &app.gl_info.cube_model);

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
