#include "rael.h"

/*
 * This is the implementation of the :File module in Rael.
 */

enum FileOpenType {
    FileOpenRead,
    FileOpenWrite,
    FileOpenReadWrite,
    FileOpenAppend,
    FileOpenReadAppend
};

enum FileSetPositionType {
    FileSetPositionStart = SEEK_SET,
    FileSetPositionEnd = SEEK_END,
    FileSetPositionCurrent = SEEK_CUR
};

RaelTypeValue RaelFileType;

typedef struct RaelFileValue {
    RAEL_VALUE_BASE;
    FILE *stream;
    RaelStringValue *filename;

    bool is_open;
    enum FileOpenType opentype;
    bool readable;
    bool writable;
    bool is_append;
} RaelFileValue;

/* returns true if successful */
bool file_close(RaelFileValue *self) {
    if (!self->is_open)
        return false;

    fclose(self->stream);
    self->is_open = false;
    return true;
}

bool file_eq(RaelFileValue *self, RaelFileValue *value) {
    return self->stream == value->stream;
}

void file_delete(RaelFileValue *self) {
    // close if possible
    file_close(self);
    value_deref((RaelValue*)self->filename);
}

void file_repr(RaelFileValue *self) {
    printf("[");
    if (self->is_open) {
        printf("Opened");
    } else {
        printf("Closed");
    }
    printf(" file ");
    string_repr(self->filename);
    printf("]");
}

static void file_write_string(RaelFileValue *self, RaelStringValue *string) {
    char *source;
    size_t length;

    assert(self->writable);

    source = string->source;
    length = string_length(string);

    fwrite(source, sizeof(char), length, self->stream);
}

static size_t file_remaining_length(RaelFileValue *self) {
    size_t current_idx, length;

    // get the current index
    current_idx = (size_t)ftell(self->stream);
    // go to the end index to be able to get the length
    fseek(self->stream, 0, SEEK_END);
    // get the length
    length = (size_t)ftell(self->stream);
    // jump back to the original position
    fseek(self->stream, current_idx, SEEK_SET);

    return length - current_idx;
}

static char *opentype_to_string(enum FileOpenType type) {
    switch (type) {
    case FileOpenRead:
        return "rb";
    case FileOpenWrite:
        return "wb";
    case FileOpenReadWrite:
        return "w+";
    case FileOpenAppend:
        return "ab";
    case FileOpenReadAppend:
        return "ab+";
    default:
        return NULL;
    }
}

static void file_set_opentype(RaelFileValue *self, enum FileOpenType type) {
    bool readable = false,
         writable = false,
         is_append = false;

    switch (type) {
    case FileOpenWrite:
        writable = true;
        break;
    case FileOpenAppend:
        is_append = true;
        writable = true;
        break;
    case FileOpenReadAppend:
        is_append = true;
        writable = true;
        readable = true;
        break;
    case FileOpenReadWrite:
        writable = true;
        readable = true;
        break;
    case FileOpenRead:
        readable = true;
        break;
    }

    self->readable = readable;
    self->writable = writable;
    self->is_append = is_append;
    self->opentype = type;
}

static void file_set_mode_member(RaelFileValue *self) {
    RaelValue *member = number_newi((RaelInt)self->opentype);
    value_set_key((RaelValue*)self, RAEL_HEAPSTR("OpenMode"), member);
    // deref local reference
    value_deref(member);
}

RaelValue *file_construct(RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *arg1;
    RaelStringValue *string_filename;
    RaelFileValue *file;
    char *filename;
    bool defines_open_type;
    enum FileOpenType open_type;
    char *open_mode;
    FILE *stream;

    (void)interpreter;
    switch (arguments_amount(args)) {
    case 1:
      defines_open_type = false;
      break;
    case 2:
      defines_open_type = true;
      break;
    default:
        RAEL_UNREACHABLE();
    }
    // get the first argument
    arg1 = arguments_get(args, 0);
    if (arg1->type != &RaelStringType) {
        return BLAME_NEW_CSTR_ST("Expected a string", *arguments_state(args, 0));
    }

    // got 2 arguments
    if (defines_open_type) {
        RaelValue *arg2;
        RaelNumberValue *number;

        // get the second argument
        arg2 = arguments_get(args, 1);
        if (arg2->type != &RaelNumberType) {
            return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, 1));
        }
        number = (RaelNumberValue*)arg2;
        if (!number_is_whole(number)) {
            return BLAME_NEW_CSTR_ST("Expected a whole number", *arguments_state(args, 1));
        }
        if (!number_positive(number)) {
            return BLAME_NEW_CSTR_ST("Expected a positive number", *arguments_state(args, 1));
        }
        open_type = (enum FileOpenType)number_to_int(number);
    } else {
        // the default is to just read
        open_type = FileOpenRead;
    }

    open_mode = opentype_to_string(open_type);
    if (!open_mode) {
        return BLAME_NEW_CSTR_ST("Invalid open option", *arguments_state(args, 1));
    }

    // get the file name
    value_ref(arg1);
    string_filename = (RaelStringValue *)arg1;
    // we need a NUL-terminated string
    filename = string_to_cstr(string_filename);

    stream = fopen(filename, open_mode);
    if (!stream) {
        return BLAME_NEW_CSTR_ST("Could not find file", *arguments_state(args, 0));
    }
    free(filename);

    // initialize the file
    file = RAEL_VALUE_NEW(RaelFileType, RaelFileValue);
    file->filename = string_filename;
    file->stream = stream;
    file->is_open = true;
    file_set_opentype(file, open_type);

    // set the name as an inner key
    value_set_key((RaelValue*)file, RAEL_HEAPSTR("Name"), (RaelValue*)string_filename);
    // set the OpenMode member
    file_set_mode_member(file);

    return (RaelValue*)file;
}

