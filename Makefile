.PHONY: all

all: artwork-utils

artwork-utils: main.c unartwork.c enartwork.c
	$(CC) `pkg-config --cflags libplist-2.0 libpng` $^ `pkg-config --libs libplist-2.0 libpng` -o $@
