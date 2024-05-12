#ifndef CUBE_h
#define CUBE_h

typedef enum {
    FC_White,
    FC_Red,
    FC_Blue,
    FC_Orange,
    FC_Green,
    FC_Yellow,

    FC_Count,
} FaceColor;

typedef enum {
    FD_North,
    FD_East,
    FD_South,
    FD_West,

    FD_Count,
} FaceDirection;

static FaceColor face_direction_colors[FC_Count][FD_Count] = {
    [FC_White] = {[FD_North] = FC_Red,
                  [FD_East] = FC_Green,
                  [FD_South] = FC_Orange,
                  [FD_West] = FC_Blue},
    [FC_Red] = {[FD_North] = FC_White,
                [FD_East] = FC_Blue,
                [FD_South] = FC_Yellow,
                [FD_West] = FC_Green},
    [FC_Blue] = {[FD_North] = FC_White,
                 [FD_East] = FC_Orange,
                 [FD_South] = FC_Yellow,
                 [FD_West] = FC_Red},
    [FC_Orange] = {[FD_North] = FC_White,
                   [FD_East] = FC_Green,
                   [FD_South] = FC_Yellow,
                   [FD_West] = FC_Blue},
    [FC_Green] = {[FD_North] = FC_White,
                  [FD_East] = FC_Red,
                  [FD_South] = FC_Yellow,
                  [FD_West] = FC_Orange},
    [FC_Yellow] = {[FD_North] = FC_Red,
                   [FD_East] = FC_Blue,
                   [FD_South] = FC_Orange,
                   [FD_West] = FC_Green},
};

typedef struct {
    FaceColor color;

    FaceColor edge_colors[FD_Count];
    // key off the face direction, and take the nearest corner,
    // counter-clockwise
    FaceColor corner_colors[FD_Count];
} Face;

typedef struct {
    Face faces[FC_Count];
} Cube;

FaceDirection face_neighbor_direction(FaceColor face, FaceColor neighbor);
void initialize_cube(Cube *cube);
void rotate(Cube *cube, FaceColor fc, int clockwise);
void print_cube(Cube *cube);

#endif // CUBE_h
