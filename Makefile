CC=gcc
LINK=-lm
CFLAGS=-Wall -Wextra -std=c99 -Wno-missing-braces -Isrc/

SRCDIR=src
MKDIR=mkdir -p
BUILDDIR=build
NAME=rael
RM=rm -f
RMDIR=rm -rf

OBJECTS=lexer.o parser.o interpreter.o value.o number.o main.o common.o string.o stack.o module.o range.o blame.o routine.o cfuncs.o varmap.o scope.o \
		mathmodule.o typesmodule.o timemodule.o randommodule.o systemmodule.o

.PHONY: clean all

all: clean $(BUILDDIR)/$(NAME)

$(BUILDDIR):
	$(MKDIR) $@

$(BUILDDIR)/$(NAME): $(OBJECTS)
	$(CC) $(CFLAGS) $(BUILDDIR)/* -o $@ $(LINK)

%.o: src/%.c $(BUILDDIR)
	$(CC) $(CFLAGS) -c -o $(BUILDDIR)/$@ $<

%.o: src/modules/%.c $(BUILDDIR)
	$(CC) $(CFLAGS) -c -o $(BUILDDIR)/$@ $<

%.o: src/types/%.c $(BUILDDIR)
	$(CC) $(CFLAGS) -c -o $(BUILDDIR)/$@ $<

clean:
	$(RMDIR) $(BUILDDIR)
