#ifndef MATH_h
#define MATH_h

#define PI 3.14159265358979323846264338327950288

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
extern int const cube_vert_count;

V3 add(V3 lhs, V3 rhs);
V3 scale(V3 v, double c);

double dot(V3 lhs, V3 rhs);
double length_sq(V3 v);
V3 as_unit(V3 v);
V3 polar_to_rectangular(V3 v);

V3 decompose(V3 target, V3 dir, V3 *perp);

#endif
