#include "rael.h"

#include <SDL2/SDL.h>

/*
 * This is the source code for the :Graphics library in Rael.
 * This library wraps SDL2 into a Rael library and allows interacting with
 * otherwise unreachable system components such as the mouse, the keyboard or
 * just a window.
 */

typedef struct WindowValue {
    RAEL_VALUE_BASE;
    SDL_Window *window;
    size_t width, height;
} WindowValue;
RaelTypeValue WindowType;

typedef struct SurfaceValue {
    RAEL_VALUE_BASE;
    WindowValue *window_ref;
    SDL_Surface *surface;
} SurfaceValue;
RaelTypeValue SurfaceType;

typedef struct ColorValue {
    RAEL_VALUE_BASE;
    uint8_t r, g, b;
} ColorValue;
RaelTypeValue ColorType;

/* Window type definition */

RaelValue *window_construct(RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *arg1, *arg2, *arg3;
    RaelNumberValue *width_number, *height_number;

    char *title;
    RaelInt width, height;

    WindowValue *window_value;
    SDL_Window *window;

    (void)interpreter;
    assert(arguments_amount(args) == 3);
    // argument for name
    arg1 = arguments_get(args, 0);
    if (arg1->type != &RaelStringType) {
        return BLAME_NEW_CSTR_ST("Expected a string", *arguments_state(args, 0));
    }

    arg2 = arguments_get(args, 1);
    if (arg2->type != &RaelNumberType) {
        return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, 1));
    }
    width_number = (RaelNumberValue*)arg2;
    if (!number_is_whole(width_number)) {
        return BLAME_NEW_CSTR_ST("Expected a whole number", *arguments_state(args, 1));
    }
    if (!number_positive(width_number)) {
        return BLAME_NEW_CSTR_ST("Expected a positive number", *arguments_state(args, 1));
    }

    arg3 = arguments_get(args, 2);
    if (arg3->type != &RaelNumberType) {
        return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, 2));
    }
    height_number = (RaelNumberValue*)arg3;
    if (!number_is_whole(height_number)) {
        return BLAME_NEW_CSTR_ST("Expected a whole number", *arguments_state(args, 2));
    }
    if (!number_positive(height_number)) {
        return BLAME_NEW_CSTR_ST("Expected a positive number", *arguments_state(args, 2));
    }

    title = string_to_cstr((RaelStringValue*)arg1);
    width = number_to_int(width_number);
    height = number_to_int(height_number);

    window = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_SHOWN);
    window_value = RAEL_VALUE_NEW(WindowType, WindowValue);
    window_value->window = window;
    window_value->width = width;
    window_value->height = height;

    /* Store the width and height in the window value */
    value_set_key((RaelValue*)window_value, RAEL_HEAPSTR("Width"), number_newi(width), true);
    value_set_key((RaelValue*)window_value, RAEL_HEAPSTR("Height"), number_newi(height), true);

    free(title);
    return (RaelValue*)window_value;
}

void window_delete(WindowValue *self) {
    SDL_DestroyWindow(self->window);
}

void window_repr(WindowValue *self) {
    printf("[Graphics Window %zux%zu]", self->width, self->height);
}

RaelValue *window_method_getSurface(WindowValue *self, RaelArgumentList *args, RaelInterpreter *interpreter) {
    SurfaceValue *surface = RAEL_VALUE_NEW(SurfaceType, SurfaceValue);

    (void)args;
    (void)interpreter;
    value_ref((RaelValue*)self);
    surface->window_ref = self;
    surface->surface = SDL_GetWindowSurface(self->window);

    return (RaelValue*)surface;
}

static RaelConstructorInfo window_constructor_info = {
    (RaelConstructorFunc)window_construct,
    true,
    3,
    3
};

RaelTypeValue WindowType = {
    RAEL_TYPE_DEF_INIT,
    .name = "Window",
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
    .constructor_info = &window_constructor_info,
    .op_ref = NULL,
    .op_deref = NULL,

    .as_bool = NULL,
    .deallocator = (RaelSingleFunc)window_delete,
    .repr = (RaelSingleFunc)window_repr,
    .logger = NULL, /* fallbacks to .repr */

    .cast = NULL,

    .at_index = NULL,
    .at_range = NULL,

    .length = NULL,

    .methods = (MethodDecl[]) {
        RAEL_CMETHOD("getSurface", window_method_getSurface, 0, 0),
        RAEL_CMETHOD_TERMINATOR
    }
};

/* Surface type definition */

void surface_delete(SurfaceValue *self) {
    value_deref((RaelValue*)self->window_ref);
    SDL_FreeSurface(self->surface);
}

/*
 * Returns true if a point can be accessed in a surface.
 * The can_be_length parameter tells whether the function
 * should return true when y == height or x == width.
 */
