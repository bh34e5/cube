#ifndef CUBE_h
#define CUBE_h

#include <stdint.h>

typedef enum {
    FC_White,
    FC_Red,
    FC_Blue,
    FC_Orange,
    FC_Green,
    FC_Yellow,

    FC_Count,
} FaceColor;

typedef struct cube Cube;

Cube *new_cube(uint32_t sides);
void free_cube(Cube *cube);
void rotate_front(Cube *cube, uint32_t depth, int clockwise);
void set_facing_side(Cube *cube, FaceColor facing_side);
void set_orientation(Cube *cube, int orientation);
void print_cube(Cube *cube);

#endif // CUBE_h
