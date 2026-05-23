#include "math.hh"

Matrix4 identityMatrix4() {
    return {
        1.0f, 0.0f, 0.0f, 0.0f, //
        0.0f, 1.0f, 0.0f, 0.0f, //
        0.0f, 0.0f, 1.0f, 0.0f, //
        0.0f, 0.0f, 0.0f, 1.0f, //
    };
}

Matrix4 xAxisRotation(float theta) {
    Matrix4 mat = identityMatrix4();

    float ct = cos(theta);
    float st = sin(theta);

    mat.vals[MAT4(1, 1)] = +ct;
    mat.vals[MAT4(1, 2)] = +st;
    mat.vals[MAT4(2, 1)] = -st;
    mat.vals[MAT4(2, 2)] = +ct;

    return mat;
}

Matrix4 yAxisRotation(float theta) {
    Matrix4 mat = identityMatrix4();

    float ct = cos(theta);
    float st = sin(theta);

    mat.vals[MAT4(0, 0)] = +ct;
    mat.vals[MAT4(0, 2)] = -st;
    mat.vals[MAT4(2, 0)] = +st;
    mat.vals[MAT4(2, 2)] = +ct;

    return mat;
}

Matrix4 zAxisRotation(float theta) {
    Matrix4 mat = identityMatrix4();

    float ct = cos(theta);
    float st = sin(theta);

    mat.vals[MAT4(0, 0)] = +ct;
    mat.vals[MAT4(0, 1)] = +st;
    mat.vals[MAT4(1, 0)] = -st;
    mat.vals[MAT4(1, 1)] = +ct;

    return mat;
}

Matrix4 translationMatr(Vector3 offset) {
    Matrix4 mat = identityMatrix4();

    mat.vals[MAT4(0, 3)] = offset.x();
    mat.vals[MAT4(1, 3)] = offset.y();
    mat.vals[MAT4(2, 3)] = offset.z();

    return mat;
}

template <unsigned int R, unsigned int K, unsigned int C>
Matrix<R, C> operator*(Matrix<R, K> const &lhs, Matrix<K, C> const &rhs) {
#define LHS(r, c) (r * K + c)
#define RHS(r, c) (r * C + c)
#define RES(r, c) (r * C + c)

    Matrix<R, C> res; // no clear because we are setting every value

    for (unsigned int i = 0; i < R; ++i) {
        for (unsigned int j = 0; j < C; ++j) {
            float r = 0.0f;
            for (unsigned int k = 0; k < K; ++k) {
                r += lhs.vals[LHS(i, k)] * rhs.vals[RHS(k, j)];
            }

            res.vals[RES(i, j)] = r;
        }
    }

    return res;

#undef RES
#undef RHS
#undef LHS
}

template <unsigned int N, unsigned int C>
Vector<N> operator*(Matrix<N, C> const &mat, Vector<C> const &v) {
    Vector<N> res; // no clear because we are setting every value

    for (unsigned int i = 0; i < N; ++i) {
        float r = 0.0f;
        for (unsigned int c = 0; c < C; ++c) {
            r += mat.vals[(i * C + c)] * v.vals[c];
        }

        res.vals[i] = r;
    }

    return res;
}

template <unsigned int N> Vector<N> operator-(Vector<N> const &v) {
    Vector<N> r;

    for (unsigned int n = 0; n < N; ++n) {
        r.vals[n] = -v.vals[n];
    }

    return r;
}

template <unsigned int N>
Vector<N> operator+(Vector<N> const &lhs, Vector<N> const &rhs) {
    Vector<N> r;

    for (unsigned int n = 0; n < N; ++n) {
        r.vals[n] = lhs.vals[n] + rhs.vals[n];
    }

    return r;
}

template <unsigned int N>
Vector<N> operator-(Vector<N> const &lhs, Vector<N> const &rhs) {

    Vector<N> r;

    for (unsigned int n = 0; n < N; ++n) {
        r.vals[n] = lhs.vals[n] - rhs.vals[n];
    }

    return r;
}

template <unsigned int N> Vector<N> operator*(float a, Vector<N> const &v) {
    Vector<N> r;

    for (unsigned int n = 0; n < N; ++n) {
        r.vals[n] = a * v.vals[n];
    }

    return r;
}

template <unsigned int N> Vector<N> normalize(Vector<N> v) {
    Vector<N> res = {};

    float lenSq = lengthSq(v);
    if (lenSq != 0.0f) {
        res = (1 / sqrtf(lenSq)) * v;
    }

    return res;
}

template <unsigned int N>
float dot(Vector<N> const &lhs, Vector<N> const &rhs) {
    float r = 0;

    for (unsigned int n = 0; n < N; ++n) {
        r += lhs.vals[n] * rhs.vals[n];
    }

    return r;
}
