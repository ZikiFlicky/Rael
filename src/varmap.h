#ifndef RAEL_VARMAP_H
#define RAEL_VARMAP_H

#include <stddef.h>
#include <stdbool.h>

typedef struct RaelValue* RaelValue;

struct VariableMap {
    struct BucketNode {
        struct BucketNode *next;
        bool dealloc_key_on_free;
        char *key;
        RaelValue value;
    } **buckets;
    size_t allocated, pairs;
};

void varmap_new(struct VariableMap *out);

bool varmap_set(struct VariableMap *varmap, char *key, RaelValue value, bool set_if_not_found, bool dealloc_key_on_free);

RaelValue *varmap_get_ptr(struct VariableMap *varmap, char *key);

RaelValue varmap_get(struct VariableMap *varmap, char *key);

void varmap_delete(struct VariableMap *varmap);

#endif /* RAEL_VARMAP_H */