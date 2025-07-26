#ifndef _artwork_h
#define _artwork_h

struct artwork_header {
	unsigned int additional_offset; // not implemented in this project yet
	unsigned int plist_offset;
	unsigned int width;
	unsigned int height;
	unsigned int colorSpace;
	unsigned int total;
	unsigned int signature;
};

#ifdef __cplusplus
extern "C" {
#endif

int unartwork_main(int argc, char *argv[]);
int enartwork_main(int argc, char *argv[]);

#ifdef __cplusplus
}
#endif

#endif
