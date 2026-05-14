#ifndef COMMON_hh
#define COMMON_hh

#include <assert.h>

#define LEN(a) (sizeof((a)) / sizeof(*(a)))
#define SLICE(T, a) (Slice<T>{LEN((a)), (a)})

template <typename T> struct ConstifyImpl {
    using V = const T;
};

template <typename T> struct ConstifyImpl<const T> {
    using V = const T;
};

template <typename T> using Constify = typename ConstifyImpl<T>::V;

template <typename T, typename Size = unsigned int> struct Slice {
    Size len;
    T *dat;

    T &operator[](Size n) {
        assert(n < len);
        return dat[n];
    }

    Slice from(Size start) {
        assert(start <= len);

        Slice res = {};
        if (start < len) {
            res.len = len - start;
            res.dat = dat + start;
        }
        return res;
    }

    Slice until(Size end) {
        assert(end <= len);
        return {end, dat};
    }

    operator Slice<Constify<T>, Size>() {
        return {len, dat};
    }

    T *begin() {
        return dat;
    }

    T *end() {
        return dat + len;
    }

    Constify<T> *cbegin() const {
        return dat;
    }

    Constify<T> *cend() const {
        return dat + len;
    }
};

#endif // COMMON_hh
