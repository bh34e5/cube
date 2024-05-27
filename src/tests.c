#include "tests.h"

#include <stdio.h>

#include "cube.h"

void test_1(void) {
    Cube *cube = new_cube(3);

    rotate_front(cube, 0, 1);
    rotate_front(cube, 0, 1);
    rotate_front(cube, 0, 0);
    rotate_front(cube, 0, 0);
    rotate_front(cube, 0, 1);

    print_cube(cube);

    free_cube(cube);
}

void test_2(void) {
    Cube *cube = new_cube(3);

    for (FaceColor col = 0; col < FC_Count; ++col) {
        set_facing_side(cube, col);

        printf("\nRotating %d\n", col);
        rotate_front(cube, 0, 1);
        print_cube(cube);
        printf("\n");
        rotate_front(cube, 0, 0);
        print_cube(cube);
    }

    free_cube(cube);
}

void test_3(void) {
    Cube *cube = new_cube(3);

    rotate_front(cube, 0, 1);
    rotate_front(cube, 0, 1);
    rotate_front(cube, 0, 0);
    rotate_front(cube, 0, 0);

    print_cube(cube);

    free_cube(cube);
}
