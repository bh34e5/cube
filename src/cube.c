#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cube.h"

struct cube {
    uint32_t sides;
    int orientation;
    FaceColor facing_side;
    FaceColor *squares;
};

#define DCHECK(cond, msg, ...)                                                 \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, msg, __VA_ARGS__);                                 \
            exit(EXIT_FAILURE);                                                \
        }                                                                      \
    } while (0)

#define NOT_IMPLEMENTED assert(!"Not implemented")

static void initialize_cube(Cube *cube);
static uint32_t get_face_in_dir(Cube *cube, int dir, int *from_dir);
static inline FaceColor get_at_rc(FaceColor *colors, uint32_t sides,
                                  uint32_t row, uint32_t col, int dir);
static inline void set_at_rc(FaceColor *colors, uint32_t sides, uint32_t row,
                             uint32_t col, int dir, FaceColor fc);

Cube *new_cube(uint32_t sides) {
    if (sides < 1) {
        return NULL;
    }

    Cube *res = NULL;

    res = (Cube *)malloc(sizeof(Cube));
    if (res == NULL) {
        return NULL;
    }

    uint32_t side_count = 6 * (sides * sides);
    FaceColor *colors = (FaceColor *)malloc(side_count * sizeof(FaceColor));

    if (colors == NULL) {
        free(res);
        return NULL;
    }

    *res = (Cube){
        .sides = sides,
        .orientation = 0,
        .facing_side = 0,
        .squares = colors,
    };

    initialize_cube(res);
    return res;
}

void free_cube(Cube *cube) {
    if (cube == NULL)
        return;

    free(cube->squares);
    free(cube);
}

void rotate_front(Cube *cube, uint32_t depth, int clockwise) {
    DCHECK(depth <= (cube->sides / 2),
           "Invalid rotation depth. Expected 0 <= depth <= %d, but got %d\n",
           (cube->sides / 2), depth);

    uint32_t sides = cube->sides;
    uint32_t colors_per_side = sides * sides;

    // rotate the squares on the front
    if (depth == 0) {
        FaceColor *face = cube->squares + (cube->facing_side * colors_per_side);
        for (uint32_t d = 0; d < sides / 2; ++d) {
            for (uint32_t c = d; c < (sides - 1) - d; ++c) {
                FaceColor ul = get_at_rc(face, sides, d, c, 0);
                FaceColor ur = get_at_rc(face, sides, d, c, 1);
                FaceColor br = get_at_rc(face, sides, d, c, 2);
                FaceColor bl = get_at_rc(face, sides, d, c, 3);

                FaceColor tmp = ul;
                if (clockwise) {
                    ul = bl;
                    bl = br;
                    br = ur;
                    ur = tmp;
                } else {
                    ul = ur;
                    ur = br;
                    br = bl;
                    bl = tmp;
                }

                set_at_rc(face, sides, d, c, 0, ul);
                set_at_rc(face, sides, d, c, 1, ur);
                set_at_rc(face, sides, d, c, 2, br);
                set_at_rc(face, sides, d, c, 3, bl);
            }
        }
    }

    // rotate the sides
    int nor_back_dir;
    int eas_back_dir;
    int sou_back_dir;
    int wes_back_dir;

    FaceColor nor_col = get_face_in_dir(cube, 0, &nor_back_dir);
    FaceColor eas_col = get_face_in_dir(cube, 1, &eas_back_dir);
    FaceColor sou_col = get_face_in_dir(cube, 2, &sou_back_dir);
    FaceColor wes_col = get_face_in_dir(cube, 3, &wes_back_dir);

    FaceColor *nor = cube->squares + (colors_per_side * nor_col);
    FaceColor *eas = cube->squares + (colors_per_side * eas_col);
    FaceColor *sou = cube->squares + (colors_per_side * sou_col);
    FaceColor *wes = cube->squares + (colors_per_side * wes_col);

    for (uint32_t c = 0; c < sides; ++c) {
        FaceColor nor_fc = get_at_rc(nor, sides, depth, c, nor_back_dir);
        FaceColor eas_fc = get_at_rc(eas, sides, depth, c, eas_back_dir);
        FaceColor sou_fc = get_at_rc(sou, sides, depth, c, sou_back_dir);
        FaceColor wes_fc = get_at_rc(wes, sides, depth, c, wes_back_dir);

        FaceColor tmp = nor_fc;
        if (clockwise) {
            nor_fc = wes_fc;
            wes_fc = sou_fc;
            sou_fc = eas_fc;
            eas_fc = tmp;
        } else {
            nor_fc = eas_fc;
            eas_fc = sou_fc;
            sou_fc = wes_fc;
            wes_fc = tmp;
        }

        set_at_rc(nor, sides, depth, c, nor_back_dir, nor_fc);
        set_at_rc(eas, sides, depth, c, eas_back_dir, eas_fc);
        set_at_rc(sou, sides, depth, c, sou_back_dir, sou_fc);
        set_at_rc(wes, sides, depth, c, wes_back_dir, wes_fc);
    }
}

