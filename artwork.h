

struct artwork_header {
	unsigned int idk1;
	unsigned int plist_offset;
	unsigned int width;
	unsigned int height;
	unsigned int unk1;
	unsigned int total;
	unsigned int signature;
};

extern int unartwork_main(int argc, char *argv[]);
extern int enartwork_main(int argc, char *argv[]);