#include "rael.h"

RaelStream *stream_new(char *code, size_t length, bool on_heap, char *name) {
    RaelStream *stream = malloc(sizeof(RaelStream));
    stream->refcount = 1;
    stream->base = code;
    stream->length = length;
    stream->on_heap = on_heap;
    stream->name = name;
    return stream;
}

void stream_ref(RaelStream *stream) {
    ++stream->refcount;
}

void stream_deref(RaelStream *stream) {
    --stream->refcount;
    if (stream->refcount == 0) {
        if (stream->on_heap)
            free(stream->base);
        free(stream);
    }
}

void stream_ptr_construct(RaelStreamPtr *stream_ptr, RaelStream *stream, size_t idx) {
    stream_ref(stream);
    stream_ptr->base = stream;
    stream_ptr->cur = &stream->base[idx];
}

void stream_ptr_destruct(RaelStreamPtr *stream_ptr) {
    stream_deref(stream_ptr->base);
}

RaelStream *rael_load_file(char* const filename) {
    FILE *file;
    char *allocated;
    size_t length;

    if (!(file = fopen(filename, "r")))
        return NULL;

    fseek(file, 0, SEEK_END);
    length = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (!(allocated = malloc((length+1) * sizeof(char))))
        return NULL;
    fread(allocated, sizeof(char), length, file);
    allocated[length] = '\0';
    fclose(file);

    return stream_new(allocated, length, true, filename);
}
