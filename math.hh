#ifndef MATH_hh
#define MATH_hh

#include <math.h>

#define FLT_EPS (1e-4)

template <unsigned int R, unsigned int C>
struct Matrix {
    static constexpr unsigned int N = R * C;
    float vals[N];
};

template <unsigned int N>
struct Vector {
    float vals[N];
};

// specialize Vector3 to have additional accessors

template <>
struct Vector<3> {
    float vals[3];

    // regular accessors
    float &x() {
        return vals[0];
    }
    float &y() {
        return vals[1];
    }
    float &z() {
        return vals[2];
    }

    // const accessors
    float x() const {
        return vals[0];
    }
    float y() const {
        return vals[1];
    }
    float z() const {
        return vals[2];
    }

    Vector<4> hom() const {
        Vector<4> v = {};

        v.vals[0] = vals[0];
        v.vals[1] = vals[1];
        v.vals[2] = vals[2];
        v.vals[3] = 1.0;

        return v;
    }
};

struct Quaternion {
    // real part
    float r;
    // vector part
    float x;
    float y;
    float z;

    inline Vector<3> vector() const {
        Vector<3> v = {x,y,z};
        return v;
    }

    inline Quaternion conj() const {
        Quaternion c = {r,-x,-y,-z};
        return c;
    }
};

#define MAT4(x, y) ((x) * 4 + (y))
typedef Matrix<4, 4> Matrix4;
typedef Vector<3> Vector3;

Matrix4 identityMatrix4();
Matrix4 xAxisRotation(float theta);
Matrix4 yAxisRotation(float theta);
Matrix4 zAxisRotation(float theta);
Matrix4 quaternionRotation(Quaternion q);
Matrix4 translationMatr(Vector3 offset);

template <unsigned int R, unsigned int K, unsigned int C>
Matrix<R, C> operator*(Matrix<R, K> const &lhs, Matrix<K, C> const &rhs);

template <unsigned int N, unsigned int C>
Vector<N> operator*(Matrix<N, C> const &mat, Vector<C> const &v);

template <unsigned int N> Vector<N> operator-(Vector<N> const &v);
template <unsigned int N> Vector<N> operator+(Vector<N> const &lhs, Vector<N> const &rhs);
template <unsigned int N> Vector<N> operator-(Vector<N> const &lhs, Vector<N> const &rhs);
template <unsigned int N> Vector<N> operator*(float a, Vector<N> const &v);
template <unsigned int N> Vector<N> normalize(Vector<N> v);
template <unsigned int N> float dot(Vector<N> const &lhs, Vector<N> const &rhs);

template <unsigned int N>
static inline float lengthSq(Vector<N> v) { return dot(v, v); }

static inline bool floatEq(float a, float b) {
    bool eq = fabsf(a - b) < FLT_EPS;
    return eq;
}

static inline Vector3 cross(Vector3 const &lhs, Vector3 const &rhs) {
    return {
        lhs.y() * rhs.z() - lhs.z() * rhs.y(),
        lhs.z() * rhs.z() - lhs.x() * rhs.z(),
        lhs.x() * rhs.y() - lhs.y() * rhs.x(),
    };
}

Quaternion operator-(Quaternion const &q);
Quaternion operator+(Quaternion const &lhs, Quaternion const &rhs);
Quaternion operator-(Quaternion const &lhs, Quaternion const &rhs);
Quaternion operator*(float a, Quaternion &q);
Quaternion operator*(Quaternion const &lhs, Quaternion const &rhs);

#endif // MATH_hh
