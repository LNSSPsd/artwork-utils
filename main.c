#include <stdio.h>
#include <string.h>

#include "artwork.h"

void print_help(FILE *f, const char *self) {
	fprintf(f, "%s unartwork [options...]\n%s enartwork [options...]\n\nInvoke --help on individual functions to see detailed help text.\n", self, self);
}

int main(int argc, char *argv[]) {
	if (argc == 1) {
		print_help(stderr, *argv);
		return 1;
	}
	if (!strcmp(argv[1], "unartwork")) {
		return unartwork_main(argc, argv);
	} else if (!strcmp(argv[1], "enartwork")) {
		return enartwork_main(argc, argv);
	}
	return 0;
}
