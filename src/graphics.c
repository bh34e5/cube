#include "graphics.h"

#include <stdio.h>

#include "common.h"
#include "my_math.h"

static void loop(Application *app);
static void update(Application *app, StateUpdate s_update, double delta_time);
static void draw(Application *app);

static void get_point_projections(Camera camera, V3 const *vertices,
                                  SDL_Vertex *projections, uint32_t count);

static int r = 0xFF;
static int g = 0xFF;
static int b = 0xFF;

static double target_mspf = 1000.0 / 60.0;
static int print_a_thing = 0;

#define CLEANUP(n)                                                             \
    do {                                                                       \
        ret = (n);                                                             \
        goto cleanup;                                                          \
    } while (0)

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
        .rho = 10.0,
        .theta = 0.0,
        .phi = PI / 2,
    };

    return (State){.x = 0.0,
                   .y = 0.0,

                   .camera = initial_camera};
}

int graphics_main(void) {
    int ret = 0;
    Application app = {0};
    SDL_Window *window = NULL;
    SDL_Renderer *window_renderer = NULL;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "Failed to init SDL\n");
        CLEANUP(1);
    }

    window = SDL_CreateWindow("My Window", SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, 680, 480, 0);

    if (window == NULL) {
        fprintf(stderr, "Failed to create window\n");
        CLEANUP(2);
    }

    // using -1 here says to let SDL choose which driver index to use
    window_renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    if (window_renderer == NULL) {
        fprintf(stderr, "Failed to create window renderer\n");
        CLEANUP(3);
    }

    app = (Application){.window = window,
                        .window_renderer = window_renderer,
                        .last_ticks = SDL_GetTicks(),

                        .state = get_initial_state()};

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

                // zoom in/out on the cube
                if (keys[SDL_SCANCODE_I] == 1) {
                    s_update.camera_rho_dir = -1;
                    print_a_thing = 1;
                } else if (keys[SDL_SCANCODE_O] == 1) {
                    s_update.camera_rho_dir = 1;
                    print_a_thing = 1;
                }

                // move the camera up and down
                if (keys[SDL_SCANCODE_DOWN] == 1) {
                    s_update.camera_phi_dir = -1;
                    print_a_thing = 1;
                } else if (keys[SDL_SCANCODE_UP] == 1) {
                    s_update.camera_phi_dir = 1;
                    print_a_thing = 1;
                }

                // move the camera left and right
                if (keys[SDL_SCANCODE_LEFT] == 1) {
                    s_update.camera_theta_dir = -1;
                    print_a_thing = 1;
                } else if (keys[SDL_SCANCODE_RIGHT] == 1) {
                    s_update.camera_theta_dir = 1;
                    print_a_thing = 1;
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
#define RHO_PIXELS_PER_SEC 1.0
#define THETA_PIXELS_PER_SEC (2 * PI / 10)
#define PHI_PIXELS_PER_SEC (PI / 10.0)

#define DANGEROUS_MAX(a, b) ((a) > (b) ? (a) : (b))
#define DANGEROUS_MIN(a, b) ((a) < (b) ? (a) : (b))

#define DANGEROUS_CLAMP(min, n, max) (DANGEROUS_MIN(max, DANGEROUS_MAX(min, n)))

double my_dmod(double arg, double modulus) {
    return arg - (modulus * (arg / modulus));
}

static void update(Application *app, StateUpdate s_update, double delta_time) {
    State *state = &app->state;
    double new_rho, new_theta, new_phi;

    state->x += s_update.xdir * delta_time * RECT_PIXELS_PER_SEC;
    state->y += s_update.ydir * delta_time * RECT_PIXELS_PER_SEC;

    new_rho = state->camera.rho +
              s_update.camera_rho_dir * delta_time * RHO_PIXELS_PER_SEC;
    new_theta = state->camera.theta +
                s_update.camera_theta_dir * delta_time * THETA_PIXELS_PER_SEC;
    new_phi = state->camera.phi +
              s_update.camera_phi_dir * delta_time * PHI_PIXELS_PER_SEC;

    // For now, these numbers are made up... they should be adjusted later
    state->camera.rho = DANGEROUS_CLAMP(10.0, new_rho, 1000.0);
    // state->camera.theta = my_dmod(new_theta, 2 * PI);
    state->camera.theta = new_theta;
    state->camera.phi = DANGEROUS_CLAMP(0.0, new_phi, PI);
}

#define RED {.r = 0xFF, .g = 0, .b = 0, .a = 0xFF}
static SDL_Vertex vertices[] = {
    {{.x = 50, .y = 50}, RED, {0}}, //
    {{.x = 40, .y = 60}, RED, {0}}, //
    {{.x = 50, .y = 70}, RED, {0}}, //
    {{.x = 60, .y = 70}, RED, {0}}, //
    {{.x = 70, .y = 60}, RED, {0}}, //
    {{.x = 60, .y = 50}, RED, {0}}, //
};
static int vertex_count = ARR_SIZE(vertices);

static int indices[] = {
    0, 1, 2, //
    0, 2, 5, //
    2, 3, 5, //
    5, 3, 4, //
};
static int index_count = ARR_SIZE(indices);

static void draw(Application *app) {
    State *state = &app->state;
    Uint8 r_, g_, b_, a_;
    SDL_Rect rect = {.x = (int)state->x, .y = (int)state->y, .w = 50, .h = 50};

    SDL_Vertex projected_verts[ARR_SIZE(cube_vertices)] = {0};
    SDL_Point projected_points[ARR_SIZE(cube_vertices)] = {0};

    get_point_projections(state->camera, cube_vertices, projected_verts,
                          cube_vert_count);

    if (print_a_thing) {
        Camera *camera = &state->camera;
        printf("camera = {\n"
               "    .rho = %f\n"
               "    .theta = %f\n"
               "    .phi = %f\n"
               "}\n",
               camera->rho, camera->theta, camera->phi);
    }
    for (int i = 0; i < vertex_count; ++i) {
        double x = projected_verts[i].position.x;
        double y = projected_verts[i].position.y;

        if (print_a_thing) {
            printf("[%d] %f\t%f\n", i, x, y);
        }

        projected_points[i] = (SDL_Point){
            .x = 500 * x + 680 / 4,
            .y = 500 * y + 480 / 4,
        };
    }
    if (print_a_thing) {
        printf("\n");
    }

    SDL_RenderClear(app->window_renderer);

    SDL_GetRenderDrawColor(app->window_renderer, &r_, &g_, &b_, &a_);
    SDL_SetRenderDrawColor(app->window_renderer, r, g, b, 0xFF);
    SDL_RenderFillRect(app->window_renderer, &rect);

    SDL_RenderDrawPoints(app->window_renderer, projected_points, vertex_count);

    SDL_SetRenderDrawColor(app->window_renderer, r_, g_, b_, a_);

    SDL_RenderGeometry(app->window_renderer, NULL, vertices, vertex_count,
                       indices, index_count);

    // SDL_Texture *texture = NULL;
    // SDL_RenderCopy(app->window_renderer, texture, NULL, NULL);

    SDL_RenderPresent(app->window_renderer);
}

static void get_point_projections(Camera camera, V3 const *vertices,
                                  SDL_Vertex *projections, uint32_t count) {
    // double similarity_ratio = CAMERA_SCREEN_DIST / camera.rho;
    V3 camera_pos = {
        .rho = camera.rho, .theta = camera.theta, .phi = camera.phi};
    V3 cube_center = scale(polar_to_rectangular(camera_pos), -1.0);
    V3 minus_center = scale(cube_center, -1.0);
    // TODO: calculate the vector that defines the plane of the camera screen
    // (the one that is CAMERA_SCREEN_DIST away from the camera)
    V3 plane_perp = {0};
    double d = dot(cube_center, plane_perp);

    // TODO: verify this math
    V3 camera_perp = {
        .rho = camera.rho,
        .theta = camera.theta + PI,
        .phi = (PI / 2.0) - camera.phi,
    };
    V3 y_axis = polar_to_rectangular(camera_perp);
    V3 y_dir = as_unit(y_axis);

    for (int i = 0; i < count; ++i) {
        V3 corner_pos = add(cube_center, vertices[i]);
        V3 corner_dir = as_unit(corner_pos);
        V3 perp, offset;
        double corner_d = dot(corner_dir, plane_perp);
        double corner_mag;

        double x_component, y_component;
        V3 diff_in_x, diff_in_y;

        DCHECK(corner_d != 0,
               "Corner is parallel to the plane of the camera\n");

        corner_mag = d / corner_d;
        // this is the vector in the plane of the camera
        perp = scale(corner_dir, corner_mag);
        offset = add(perp, minus_center);

        // decompose(vertices[0], corner_dir, &perp);
        // perp = scale(as_unit(perp), similarity_ratio);

        // diff_in_y = decompose(perp, y_dir, &diff_in_x);

        diff_in_y = decompose(offset, y_dir, &diff_in_x);
        x_component = sqrt(length_sq(diff_in_x));
        y_component = sqrt(length_sq(diff_in_y));

        projections[i] = (SDL_Vertex){0};
        projections[i].position = (SDL_FPoint){
            .x = x_component,
            .y = y_component,
        };
    }
}
