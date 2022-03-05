#ifndef RAEL_RAEL_H
#define RAEL_RAEL_H

/*
 * This file is a forward file, and it should be included by every C file
 * in the source code.
 */

#ifndef __unix__
#error Expected a unix system
#endif

#define _POSIX_C_SOURCE 199309L
#define __USE_POSIX199309
#include <unistd.h>

#if !(_POSIX_TIMERS > 0)
#error Expected _POSIX_TIMERS to be bigger than 0
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <math.h>
#include <time.h>
#include <inttypes.h>

#include "interpreter.h"
#include "stream.h"
#include "common.h"
#include "lexer.h"
#include "parser.h"
#include "scope.h"
#include "value.h"
#include "varmap.h"
#include "types/blame.h"
#include "types/number.h"
#include "types/string.h"
#include "types/stack.h"
#include "types/module.h"
#include "types/range.h"
#include "types/routine.h"
#include "types/cfuncs.h"
#include "types/struct.h"

#endif /* RAEL_RAEL_H */
