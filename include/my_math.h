#ifndef MATH_h
#define MATH_h

#include <stdint.h>

#include "cube.h"

#define PI 3.14159265358979323846264338327950288
#define TWO_PI (2.0 * PI)
#define PI_2 (PI / 2.0)

typedef struct {
    union {
        struct {
            float x, y;
        };
        float xy[2];
    };
} V2;

typedef struct {
    union {
        struct {
            float x, y, z;
        };
        struct {
            float rho, theta, phi;
        };
        float xyz[3];
        struct {
            V2 xy;
            float __ignore__;
        };
    };
} V3;

#define V3_of(x_v, y_v, z_v) ((V3){.x = (x_v), .y = (y_v), .z = (z_v)})

#define CUBE_FACES 6
#define SQUARE_CORNERS 4

extern V3 const cube_vertices[8];
extern int const face_indices[24];
extern int const cube_vert_count;
extern int const face_index_count;

V3 point_to_face_center(V3 point);
FaceColor get_cube_face(V3 point);

#define VERTEX_COUNT_TO_TRIANGLE_COUNT(vertex_count) ((vertex_count) * 3 - 6)
void expand_vertices_to_triangles(int const *indices, uint32_t index_count,
                                  uint32_t indices_per_face, int *triangles);

V3 add3(V3 lhs, V3 rhs);
V3 scale3(V3 v, float c);

V2 add2(V2 lhs, V2 rhs);
V2 scale2(V2 v, float c);

V3 v_lerp(V3 l, float factor, V3 r);

float dot2(V2 lhs, V2 rhs);
float dot(V3 lhs, V3 rhs);
V3 cross(V3 lhs, V3 rhs);
float length_sq(V3 v);
V3 as_unit(V3 v);
V3 polar_to_rectangular(V3 v);

V3 decompose(V3 target, V3 dir, V3 *perp);
V3 complete_decomp(V3 target, V3 x, V3 y, V3 z);
V3 compose(V3 target, V3 x_dir, V3 y_dir, V3 z_dir);

#endif
