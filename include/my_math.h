#ifndef MATH_h
#define MATH_h

#include <stdint.h>

#define PI 3.14159265358979323846264338327950288
#define TWO_PI (2.0 * PI)
#define PI_2 (PI / 2.0)

typedef struct {
    union {
        struct {
            double x, y, z;
        };
        struct {
            double rho, theta, phi;
        };
    };
} V3;

extern V3 const cube_vertices[8];
extern int const face_indices[24];
extern int const cube_vert_count;
extern int const face_index_count;

#define VERTEX_COUNT_TO_TRIANGLE_COUNT(vertex_count) ((vertex_count) * 3 - 6)
void expand_vertices_to_triangles(int const *indices, uint32_t index_count,
                                  uint32_t indices_per_face, int *triangles);

V3 add(V3 lhs, V3 rhs);
V3 scale(V3 v, double c);

double dot(V3 lhs, V3 rhs);
V3 cross(V3 lhs, V3 rhs);
double length_sq(V3 v);
V3 as_unit(V3 v);
V3 polar_to_rectangular(V3 v);

V3 decompose(V3 target, V3 dir, V3 *perp);

#endif
