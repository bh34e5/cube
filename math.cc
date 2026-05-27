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

Matrix4 quaternionRotation(Quaternion q) {
    float r00 = 1 - 2 * (q.y * q.y + q.z * q.z);
    float r01 = 2 * (q.x * q.y - q.z * q.r);
    float r02 = 2 * (q.x * q.z + q.y * q.r);

    float r10 = 2 * (q.x * q.y + q.z * q.r);
    float r11 = 1 - 2 * (q.x * q.x + q.z * q.z);
    float r12 = 2 * (q.y * q.z - q.x * q.r);

    float r20 = 2 * (q.x * q.z - q.y * q.r);
    float r21 = 2 * (q.y * q.z + q.x * q.r);
    float r22 = 1 - 2 * (q.x * q.x + q.y * q.y);

    Matrix4 res = {
        r00, r01, r02, 0, //
        r10, r11, r12, 0, //
        r20, r21, r22, 0, //
        0,   0,   0,   1, //
    };
    return res;
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
    Vector<N> r; // no clear because we are setting every value

    for (unsigned int n = 0; n < N; ++n) {
        r.vals[n] = -v.vals[n];
    }

    return r;
}

template <unsigned int N>
Vector<N> operator+(Vector<N> const &lhs, Vector<N> const &rhs) {
    Vector<N> r; // no clear because we are setting every value

    for (unsigned int n = 0; n < N; ++n) {
        r.vals[n] = lhs.vals[n] + rhs.vals[n];
    }

    return r;
}

template <unsigned int N>
Vector<N> operator-(Vector<N> const &lhs, Vector<N> const &rhs) {
    Vector<N> r; // no clear because we are setting every value

    for (unsigned int n = 0; n < N; ++n) {
        r.vals[n] = lhs.vals[n] - rhs.vals[n];
    }

    return r;
}

template <unsigned int N> Vector<N> operator*(float a, Vector<N> const &v) {
    Vector<N> r; // no clear because we are setting every value

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

Quaternion operator-(Quaternion const &q) {
    Quaternion r = {
        -q.r,
        -q.x,
        -q.y,
        -q.z,
    };
    return r;
}

Quaternion operator+(Quaternion const &lhs, Quaternion const &rhs) {
    Quaternion q = {
        lhs.r + rhs.r,
        lhs.x + rhs.x,
        lhs.y + rhs.y,
        lhs.z + rhs.z,
    };
    return q;
}

Quaternion operator-(Quaternion const &lhs, Quaternion const &rhs) {
    Quaternion q = {
        lhs.r - rhs.r,
        lhs.x - rhs.x,
        lhs.y - rhs.y,
        lhs.z - rhs.z,
    };
    return q;
}

Quaternion operator*(float a, Quaternion &q) {
    Quaternion r = {
        a * q.r,
        a * q.x,
        a * q.y,
        a * q.z,
    };
    return r;
}

Quaternion operator*(Quaternion const &lhs, Quaternion const &rhs) {
    float lr = lhs.r;
    float rr = rhs.r;

    Vector3 lv = lhs.vector();
    Vector3 rv = rhs.vector();

    float real = lr * rr - dot(lv, rv);
    Vector3 vector = (lr * rv) + (rr * lv) + cross(lv, rv);

    Quaternion result = {
        real,
        vector.x(),
        vector.y(),
        vector.z(),
    };
    return result;
}