static bool surface_point_valid(SurfaceValue *self, RaelInt x, RaelInt y, bool can_be_length) {
    if (can_be_length) {
        return x >= 0 && x <= self->surface->w && y >= 0 && y <= self->surface->h;
    } else {
        return x >= 0 && x < self->surface->w && y >= 0 && y < self->surface->h;
    }
}

static void surface_draw_pixel(SurfaceValue *self, size_t x, size_t y, ColorValue *color) {
    uint32_t computed_color;
    uint8_t *pixel_ptr;
    size_t bpp = self->surface->format->BytesPerPixel;

    assert(surface_point_valid(self, (RaelInt)x, (RaelInt)y, false));

    pixel_ptr = &((uint8_t*)self->surface->pixels)[y * self->surface->pitch + x * bpp];
    computed_color = SDL_MapRGB(self->surface->format, color->r, color->g, color->b);

    switch (bpp) {
    case 1:
      *pixel_ptr = computed_color;
      break;
    case 2:
      *(uint16_t*)pixel_ptr = computed_color;
      break;
    case 3:
        for (int i = 0; i < 3; ++i) {
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
            pixel_ptr[i] = computed_color >> ((3 - i - 1) * 8) & 0xff;
#else
            pixel_ptr[i] = computed_color >> (i * 8) & 0xff;
#endif
        }
        break;
    case 4:
        *(uint32_t*)pixel_ptr = computed_color;
        break;
    default:
        RAEL_UNREACHABLE();
    }
}

/* fill a rectangle of a certain size in a certain place with a solid color */
static void surface_draw_rect(SurfaceValue *self, size_t x, size_t y, size_t w, size_t h, ColorValue *color) {
    for (size_t y2 = 0; y2 < h; ++y2) {
        for (size_t x2 = 0; x2 < w; ++x2) {
            surface_draw_pixel(self, x + x2, y + y2, color);
        }
    }
}

/* draw a line */
static void surface_draw_line(SurfaceValue *self, size_t x1, size_t y1, size_t x2, size_t y2, ColorValue *color) {
    RaelInt length_x = (RaelInt)x2 - (RaelInt)x1;
    RaelInt length_y = (RaelInt)y2 - (RaelInt)y1;
    RaelFloat length = sqrt(length_x * length_x + length_y * length_y);

    for (size_t i = 0; i < length; ++i) {
        surface_draw_pixel(self,
                        x1 + (RaelInt)(i * length_x / length),
                        y1 + (RaelInt)(i * length_y / length),
                        color);
    }
}

RaelValue *surface_method_drawPixel(SurfaceValue *self, RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *arg;
    RaelNumberValue *number;
    ColorValue *color;
    RaelInt x, y;

    (void)interpreter;
    assert(arguments_amount(args) == 3);

    arg = arguments_get(args, 0);
    if (arg->type != &RaelNumberType)
        return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, 0));
    number = (RaelNumberValue*)arg;
    if (!number_is_whole(number)) {
        return BLAME_NEW_CSTR_ST("Expected a whole number", *arguments_state(args, 0));
    }
    x = number_to_int(number);

    arg = arguments_get(args, 1);
    if (arg->type != &RaelNumberType)
        return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, 1));
    number = (RaelNumberValue*)arg;
    if (!number_is_whole(number))
        return BLAME_NEW_CSTR_ST("Expected a whole number", *arguments_state(args, 1));
    y = number_to_int(number);

    arg = arguments_get(args, 2);
    if (arg->type != &ColorType) {
        return BLAME_NEW_CSTR_ST("Expected a color value", *arguments_state(args, 2));
    }
    color = (ColorValue*)arg;

    if (surface_point_valid(self, x, y, false))
        return BLAME_NEW_CSTR("Invalid coordinates");
    surface_draw_pixel(self, x, y, color);

    return void_new();
}

RaelValue *surface_method_drawRect(SurfaceValue *self, RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *arg;
    RaelNumberValue *number;
    ColorValue *color;
    RaelInt x, y, width, height;

    (void)interpreter;
    assert(arguments_amount(args) == 5);

    // get x
    arg = arguments_get(args, 0);
    if (arg->type != &RaelNumberType) {
        return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, 0));
    }
    number = (RaelNumberValue*)arg;
    if (!number_is_whole(number)) {
        return BLAME_NEW_CSTR_ST("Expected a whole number", *arguments_state(args, 0));
    }
    x = number_to_int(number);

    // get y
    arg = arguments_get(args, 1);
    if (arg->type != &RaelNumberType) {
        return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, 1));
    }
    number = (RaelNumberValue*)arg;
    if (!number_is_whole(number) || !number_positive(number)) {
        return BLAME_NEW_CSTR_ST("Expected a whole number", *arguments_state(args, 1));
    }
    y = number_to_int(number);

    // get width
    arg = arguments_get(args, 2);
    if (arg->type != &RaelNumberType) {
        return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, 2));
    }
    number = (RaelNumberValue*)arg;
    if (!number_is_whole(number) || !number_positive(number)) {
        return BLAME_NEW_CSTR_ST("Expected a whole positive number", *arguments_state(args, 2));
    }
    width = number_to_int(number);

    // get height
    arg = arguments_get(args, 3);
    if (arg->type != &RaelNumberType)
        return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, 3));
    number = (RaelNumberValue*)arg;
    if (!number_is_whole(number) || !number_positive(number))
        return BLAME_NEW_CSTR_ST("Expected a whole positive number", *arguments_state(args, 3));
    height = number_to_int(number);

    // get color
    arg = arguments_get(args, 4);
    if (arg->type != &ColorType) {
        return BLAME_NEW_CSTR_ST("Expected a color value", *arguments_state(args, 4));
    }
    color = (ColorValue*)arg;

    if (!surface_point_valid(self, x, y, true) || !surface_point_valid(self, x + width, y + height, true))
        return BLAME_NEW_CSTR("Invalid rect");

    // finally draw the rect
    surface_draw_rect(self, x, y, width, height, color);

    return void_new();
}

