#ifndef COMMON_hh
#define COMMON_hh

#include <assert.h>
#include <stdlib.h>

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

template <typename T, typename Size = unsigned int> struct DList {
    Size cap;
    Size len;
    T *dat;

    T &operator[](Size n) {
        assert(n < len);
        return dat[n];
    }

    T &push(T elem) {
        ensureSize(len + 1);

        T &next = dat[len++];

        next = elem;
        return next;
    }

    void ensureSize(Size min_cap) {
        if (cap < min_cap) {
            Size next_cap = capFrom(cap, min_cap);
            T *next = (T *)realloc((void *)dat, next_cap * sizeof(T));
            assert(next != nullptr);

            cap = next_cap;
            dat = next;
        }
    }

    Size capFrom(Size from, Size target) {
        if (from == 0) {
            from = 8;
        }

        while (from < target) {
            assert(from < 2 * from);
            from = 2 * from;
        }

        return from;
    }

    void clearRetainCapacity() {
        len = 0;
    }

    void erase() {
        free((void *)dat);

        *this = {};
    }

    Slice<T, Size> items() {
        return {len, dat};
    }

    Slice<Constify<T>, Size> citems() const {
        return {len, dat};
    }
};

#endif // COMMON_hh
