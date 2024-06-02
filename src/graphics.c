#include "graphics.h"

#include <stdio.h>

#include "common.h"
#include "my_math.h"

static void loop(Application *app);
static void update(Application *app, StateUpdate s_update, double delta_time);
static void draw(Application *app);

static void get_point_projections(Camera camera, V3 const *vertices,
                                  SDL_FPoint *projections, float *zs,
                                  uint32_t count);

static void get_convex_hull(SDL_FPoint const *projections, int *is_hull,
                            uint32_t count);

static void get_visible_faces(int const *is_hull, float const *zs,
                              uint32_t vertex_count, int const *face_indices,
                              uint32_t face_index_count, int *face_visible,
                              uint32_t face_count);

typedef struct {
    int r, g, b;
} Color;

#define CUBE_FACES 6
#define TRIANGLE_IND_COUNT (CUBE_FACES * VERTEX_COUNT_TO_TRIANGLE_COUNT(4))

Color const face_colors[CUBE_FACES] = {
    {.r = 0xFF, .g = 0xFF, .b = 0xFF}, //
    {.r = 0xFF, .g = 0, .b = 0},       //
    {.r = 0, .g = 0, .b = 0xFF},       //
    {.r = 0xFF, .g = 0x80, .b = 0x80}, //
    {.r = 0, .g = 0xFF, .b = 0},       //
    {.r = 0xFF, .g = 0xFF, .b = 0},    //
};

static int r = 0xFF;
static int g = 0xFF;
static int b = 0xFF;

#define FRAMES 60.0
static double target_mspf = 1000.0 / FRAMES;
static int print_a_thing = 0;

static double theta_rot_dir = 1.0;
static double phi_rot_dir = 1.0;
static int triangle_indices_for_cube[TRIANGLE_IND_COUNT] = {0};

static int const single_face_ind_count = VERTEX_COUNT_TO_TRIANGLE_COUNT(4);

/**
 * The broad idea I have for this:
 *
 * I have a cube with side length 2 placed centered at the origin. Then I have a
 * camera that is placed at a distance R from the origin. This R value can be
 * changed using some keys, but must be at least Sqrt(3) (I think) away so that
 * it doesn't move inside the cube.
 *
 * The camera can also change its phi and its psi angles so that it is free to
 * move around horizontally and vertically.
 *
 * To render, what I need to do is find the _projected_ locations of each of the
 * cube's 8 corners. The projection will be onto some plane orthogonal to the
 * ray cast from the camera to the origin. And this plane should remain a fixed
 * distance from the camera (I think).
 *
 * Once I have these projected locations, I should calculated the convex hull of
 * the 8 points and fill that with some color. Once I have that, I will need to
 * paint the other colors on top of that surface, but I'll worry about that a
 * bit later. I just want to make sure that I can see a reasonable convex hull
 * first.
 *
 * I can try to lookup the algorithm for calculating the convex hull, but I want
 * to try to take a stab at it myself first. I vaguely remember something, and
 * the way that it works is:
 *
 * 1. sort the points by the x coordinate. Find the point(s) with the largest x
 * coordinate and take the one with the largest y. That point is on the
 * boundary.
 *
 * 2. Take the vertical line that runs through that point and pivot it around
 * the point in a counter-clockwise manner until intersecting with another
 * point. That point is on the boundary.
 *
 * 3. From this new point, and the starting slope the same, continue rotating
 * the line counter-clockwise until hitting another point.
 *
 * 4. Continue this until you end up back at the first point. These points that
 * were found are all the points that make up the boundary of the hull.
 */

static State get_initial_state(void) {
    Camera initial_camera = {
        .rho = 2.0 * CAMERA_SCREEN_DIST,
        .theta = 0.0,
        .phi = PI_2,
    };

    return (State){.x = 0.0,
                   .y = 0.0,

                   .camera = initial_camera};
}

int graphics_main(void) {
#define CLEANUP _CLEANUP(__COUNTER__ + 1)
#define _CLEANUP(n)                                                            \
    do {                                                                       \
        ret = (n);                                                             \
        goto cleanup;                                                          \
    } while (0)

    int ret = 0;
    Arena *arena;
    Application app;
    SDL_Window *window;
    SDL_Renderer *window_renderer;

    arena = alloc_arena();
    if (arena == NULL) {
        fprintf(stderr, "Failed to allocate arena\n");
        CLEANUP;
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "Failed to init SDL\n");
        CLEANUP;
    }

    window = SDL_CreateWindow("My Window", SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, 680, 480, 0);

    if (window == NULL) {
        fprintf(stderr, "Failed to create window\n");
        CLEANUP;
    }

    // using -1 here says to let SDL choose which driver index to use
    window_renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    if (window_renderer == NULL) {
        fprintf(stderr, "Failed to create window renderer\n");
        CLEANUP;
    }

    app = (Application){.window = window,
                        .window_renderer = window_renderer,
                        .last_ticks = SDL_GetTicks(),

                        .arena = arena,
                        .state = get_initial_state()};

    expand_vertices_to_triangles(face_indices, ARR_SIZE(face_indices), 4,
                                 triangle_indices_for_cube);

    loop(&app);

