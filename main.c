#include <stdio.h>
#include <string.h>

void print_help(FILE *f, const char *self) {
	fprintf(f,"%s unartwork [options...]\n\
%s enartwork [options...]\n\n\
Invoke --help on individual functions to see detailed help text.\n",self,self);
}

int unartwork_main(int argc, char *argv[]);

int main(int argc, char *argv[]) {
	if(argc==1) {
		print_help(stderr,*argv);
		return 1;
	}
	if(!strcmp(argv[1],"unartwork")) {
		return unartwork_main(argc-1,argv+1);
	}else if(!strcmp(argv[1],"enartwork")) {
		fprintf(stderr, "enartwork is not implemented yet.\n");
		return 1;
	}
	return 0;
}