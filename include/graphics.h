#ifndef GRAPHICS_h
#define GRAPHICS_h

#include <SDL2/SDL.h>

typedef struct {
    SDL_Window *window;
    SDL_Renderer *window_renderer;
    Uint32 last_ticks;
} Application;

int graphics_main(void);

#endif // GRAPHICS_h
