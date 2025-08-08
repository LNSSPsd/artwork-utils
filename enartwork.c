#if !defined(__APPLE__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include <dirent.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <plist/plist.h>
#include <png.h>

#include "artwork.h"

static int png_filter(const struct dirent *val) {
	if (val->d_type != DT_REG)
		return 0;
	if (!memmem(val->d_name, strlen(val->d_name) + 1, ".png", 5)) {
		fprintf(stderr, "WARNING: %s is not a .png file, ignoring.\n", val->d_name);
		return 0;
	}
	return 1;
}

static void enartwork_print_help(FILE *fp, const char *self_name) {
	fprintf(fp, "%s enartwork [options...] [-c] <contents directory> [-o] <output>\n", self_name);

	const char *options[] = {
		"--help                   Display this help message.",
		//"-S, --append-scale       Append image scale to file name. (e.g. output@2x.artwork)",
		"-c, --contents-directory Specify the directory containing all input images.",
		"-o, --output             Specify the output artwork file.",
		NULL
	};

	for (const char **opt = options; *opt != NULL; ++opt) {
		fprintf(fp, "\t%s\n", *opt);
	}
}

int enartwork_main(int argc, char *argv[]) {
	argc--;
	int         ret          = EXIT_FAILURE;
	const char *self_name    = *(argv++);
	const char *contents_dir = NULL;
	char       *output_file  = NULL;
	int         append_scale = 0;
	while (1) {
		int                  opt_index;
		static struct option longopts[] = {
			{"help",                no_argument,       0, 0  },
 //{ "append-scale",       no_argument,       0, 'S'},
			{ "contents-directory", required_argument, 0, 'c'},
			{ "output",             required_argument, 0, 'o'},
			{ 0,			        0,                 0, 0  }
		};
		int c = getopt_long(argc, argv, "c:o:", longopts, &opt_index);
		if (c == -1)
			break;
		switch (c) {
		case 0:
			if (opt_index == 0) {
				enartwork_print_help(stdout, self_name);
				return 0;
			}
			break;
		case 'S':
			append_scale = 1;
			break;
		case 'c':
			if (contents_dir) {
				fprintf(stderr, "ERROR: Duplicated -c: %s and %s\n", contents_dir, optarg);
				enartwork_print_help(stderr, self_name);
				return 1;
			}
			contents_dir = optarg;
			break;
		case 'o':
			if (output_file) {
				fprintf(stderr, "ERROR: Duplicated -o: %s and %s\n", output_file, optarg);
				enartwork_print_help(stderr, self_name);
				return 1;
			}
			output_file = optarg;
			break;
		case '?':
			enartwork_print_help(stderr, self_name);
			return 1;
		}
	}
	while (optind < argc) {
		if (!contents_dir) {
			contents_dir = argv[optind];
		} else if (!output_file) {
			output_file = argv[optind];
		} else {
			fprintf(stderr, "ERROR: Unexpected argument: %s\n", argv[optind]);
			enartwork_print_help(stderr, self_name);
			return 1;
		}
		optind++;
	}
	if (!contents_dir || !output_file) {
		fprintf(stderr, "ERROR: Too few arguments\n");
		enartwork_print_help(stderr, self_name);
		return 1;
	}
	struct dirent **img_files;
	int             img_cnt = scandir(contents_dir, &img_files, png_filter, alphasort);
	if (img_cnt == -1) {
		perror("Failed to open artwork directory");
		return 3;
	}
	plist_t      output_plist   = plist_new_array();
	unsigned int artwork_width  = 0;
	unsigned int artwork_height = 0;
	struct {
		png_structp s;
		png_infop   i;
	} *all_imgs = calloc(img_cnt, 2 * sizeof(void *));
	for (int dind = 0; dind != img_cnt; dind++) {
		png_structp cur_png_r = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
		if (!cur_png_r) {
			fprintf(stderr, "ERROR: Failed to create read struct for png\n");
			goto cleanup;
		}
		png_infop info_ptr = png_create_info_struct(cur_png_r);
		if (!info_ptr) {
			png_destroy_read_struct(&cur_png_r, NULL, NULL);
			fprintf(stderr, "ERROR: Failed to create png info\n");
			goto cleanup;
		}
		char png_fn[PATH_MAX];
		sprintf(png_fn, "%s/%s", contents_dir, img_files[dind]->d_name);
		FILE *png_file = fopen(png_fn, "rb");
		if (!png_file || setjmp(png_jmpbuf(cur_png_r))) {
			fclose(png_file);
			png_destroy_read_struct(&cur_png_r, &info_ptr, NULL);
			fprintf(stderr, "ERROR: PNG read failed for file %s.\n", png_fn);
			goto cleanup;
		}
		png_init_io(cur_png_r, png_file);
		png_read_png(cur_png_r, info_ptr, PNG_TRANSFORM_BGR, NULL);
		unsigned int cur_width, cur_height;
		int          bit_depth, color_type;
		png_get_IHDR(cur_png_r, info_ptr, &cur_width, &cur_height, &bit_depth, &color_type, NULL, NULL, NULL);

		png_size_t rowbytes = png_get_rowbytes(cur_png_r, info_ptr);
		int        channels = png_get_channels(cur_png_r, info_ptr);

		/* quick sanity checks */
		if (bit_depth != 8) {
			fprintf(stderr, "WARNING: expected 8-bit image after transforms, got %d-bit\n", bit_depth);
		}

		/* Only premultiply when we have 4 channels (BGRA due to PNG_TRANSFORM_BGR) */
		if (channels == 4) {
			png_bytep *rows = png_get_rows(cur_png_r, info_ptr);
			for (unsigned int y = 0; y < cur_height; ++y) {
				png_bytep row = rows[y];
				/* each pixel is 4 bytes: B,G,R,A */
				for (unsigned int x = 0; x < cur_width; ++x) {
					png_bytep    px = row + x * 4;
					unsigned int a  = px[3]; /* alpha 0..255 */
					if (a == 255)
						continue; /* no change needed */
					/* premultiply channels: round((C * A) / 255) */
					px[0] = (png_byte)((px[0] * a + 127) / 255); /* B */
					px[1] = (png_byte)((px[1] * a + 127) / 255); /* G */
					px[2] = (png_byte)((px[2] * a + 127) / 255); /* R */
					/* alpha remains px[3] */
				}
			}
		} else {
			/* If channels != 4 we can't premultiply in-place safely here.
			   Best practice: normalize earlier using png_set_filler(..., 0xFF, PNG_FILLER_AFTER)
			   or use png_set_expand() so you always get 4 channels. For now issue a warning. */
			fprintf(stderr, "WARNING: PNG has %d channels (expected 4 BGRA). Skipping premultiplication for this image (%s).\n", channels, png_fn);
		}
		if (artwork_width < cur_width) {
			artwork_width = cur_width;
		}
		if (artwork_height < cur_height) {
			artwork_height = cur_height;
		}
		// png_bytep *rowptrs=png_get_rows(cur_png_r,info_ptr);
		all_imgs[dind].s = cur_png_r;
		all_imgs[dind].i = info_ptr;
		fclose(png_file);
	}
	unsigned int line_length       = 4 * ((16 + artwork_width - 1) & (-16));
	unsigned int normal_img_length = (line_length * artwork_height + 4095) & 0xfffff000;
	unsigned int total_height      = normal_img_length / line_length;
	static char  output_fn[PATH_MAX];
	/*if (append_scale) {
	    char *ext = memmem(output_file, strlen(output_file) + 1, ".artwork", 9);
	    if (ext)
	        *ext = 0;
	    snprintf(output_fn, PATH_MAX, "%s@%ux.artwork", output_file, final_scale);
	} else {*/
	strncpy(output_fn, output_file, PATH_MAX);
	//}
	int output_fd = open(output_fn, O_CREAT | O_WRONLY | O_TRUNC, 0644);
	if (output_fd == -1) {
		perror("Failed to open output artwork file for writing");
		goto cleanup;
	}
	for (int i = 0; i < img_cnt; i++) {
		*(char *)memmem(img_files[i]->d_name, strlen(img_files[i]->d_name) + 1, ".png", 5) = 0;
		unsigned int cur_width, cur_height;
		png_get_IHDR(all_imgs[i].s, all_imgs[i].i, &cur_width, &cur_height, NULL, NULL, NULL, NULL, NULL);
		png_bytep *cur = png_get_rows(all_imgs[i].s, all_imgs[i].i);
		for (int row = 0; row < total_height; row++) {
			if (row >= artwork_height) {
				if (i == img_cnt - 1)
					break;
				for (int ext = 0; ext < line_length; ext += 4) {
					unsigned int val = 0;
					write(output_fd, &val, 4);
				}
				continue;
			}
			write(output_fd, cur[row], artwork_width * 4);
			for (int ext = artwork_width * 4; ext < line_length; ext += 4) {
				unsigned int val = 0;
				write(output_fd, &val, 4);
			}
		}
		plist_array_append_item(output_plist, plist_new_string(img_files[i]->d_name));
	}
	char       *plbin;
	uint32_t    pllen;
	plist_err_t conv_err = plist_to_bin(output_plist, &plbin, &pllen);
	if (conv_err != PLIST_ERR_SUCCESS) {
		fprintf(stderr, "ERROR: Failed to convert plist file\n");
		goto cleanup;
	}
	write(output_fd, plbin, pllen);
	plist_mem_free(plbin);
	struct artwork_header hdr = { 0, pllen, artwork_width, artwork_height, 1, img_cnt, 0xdcb543a2 };
	write(output_fd, &hdr, sizeof(struct artwork_header));

	ret = EXIT_SUCCESS;
cleanup:
	for (int i = 0; i < img_cnt; i++) {
		if (all_imgs[i].s)
			png_destroy_read_struct(&all_imgs[i].s, &all_imgs[i].i, NULL);
		free(img_files[i]);
	}
	free(all_imgs);
	free(img_files);
	plist_free(output_plist);
	close(output_fd);

	return ret;
}
