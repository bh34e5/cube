#ifndef GRAPHICS_h
#define GRAPHICS_h

#include <SDL2/SDL.h>

#include "memory.h"

// TODO: figure out how I want to deal with distances, pixels to meters, etc.
#define CAMERA_SCREEN_DIST 5.0
typedef struct {
    double rho;
    double theta;
    double phi;
} Camera;

typedef struct {
    double x, y;
    int should_rotate;
    Camera camera;
} State;

typedef struct {
    int xdir;
    int ydir;

    int toggle_rotate;
    int camera_rho_dir;
    int camera_theta_dir;
    int camera_phi_dir;
} StateUpdate;

typedef struct {
    SDL_Window *window;
    SDL_Renderer *window_renderer;
    Uint32 last_ticks;

    Arena *arena;
    State state;
} Application;

int graphics_main(void);

#endif // GRAPHICS_h
