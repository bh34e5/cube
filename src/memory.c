#include "memory.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define MEGABYTES(n) (n << 20)

#define BASE_SIZE MEGABYTES(4)

typedef struct header {
    struct header *prev;
    char *next_byte;
} Header;

struct arena {
    Header *cur;
    char bytes[BASE_SIZE];
};

Arena *alloc_arena(void) {
    // zero allocate the information
    Arena *arena = (Arena *)calloc(1, sizeof(Arena));

    arena->cur = (Header *)&arena->bytes;
    *arena->cur = (Header){
        .prev = NULL,
        .next_byte = (char *)(arena->cur + 1),
    };

    return arena;
}

void free_arena(Arena *arena) { free(arena); }

int arena_begin(Arena *arena) {
    Header *next_header;

    if ((arena == NULL) ||
        ((arena->cur->next_byte + sizeof(Header)) - arena->bytes >=
         BASE_SIZE)) {
        return -1;
    }

    next_header = (Header *)(arena->cur->next_byte);
    *next_header = (Header){
        .prev = arena->cur,
        .next_byte = (char *)(next_header + 1),
    };
    arena->cur = next_header;

    return 0;
}

void *arena_push_size(Arena *arena, uint32_t size, int clear) {
    void *res;

    if ((arena == NULL) ||
        ((arena->cur->next_byte + size) - arena->bytes >= BASE_SIZE)) {
        return NULL;
    }

    res = (void *)arena->cur->next_byte;
    arena->cur->next_byte += size;

    if (clear) {
        memset(res, 0, size);
    }

    return res;
}

void arena_pop(Arena *arena) {
    if (arena == NULL || arena->cur == (Header *)arena->bytes) {
        // either no arena or empty arena
        // TODO: make this an error?
        return;
    }

    arena->cur = arena->cur->prev;
}
