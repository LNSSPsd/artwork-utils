## artwork-utlis

utilities to manipulate Apple deprecated `.artwork` files.

Should work with the ones appear on iOS 8 ~ 15, etc. Not compatible with pre iOS 8 format.

### Usage

```bash
artwork-utils enartwork [options...] [-c] <contents directory> [-o] <output>
	--help                   Display this help message.
	-S, --append-scale       Append image scale to file name. (e.g. output@2x.artwork)
	-c, --contents-directory Specify the directory containing all input images.
	-o, --output             Specify the output artwork file.

artwork-utils unartwork [options...] [-i|--artwork] <artwork file> [-D|--output-directory] <output directory>
	--help                  Display this help message.
	--raw                   Export raw BGRA data.
	-i, --artwork           Specify artwork file.
	-D, --output-directory  Specify output directory.
	--force-scale           Specify artwork scale, overriding autodetected value.
	-l, --list              Print the contents, do not generate output.
```
