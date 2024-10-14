#ifndef COMMON_h
#define COMMON_h

#define DCHECK(cond, ...)                                                      \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, __VA_ARGS__);                                      \
            exit(EXIT_FAILURE);                                                \
        }                                                                      \
    } while (0)

#define NOT_IMPLEMENTED assert(!"Not implemented")

#define ARR_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

#endif
