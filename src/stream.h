#ifndef RAEL_STREAM_H
#define RAEL_STREAM_H

#include <stdbool.h>
#include <stddef.h>

/* Stores information about our current character stream */
typedef struct RaelStream {
    /* Counts the amount of references this struct has */
    size_t refcount;
    /* Name of stream. This is usually the filename */
    char *name;
    /* Pointer to first char */
    char *base;
    /* Length of stream */
    size_t length;
    /* This decides whether we can `free()` this */
    bool on_heap;
} RaelStream;

typedef struct RaelStreamPtr {
    RaelStream *base;
    /* Pointer to current char */
    char *cur;
} RaelStreamPtr;


RaelStream *stream_new(char *code, size_t length, bool on_heap, char *name);

RaelStream *load_file(char* const filename);

void stream_ref(RaelStream *stream);

void stream_deref(RaelStream *stream);

void stream_ptr_construct(RaelStreamPtr *stream_ptr, RaelStream *stream, size_t idx);

void stream_ptr_destruct(RaelStreamPtr *stream_ptr);

#endif // RAEL_COMMON_H
