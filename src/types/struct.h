#include "value.h"

/*
 * This is the header for the struct data type in Rael.
 * A struct in Rael is a named collection of keys.
 * A struct cannot be operated on, like more complex
 * data types such as a number or a routine.
 */

typedef struct RaelStructValue {
    RAEL_VALUE_BASE;
    char *name;
} RaelStructValue;

extern RaelTypeValue RaelStructType;

RaelStructValue *struct_new(char *name);

void struct_add_entry(RaelStructValue *self, char *name, RaelValue *value);
