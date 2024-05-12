#ifndef TESTS_h
#define TESTS_h

#define TESTS                                                                  \
    X(test_1)                                                                  \
    X(test_2)                                                                  \
    X(test_3)

#define X(t) void t(void);
TESTS
#undef X

#endif // TESTS_h
