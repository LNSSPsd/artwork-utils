.PHONY: all

all: artwork-utils

artwork-utils: main.c unartwork.c enartwork.c
	$(CC) `pkg-config --cflags libplist-2.0` $^ `pkg-config --libs libplist-2.0` -lpng -o $@
