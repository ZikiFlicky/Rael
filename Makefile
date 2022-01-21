CC=gcc
LINK=-lm
CFLAGS=-Wall -Wextra -std=c99 -Wno-missing-braces

SRCDIR=src
MKDIR=mkdir -p
BUILDDIR=build
NAME=rael
RM=rm -f
RMDIR=rm -rf

OBJECTS=lexer.o parser.o value.o scope.o number.o interpreter.o main.o common.o string.o stack.o varmap.o module.o \
		mathmodule.o typesmodule.o timemodule.o

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

clean:
	$(RMDIR) $(BUILDDIR)
