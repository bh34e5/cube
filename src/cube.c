#include <assert.h>
#include <stdio.h>

#include "cube.h"

static const char *face_names[FC_Count] = {
    [FC_White] = "WHITE",   [FC_Red] = "RED",     [FC_Blue] = "BLUE",
    [FC_Orange] = "ORANGE", [FC_Green] = "GREEN", [FC_Yellow] = "YELLOW",
};

#define INC_DIR(dir) (step_dir(dir, 1))
static inline FaceDirection step_dir(FaceDirection dir, int step) {
    return (dir + FD_Count + step) % FD_Count;
}

FaceDirection face_neighbor_direction(FaceColor face, FaceColor neighbor) {
    FaceColor *direction_colors = face_direction_colors[face];

    for (FaceDirection dir = 0; dir < FD_Count; ++dir) {
        FaceColor col = direction_colors[dir];
        if (col == neighbor) {
            return dir;
        }
    }
    assert(!"Invalid neighbor provided");
}

void initialize_cube(Cube *cube) {
    for (FaceColor col = 0; col < FC_Count; ++col) {
        Face *face = &cube->faces[col];
        face->color = col;
        for (FaceDirection dir = 0; dir < FD_Count; ++dir) {
            face->edge_colors[dir] = col;
            face->corner_colors[dir] = col;
        }
    }
}

void rotate(Cube *cube, FaceColor fc, int clockwise) {
    Face *face = &cube->faces[fc];

    int step = clockwise ? -1 : +1;

    { // rotate the current face
        FaceColor corner_tmp = face->corner_colors[FD_North];
        FaceColor edge_tmp = face->edge_colors[FD_North];

        FaceDirection prev_dir = FD_North;
        FaceDirection dir = step_dir(prev_dir, step);
        for (int i = 1; i < FD_Count; ++i) {
            face->corner_colors[prev_dir] = face->corner_colors[dir];
            face->edge_colors[prev_dir] = face->edge_colors[dir];

            prev_dir = dir;
            dir = step_dir(prev_dir, step);
        }

        face->corner_colors[prev_dir] = corner_tmp;
        face->edge_colors[prev_dir] = edge_tmp;
    }

    { // rotate the other faces
        FaceColor north_color = face_direction_colors[fc][FD_North];
        Face *north_face = &cube->faces[north_color];
        FaceDirection north_face_neighbor_dir =
            face_neighbor_direction(north_color, fc);

        FaceColor tmp_colors[3] = {
            north_face->corner_colors[north_face_neighbor_dir],
            north_face->edge_colors[north_face_neighbor_dir],
            north_face->corner_colors[INC_DIR(north_face_neighbor_dir)],
        };

        Face *prev_face = north_face;
        FaceDirection prev_face_neighbor_dir = north_face_neighbor_dir;
        FaceDirection dir = step_dir(FD_North, step);
        for (int i = 1; i < FD_Count; ++i) {
            FaceColor cur_col = face_direction_colors[fc][dir];
            Face *cur_face = &cube->faces[cur_col];
            FaceDirection cur_neighbor_dir =
                face_neighbor_direction(cur_col, fc);

            prev_face->corner_colors[prev_face_neighbor_dir] =
                cur_face->corner_colors[cur_neighbor_dir];
            prev_face->edge_colors[prev_face_neighbor_dir] =
                cur_face->edge_colors[cur_neighbor_dir];
            prev_face->corner_colors[INC_DIR(prev_face_neighbor_dir)] =
                cur_face->corner_colors[INC_DIR(cur_neighbor_dir)];

            prev_face = cur_face;
            prev_face_neighbor_dir = cur_neighbor_dir;
            dir = step_dir(dir, step);
        }

        prev_face->corner_colors[prev_face_neighbor_dir] = tmp_colors[0];
        prev_face->edge_colors[prev_face_neighbor_dir] = tmp_colors[1];
        prev_face->corner_colors[INC_DIR(prev_face_neighbor_dir)] =
            tmp_colors[2];
    }
}

void print_cube(Cube *cube) {
    for (FaceColor col = 0; col < FC_Count; ++col) {
        if (col != 0) {
            printf("\n");
        }

        Face *face = &cube->faces[col];

        FaceColor *cc = face->corner_colors;
        FaceColor *ec = face->edge_colors;

        printf("%s\n", face_names[col]);
        // TODO: change these to color strings later
        printf("%d %d %d\n"                              // these comments are
               "%d %d %d\n"                              // here mainly to avoid
               "%d %d %d\n",                             // auto formatting
               cc[FD_North], ec[FD_North], cc[FD_East],  // trying to change
               ec[FD_West], face->color, ec[FD_East],    // the line spacing
               cc[FD_West], ec[FD_South], cc[FD_South]); // of this print
    }
}