/* fill a whole surface with a solid color */
RaelValue *surface_method_fill(SurfaceValue *self, RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *arg;
    ColorValue *color;

    (void)interpreter;
    assert(arguments_amount(args) == 1);

    arg = arguments_get(args, 0);
    if (arg->type != &ColorType)
        return BLAME_NEW_CSTR_ST("Expected a color value", *arguments_state(args, 0));
    color = (ColorValue*)arg;

    surface_draw_rect(self, 0, 0, (size_t)self->surface->w, (size_t)self->surface->h, color);
    return void_new();
}

RaelValue *surface_method_drawLine(SurfaceValue *self, RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *arg;
    RaelNumberValue *number;
    ColorValue *color;
    RaelInt x1, y1, x2, y2;

    (void)interpreter;
    assert(arguments_amount(args) == 5);

    // get x
    arg = arguments_get(args, 0);
    if (arg->type != &RaelNumberType) {
        return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, 0));
    }
    number = (RaelNumberValue*)arg;
    if (!number_is_whole(number) || !number_positive(number)) {
        return BLAME_NEW_CSTR_ST("Expected a whole positive number", *arguments_state(args, 0));
    }
    x1 = number_to_int(number);

    // get y
    arg = arguments_get(args, 1);
    if (arg->type != &RaelNumberType) {
        return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, 1));
    }
    number = (RaelNumberValue*)arg;
    if (!number_is_whole(number) || !number_positive(number)) {
        return BLAME_NEW_CSTR_ST("Expected a whole positive number", *arguments_state(args, 1));
    }
    y1 = number_to_int(number);

    // get width
    arg = arguments_get(args, 2);
    if (arg->type != &RaelNumberType) {
        return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, 2));
    }
    number = (RaelNumberValue*)arg;
    if (!number_is_whole(number)) {
        return BLAME_NEW_CSTR_ST("Expected a whole number", *arguments_state(args, 2));
    }
    x2 = number_to_int(number);

    // get height
    arg = arguments_get(args, 3);
    if (arg->type != &RaelNumberType) {
        return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, 3));
    }
    number = (RaelNumberValue*)arg;
    if (!number_is_whole(number)) {
        return BLAME_NEW_CSTR_ST("Expected a whole number", *arguments_state(args, 3));
    }
    y2 = number_to_int(number);

    // get color
    arg = arguments_get(args, 4);
    if (arg->type != &ColorType)
        return BLAME_NEW_CSTR_ST("Expected a color value", *arguments_state(args, 4));
    color = (ColorValue*)arg;

    if (!surface_point_valid(self, x1, y1, true) || !surface_point_valid(self, x2, y2, true))
        return BLAME_NEW_CSTR("Invalid coordinates");

    // finally draw the line
    surface_draw_line(self, x1, y1, x2, y2, color);

    return void_new();
}

RaelValue *surface_method_update(SurfaceValue *self, RaelArgumentList *args, RaelInterpreter *interpreter) {
    (void)interpreter;
    assert(arguments_amount(args) == 0);

    SDL_UpdateWindowSurface(self->window_ref->window);
    return void_new();
}

RaelTypeValue SurfaceType = {
    RAEL_TYPE_DEF_INIT,
    .name = "Surface",
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
    .deallocator = NULL,
    .repr = NULL,
    .logger = NULL,

    .cast = NULL,

    .at_index = NULL,
    .at_range = NULL,

    .length = NULL,

    .methods = (MethodDecl[]) {
        RAEL_CMETHOD("drawPixel", surface_method_drawPixel, 3, 3),
        RAEL_CMETHOD("update", surface_method_update, 0, 0),
        RAEL_CMETHOD("drawRect", surface_method_drawRect, 5, 5),
        RAEL_CMETHOD("drawLine", surface_method_drawLine, 5, 5),
        RAEL_CMETHOD("fill", surface_method_fill, 1, 1),
        RAEL_CMETHOD_TERMINATOR
    }
};

