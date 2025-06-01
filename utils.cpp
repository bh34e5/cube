#include "includes.hpp"

GLuint compile_shader(GLenum type, char const *text) {
    GLuint shader = glCreateShader(type);

    glShaderSource(shader, 1, &text, nullptr);
    glCompileShader(shader);

    GLint status = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

    if (status != GL_TRUE) {
        GLint len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);

        char *log = new char[len];
        glGetShaderInfoLog(shader, len, nullptr, log);
        fprintf(stderr, "Shader Log: %.*s\n", len, log);
    }
    assert(status == GL_TRUE);

    return shader;
}

GLuint compile_link_program(char const *vert, char const *frag) {
    GLuint program = glCreateProgram();

    GLuint vert_shader = compile_shader(GL_VERTEX_SHADER, vert);
    GLuint frag_shader = compile_shader(GL_FRAGMENT_SHADER, frag);

    glAttachShader(program, vert_shader);
    glAttachShader(program, frag_shader);

    glLinkProgram(program);

    GLint status = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &status);

    if (status != GL_TRUE) {
        GLint len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);

        char *log = new char[len];
        glGetProgramInfoLog(program, len, nullptr, log);
        fprintf(stderr, "Program Log: %.*s\n", len, log);
    }
    assert(status == GL_TRUE);

    glDeleteShader(vert_shader);
    glDeleteShader(frag_shader);

    return program;
}

template <GLuint N>
inline GLfloat vecdot(GLfloat const lhs[N], GLfloat const rhs[N]) {
    GLfloat res = 0.0f;
    for (GLuint i = 0; i < N; ++i) {
        res += lhs[i] * rhs[i];
    }
    return res;
}

template <GLuint N>
inline GLfloat veclen(GLfloat const vec[N]) {
    return vecdot<N>(vec, vec);
}