cleanup:
    // getting fields from `app` in case they got changed
    if (app.window_renderer != NULL) {
        SDL_DestroyRenderer(app.window_renderer);
    }
    if (app.window != NULL) {
        SDL_DestroyWindow(app.window);
    }
    SDL_Quit();

    return ret;

#undef _CLEANUP
#undef CLEANUP
}

static void loop(Application *app) {
    int should_not_close = 1;
    while (should_not_close) {
        SDL_Event event;
        Uint32 last_ticks, ticks;
        StateUpdate s_update = {0};
        double cur_delta_ms;

        print_a_thing = 0;
        while (SDL_PollEvent(&event) > 0) {
            switch (event.type) {
            case SDL_QUIT: {
                should_not_close = 0;
            } break;
            case SDL_KEYDOWN: {
                Uint8 const *keys = SDL_GetKeyboardState(NULL);
                // change the color of the rectangle
                if (keys[SDL_SCANCODE_R] == 1) {
                    r = 0xFF - r;
                } else if (keys[SDL_SCANCODE_G] == 1) {
                    g = 0xFF - g;
                } else if (keys[SDL_SCANCODE_B] == 1) {
                    b = 0xFF - b;
                }

                // move the rectangle up and down
                if (keys[SDL_SCANCODE_W] == 1) {
                    s_update.ydir = -1;
                } else if (keys[SDL_SCANCODE_S] == 1) {
                    s_update.ydir = 1;
                }

                // move the rectangle left and right
                if (keys[SDL_SCANCODE_A] == 1) {
                    s_update.xdir = -1;
                } else if (keys[SDL_SCANCODE_D] == 1) {
                    s_update.xdir = 1;
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

        update(app, s_update, cur_delta_ms / 1000.0);
        draw(app);

        app->last_ticks = ticks;
    }
}

#define RECT_PIXELS_PER_SEC 500.0
#define RHO_PIXELS_PER_SEC 10.0
#define THETA_PIXELS_PER_SEC (2.0 * PI / 10.0)
#define PHI_PIXELS_PER_SEC (-2.0 * PI / 10.0)

#define DANGEROUS_MAX(a, b) ((a) > (b) ? (a) : (b))
#define DANGEROUS_MIN(a, b) ((a) < (b) ? (a) : (b))

#define DANGEROUS_CLAMP(min, n, max) (DANGEROUS_MIN(max, DANGEROUS_MAX(min, n)))

static void update(Application *app, StateUpdate s_update, double delta_time) {
    State *state = &app->state;
    double new_rho, new_theta, new_phi;

    state->x += s_update.xdir * delta_time * RECT_PIXELS_PER_SEC;
    state->y += s_update.ydir * delta_time * RECT_PIXELS_PER_SEC;

    if ((state->camera.theta <= 0.0 + 0.001 && theta_rot_dir < 0) ||
        (state->camera.theta >= 2 * PI - 0.001 && theta_rot_dir > 0)) {
        theta_rot_dir *= -1.0;
    }

    if ((state->camera.phi <= 0.0 + 0.001 && phi_rot_dir < 0) ||
        (state->camera.phi >= PI - 0.001 && phi_rot_dir > 0)) {
        phi_rot_dir *= -1.0;
    }

    new_rho = state->camera.rho +
              s_update.camera_rho_dir * delta_time * RHO_PIXELS_PER_SEC;
    new_theta = state->camera.theta +
                s_update.camera_theta_dir * delta_time * THETA_PIXELS_PER_SEC;
    new_phi = state->camera.phi +
              s_update.camera_phi_dir * delta_time * PHI_PIXELS_PER_SEC;

    if (state->should_rotate) {
        new_theta += theta_rot_dir * THETA_PIXELS_PER_SEC * delta_time;
        new_phi -= phi_rot_dir * PHI_PIXELS_PER_SEC * delta_time;
    }

    // For now, these numbers are made up... they should be adjusted later
    state->camera.rho =
        DANGEROUS_CLAMP(1.1 * CAMERA_SCREEN_DIST, new_rho, 1000.0);
    state->camera.theta = new_theta; // TODO: mod by 2pi
    state->camera.phi = DANGEROUS_CLAMP(0.0, new_phi, PI);
    if (s_update.toggle_rotate) {
        state->should_rotate = 1 - state->should_rotate;
    }
}

static void draw(Application *app) {
    State *state = &app->state;

    SDL_FPoint *projected_verts;
    float *zs;
    int *is_hull_points;
    SDL_Vertex *face_vertices;
    int visible_faces[CUBE_FACES] = {0};

    int width, height;

    SDL_GetWindowSize(app->window, &width, &height);

    if (arena_begin(app->arena) >= 0) {
        projected_verts = ARENA_PUSH_N(SDL_FPoint, app->arena, cube_vert_count);
        zs = ARENA_PUSH_N(float, app->arena, cube_vert_count);
        is_hull_points = ARENA_PUSH_N(int, app->arena, cube_vert_count);
        face_vertices =
            ARENA_PUSH_N(SDL_Vertex, app->arena, TRIANGLE_IND_COUNT);

        if (projected_verts == NULL || zs == NULL || is_hull_points == NULL ||
            face_vertices == NULL) {
            fprintf(stderr, "Could not allocate at least one array\n");
            goto skip_this;
        }

        get_point_projections(state->camera, cube_vertices, projected_verts, zs,
                              cube_vert_count);

        get_convex_hull(projected_verts, is_hull_points,
                        ARR_SIZE(cube_vertices));
        get_visible_faces(is_hull_points, zs, ARR_SIZE(cube_vertices),
                          triangle_indices_for_cube,
                          ARR_SIZE(triangle_indices_for_cube), visible_faces,
                          ARR_SIZE(visible_faces));

        if (print_a_thing) {
            Camera *camera = &state->camera;
            printf("camera = {\n"
                   "    .rho   = %f\n"
                   "    .theta = %f\n"
                   "    .phi   = %f\n"
                   "}\n"
                   "theta_rot_dir = %f\n"
                   "phi_rot_dir = %f\n",
                   camera->rho, camera->theta, camera->phi, theta_rot_dir,
                   phi_rot_dir);
        }

        uint32_t cur_face_dest = 0;
        for (int i = 0; i < CUBE_FACES; ++i) {
            if (!visible_faces[i])
                continue;

            double factor = 100.0;
            int const *cur_face_indices =
                triangle_indices_for_cube + (single_face_ind_count * i);

            for (int j = 0; j < single_face_ind_count; ++j) {
                Color const c = face_colors[i];
                int const vert_ind = cur_face_indices[j];

                SDL_Vertex *dest =
                    face_vertices + (cur_face_dest * single_face_ind_count + j);

                *dest = (SDL_Vertex){
                    .color = {.r = c.r, .g = c.g, .b = c.b, .a = 0xFF},
                    .position =
                        (SDL_FPoint){
                            .x = width / 2 +
                                 projected_verts[vert_ind].x * factor,
                            .y = height / 2 +
                                 projected_verts[vert_ind].y * factor,
                        },
                    .tex_coord = (SDL_FPoint){0}};
            };

            cur_face_dest += 1;
        }
        if (print_a_thing) {
            printf("\n");
        }

        SDL_SetRenderDrawColor(app->window_renderer, 0, 0, 0, 0xFF);
        SDL_RenderClear(app->window_renderer);
        SDL_RenderGeometry(app->window_renderer, NULL, face_vertices,
                           TRIANGLE_IND_COUNT, NULL, 0);

        SDL_RenderPresent(app->window_renderer);

    skip_this:
        arena_pop(app->arena);
    }
}

static void get_point_projections(Camera camera, V3 const *vertices,
                                  SDL_FPoint *projections, float *zs,
                                  uint32_t count) {
    V3 camera_pos = {
        .rho = camera.rho, .theta = camera.theta, .phi = camera.phi};

    V3 minus_center = polar_to_rectangular(camera_pos);
    V3 cube_center = scale(minus_center, -1.0);
    V3 unit_center = as_unit(cube_center);
    double d_over_rho = CAMERA_SCREEN_DIST / camera_pos.rho;
    double dist_sq = length_sq(cube_center);
    double numerator = d_over_rho * dist_sq;

    V3 y_dir_polar = {
        .rho = 1,
        .theta =
            camera_pos.phi >= PI_2 ? camera_pos.theta : camera_pos.theta + PI,
        .phi = camera_pos.phi >= PI_2 ? camera_pos.phi - PI_2
                                      : PI_2 - camera_pos.phi,
    };
    V3 y_dir = polar_to_rectangular(y_dir_polar);
    V3 x_dir = cross(unit_center, y_dir);

    for (int i = 0; i < count; ++i) {
        V3 corner_pos = add(cube_center, vertices[i]);
        double corner_dot_center = dot(corner_pos, cube_center);
        double scale_factor = numerator / corner_dot_center;

        V3 projected_3d = scale(corner_pos, scale_factor);
        V3 projected_2d, plane_x, plane_y;

        double x_component, y_component, z_component;

        decompose(projected_3d, unit_center, &projected_2d);
        plane_y = decompose(projected_2d, y_dir, &plane_x);

        x_component = dot(plane_x, x_dir);
        y_component = dot(plane_y, y_dir);
        z_component = -1.0f * dot(vertices[i], unit_center);

        projections[i] = (SDL_FPoint){
            .x = x_component,
            .y = y_component,
        };

        zs[i] = z_component;

        if (print_a_thing) {
            printf("\n\tpre : x = %f,\ty = %f,\t z = %f\n\tproj: x = %f,\ty = "
                   "%f\n",
                   corner_pos.x, corner_pos.y, corner_pos.z, x_component,
                   y_component);
        }
    }
    if (print_a_thing) {
        printf("y_dir = {\n"
               "    .x = %f\n"
               "    .y = %f\n"
               "    .z = %f\n"
               "}\n",
               y_dir.x, y_dir.y, y_dir.z);
    }
}

static inline double slope_of(SDL_FPoint target, SDL_FPoint base) {
    return atan2(target.y - base.y, target.x - base.x);
}

static uint32_t ind_of_closest_angle(SDL_FPoint const *projections,
                                     uint32_t count, uint32_t cur_index,
                                     double from_angle) {
    uint32_t cur_closest = (cur_index + 1) % count;
    double cur_slope =
        slope_of(projections[cur_closest], projections[cur_index]);

    cur_slope = cur_slope < from_angle ? cur_slope + TWO_PI : cur_slope;
    double distance = cur_slope - from_angle;

    for (int i = 2; i < count; ++i) {
        uint32_t vertex_ind = (cur_index + i) % count;
        double slope =
            slope_of(projections[vertex_ind], projections[cur_index]);

        slope = slope < from_angle ? slope + TWO_PI : slope;
        double new_distance = slope - from_angle;

        if (new_distance < distance) {
            cur_closest = vertex_ind;
            distance = new_distance;
        }
    }

    return cur_closest;
}

static void get_convex_hull(SDL_FPoint const *projections, int *is_hull,
                            uint32_t count) {
    uint32_t max_point_ind = 0;
    uint32_t last_ind, cur_ind;
    double cur_slope = PI_2;

    // special cases?
    if (count == 1) {
        is_hull[0] = 1;
        return;
    } else if (count == 2) {
        is_hull[0] = 1;
        is_hull[1] = 1;
        return;
    }

    for (int vertex_id = 0; vertex_id < count; ++vertex_id) {
        is_hull[vertex_id] = 0;
    }

    for (int vertex_id = 1; vertex_id < count; ++vertex_id) {
        if ((projections[vertex_id].x > projections[max_point_ind].x) ||
            ((projections[vertex_id].x == projections[max_point_ind].x) &&
             (projections[vertex_id].y > projections[max_point_ind].y))) {
            max_point_ind = vertex_id;
        }
    }

    last_ind = max_point_ind;

    is_hull[last_ind] = 1;
    cur_ind = ind_of_closest_angle(projections, count, last_ind, cur_slope);

    while (cur_ind != max_point_ind) {
        cur_slope = slope_of(projections[cur_ind], projections[last_ind]);
        last_ind = cur_ind;

        is_hull[last_ind] = 1;
        cur_ind = ind_of_closest_angle(projections, count, last_ind, cur_slope);
    }
}

static void get_visible_faces(int const *is_hull, float const *zs,
                              uint32_t vertex_count, int const *face_indices,
                              uint32_t face_index_count, int *face_visible,
                              uint32_t face_count) {
    uint32_t indices_per_face = face_index_count / face_count;

    for (int face_id = 0; face_id < face_count; ++face_id) {
        int count_vis = 0;

        for (int index_id = 0; index_id < indices_per_face; ++index_id) {
            uint32_t cur_index =
                face_indices[indices_per_face * face_id + index_id];

            if (is_hull[cur_index] || zs[cur_index] > 0) {
                ++count_vis;
            }
        }

        face_visible[face_id] = count_vis == indices_per_face;
    }
}