void set_facing_side(Cube *cube, FaceColor facing_side) {
    cube->facing_side = facing_side;
}

void set_orientation(Cube *cube, int orientation) {
    cube->orientation = orientation;
}

void print_cube(Cube *cube) {
    uint32_t sides = cube->sides;
    uint32_t colors_per_side = sides * sides;
    for (FaceColor col = 0; col < FC_Count; ++col) {
        FaceColor *face = cube->squares + (col * colors_per_side);

        if (col != 0) {
            printf("\n");
        }

        for (uint32_t r = 0; r < sides; ++r) {
            for (uint32_t c = 0; c < sides; ++c) {
                FaceColor fc = face[(sides * r) + c];
                if (c == 0) {
                    printf("%d", fc);
                } else {
                    printf(" %d", fc);
                }
            }
            printf("\n");
        }
    }
}

static inline FaceColor get_at_rc(FaceColor *colors, uint32_t sides,
                                  uint32_t row, uint32_t col, int dir) {
    DCHECK(0 <= dir && dir < 4,
           "Invalid direction in getter. Expected 0 <= direction < 4, but got "
           "%d\n",
           dir);

    DCHECK(row < sides,
           "Invalid row in getter. Expected 0 <= row < %d, but got %d\n", sides,
           row);

    DCHECK(col < sides,
           "Invalid col in getter. Expected 0 <= col < %d, but got %d\n", sides,
           col);

    switch (dir) {
    case 0: {
        return colors[(sides * row) + col];
    } break;
    case 1: {
        return colors[(sides * col) + ((sides - 1) - row)];
    } break;
    case 2: {
        return colors[(sides * ((sides - 1) - row)) + ((sides - 1) - col)];
    } break;
    case 3: {
        return colors[(sides * ((sides - 1) - col)) + row];
    } break;
    default:
        assert(!"Unreachable");
    }
}

static inline void set_at_rc(FaceColor *colors, uint32_t sides, uint32_t row,
                             uint32_t col, int dir, FaceColor fc) {
    DCHECK(0 <= dir && dir < 4,
           "Invalid direction in getter. Expected 0 <= direction < 4, but got "
           "%d\n",
           dir);

    DCHECK(row < sides,
           "Invalid row in getter. Expected 0 <= row < %d, but got %d\n", sides,
           row);

    DCHECK(col < sides,
           "Invalid col in getter. Expected 0 <= col < %d, but got %d\n", sides,
           col);

    switch (dir) {
    case 0: {
        colors[(sides * row) + col] = fc;
    } break;
    case 1: {
        colors[(sides * col) + ((sides - 1) - row)] = fc;
    } break;
    case 2: {
        colors[(sides * ((sides - 1) - row)) + ((sides - 1) - col)] = fc;
    } break;
    case 3: {
        colors[(sides * ((sides - 1) - col)) + row] = fc;
    } break;
    default:
        assert(!"Unreachable");
    }
}