inline void set_identity(GLfloat mat[16]) {
    static GLfloat identity[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    memmove(mat, identity, sizeof(identity));
}

inline GLfloat row_mult(GLfloat const lhs[16], GLfloat const rhs[16], GLuint r,
                        GLuint c) {
    GLfloat val = 0.0f;
    for (GLuint i = 0; i < 4; ++i) {
        val += lhs[r * 4 + i] * rhs[i * 4 + c];
    }
    return val;
}

void mat_mult(GLfloat const lhs[16], GLfloat const rhs[16], GLfloat res[16]) {
    for (GLuint r = 0; r < 4; ++r) {
        for (GLuint c = 0 ; c < 4; ++c) {
            res[r * 4 + c] = row_mult(lhs, rhs, r, c);
        }
    }
}

void mat_mult_left(GLfloat const lhs[16], GLfloat rhs[16]) {
    GLfloat res[16];
    mat_mult(lhs, rhs, res);

    memmove(rhs, res, sizeof(res));
}

template <GLuint N>
inline GLfloat row_apply(GLfloat const lhs[N * N], GLfloat const rhs[N],
                         GLuint res) {
    GLfloat val = 0.0f;
    for (GLuint i = 0; i < N; ++i) {
        val += lhs[res * N + i] * rhs[i];
    }
    return val;
}

template <GLuint N>
void mat_apply(GLfloat lhs[N * N], GLfloat rhs[N], GLfloat res[N]) {
    for (GLuint r = 0; r < N; ++r) {
        res[r] = row_apply<N>(lhs, rhs, r);
    }
}

template <GLuint N>
void mat_apply_left(GLfloat lhs[N * N], GLfloat rhs[N]) {
    GLfloat res[N];
    mat_apply<N>(lhs, rhs, res);

    memmove(rhs, res, sizeof(res));
}

void rotate_in_x(GLfloat mat[16], GLfloat delta) {
    GLfloat rotx_mat[16] = {
        1.0f, 0.0f,        0.0f,         0.0f,
        0.0f, cosf(delta), -sinf(delta), 0.0f,
        0.0f, sinf(delta), cosf(delta),  0.0f,
        0.0f, 0.0f,        0.0f,         1.0f,
    };

    mat_mult_left(mat, rotx_mat);
    memmove(mat, rotx_mat, sizeof(rotx_mat));
}

void rotate_in_y(GLfloat mat[16], GLfloat delta) {
    GLfloat roty_mat[16] = {
        cosf(delta),  0.0f, sinf(delta), 0.0f,
        0.0f,         1.0f, 0.0f,        0.0f,
        -sinf(delta), 0.0f, cosf(delta), 0.0f,
        0.0f,         0.0f, 0.0f,        1.0f,
    };

    mat_mult_left(mat, roty_mat);
    memmove(mat, roty_mat, sizeof(roty_mat));
}

void rotate_in_z(GLfloat mat[16], GLfloat delta) {
    GLfloat rotz_mat[16] = {
        cosf(delta), -sinf(delta), 0.0f, 0.0f,
        sinf(delta), cosf(delta),  0.0f, 0.0f,
        0.0f,        0.0f,         1.0f, 0.0f,
        0.0f,        0.0f,         0.0f, 1.0f,
    };

    mat_mult_left(mat, rotz_mat);
    memmove(mat, rotz_mat, sizeof(rotz_mat));
}

// NOTE: this uses some hand-rolled method of testing whether a line
// intersects with a triangle. This math is done in 3D, but apparently there is
// a pretty easy way to check in 2D by testing whether the line is on the same
// side of each of the sides of the triangle.
bool triangle_intersects(GLfloat x, GLfloat y, GLfloat z, GLfloat a[3],
                         GLfloat b[3], GLfloat c[3], GLfloat ix[3]) {
    // We have a ray from the origin, and a plane in space. If the plane went
    // through the origin as well, then the intersection point would be the
    // origin. We can define the plane with (a) and the normal of the plane
    // N = (b-a) x (c-a). We see that the vector a shifts the plane by some
    // amount in the direction of N (a . N / |N|). Additionally, (x,y,z) has
    // some component in the direction of N, and we can find that similarly as
    // ((x,y,z) . N / |N|). Then for an intersection, we would have (x,y,z) * t
    // on the plane, which means the distance in the normal direction would be
    // the same as (a), so we have t = (a . N) / ((x,y,z) . N)
    //
    // Now that we have a point on the plane, we need to see if it's in the
    // triangle. We want to find the components of (b-a) and (c-a) in
    // (intersection-a) The following matrix maps e_1 to b-a, e_2 to c-a, and
    // e_3 to their cross product. So if we take the inverse of this, and then
    // apply it to the intersection point, we should find the component we want.
    // [ (b-a)_x  (c-a)_x  cross_x ]
    // [ (b-a)_y  (c-a)_y  cross_y ]
    // [ (b-a)_z  (c-a)_z  cross_z ]
    // Using the formula A * adj(A) = det(A) * I, we need to find the adjugate
    // matrix and the determinant to get the inverse. The determinant appears to
    // just be the length of the cross product vector for some reason.

    GLfloat ray[3] = {x, y, z};

    GLfloat ba[3] = {
        b[0] - a[0],
        b[1] - a[1],
        b[2] - a[2],
    };

    GLfloat ca[3] = {
        c[0] - a[0],
        c[1] - a[1],
        c[2] - a[2],
    };

    GLfloat cross[3] = {
        +(ba[1] * ca[2] - ba[2] * ca[1]),
        -(ba[0] * ca[2] - ba[2] * ca[0]),
        +(ba[0] * ca[1] - ba[1] * ca[0]),
    };

    GLfloat det = veclen<3>(cross);
    if (fabsf(det) < 1e-3) {
        // singular matrix, we have a degenerate triangle
        return false;
    }

    GLfloat det_inv = 1.0f / det;
    GLfloat inv[9] = {
        +det_inv * (ca[1] * cross[2] - ca[2] * cross[1]),
        -det_inv * (ca[0] * cross[2] - ca[2] * cross[0]),
        +det_inv * (ca[0] * cross[1] - ca[1] * cross[0]),

        -det_inv * (ba[1] * cross[2] - ba[2] * cross[1]),
        +det_inv * (ba[0] * cross[2] - ba[2] * cross[0]),
        -det_inv * (ba[0] * cross[1] - ba[1] * cross[0]),

        +det_inv * (ba[1] * ca[2] - ba[2] * ca[1]),
        -det_inv * (ba[0] * ca[2] - ba[2] * ca[0]),
        +det_inv * (ba[0] * ca[1] - ba[1] * ca[0]),
    };

    GLfloat a_cross = vecdot<3>(a, cross);
    GLfloat xyz_cross = vecdot<3>(ray, cross);

    if (xyz_cross > 0.0f) {
        // pretty sure there is no way for this to be intersecting?
        return false;
    }

    GLfloat t = a_cross / xyz_cross;
    if (t < 0) {
        return false;
    }

    GLfloat ix_adjusted[3] = {x * t - a[0], y * t - a[1], z * t - a[2]};
    GLfloat components[3] = {};

    mat_apply<3>(inv, ix_adjusted, components);

    GLfloat x_comp = components[0];
    GLfloat y_comp = components[1];
    if (x_comp > 0 && y_comp > 0 && ((x_comp + y_comp) <= 1.0f)) {
        GLfloat unadjusted[3] = {x * t, y * t, z * t};
        memmove(ix, unadjusted, sizeof(unadjusted));

        return true;
    }
    return false;
}
