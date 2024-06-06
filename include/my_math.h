#ifndef MATH_h
#define MATH_h

#include <stdint.h>

#define PI 3.14159265358979323846264338327950288
#define TWO_PI (2.0 * PI)
#define PI_2 (PI / 2.0)

typedef struct {
    union {
        struct {
            float x, y, z;
        };
        struct {
            float rho, theta, phi;
        };
        float xyz[3];
    };
} V3;

#define V3_of(x_v, y_v, z_v) ((V3){.x = (x_v), .y = (y_v), .z = (z_v)})

extern V3 const cube_vertices[8];
extern int const face_indices[24];
extern int const cube_vert_count;
extern int const face_index_count;

#define VERTEX_COUNT_TO_TRIANGLE_COUNT(vertex_count) ((vertex_count) * 3 - 6)
void expand_vertices_to_triangles(int const *indices, uint32_t index_count,
                                  uint32_t indices_per_face, int *triangles);

V3 add(V3 lhs, V3 rhs);
V3 scale(V3 v, float c);

float dot(V3 lhs, V3 rhs);
V3 cross(V3 lhs, V3 rhs);
float length_sq(V3 v);
V3 as_unit(V3 v);
V3 polar_to_rectangular(V3 v);

V3 decompose(V3 target, V3 dir, V3 *perp);

#endif
