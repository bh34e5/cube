#include "graphics.h"

#include <stdio.h>

#include "common.h"
#include "my_math.h"

static void loop(Application *app);
static void update(Application *app, double delta_time);
static void draw(Application *app);

static void get_point_projections(Camera camera, V3 const *vertices,
                                  SDL_Vertex *projections, uint32_t count);

static int r = 0;
static int g = 0;
static int b = 0;

static double x = 0.0;
static double y = 0.0;

static int xdir = 0;
static int ydir = 0;

static double target_mspf = 1000.0 / 60.0;

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
                        .camera = {
                            .rho = 10.0,
                            .theta = 0.0,
                            .phi = PI / 2.0,
                        }};

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
        double cur_delta_ms;

        xdir = 0;
        ydir = 0;
        while (SDL_PollEvent(&event) > 0) {
            switch (event.type) {
            case SDL_QUIT: {
                should_not_close = 0;
            } break;
            case SDL_KEYDOWN: {
                Uint8 const *keys = SDL_GetKeyboardState(NULL);
                if (keys[SDL_SCANCODE_R] == 1) {
                    r = 0xFF - r;
                } else if (keys[SDL_SCANCODE_G] == 1) {
                    g = 0xFF - g;
                } else if (keys[SDL_SCANCODE_B] == 1) {
                    b = 0xFF - b;
                }

                if (keys[SDL_SCANCODE_W] == 1) {
                    ydir = -1;
                } else if (keys[SDL_SCANCODE_S] == 1) {
                    ydir = 1;
                }

                if (keys[SDL_SCANCODE_A] == 1) {
                    xdir = -1;
                } else if (keys[SDL_SCANCODE_D] == 1) {
                    xdir = 1;
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

        update(app, cur_delta_ms / 1000.0);
        draw(app);

        app->last_ticks = ticks;
    }
}

static void update(Application *app, double delta_time) {
    x += xdir * delta_time * 500.0;
    y += ydir * delta_time * 500.0;
}

#define RED                                                                    \
    { .r = 0xFF, .g = 0, .b = 0, .a = 0xFF }
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
    Uint8 r_, g_, b_, a_;
    SDL_Rect rect = {.x = (int)x, .y = (int)y, .w = 50, .h = 50};

    SDL_Vertex projected_verts[ARR_SIZE(cube_vertices)] = {0};
    SDL_Point projected_points[ARR_SIZE(cube_vertices)] = {0};

    get_point_projections(app->camera, cube_vertices, projected_verts,
                          cube_vert_count);

    for (int i = 0; i < vertex_count; ++i) {
        double x = projected_verts[i].position.x;
        double y = projected_verts[i].position.y;
        printf("[%d] %f\t%f\n", i, x, y);
        projected_points[i] = (SDL_Point){
            .x = x,
            .y = y,
        };
    }
    printf("\n");

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
    double similarity_ratio = CAMERA_SCREEN_DIST / camera.rho;
    V3 camera_pos = {
        .rho = camera.rho, .theta = camera.theta, .phi = camera.phi};
    V3 cube_center = scale(polar_to_rectangular(camera_pos), -1.0);

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
        V3 perp = {0};
        double x_component, y_component;
        V3 diff_in_x, diff_in_y;

        decompose(vertices[0], corner_dir, &perp);
        perp = scale(as_unit(perp), similarity_ratio);

        diff_in_y = decompose(perp, y_dir, &diff_in_x);
        x_component = sqrt(length_sq(diff_in_x));
        y_component = sqrt(length_sq(diff_in_y));

        projections[i] = (SDL_Vertex){0};
        projections[i].position = (SDL_FPoint){
            .x = x_component,
            .y = y_component,
        };
    }
}
