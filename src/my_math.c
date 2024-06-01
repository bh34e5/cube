#include "my_math.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#define V3_of(x_v, y_v, z_v) ((V3){.x = (x_v), .y = (y_v), .z = (z_v)})

V3 const cube_vertices[8] = {
    {.x = (+1.0), .y = (+1.0), .z = (+1.0)},
    {.x = (-1.0), .y = (+1.0), .z = (+1.0)},
    {.x = (-1.0), .y = (-1.0), .z = (+1.0)},
    {.x = (+1.0), .y = (-1.0), .z = (+1.0)},
    {.x = (+1.0), .y = (-1.0), .z = (-1.0)},
    {.x = (-1.0), .y = (-1.0), .z = (-1.0)},
    {.x = (-1.0), .y = (+1.0), .z = (-1.0)},
    {.x = (+1.0), .y = (+1.0), .z = (-1.0)},
};

// TODO: map the faces with the colors...
int const face_indices[6 * 4] = {
    0, 1, 2, 3, //
    2, 3, 4, 5, //
    1, 2, 5, 6, //
    0, 1, 6, 7, //
    4, 5, 6, 7, //
    0, 3, 4, 7, //
};

int const cube_vert_count = ARR_SIZE(cube_vertices);
int const face_index_count = ARR_SIZE(face_indices);

void expand_vertices_to_triangles(int const *indices, uint32_t index_count,
                                  uint32_t indices_per_face, int *triangles) {
    uint32_t face_count = index_count / indices_per_face;
    uint32_t triangle_indices_per_face =
        VERTEX_COUNT_TO_TRIANGLE_COUNT(indices_per_face);

    for (uint32_t face_id = 0; face_id < face_count; ++face_id) {
        int f_base = face_id * indices_per_face;
        int tf_base = face_id * triangle_indices_per_face;
        int last_end = 2;

        triangles[tf_base + 0] = indices[f_base + 0];
        triangles[tf_base + 1] = indices[f_base + 1];
        triangles[tf_base + 2] = indices[f_base + 2];

        for (int i = 1; i < index_count; ++i) {
            int offset = 3 * i;

            triangles[tf_base + offset + 0] = indices[f_base + 0];
            triangles[tf_base + offset + 1] = indices[f_base + last_end + 0];
            triangles[tf_base + offset + 2] = indices[f_base + last_end + 1];

            last_end = last_end + 1;
        }
    }
}

inline V3 add(V3 lhs, V3 rhs) {
    return (V3){
        .x = lhs.x + rhs.x,
        .y = lhs.y + rhs.y,
        .z = lhs.z + rhs.z,
    };
}

inline V3 scale(V3 v, double c) {
    return (V3){
        .x = v.x * c,
        .y = v.y * c,
        .z = v.z * c,
    };
}

inline double dot(V3 lhs, V3 rhs) {
    return (lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z);
}

inline V3 cross(V3 lhs, V3 rhs) {
    return (V3){
        .x = +1.0 * (lhs.y * rhs.z - lhs.z * rhs.y),
        .y = -1.0 * (lhs.x * rhs.z - lhs.z * rhs.x),
        .z = +1.0 * (lhs.x * rhs.y - lhs.y * rhs.x),
    };
}

inline double length_sq(V3 v) { return dot(v, v); }

inline V3 as_unit(V3 v) {
    double length_inv = 1.0 / sqrt(length_sq(v));
    return (V3){
        .x = v.x * length_inv,
        .y = v.y * length_inv,
        .z = v.z * length_inv,
    };
}

inline V3 polar_to_rectangular(V3 v) {
    return (V3){.x = v.rho * cos(v.theta) * sin(v.phi),
                .y = v.rho * sin(v.theta) * sin(v.phi),
                .z = v.rho * cos(v.phi)};
}

V3 decompose(V3 target, V3 dir, V3 *perp) {
    double dotted = dot(target, dir);
    V3 res = scale(dir, dotted);

    if (perp != NULL) {
        V3 diff = add(target, scale(res, -1.0));
        *perp = diff;
    }

    return res;
}
