##
## Makefile for hot_patch
## by lenorm_f
##

NAME = hot_patch

SRC = \
      hot_patch.c \
      main.c

OBJ = $(SRC:.c=.o)

CC = gcc
CFLAGS = -Wall -Wextra -DDEBUG

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(NAME): $(OBJ)
	$(CC) $(OBJ) -o $(NAME) $(LDFLAGS)

all: $(NAME)

clean:
	rm -f $(OBJ)

distclean: clean
	rm -f $(NAME)

re: distclean all