RaelValue *color_new(uint8_t r, uint8_t g, uint8_t b) {
    ColorValue *self = RAEL_VALUE_NEW(ColorType, ColorValue);
    self->r = r;
    self->g = g;
    self->b = b;
    value_set_key((RaelValue*)self, RAEL_HEAPSTR("R"), number_newi(r), true);
    value_set_key((RaelValue*)self, RAEL_HEAPSTR("G"), number_newi(g), true);
    value_set_key((RaelValue*)self, RAEL_HEAPSTR("B"), number_newi(b), true);
    return (RaelValue*)self;
}

RaelValue *color_construct(RaelArgumentList *args, RaelInterpreter *interpreter) {
    RaelValue *arg;
    RaelNumberValue *number;
    RaelInt r, g, b;

    (void)interpreter;
    assert(arguments_amount(args) == 3);

    // get red
    arg = arguments_get(args, 0);
    if (arg->type != &RaelNumberType)
        return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, 0));
    number = (RaelNumberValue*)arg;
    if (!number_is_whole(number))
        return BLAME_NEW_CSTR_ST("Expected a whole number", *arguments_state(args, 0));
    r = number_to_int(number);

    // get green
    arg = arguments_get(args, 1);
    if (arg->type != &RaelNumberType)
        return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, 1));
    number = (RaelNumberValue*)arg;
    if (!number_is_whole(number))
        return BLAME_NEW_CSTR_ST("Expected a whole number", *arguments_state(args, 1));
    g = number_to_int(number);

    // get blue
    arg = arguments_get(args, 2);
    if (arg->type != &RaelNumberType)
        return BLAME_NEW_CSTR_ST("Expected a number", *arguments_state(args, 2));
    number = (RaelNumberValue*)arg;
    if (!number_is_whole(number))
        return BLAME_NEW_CSTR_ST("Expected a whole number", *arguments_state(args, 2));
    b = number_to_int(number);

    if (r < 0 || r > UINT8_MAX) {
        return BLAME_NEW_CSTR_ST("Expected a whole number between 0 and 255", *arguments_state(args, 0));
    }

    if (g < 0 || g > UINT8_MAX)
        return BLAME_NEW_CSTR_ST("Expected a whole number between 0 and 255",
                                *arguments_state(args, 1));

    if (b < 0 || b > UINT8_MAX)
        return BLAME_NEW_CSTR_ST("Expected a whole number between 0 and 255",
                                *arguments_state(args, 2));

    return color_new((uint8_t)r, (uint8_t)g, (uint8_t)b);
}

void color_repr(ColorValue *self) {
    printf("[Color %d, %d, %d]", self->r, self->g, self->b);
}

RaelConstructorInfo color_constructor_info = {
    color_construct,
    true,
    3,
    3
};

RaelTypeValue ColorType = {
    RAEL_TYPE_DEF_INIT,
    .name = "Color",
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
    .constructor_info = &color_constructor_info,
    .op_ref = NULL,
    .op_deref = NULL,

    .as_bool = NULL,
    .deallocator = NULL,
    .repr = (RaelSingleFunc)color_repr,
    .logger = NULL, /* fallbacks to .repr */

    .cast = NULL,

    .at_index = NULL,
    .at_range = NULL,

    .length = NULL,

    .methods = NULL
};

RaelValue *module_graphics_Init(RaelArgumentList *args, RaelInterpreter *interpreter) {
    (void)interpreter;
    assert(arguments_amount(args) == 0);
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS);
    return void_new();
}

RaelValue *module_graphics_Deinit(RaelArgumentList *args, RaelInterpreter *interpreter) {
    (void)interpreter;
    assert(arguments_amount(args) == 0);
    SDL_Quit();
    return void_new();
}

