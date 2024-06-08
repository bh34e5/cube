#ifndef COMMON_h
#define COMMON_h

#define DCHECK(cond, ...)                                                      \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, __VA_ARGS__);                                      \
            exit(EXIT_FAILURE);                                                \
        }                                                                      \
    } while (0)

#define CLEANUP_IF(check, ...)                                                 \
    do {                                                                       \
        if (check) {                                                           \
            CLEANUP(__VA_ARGS__);                                              \
        }                                                                      \
    } while (0)

#define CLEANUP(...) _CLEANUP(__COUNTER__ + 1, __VA_ARGS__)
#define _CLEANUP(n, ...)                                                       \
    do {                                                                       \
        fprintf(stderr, __VA_ARGS__);                                          \
        ret = n;                                                               \
        goto cleanup;                                                          \
    } while (0)

#define NOT_IMPLEMENTED assert(!"Not implemented")

#define ARR_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

#endif
