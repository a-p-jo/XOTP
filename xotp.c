#include <stdbool.h>
#include <limits.h>  /* UCHAR_MAX */
#include <string.h>  /* strlen(), strcmp(), memcpy(), strerror() */
#include <stdlib.h>  /* EXIT_SUCCESS, EXIT_FAILURE, malloc(), free(),
                      * exit(), rand(), srand() */
#include <errno.h>
#include <stdio.h>   /* FILE, fopen(), fclose(), fprintf(), stdin, remove()
                      * stdout, stderr, fread(), fwrite(), ferror(), feof() */

#if defined _WIN32
	#define OS_WIN
#elif defined __unix__ || (defined __APPLE__ && defined __MACH__)
	#define OS_NIX
	#define NIX_RAND_DEV "/dev/urandom"
#else
	#define OS_OTHER
	#include <time.h> /* srand(time(0)) */
#endif

#define DEFAULT_PAD_FILENAME  "pad"
#define DEFAULT_PAD_EXTENSION ".pad"
#define IO_BLKSZ              (64*1024) /* 64 KiB should be fine */

typedef struct {
	bool is_generating_pad : 1, is_encrypting : 1,
	is_writing_to_pad : 1, is_writing_to_default_pad : 1,
	is_writing_to_outfile : 1;

	char *in_filename, *pad_filename, *out_filename;

	FILE *plainfile, *cipherfile, *padfile;
	void *tofree;
} xotpState;

/* Returns malloc'd copy of s1+s2; all are '\0'-terminated */
static inline char *strdupcat(const char *s1, const char *s2)
{
	size_t l1 = strlen(s1), l2 = strlen(s2);
	char *res = malloc(l1+l2 + 1);
	if (res)
		memcpy(res, s1, l1), memcpy(res+l1, s2, l2), res[l1+l2] = '\0';
	return res;
}

static inline void errexit(const xotpState *state)
{
	if (state->pad_filename && (state->is_writing_to_pad || state->is_writing_to_default_pad))
		remove(state->pad_filename);
	if (state->out_filename && state->is_writing_to_outfile)
		remove(state->out_filename);
	free(state->tofree);
	exit(EXIT_FAILURE);
}

static inline bool streq(const char *s1, const char *s2) { return strcmp(s1, s2) == 0; }

static inline FILE **srcfile(const xotpState *state)
{
	return (FILE **) (state->is_encrypting ? &state->plainfile : &state->cipherfile); /* const cast */
}
static inline FILE **dstfile(const xotpState *state)
{
	return (FILE **) (state->is_encrypting ? &state->cipherfile : &state->plainfile); /* const cast */
}

/* Requires argc >= 2 */
static xotpState xotpState_init(int argc, char **argv)
{
	xotpState state = {.is_generating_pad = true, .is_encrypting = streq("e", argv[1])};

	if (!state.is_encrypting) {
		if (streq("d", argv[1])) 
			state.is_generating_pad = false;
		else {
			fprintf(stderr, "Error : Unrecognised argument : \"%s\".\n" 
				"Only \"e\" and \"d\" are valid modes.\n", argv[1]);
			exit(EXIT_FAILURE);
		}
	}

	/* Search argv for options. Once found, subsequent matches ignored. */
	for (int i = 2; i < argc; i++) {
		if (!state.in_filename && streq("-f", argv[i]))
			state.in_filename = argv[++i];
		else if (!state.pad_filename && streq("-p", argv[i]))
			state.pad_filename = argv[++i];
		else if (!state.out_filename && streq("-o", argv[i]))
			state.out_filename = argv[++i];

		if (state.in_filename && state.pad_filename && state.out_filename)
			break;				
	}

	if (state.in_filename) {
		if (!( *srcfile(&state) = fopen(state.in_filename, "rb") )) {
			fprintf(stderr,"Error : Could not open file \"%s\"%s%s\n",
					state.in_filename, errno? " : " : ".",
					errno? strerror(errno) : "");
			exit(EXIT_FAILURE);
		}
	} else 
		*srcfile(&state) = stdin;

	if (state.pad_filename) {
		state.padfile = fopen(state.pad_filename, "rb");
		state.is_generating_pad = false;

		if (state.is_encrypting && !state.padfile) {
			state.padfile = fopen(state.pad_filename, "wb");
			state.is_writing_to_pad = true;
			state.is_generating_pad = true;
		}
		if (!state.padfile) {
			fprintf(stderr, "Error : Could not open pad \"%s\"%s%s\n",
					state.pad_filename, errno? " : " : ".",
					errno? strerror(errno) : "");
			exit(EXIT_FAILURE);
		}
	} else {
		if (state.is_encrypting) {
			state.is_writing_to_default_pad = true;

			if (!( state.pad_filename = strdupcat(state.in_filename? state.in_filename : DEFAULT_PAD_FILENAME, DEFAULT_PAD_EXTENSION) )) {
				fprintf(stderr, "Error : Could not malloc for pad filename.\n");
				exit(EXIT_FAILURE);
			}

			if (!( state.padfile = fopen(state.pad_filename, "wb") )) {
				fprintf(stderr, "Error : Could not open \"%s\" to save pad%s%s\n",
						state.pad_filename, errno? " : " : ".",
						errno? strerror(errno) : "Unknown error");
				free(state.pad_filename);
				exit(EXIT_FAILURE);
			}
			state.tofree = state.pad_filename;
		} else if (state.in_filename)
			state.padfile = stdin;
		else {
			fprintf(stderr, "Error : No pad specified.\n");
			exit(EXIT_FAILURE);
		}
	}

	if (state.out_filename) {
		state.is_writing_to_outfile = true;

		if (!( *dstfile(&state) = fopen(state.out_filename, "wb") )) {
			fprintf(stderr,"Error : Could not open \"%s\" to write output%s%s\n",
					state.out_filename, errno? " : " : ".",
					errno? strerror(errno) : "");
			state.out_filename = NULL; /* errexit() won't remove(oname) if NULL */
			errexit(&state);
		}
	} else
		*dstfile(&state) = stdout;

	return state;
}