RaelValue *module_graphics_GetEvent(RaelArgumentList *args, RaelInterpreter *interpreter) {
    SDL_Event event;

    (void)interpreter;
    assert(arguments_amount(args) == 0);

    if (SDL_PollEvent(&event)) {
        RaelStructValue *event_struct = struct_new(RAEL_HEAPSTR("Event"));

        struct_add_entry(event_struct, RAEL_HEAPSTR("Type"), number_newi(event.type));
        switch (event.type) {
        case SDL_KEYUP:
        case SDL_KEYDOWN: {
            RaelStructValue *key_info = struct_new(RAEL_HEAPSTR("KeyboardEventInfo"));
            struct_add_entry(key_info, RAEL_HEAPSTR("KeyCode"), number_newi(event.key.keysym.sym));
            struct_add_entry(key_info, RAEL_HEAPSTR("Repeat"), number_newi(event.key.repeat != 0));

            // add to event struct
            struct_add_entry(event_struct, RAEL_HEAPSTR("KeyboardEventInfo"), (RaelValue*)key_info);
            break;
        }
        case SDL_MOUSEMOTION: {
            RaelStructValue *motion_info = struct_new(RAEL_HEAPSTR("MouseMotionEventInfo"));
            struct_add_entry(motion_info, RAEL_HEAPSTR("X"), number_newi(event.motion.x));
            struct_add_entry(motion_info, RAEL_HEAPSTR("Y"), number_newi(event.motion.y));
            struct_add_entry(motion_info, RAEL_HEAPSTR("RelX"), number_newi(event.motion.xrel));
            struct_add_entry(motion_info, RAEL_HEAPSTR("RelX"), number_newi(event.motion.yrel));

            // add to event struct
            struct_add_entry(event_struct, RAEL_HEAPSTR("MouseMotionEventInfo"), (RaelValue*)motion_info);
            break;
        }
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
            RaelStructValue *mouse_info = struct_new(RAEL_HEAPSTR("MouseClickEventInfo"));
            struct_add_entry(mouse_info, RAEL_HEAPSTR("ButtonType"), number_newi(event.button.button));
            struct_add_entry(mouse_info, RAEL_HEAPSTR("AmountClicks"), number_newi(event.button.clicks));
            struct_add_entry(mouse_info, RAEL_HEAPSTR("X"), number_newi(event.button.x));
            struct_add_entry(mouse_info, RAEL_HEAPSTR("Y"), number_newi(event.button.y));

            // add to event struct
            struct_add_entry(event_struct, RAEL_HEAPSTR("MouseClickEventInfo"), (RaelValue*)mouse_info);
            break;
        }
        case SDL_MOUSEWHEEL: {
            RaelStructValue *mousewheel_info = struct_new(RAEL_HEAPSTR("MouseWheelEventInfo"));
            struct_add_entry(mousewheel_info, RAEL_HEAPSTR("FlippedDirection"),
                            number_newi(event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED));
            struct_add_entry(mousewheel_info, RAEL_HEAPSTR("ScrollY"), number_newi(event.wheel.y));
            struct_add_entry(mousewheel_info, RAEL_HEAPSTR("ScrollY"), number_newi(event.wheel.y));

            // add to event struct
            struct_add_entry(event_struct, RAEL_HEAPSTR("MouseWheelEventInfo"), (RaelValue*)mousewheel_info);
            break;
        }
        case SDL_QUIT:
        default:
            break;
        }
        return (RaelValue*)event_struct;
    } else {
        return void_new();
    }
}

