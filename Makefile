.PHONY: all

all: artwork-utils

artwork-utils: main.c unartwork.c
	$(CC) $^ -lplist-2.0 -lpng -o $@