typedef unsigned char byte;

static inline bool getpad(xotpState *state, byte *restrict dst, FILE *src, size_t nbytes)
{
	if (state->is_generating_pad) {
		#ifdef OS_WIN
       			return RtlGenRandom(dst, nbytes)
		#elif defined OS_NIX
			return fread(dst, 1, nbytes, src) == nbytes;
		#else
			for (size_t i = 0; i < nbytes; i++)
				dst[i] = rand() % (UCHAR_MAX+1);
			return true;
		#endif
	} else
		return fread(dst, 1, nbytes, state->padfile) == nbytes;
}

int main(int argc, char **argv)
{
	if (argc >= 2 && !streq("-h", argv[1]) && !streq("--help", argv[1])) {
		xotpState state = xotpState_init(argc, argv);
		byte buf1[IO_BLKSZ], buf2[IO_BLKSZ];
		FILE *src = *srcfile(&state), *dst = *dstfile(&state), *randf = NULL;
		
		#ifdef OS_NIX
			if (!( randf = fopen(NIX_RAND_DEV, "rb") )) {
				fprintf(stderr, "Error : Could not open "NIX_RAND_DEV" to generate pad%s%s\n",
						errno? " : " : ".",
						errno? strerror(errno) : "");
				errexit(&state);
			}
		#elif defined OS_OTHER
			srand(time(0));
		#endif

		
		while (!feof(src)) {
			size_t nsrc = fread(buf1, 1, sizeof(buf1), src);
			
			if (ferror(src)) {
				fprintf(stderr,"Error reading input%s%s\n",
						errno? " : " : ".",
						errno? strerror(errno) : "");
				errexit(&state);
			}
			
			if (!getpad(&state, buf2, randf, nsrc)) {
				fprintf(stderr, "Error getting sufficient pad data%s%s\n",
						errno? " : " : ".",
						errno? strerror(errno) : "");
				errexit(&state);
			}
			
			size_t npad;
			if (state.is_generating_pad && (npad = fwrite(buf2, 1, nsrc, state.padfile)) != nsrc) {		
				fprintf(stderr,"Error saving pad data%s%s\n",
						errno? " : " : ".",
						errno? strerror(errno) : "");
				errexit(&state);
			}

			for (size_t i = 0; i < nsrc; i++)
				buf1[i] ^= buf2[i];

			size_t ndst = fwrite(buf1, 1, nsrc, dst);
			if (ndst != nsrc) {
				fprintf(stderr, "Error writing output%s%s\n",
						errno? " : " : ".",
						errno? strerror(errno) : "");
				errexit(&state);
			}
		}

		free(state.tofree);
		if (fclose(dst) == EOF) {
			fprintf(stderr,"Error saving output%s%s\n",
					errno? " : " : ".",
					errno? strerror(errno) : "");
			exit(EXIT_FAILURE);
		}
		if (state.is_generating_pad && fclose(state.padfile) == EOF) {
			fprintf(stderr,"Error saving pad%s%s\n",
					errno? " : " : ".",
					errno? strerror(errno) : "");
			exit(EXIT_FAILURE);
		} else
			exit(EXIT_SUCCESS);
	} else {
		fprintf(stderr,
		"Invalid arguments, see usage below for help.\n\n"
		"Simple Usage :\n"
		"-> \"xotp e -f foo -o foo.xotp\"\n"
		"\tEncrypts foo, pad generated and saved to foo.pad, ciphertext saved to foo.xotp .\n"
		"-> \"xotp d -f foo.xotp -p foo.pad -o foo\"\n"
		"\tDecrypts foo.xotp with pad foo.pad and saves to foo.\n"
		"\n"
		"Detailed Usage :\n"
		"-> xotp <command : e or d> [(optional) option] [(optional) option's argument] ...\n"
		"\n"
		"Options :\n"
		"\t1. \"-f\" : Give input file.\n"
		"\t Without \"-f\", input is read from stdin.\n"
		"\n"
		"\t2. \"-p\" : Give pad file.\n"
		"\t\t2.1 : If encrypting : if the pad file exists, it is read as the pad.\n"
	       	"\t\tIf not, the generated pad is saved to it.\n"
		"\t\tWithout \"-p\" , pad is saved to the input file (if specified) with a \".pad\" extension.\n"
		"\t\tOtherwise, it is saved to \"pad.pad\"\n"
		"\t\t2.2 : If decrypting : the pad file is read to decrypt the input.\n"
		"\t\tWithout \"-p\", if \"-f\" is supplied, pad is read from stdin.\n"
		"\n"
		"\t3. \"-o\" : Give output file.\n"
		"\tWithout \"-o\", output is written to stdout.\n"
		"\n"
		"See https://github.com/a-p-jo/XOTP for more information.\n");
		exit(EXIT_SUCCESS);
	}
}
