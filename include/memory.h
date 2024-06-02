#ifndef MEMORY_h
#define MEMORY_h

#include <stdint.h>

typedef struct arena Arena;

Arena *alloc_arena(void);
void free_arena(Arena *arena);

#define ARENA_PUSH_N(type, arena, n)                                           \
    ((type *)arena_push_size((arena), ((n) * sizeof(type)), 1))

int arena_begin(Arena *arena);
void *arena_push_size(Arena *arena, uint32_t size, int clear);
void arena_pop(Arena *arena);

#endif
