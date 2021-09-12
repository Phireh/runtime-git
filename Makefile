CFLAGS=-Wall -Wextra -fPIC -fPIE -g
LIBS=-lncurses -ltinfo -ldl -lgit2

all: game platform_layer

.PHONY: game clean

game: game.c common.h
	gcc $(CFLAGS) $(LIBS) -shared $^ -o game$(SUFFIX).so

platform_layer: platform_layer.c game.so
	gcc $(CFLAGS) $(LIBS) platform_layer.c -o platform_layer

clean:
	rm -r temp*