RaelValue *module_graphics_new(RaelInterpreter *interpreter) {
    RaelModuleValue *m;

    (void)interpreter;

    m = (RaelModuleValue*)module_new(RAEL_HEAPSTR("Graphics"));
    value_ref((RaelValue*)&WindowType);
    value_ref((RaelValue*)&ColorType);
    value_ref((RaelValue*)&SurfaceType);
    module_set_key(m, RAEL_HEAPSTR("Window"), (RaelValue*)&WindowType);
    module_set_key(m, RAEL_HEAPSTR("Color"), (RaelValue*)&ColorType);
    module_set_key(m, RAEL_HEAPSTR("Surface"), (RaelValue*)&SurfaceType);
    module_set_key(m, RAEL_HEAPSTR("Init"), cfunc_new(RAEL_HEAPSTR("Init"), module_graphics_Init, 0));
    module_set_key(m, RAEL_HEAPSTR("Deinit"), cfunc_new(RAEL_HEAPSTR("Deinit"), module_graphics_Deinit, 0));
    module_set_key(m, RAEL_HEAPSTR("GetEvent"), cfunc_new(RAEL_HEAPSTR("GetEvent"), module_graphics_GetEvent, 0));

    // event constants
    module_set_key(m, RAEL_HEAPSTR("EVENT_KEYDOWN"), number_newi(SDL_KEYDOWN));
    module_set_key(m, RAEL_HEAPSTR("EVENT_KEYUP"), number_newi(SDL_KEYUP));
    module_set_key(m, RAEL_HEAPSTR("EVENT_QUIT"), number_newi(SDL_QUIT));
    module_set_key(m, RAEL_HEAPSTR("EVENT_MOUSEMOTION"), number_newi(SDL_MOUSEMOTION));
    module_set_key(m, RAEL_HEAPSTR("EVENT_MOUSEBUTTONDOWN"), number_newi(SDL_MOUSEBUTTONDOWN));
    module_set_key(m, RAEL_HEAPSTR("EVENT_MOUSEBUTTONUP"), number_newi(SDL_MOUSEBUTTONUP));
    module_set_key(m, RAEL_HEAPSTR("EVENT_MOUSEWHEEL"), number_newi(SDL_MOUSEWHEEL));

    // keycodes
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_UNKNOWN"), number_newi(SDLK_UNKNOWN));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_RETURN"), number_newi(SDLK_RETURN));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_ESCAPE"), number_newi(SDLK_ESCAPE));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_BACKSPACE"), number_newi(SDLK_BACKSPACE));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_TAB"), number_newi(SDLK_TAB));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_SPACE"), number_newi(SDLK_SPACE));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_EXCLAIM"), number_newi(SDLK_EXCLAIM));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_QUOTEDBL"), number_newi(SDLK_QUOTEDBL));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_HASH"), number_newi(SDLK_HASH));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_PERCENT"), number_newi(SDLK_PERCENT));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_DOLLAR"), number_newi(SDLK_DOLLAR));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_AMPERSAND"), number_newi(SDLK_AMPERSAND));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_QUOTE"), number_newi(SDLK_QUOTE));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_LEFTPAREN"), number_newi(SDLK_LEFTPAREN));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_RIGHTPAREN"), number_newi(SDLK_RIGHTPAREN));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_ASTERISK"), number_newi(SDLK_ASTERISK));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_PLUS"), number_newi(SDLK_PLUS));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_COMMA"), number_newi(SDLK_COMMA));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_MINUS"), number_newi(SDLK_MINUS));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_PERIOD"), number_newi(SDLK_PERIOD));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_SLASH"), number_newi(SDLK_SLASH));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_0"), number_newi(SDLK_0));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_1"), number_newi(SDLK_1));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_2"), number_newi(SDLK_2));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_3"), number_newi(SDLK_3));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_4"), number_newi(SDLK_4));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_5"), number_newi(SDLK_5));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_6"), number_newi(SDLK_6));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_7"), number_newi(SDLK_7));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_8"), number_newi(SDLK_8));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_9"), number_newi(SDLK_9));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_COLON"), number_newi(SDLK_COLON));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_SEMICOLON"), number_newi(SDLK_SEMICOLON));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_LESS"), number_newi(SDLK_LESS));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_EQUALS"), number_newi(SDLK_EQUALS));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_GREATER"), number_newi(SDLK_GREATER));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_QUESTION"), number_newi(SDLK_QUESTION));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_AT"), number_newi(SDLK_AT));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_LEFTBRACKET"), number_newi(SDLK_LEFTBRACKET));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_BACKSLASH"), number_newi(SDLK_BACKSLASH));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_RIGHTBRACKET"), number_newi(SDLK_RIGHTBRACKET));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_CARET"), number_newi(SDLK_CARET));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_UNDERSCORE"), number_newi(SDLK_UNDERSCORE));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_BACKQUOTE"), number_newi(SDLK_BACKQUOTE));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_a"), number_newi(SDLK_a));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_b"), number_newi(SDLK_b));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_c"), number_newi(SDLK_c));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_d"), number_newi(SDLK_d));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_e"), number_newi(SDLK_e));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_f"), number_newi(SDLK_f));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_g"), number_newi(SDLK_g));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_h"), number_newi(SDLK_h));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_i"), number_newi(SDLK_i));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_j"), number_newi(SDLK_j));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_k"), number_newi(SDLK_k));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_l"), number_newi(SDLK_l));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_m"), number_newi(SDLK_m));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_n"), number_newi(SDLK_n));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_o"), number_newi(SDLK_o));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_p"), number_newi(SDLK_p));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_q"), number_newi(SDLK_q));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_r"), number_newi(SDLK_r));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_s"), number_newi(SDLK_s));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_t"), number_newi(SDLK_t));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_u"), number_newi(SDLK_u));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_v"), number_newi(SDLK_v));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_w"), number_newi(SDLK_w));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_x"), number_newi(SDLK_x));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_y"), number_newi(SDLK_y));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_z"), number_newi(SDLK_z));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_CAPSLOCK"), number_newi(SDLK_CAPSLOCK));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_F1"), number_newi(SDLK_F1));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_F2"), number_newi(SDLK_F2));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_F3"), number_newi(SDLK_F3));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_F4"), number_newi(SDLK_F4));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_F5"), number_newi(SDLK_F5));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_F6"), number_newi(SDLK_F6));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_F7"), number_newi(SDLK_F7));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_F8"), number_newi(SDLK_F8));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_F9"), number_newi(SDLK_F9));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_F10"), number_newi(SDLK_F10));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_F11"), number_newi(SDLK_F11));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_F12"), number_newi(SDLK_F12));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_PRINTSCREEN"), number_newi(SDLK_PRINTSCREEN));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_SCROLLLOCK"), number_newi(SDLK_SCROLLLOCK));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_PAUSE"), number_newi(SDLK_PAUSE));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_INSERT"), number_newi(SDLK_INSERT));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_HOME"), number_newi(SDLK_HOME));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_PAGEUP"), number_newi(SDLK_PAGEUP));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_DELETE"), number_newi(SDLK_DELETE));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_END"), number_newi(SDLK_END));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_PAGEDOWN"), number_newi(SDLK_PAGEDOWN));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_RIGHT"), number_newi(SDLK_RIGHT));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_LEFT"), number_newi(SDLK_LEFT));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_DOWN"), number_newi(SDLK_DOWN));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_UP"), number_newi(SDLK_UP));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_NUMLOCKCLEAR"), number_newi(SDLK_NUMLOCKCLEAR));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_DIVIDE"), number_newi(SDLK_KP_DIVIDE));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_MULTIPLY"), number_newi(SDLK_KP_MULTIPLY));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_MINUS"), number_newi(SDLK_KP_MINUS));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_PLUS"), number_newi(SDLK_KP_PLUS));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_ENTER"), number_newi(SDLK_KP_ENTER));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_1"), number_newi(SDLK_KP_1));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_2"), number_newi(SDLK_KP_2));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_3"), number_newi(SDLK_KP_3));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_4"), number_newi(SDLK_KP_4));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_5"), number_newi(SDLK_KP_5));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_6"), number_newi(SDLK_KP_6));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_7"), number_newi(SDLK_KP_7));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_8"), number_newi(SDLK_KP_8));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_9"), number_newi(SDLK_KP_9));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_0"), number_newi(SDLK_KP_0));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_PERIOD"), number_newi(SDLK_KP_PERIOD));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_APPLICATION"), number_newi(SDLK_APPLICATION));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_POWER"), number_newi(SDLK_POWER));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_EQUALS"), number_newi(SDLK_KP_EQUALS));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_F13"), number_newi(SDLK_F13));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_F14"), number_newi(SDLK_F14));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_F15"), number_newi(SDLK_F15));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_F16"), number_newi(SDLK_F16));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_F17"), number_newi(SDLK_F17));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_F18"), number_newi(SDLK_F18));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_F19"), number_newi(SDLK_F19));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_F20"), number_newi(SDLK_F20));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_F21"), number_newi(SDLK_F21));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_F22"), number_newi(SDLK_F22));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_F23"), number_newi(SDLK_F23));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_F24"), number_newi(SDLK_F24));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_EXECUTE"), number_newi(SDLK_EXECUTE));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_HELP"), number_newi(SDLK_HELP));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_MENU"), number_newi(SDLK_MENU));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_SELECT"), number_newi(SDLK_SELECT));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_STOP"), number_newi(SDLK_STOP));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_AGAIN"), number_newi(SDLK_AGAIN));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_UNDO"), number_newi(SDLK_UNDO));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_CUT"), number_newi(SDLK_CUT));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_COPY"), number_newi(SDLK_COPY));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_PASTE"), number_newi(SDLK_PASTE));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_FIND"), number_newi(SDLK_FIND));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_MUTE"), number_newi(SDLK_MUTE));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_VOLUMEUP"), number_newi(SDLK_VOLUMEUP));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_VOLUMEDOWN"), number_newi(SDLK_VOLUMEDOWN));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_COMMA"), number_newi(SDLK_KP_COMMA));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_EQUALSAS400"), number_newi(SDLK_KP_EQUALSAS400));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_ALTERASE"), number_newi(SDLK_ALTERASE));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_SYSREQ"), number_newi(SDLK_SYSREQ));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_CANCEL"), number_newi(SDLK_CANCEL));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_CLEAR"), number_newi(SDLK_CLEAR));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_PRIOR"), number_newi(SDLK_PRIOR));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_RETURN2"), number_newi(SDLK_RETURN2));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_SEPARATOR"), number_newi(SDLK_SEPARATOR));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_OUT"), number_newi(SDLK_OUT));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_OPER"), number_newi(SDLK_OPER));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_CLEARAGAIN"), number_newi(SDLK_CLEARAGAIN));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_CRSEL"), number_newi(SDLK_CRSEL));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_EXSEL"), number_newi(SDLK_EXSEL));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_00"), number_newi(SDLK_KP_00));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_000"), number_newi(SDLK_KP_000));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_THOUSANDSSEPARATOR"), number_newi(SDLK_THOUSANDSSEPARATOR));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_DECIMALSEPARATOR"), number_newi(SDLK_DECIMALSEPARATOR));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_CURRENCYUNIT"), number_newi(SDLK_CURRENCYUNIT));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_CURRENCYSUBUNIT"), number_newi(SDLK_CURRENCYSUBUNIT));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_LEFTPAREN"), number_newi(SDLK_KP_LEFTPAREN));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_RIGHTPAREN"), number_newi(SDLK_KP_RIGHTPAREN));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_LEFTBRACE"), number_newi(SDLK_KP_LEFTBRACE));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_RIGHTBRACE"), number_newi(SDLK_KP_RIGHTBRACE));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_TAB"), number_newi(SDLK_KP_TAB));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_BACKSPACE"), number_newi(SDLK_KP_BACKSPACE));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_A"), number_newi(SDLK_KP_A));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_B"), number_newi(SDLK_KP_B));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_C"), number_newi(SDLK_KP_C));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_D"), number_newi(SDLK_KP_D));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_E"), number_newi(SDLK_KP_E));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_F"), number_newi(SDLK_KP_F));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_XOR"), number_newi(SDLK_KP_XOR));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_POWER"), number_newi(SDLK_KP_POWER));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_PERCENT"), number_newi(SDLK_KP_PERCENT));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_LESS"), number_newi(SDLK_KP_LESS));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_GREATER"), number_newi(SDLK_KP_GREATER));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_AMPERSAND"), number_newi(SDLK_KP_AMPERSAND));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_DBLAMPERSAND"), number_newi(SDLK_KP_DBLAMPERSAND));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_VERTICALBAR"), number_newi(SDLK_KP_VERTICALBAR));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_DBLVERTICALBAR"), number_newi(SDLK_KP_DBLVERTICALBAR));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_COLON"), number_newi(SDLK_KP_COLON));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_HASH"), number_newi(SDLK_KP_HASH));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_SPACE"), number_newi(SDLK_KP_SPACE));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_AT"), number_newi(SDLK_KP_AT));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_EXCLAM"), number_newi(SDLK_KP_EXCLAM));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_MEMSTORE"), number_newi(SDLK_KP_MEMSTORE));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_MEMRECALL"), number_newi(SDLK_KP_MEMRECALL));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_MEMCLEAR"), number_newi(SDLK_KP_MEMCLEAR));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_MEMADD"), number_newi(SDLK_KP_MEMADD));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_MEMSUBTRACT"), number_newi(SDLK_KP_MEMSUBTRACT));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_MEMMULTIPLY"), number_newi(SDLK_KP_MEMMULTIPLY));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_MEMDIVIDE"), number_newi(SDLK_KP_MEMDIVIDE));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_PLUSMINUS"), number_newi(SDLK_KP_PLUSMINUS));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_CLEAR"), number_newi(SDLK_KP_CLEAR));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_CLEARENTRY"), number_newi(SDLK_KP_CLEARENTRY));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_BINARY"), number_newi(SDLK_KP_BINARY));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_OCTAL"), number_newi(SDLK_KP_OCTAL));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_DECIMAL"), number_newi(SDLK_KP_DECIMAL));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KP_HEXADECIMAL"), number_newi(SDLK_KP_HEXADECIMAL));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_LCTRL"), number_newi(SDLK_LCTRL));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_LSHIFT"), number_newi(SDLK_LSHIFT));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_LALT"), number_newi(SDLK_LALT));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_LGUI"), number_newi(SDLK_LGUI));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_RCTRL"), number_newi(SDLK_RCTRL));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_RSHIFT"), number_newi(SDLK_RSHIFT));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_RALT"), number_newi(SDLK_RALT));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_RGUI"), number_newi(SDLK_RGUI));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_MODE"), number_newi(SDLK_MODE));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_AUDIONEXT"), number_newi(SDLK_AUDIONEXT));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_AUDIOPREV"), number_newi(SDLK_AUDIOPREV));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_AUDIOSTOP"), number_newi(SDLK_AUDIOSTOP));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_AUDIOPLAY"), number_newi(SDLK_AUDIOPLAY));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_AUDIOMUTE"), number_newi(SDLK_AUDIOMUTE));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_MEDIASELECT"), number_newi(SDLK_MEDIASELECT));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_WWW"), number_newi(SDLK_WWW));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_MAIL"), number_newi(SDLK_MAIL));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_CALCULATOR"), number_newi(SDLK_CALCULATOR));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_COMPUTER"), number_newi(SDLK_COMPUTER));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_AC_SEARCH"), number_newi(SDLK_AC_SEARCH));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_AC_HOME"), number_newi(SDLK_AC_HOME));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_AC_BACK"), number_newi(SDLK_AC_BACK));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_AC_FORWARD"), number_newi(SDLK_AC_FORWARD));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_AC_STOP"), number_newi(SDLK_AC_STOP));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_AC_REFRESH"), number_newi(SDLK_AC_REFRESH));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_AC_BOOKMARKS"), number_newi(SDLK_AC_BOOKMARKS));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_BRIGHTNESSDOWN"), number_newi(SDLK_BRIGHTNESSDOWN));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_BRIGHTNESSUP"), number_newi(SDLK_BRIGHTNESSUP));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_DISPLAYSWITCH"), number_newi(SDLK_DISPLAYSWITCH));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KBDILLUMTOGGLE"), number_newi(SDLK_KBDILLUMTOGGLE));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KBDILLUMDOWN"), number_newi(SDLK_KBDILLUMDOWN));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_KBDILLUMUP"), number_newi(SDLK_KBDILLUMUP));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_EJECT"), number_newi(SDLK_EJECT));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_SLEEP"), number_newi(SDLK_SLEEP));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_APP1"), number_newi(SDLK_APP1));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_APP2"), number_newi(SDLK_APP2));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_AUDIOREWIND"), number_newi(SDLK_AUDIOREWIND));
    module_set_key(m, RAEL_HEAPSTR("KEYCODE_AUDIOFASTFORWARD"), number_newi(SDLK_AUDIOFASTFORWARD));

    return (RaelValue*)m;
}
