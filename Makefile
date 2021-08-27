CC=gcc
LINK=-lm
CFLAGS=-Wall -Wextra -std=c99 -Wno-missing-braces
NAME=rael
RM=rm -f

all: clean $(NAME)

$(NAME): src/lexer.c src/number.c src/parser.c src/varmap.c src/interpreter.c src/main.c
	$(CC) $(CFLAGS) -o $(NAME) $^ $(LINK)

clean:
	$(RM) $(NAME)
