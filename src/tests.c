#include <stdio.h>

#include "cube.h"
#include "tests.h"

static void fudge_cube(Cube *cube) {
    Face *f = &cube->faces[FC_White];
    for (FaceDirection d = FD_North; d < FD_Count; ++d) {
        f->corner_colors[d] = (FaceColor)d;
        f->edge_colors[d] = (FaceColor)d;
    }
}

void test_1(void) {
    Cube cube;
    initialize_cube(&cube);

    fudge_cube(&cube);

    rotate(&cube, FC_White, 1);
    rotate(&cube, FC_White, 1);
    rotate(&cube, FC_White, 0);
    rotate(&cube, FC_White, 0);
    rotate(&cube, FC_White, 1);

    print_cube(&cube);
}

void test_2(void) {
    Cube cube;
    initialize_cube(&cube);

    for (FaceColor col = 0; col < FC_Count; ++col) {
        printf("\nRotating %d\n", col);
        rotate(&cube, col, 1);
        print_cube(&cube);
        printf("\n");
        rotate(&cube, col, 0);
        print_cube(&cube);
    }
}

void test_3(void) {
    Cube cube;
    initialize_cube(&cube);

    rotate(&cube, FC_White, 1);
    rotate(&cube, FC_Red, 1);
    rotate(&cube, FC_Blue, 0);
    rotate(&cube, FC_Yellow, 0);

    print_cube(&cube);
}