static void initialize_cube(Cube *cube) {
    uint32_t sides = cube->sides;
    uint32_t colors_per_side = sides * sides;

    for (FaceColor col = 0; col < FC_Count; ++col) {
        FaceColor *cur = cube->squares + (col * colors_per_side);

        for (uint32_t fc = 0; fc < colors_per_side; ++fc) {
            cur[fc] = col;
        }
    }
}

static uint32_t get_face_in_dir(Cube *cube, int dir, int *from_dir) {
#define SET_FROM_IF_PASSED(d)                                                  \
    do {                                                                       \
        if (from_dir != NULL) {                                                \
            *from_dir = (d);                                                   \
        }                                                                      \
    } while (0)

    DCHECK(0 <= dir && dir < 4,
           "Invalid direction in getter. Expected 0 <= direction < 4, but got "
           "%d\n",
           dir);

    switch (cube->facing_side) {
    case FC_White: {
        switch (dir) {
        case 0:
            SET_FROM_IF_PASSED(3);
            return FC_Red;
        case 1:
            SET_FROM_IF_PASSED(0);
            return FC_Green;
        case 2:
            SET_FROM_IF_PASSED(3);
            return FC_Orange;
        case 3:
            SET_FROM_IF_PASSED(2);
            return FC_Blue;
        default:
            assert(!"Unreachable");
        }
    } break;
    case FC_Red: {
        switch (dir) {
        case 0:
            SET_FROM_IF_PASSED(1);
            return FC_Blue;
        case 1:
            SET_FROM_IF_PASSED(2);
            return FC_Yellow;
        case 2:
            SET_FROM_IF_PASSED(1);
            return FC_Green;
        case 3:
            SET_FROM_IF_PASSED(0);
            return FC_White;
        default:
            assert(!"Unreachable");
        }
    } break;
    case FC_Blue: {
        switch (dir) {
        case 0:
            SET_FROM_IF_PASSED(3);
            return FC_Yellow;
        case 1:
            SET_FROM_IF_PASSED(0);
            return FC_Red;
        case 2:
            SET_FROM_IF_PASSED(3);
            return FC_White;
        case 3:
            SET_FROM_IF_PASSED(2);
            return FC_Orange;
        default:
            assert(!"Unreachable");
        }
    } break;
    case FC_Orange: {
        switch (dir) {
        case 0:
            SET_FROM_IF_PASSED(3);
            return FC_Green;
        case 1:
            SET_FROM_IF_PASSED(0);
            return FC_Yellow;
        case 2:
            SET_FROM_IF_PASSED(3);
            return FC_Blue;
        case 3:
            SET_FROM_IF_PASSED(2);
            return FC_White;
        default:
            assert(!"Unreachable");
        }
    } break;
    case FC_Green: {
        switch (dir) {
        case 0:
            SET_FROM_IF_PASSED(1);
            return FC_White;
        case 1:
            SET_FROM_IF_PASSED(2);
            return FC_Red;
        case 2:
            SET_FROM_IF_PASSED(1);
            return FC_Yellow;
        case 3:
            SET_FROM_IF_PASSED(0);
            return FC_Orange;
        default:
            assert(!"Unreachable");
        }
    } break;
    case FC_Yellow: {
        switch (dir) {
        case 0:
            SET_FROM_IF_PASSED(1);
            return FC_Orange;
        case 1:
            SET_FROM_IF_PASSED(2);
            return FC_Green;
        case 2:
            SET_FROM_IF_PASSED(1);
            return FC_Red;
        case 3:
            SET_FROM_IF_PASSED(0);
            return FC_Blue;
        default:
            assert(!"Unreachable");
        }
    } break;
    default:
        assert(!"Unreachable");
    }

#undef SET_FROM_IF_PASSED
}
