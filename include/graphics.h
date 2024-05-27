#ifndef GRAPHICS_h
#define GRAPHICS_h

#include <SDL2/SDL.h>

// TODO: figure out how I want to deal with distances, pixels to meters, etc.
#define CAMERA_SCREEN_DIST 1.0
typedef struct {
    double rho;
    double theta;
    double phi;
} Camera;

typedef struct {
    SDL_Window *window;
    SDL_Renderer *window_renderer;
    Uint32 last_ticks;

    Camera camera;
} Application;

int graphics_main(void);

#endif // GRAPHICS_h
