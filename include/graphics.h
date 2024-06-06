#ifndef GRAPHICS_h
#define GRAPHICS_h

#include <GL/glew.h>
#include <SDL2/SDL.h>

#include "cube.h"
#include "memory.h"

// TODO: figure out how I want to deal with distances, pixels to meters, etc.
#define CAMERA_SCREEN_DIST 5.0f
typedef struct {
    double rho;
    double theta;
    double phi;
} Camera;

typedef struct {
    int theta_rot_dir, phi_rot_dir;

    int should_rotate;
    int should_close;
    Camera camera;
    Cube *cube;
} State;

typedef struct {
    int toggle_rotate;
    int toggle_close;
    int camera_rho_dir;
    int camera_theta_dir;
    int camera_phi_dir;

    double delta_time;
    Uint32 ticks;
} StateUpdate;

typedef struct {
    SDL_GLContext *gl_context;
    GLuint gl_program;
    GLuint vao, vbo[2], ebo, texture;
} Application_GL_Info;

typedef struct {
    SDL_Window *window;
    Uint32 last_ticks;

    Application_GL_Info gl_info;

    Arena *arena;
    State state;
} Application;

int graphics_main(void);

#endif // GRAPHICS_h
