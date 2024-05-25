#include <stdio.h>

#include "graphics.h"

static void loop(Application *app);
static void update(Application *app, double delta_time);
static void draw(Application *app);

#define CLEANUP(n)                                                             \
    do {                                                                       \
        ret = (n);                                                             \
        goto cleanup;                                                          \
    } while (0)

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
                        .last_ticks = SDL_GetTicks()};

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

static int r = 0;
static int g = 0;
static int b = 0;

static double x = 0.0;
static double y = 0.0;

static int xdir = 0;
static int ydir = 0;

static double target_mspf = 1000.0 / 60.0;

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

static void draw(Application *app) {
    Uint8 r_, g_, b_, a_;
    SDL_Rect rect = {.x = (int)x, .y = (int)y, .w = 50, .h = 50};

    SDL_RenderClear(app->window_renderer);

    SDL_GetRenderDrawColor(app->window_renderer, &r_, &g_, &b_, &a_);
    SDL_SetRenderDrawColor(app->window_renderer, r, g, b, 0xFF);
    SDL_RenderFillRect(app->window_renderer, &rect);
    SDL_SetRenderDrawColor(app->window_renderer, r_, g_, b_, a_);

    // SDL_Texture *texture = NULL;
    // SDL_RenderCopy(app->window_renderer, texture, NULL, NULL);

    SDL_RenderPresent(app->window_renderer);
}
