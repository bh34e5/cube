#ifndef GRAPHICS_h
#define GRAPHICS_h

#include <GL/glew.h>
#include <SDL2/SDL.h>

#include "cube.h"
#include "memory.h"
#include "my_math.h"

// TODO: figure out how I want to deal with distances, pixels to meters, etc.
#define CAMERA_SCREEN_DIST 5.0f
typedef struct {
    double rho;
    double theta;
    double phi;
} Camera;

typedef struct {
    int theta_rot_dir, phi_rot_dir;

    int rotate_depth;

    struct {
        uint32_t should_rotate : 1;
        uint32_t should_close : 1;
        uint32_t mouse_held : 1;
        uint32_t cube_intersection_found : 1;
    };

    uint32_t screen_mouse_x;
    uint32_t screen_mouse_y;

    uint32_t hold_screen_mouse_x;
    uint32_t hold_screen_mouse_y;

    uint32_t window_width;
    uint32_t window_height;

    FaceColor hover_face;
    V3 cube_intersection;

    V2 mouse;
    Camera camera;
    Cube *cube;
} State;

typedef struct {
    struct {
        uint32_t toggle_rotate : 1;
        uint32_t toggle_close : 1;
        uint32_t rotate_front : 1;
        uint32_t set_face : 1;
        uint32_t set_depth : 1;
        uint32_t mouse_moved : 1;
        uint32_t window_resized : 1;
        uint32_t toggle_mouse_click : 1;
    };

    int camera_rho_dir;
    int camera_theta_dir;
    int camera_phi_dir;

    FaceColor target_face;
    int rotate_depth;

    uint32_t mouse_x;
    uint32_t mouse_y;

    uint32_t window_width;
    uint32_t window_height;

    double delta_time;
    Uint32 ticks;
} StateUpdate;

typedef struct {
    V3 position;
    float face_num;
} VertexInformation;

typedef struct {
    float intersecting;
} VertexAttributes;

typedef struct {
    GLuint vao, vbo[2], ebo, texture;

    VertexInformation *info;
    uint32_t vertex_count;

    int *indices;
    uint32_t index_count;
} CubeModel;

typedef struct {
    SDL_GLContext *gl_context;
    GLuint gl_program;

    CubeModel cube_model;
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