RaelValue *file_method_read(RaelFileValue *self, RaelArgumentList *args, RaelInterpreter *interpreter) {
    size_t amount_args, read_amount;
    char *buff;

    (void)interpreter;
    amount_args = arguments_amount(args);
    switch (amount_args) {
    case 0:
        if (!self->is_open) {
            return BLAME_NEW_CSTR("File is not open for read");
        }
        if (!self->readable) {
            return BLAME_NEW_CSTR("File does not have read permission");
        }
        read_amount = file_remaining_length(self);
        break;
    case 1: {
        RaelValue *arg1 = arguments_get(args, 0);
        RaelNumberValue *number;
        size_t n;

        if (arg1->type != &RaelNumberType) {
            return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, 0));;
        }
        number = (RaelNumberValue*)arg1;
        if (!number_is_whole(number)) {
            return BLAME_NEW_CSTR_ST("Expected a whole number", *arguments_state(args, 0));;
        }
        // the number must be positive or zero
        if (!number_positive(number)) {
            return BLAME_NEW_CSTR_ST("Expected a positive number", *arguments_state(args, 0));;
        }

        n = (size_t)number_to_int(number);

        if (!self->is_open) {
            return BLAME_NEW_CSTR("File is not open for read");
        }
        if (!self->readable) {
            return BLAME_NEW_CSTR("File does not have read permission");
        }
        read_amount = file_remaining_length(self);
        // don't read more than the length of the file
        if (n < read_amount) {
            read_amount = n;
        }
        break;
    }
    default:
        RAEL_UNREACHABLE();
    }

    buff = malloc(read_amount * sizeof(char));
    fread(buff, sizeof(char), read_amount, self->stream);

    return string_new_pure(buff, read_amount, true);
}

RaelValue *file_method_write(RaelFileValue *self, RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *arg1;
    RaelStringValue *string;

    (void)interpreter;
    arg1 = arguments_get(args, 0);
    if (arg1->type != &RaelStringType) {
        return BLAME_NEW_CSTR_ST("Expected a string", *arguments_state(args, 0));
    }

    // file must be open
    if (!self->is_open) {
        return BLAME_NEW_CSTR("File is not open for read");
    }

    // file must be writable
    if (!self->writable) {
        return BLAME_NEW_CSTR_ST("File not writable", *arguments_state(args, 0));
    }
    if (self->is_append) {
        return BLAME_NEW_CSTR_ST("File does not allow writing, but allows appending. For appending to the file call :append instead",
                                 *arguments_state(args, 0));
    }

    string = (RaelStringValue*)arg1;
    file_write_string(self, string);

    return number_newi((RaelInt)string_length(string));
}

RaelValue *file_method_append(RaelFileValue *self, RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *arg1;
    RaelStringValue *string;

    (void)interpreter;
    arg1 = arguments_get(args, 0);
    if (arg1->type != &RaelStringType) {
        return BLAME_NEW_CSTR_ST("Expected a string", *arguments_state(args, 0));
    }

    // file must be open
    if (!self->is_open) {
        return BLAME_NEW_CSTR("File is not open for read");
    }

    if (!self->writable || !self->is_append) {
      return BLAME_NEW_CSTR_ST("File not appendable with such open type", *arguments_state(args, 0));
    }

    string = (RaelStringValue*)arg1;
    file_write_string(self, string);

    return number_newi((RaelInt)string_length(string));
}

RaelValue *file_method_remainingLength(RaelFileValue *self, RaelArgumentList *args, RaelInterpreter *interpreter) {
    (void)args;
    (void)interpreter;
    return number_newi((RaelInt)(file_remaining_length(self)));
}

RaelValue *file_method_getPosition(RaelFileValue *self, RaelArgumentList *args, RaelInterpreter *interpreter) {
    (void)args;
    (void)interpreter;
    return number_newi((RaelInt)ftell(self->stream));
}

static bool fileposition_validate(RaelInt n) {
    switch (n) {
    case FileSetPositionStart:
    case FileSetPositionEnd:
    case FileSetPositionCurrent:
        return true;
    default:
        return false;
    }
}

