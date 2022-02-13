#include "rael.h"

RaelStructValue *struct_new(char *name) {
    RaelStructValue *self = RAEL_VALUE_NEW(RaelStructType, RaelStructValue);
    self->name = name;
    return self;
}

void struct_add_entry(RaelStructValue *self, char *name, RaelValue *value) {
    value_set_key((RaelValue*)self, name, value, true);
}

void struct_repr(RaelStructValue *self) {
    const struct VariableMap *map = &((RaelValue*)self)->keys;
    size_t pairs_left = map->pairs;

    printf("[Struct %s { ", self->name);
    for (size_t i = 0; pairs_left > 0 && i < map->allocated; ++i) {
        for (struct BucketNode *bucket_ptr = map->buckets[i]; bucket_ptr; bucket_ptr = bucket_ptr->next) {
            if (pairs_left < map->pairs)
                printf(", ");
            printf(":%s ?= ", bucket_ptr->key);
            value_repr(bucket_ptr->value);
            --pairs_left;
        }
    }
    printf(" }]");
}

size_t struct_length(RaelStructValue *self) {
    return ((RaelValue*)self)->keys.pairs;
}

void struct_delete(RaelStructValue *self) {
    free(self->name);
}

RaelTypeValue RaelStructType = {
    RAEL_TYPE_DEF_INIT,
    .name = "Struct",
    .op_add = NULL,
    .op_sub = NULL,
    .op_mul = NULL,
    .op_div = NULL,
    .op_mod = NULL,
    .op_red = NULL,
    .op_eq = NULL,
    .op_smaller = NULL,
    .op_bigger = NULL,
    .op_smaller_eq = NULL,
    .op_bigger_eq = NULL,

    .op_neg = NULL,

    .callable_info = NULL,
    .constructor_info = NULL,
    .op_ref = NULL,
    .op_deref = NULL,

    .as_bool = NULL,
    .deallocator = (RaelSingleFunc)struct_delete,
    .repr = (RaelSingleFunc)struct_repr,
    .logger = NULL, /* fallbacks to .repr */

    .cast = NULL,

    .at_index = NULL,
    .at_range = NULL,

    .length = (RaelLengthFunc)struct_length,

    .methods = NULL
};
