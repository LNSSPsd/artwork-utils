#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>

#include <plist/plist.h>
#include <png.h>

struct artwork_header {
	unsigned int idk1;
	unsigned int plist_offset;
	unsigned int width;
	unsigned int height;
	unsigned int unk1;
	unsigned int total;
	unsigned int signature;
};

void unartwork_print_help(FILE *f, const char *self) {
	fprintf(f,"%s unartwork [options...] [-i|--artwork] <artwork file> [-D|--output-directory] <output directory>\n\
\t--help: \t\tDisplay this help message.\n\
\t--raw: \t\t\tExport raw BGRA data.\n\
\t-i, --artwork: \t\tSpecify artwork file.\n\
\t-D, --output-directory: Specify output directory.\n\
\t--force-scale: \t\tSpecify artwork scale, overriding autodetected value.\n\
\t-l, --list: \t\tPrint the contents, do not generate output.\n",self);
}

int unartwork_main(int argc, char *argv[]) {
	int raw_output=0;
	const char *artwork_fn=NULL;
	const char *output_dir=NULL;
	int artwork_scale=-1;
	int list_only=0;
	while(1) {
		static struct option longopts[]={
			{"help", no_argument, 0, 0},
			{"raw", no_argument, 0, 0},
			{"force-scale", required_argument,0,0},
			{"artwork", required_argument,0,'i'},
			{"output-directory", required_argument,0,'D'},
			{"list", no_argument, 0, 'l'},
			{0,0,0,0}
		};
		int index;
		int c=getopt_long(argc,argv,"D:i:l",longopts,&index);
		if(c==-1)
			break;
		switch(c) {
		case 0:
			if(index==0) {
				unartwork_print_help(stdout,*argv);
				return 0;
			}else if(index==1) {
				raw_output=1;
				break;
			}else if(index==2) {
				artwork_scale=atoi(optarg);
				if(artwork_scale<1||artwork_scale>9) {
					fprintf(stderr,"ERROR: --force-scale: Invalid artwork scale.\n");
					unartwork_print_help(stderr,*argv);
					return 1;
				}
				break;
			}
		case 'l':
			list_only=1;
			break;
		case 'i':
			if(artwork_fn) {
				fprintf(stderr,"ERROR: Duplicated -i: %s and %s.\n",artwork_fn,optarg);
				unartwork_print_help(stderr,*argv);
				return 1;
			}
			artwork_fn=optarg;
			break;
		case 'D':
			if(output_dir) {
				fprintf(stderr,"ERROR: Duplicated -D: %s and %s.\n",output_dir,optarg);
				unartwork_print_help(stderr,*argv);
				return 1;
			}
			output_dir=optarg;
			break;
		}	
	}
	while(optind<argc) {
		if(!artwork_fn) {
			artwork_fn=argv[optind];
		}else if(!list_only&&!output_dir) {
			output_dir=argv[optind];
		}else{
			fprintf(stderr,"ERROR: Unrecognized argument: %s\n",argv[optind]);
			unartwork_print_help(stderr,*argv);
			return 1;
		}
		optind++;
	}
	if(!artwork_fn||(!list_only&&!output_dir)) {
		fprintf(stderr,"ERROR: Too few arguments\n");
		unartwork_print_help(stderr,*argv);
		return 1;
	}
	if(list_only&&output_dir) {
		fprintf(stderr,"ERROR: Conflicting arguments: --list and --output-directory\n");
		unartwork_print_help(stderr,*argv);
		return 1;
	}
	if(output_dir) {
		struct stat odir_stat;
		if(stat(output_dir,&odir_stat)==-1) {
			if(errno==ENOENT&&mkdir(output_dir,0755)==-1) {
				perror("Failed to create output directory");
				return 1;
			}else if(errno!=ENOENT){
				perror("Output directory not accessible");
				return 1;
			}
		}else if(!S_ISDIR(odir_stat.st_mode)) {
			fprintf(stderr,"ERROR: Output path is not a directory\n");
			return 1;
		}
	}
	int artwork_fd=open(artwork_fn,O_RDONLY);
	if(artwork_fd==-1) {
		perror("Failed to open artwork file");
		return 1;
	}
	struct stat aw_st;
	if(fstat(artwork_fd,&aw_st)==-1) {
		perror("Failed to get information for the file");
		close(artwork_fd);
		return 1;
	}
	void *artwork_file=mmap(NULL,aw_st.st_size,PROT_READ|PROT_WRITE,MAP_PRIVATE,artwork_fd,0);
	if(artwork_file==MAP_FAILED) {
		perror("Failed to invoke mmap()");
		close(artwork_fd);
		return 1;
	}
	close(artwork_fd);
	struct artwork_header *hdr=artwork_file+aw_st.st_size-sizeof(struct artwork_header);
	if(hdr->signature!=0xdcb543a2) {
		fprintf(stderr,"ERROR: Not a valid artwork file\n");
		munmap(artwork_file,aw_st.st_size);
		return 1;
	}
	//printf("plist at %p\n", aw_st.st_size-sizeof(struct artwork_header)-hdr->plist_offset);
	plist_t plist_data;
	plist_err_t pl_err=plist_from_memory((const char *)hdr -hdr->plist_offset,hdr->plist_offset,&plist_data,NULL);
	if(pl_err!=PLIST_ERR_SUCCESS) {
		fprintf(stderr,"ERROR: Artwork file does not contain a valid plist\n");
		munmap(artwork_file,aw_st.st_size);
		return 1;
	}
	if(!PLIST_IS_ARRAY(plist_data)) {
		fprintf(stderr,"ERROR: Invalid plist datastructure\n");
		plist_free(plist_data);
		munmap(artwork_file,aw_st.st_size);
		return 1;
	}
	for(int i=1;i<=9;i++) {
		if((4*i*32*((hdr->total-1)*32*i + hdr->width))==aw_st.st_size-sizeof(struct artwork_header)-hdr->plist_offset) {
			artwork_scale=i;
			break;
		}
	}
	if(!list_only&&artwork_scale==-1) {
		fprintf(stderr, "ERROR: Failed to detect artwork scale, possibly invalid, consider setting --force-scale\n");
		plist_free(plist_data);
		munmap(artwork_file,aw_st.st_size);
		return 1;
	}
	int arr_size=plist_array_get_size(plist_data);
	for(int i=0;i<arr_size;i++) {
		void *current_begin=artwork_file+4*artwork_scale*artwork_scale*32*32*i;
		plist_t name=plist_array_get_item(plist_data,i);
		if(!name||!PLIST_IS_STRING(name)) {
			fprintf(stderr,"ERROR: Invalid plist data structure\n");
			plist_free(plist_data);
			munmap(artwork_file,aw_st.st_size);
			return 1;
		}
		const char *imgn=plist_get_string_ptr(name,NULL);
		if(list_only) {
			printf("%s\n",imgn);
			continue;
		}
		for(const char *c=imgn;*c!=0;c++) {
			if(*c=='/') {
				fprintf(stderr,"ERROR: Invalid plist data structure\n");
				plist_free(plist_data);
				munmap(artwork_file,aw_st.st_size);
				return 1;
			}
		}
		//printf("%d: %s\n",i,plist_get_string_ptr(name,NULL));
		char fn[32];
		sprintf(fn,"%s/%.20s.%s",output_dir,imgn,raw_output?"raw":"png");
		if(raw_output) {
			FILE *file=fopen(fn,"wb");
			fwrite(current_begin,1,4*32*artwork_scale*(i==arr_size-1?hdr->width:32*artwork_scale),file);
			fclose(file);
			continue;
		}
		png_structp png_ptr=png_create_write_struct(PNG_LIBPNG_VER_STRING,NULL,NULL,NULL);
		if(!png_ptr) {
			fprintf(stderr,"ERROR: Failed to create write struct for png\n");
			goto cleanup;
		}
		png_infop info_ptr=png_create_info_struct(png_ptr);
		if(!info_ptr) {
			png_destroy_write_struct(&png_ptr,NULL);
			fprintf(stderr,"ERROR: Failed to create png info ptr\n");
			goto cleanup;
		}
		FILE *output_file=NULL;
		png_bytep *png_rows=NULL;
		if(setjmp(png_jmpbuf(png_ptr))) {
			png_destroy_write_struct(&png_ptr,NULL);
			if(png_rows)
				free(png_rows);
			if(output_file)
				fclose(output_file);
			fprintf(stderr,"ERROR: Failed to write to png\n");
			goto cleanup;
		}
		output_file=fopen(fn,"wb");
		if(!output_file) {
			fprintf(stderr,"ERROR: Failed to open output file\n");
			png_destroy_write_struct(&png_ptr,NULL);
			goto cleanup;
		}
		png_init_io(png_ptr,output_file);
		png_set_IHDR(png_ptr,info_ptr,hdr->width,hdr->height,8,PNG_COLOR_TYPE_RGB_ALPHA,PNG_INTERLACE_NONE,PNG_COMPRESSION_TYPE_BASE,PNG_FILTER_TYPE_BASE);;
		png_rows=malloc(sizeof(png_bytep)*hdr->height);
		for(int row=0;row<hdr->height;row++) {
			png_rows[row]=current_begin+4*row*artwork_scale*32;
		}
		png_set_rows(png_ptr,info_ptr,png_rows);
		png_write_png(png_ptr,info_ptr,PNG_TRANSFORM_BGR,NULL);
		png_destroy_write_struct(&png_ptr,NULL);
		free(png_rows);
		fclose(output_file);
	}
cleanup:
	plist_free(plist_data);
	munmap(artwork_file,aw_st.st_size);
	return 0;
}