RaelValue *file_method_setPosition(RaelFileValue *self, RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *arg1;
    RaelInt position;
    enum FileSetPositionType relpos;

    (void)interpreter;
    arg1 = arguments_get(args, 0);
    if (arg1->type != &RaelNumberType || !number_is_whole((RaelNumberValue*)arg1)) {
        return BLAME_NEW_CSTR_ST("Expected a whole number", *arguments_state(args, 0));
    }
    position = number_to_int((RaelNumberValue*)arg1);

    switch (arguments_amount(args)) {
    case 1:
        relpos = FileSetPositionStart;
        break;
    case 2: {
        RaelValue *arg2 = arguments_get(args, 1);
        RaelInt n;

        if (arg2->type != &RaelNumberType) {
            return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, 1));
        }
        n = number_to_int((RaelNumberValue*)arg2);
        if (fileposition_validate(n)) {
            relpos = (enum FileSetPositionType)n;
        } else {
            return BLAME_NEW_CSTR_ST("Invalid position indicator", *arguments_state(args, 0));
        }
        break;
    }
    default:
        return BLAME_NEW_CSTR("Expected 1 or 2 arguments");
    }

    fseek(self->stream, position, relpos);
    return void_new();
}

RaelValue *file_method_close(RaelFileValue *self, RaelArgumentList *args, RaelInterpreter *interpreter) {
    (void)args;
    (void)interpreter;
    if (!file_close(self)) {
        return BLAME_NEW_CSTR("Can't close file because it is not open");
    }
    return void_new();
}

static RaelConstructorInfo file_constructor_info = {
    (RaelConstructorFunc)file_construct,
    true,
    1,
    2
};

RaelTypeValue RaelFileType = {
    RAEL_TYPE_DEF_INIT,
    .name = "FileStream",
    .op_add = NULL,
    .op_sub = NULL,
    .op_mul = NULL,
    .op_div = NULL,
    .op_mod = NULL,
    .op_red = NULL,
    .op_eq = (RaelBinCmpFunc)file_eq,
    .op_smaller = NULL,
    .op_bigger = NULL,
    .op_smaller_eq = NULL,
    .op_bigger_eq = NULL,

    .op_neg = NULL,

    .callable_info = NULL,
    .constructor_info = &file_constructor_info,
    .op_ref = NULL,
    .op_deref = NULL,

    .as_bool = NULL,
    .deallocator = (RaelSingleFunc)file_delete,
    .repr = (RaelSingleFunc)file_repr,
    .logger = NULL, /* fallbacks to .repr */

    .cast = NULL,

    .at_index = NULL,
    .at_range = NULL,

    .length = NULL,

    .methods = (MethodDecl[]) {
        RAEL_CMETHOD("close", file_method_close, 0, 0),
        RAEL_CMETHOD("read", file_method_read, 0, 1),
        RAEL_CMETHOD("write", file_method_write, 1, 1 ),
        RAEL_CMETHOD("append", file_method_append, 1, 1),
        RAEL_CMETHOD("remainingLength", file_method_remainingLength, 0, 0),
        RAEL_CMETHOD("getPosition", file_method_getPosition, 0, 0),
        RAEL_CMETHOD("setPosition", file_method_setPosition, 1, 1),
        RAEL_CMETHOD_TERMINATOR
    }
};

RaelValue *module_file_new(RaelInterpreter *interpreter) {
    RaelModuleValue *m;

    (void)interpreter;
    // create module
    m = (RaelModuleValue*)module_new(RAEL_HEAPSTR("File"));

    value_ref((RaelValue*)&RaelFileType);

    module_set_key(m, RAEL_HEAPSTR("FileStream"), (RaelValue*)&RaelFileType);
    module_set_key(m, RAEL_HEAPSTR("OpenRead"), number_newi((RaelInt)FileOpenRead));
    module_set_key(m, RAEL_HEAPSTR("OpenWrite"), number_newi((RaelInt)FileOpenWrite));
    module_set_key(m, RAEL_HEAPSTR("OpenReadWrite"), number_newi((RaelInt)FileOpenReadWrite));
    module_set_key(m, RAEL_HEAPSTR("OpenAppend"), number_newi((RaelInt)FileOpenAppend));
    module_set_key(m, RAEL_HEAPSTR("OpenReadAppend"), number_newi((RaelInt)FileOpenReadAppend));
    module_set_key(m, RAEL_HEAPSTR("SetPositionStart"), number_newi((RaelInt)FileSetPositionStart));
    module_set_key(m, RAEL_HEAPSTR("SetPositionEnd"), number_newi((RaelInt)FileSetPositionEnd));
    module_set_key(m, RAEL_HEAPSTR("SetPositionCurrent"), number_newi((RaelInt)FileSetPositionCurrent));

    return (RaelValue*)m;
}
