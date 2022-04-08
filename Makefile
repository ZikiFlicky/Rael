CC=gcc
LINK=-lm -lSDL2
CFLAGS=-Wall -Wextra -std=c99 -Wno-missing-braces -Isrc/

SRCDIR=src
MODULESDIR=$(SRCDIR)/modules
TYPESDIR=$(SRCDIR)/types
MKDIR=mkdir -p
BUILDDIR=build
NAME=rael
RM=rm -f
RMDIR=rm -rf

OBJECTS=main.o             \
		lexer.o            \
		parser.o           \
		interpreter.o      \
		value.o            \
		number.o           \
		common.o           \
		string.o           \
		stack.o            \
		module.o           \
		range.o            \
		blame.o            \
		routine.o          \
		cfuncs.o           \
		struct.o           \
		varmap.o           \
		scope.o            \
		stream.o           \
		mathmodule.o       \
		typesmodule.o      \
		timemodule.o       \
		randommodule.o     \
		systemmodule.o     \
		filemodule.o       \
		functionalmodule.o \
		binmodule.o        \
		graphicsmodule.o   \
		encodingsmodule.o

.PHONY: clean all debug

debug: CFLAGS+=-g
debug: clean $(BUILDDIR)/$(NAME)

all: CFLAGS+=-DNDEBUG
all: clean $(BUILDDIR)/$(NAME)

$(BUILDDIR):
	$(MKDIR) $@

$(BUILDDIR)/$(NAME): $(OBJECTS)
	$(CC) $(CFLAGS) $(BUILDDIR)/* -o $@ $(LINK)

%.o: $(SRCDIR)/%.c $(BUILDDIR)
	$(CC) $(CFLAGS) -c -o $(BUILDDIR)/$@ $<

%.o: $(MODULESDIR)/%.c $(BUILDDIR)
	$(CC) $(CFLAGS) -c -o $(BUILDDIR)/$@ $<

%.o: $(TYPESDIR)/%.c $(BUILDDIR)
	$(CC) $(CFLAGS) -c -o $(BUILDDIR)/$@ $<

clean:
	$(RMDIR) $(BUILDDIR)
