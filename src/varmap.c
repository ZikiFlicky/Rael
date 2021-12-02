#include "varmap.h"
#include "value.h"

#include <stddef.h>
#include <assert.h>
#include <string.h>

void varmap_new(struct VariableMap *out) {    
    out->buckets = NULL;
    out->allocated = 0;
    out->pairs = 0;
}

/* hash according to the size of the varmap */
static size_t varmap_hash(struct VariableMap *varmap, char* const to_hash) {
    size_t value = 0;
    char *cur_idx;
    assert(varmap->allocated);

    for (cur_idx = to_hash; *cur_idx; ++cur_idx) {
        value *= 256;
        value += *cur_idx;
        value %= varmap->allocated;
    }

    return value;
}

struct BucketNode *varmap_find_key_node(struct VariableMap *varmap, char *key) {
    if (varmap->allocated == 0)
        return NULL;
    for (struct BucketNode *node = varmap->buckets[varmap_hash(varmap, key)]; node; node = node->next) {
        if (strcmp(node->key, key) == 0) {
            return node;
        }
    }
    return NULL;
}

bool varmap_set(struct VariableMap *varmap, char *key, RaelValue value, bool set_if_not_found, bool dealloc_key_on_free) {
    struct BucketNode *node, *last_node = NULL;

    if (varmap->allocated > 0) { // look for a matching bucket node
        size_t hash_result = varmap_hash(varmap, key);
        // loop all nodes in bucket and find a matching key
        for (node = varmap->buckets[hash_result]; node; node = node->next) {
            if (strcmp(node->key, key) == 0) {
                // dereference current value at that position
                value_deref(node->value);
                // if there is no ast reference to the key, deallocate its value
                if (node->dealloc_key_on_free)
                    free(node->key);
                node->key = key;
                node->value = value;
                node->dealloc_key_on_free = dealloc_key_on_free;
                return true;
            }
            last_node = node;
        }
    }

    if (!set_if_not_found)
        return false;

    if (varmap->allocated == 0)
        varmap->buckets = calloc((varmap->allocated = 8), sizeof(struct BucketNode *));

    // create new node
    node = malloc(sizeof(struct BucketNode));
    node->key = key;
    node->value = value;
    node->dealloc_key_on_free = dealloc_key_on_free;
    node->next = NULL;

    // add node
    if (last_node)
        last_node->next = node;
    else
        varmap->buckets[varmap_hash(varmap, key)] = node;

    // increment counter (to later be used for extanding map)
    ++varmap->pairs;
    return true;
}

RaelValue varmap_get(struct VariableMap *varmap, char *key) {
    if (varmap->allocated == 0)
        return NULL;
    // iterate bucket nodes and try to find a match
    for (struct BucketNode *node = varmap->buckets[varmap_hash(varmap, key)]; node; node = node->next) {
        if (strcmp(key, node->key) == 0) {
            value_ref(node->value);
            return node->value;
        }
    }
    return NULL;
}

void varmap_delete(struct VariableMap *varmap) {
    if (varmap->allocated > 0) {
        for (size_t i = 0; i < varmap->allocated; ++i) {
            struct BucketNode *next;
            for (struct BucketNode *node = varmap->buckets[i]; node; node = next) {
                value_deref(node->value);
                next = node->next;
                if (node->dealloc_key_on_free)
                    free(node->key);
                free(node);
            }
        }
        free(varmap->buckets);
    }
}
