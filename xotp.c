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

	char *infile_name, *padfile_name, *outfile_name;

	FILE *infile, *padfile, *outfile;
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
	if (
		state->padfile_name && (state->is_writing_to_pad 
		|| state->is_writing_to_default_pad)
	)
		remove(state->padfile_name);
	if (state->outfile_name && state->is_writing_to_outfile)
		remove(state->outfile_name);
	free(state->tofree);
	exit(EXIT_FAILURE);
}

static inline bool streq(const char *s1, const char *s2)
{
	return strcmp(s1, s2) == 0;
}

/* Print error message and exit */
#define P_EMSG_X(...) \
	fprintf(stderr, "Error : "__VA_ARGS__), exit(EXIT_FAILURE)

/* Print system error message and do action */
#define P_SYSE_DO(action, ...)  do {                                   \
	fprintf(stderr, "Error : "__VA_ARGS__);                        \
	int e = errno;                                                 \
	if (e) fprintf(stderr, "System Message : %s.\n", strerror(e)); \
	action;                                                        \
} while (0)

/* Print system error message and exit */
#define P_SYSE_X(...) P_SYSE_DO(exit(EXIT_FAILURE), __VA_ARGS__)

/* Requires argc >= 2 */
static xotpState xotpState_init(int argc, char **argv)
{
	xotpState state = {
		.is_generating_pad = true, 
		.is_encrypting = streq("e", argv[1])
	};

	if (!state.is_encrypting) {
		if (streq("d", argv[1])) 
			state.is_generating_pad = false;
		else
			P_EMSG_X(
				"Unrecognised argument : \"%s\".\n"
				"Only \"e\" and \"d\" are valid modes.\n", argv[1]
			);
	}

	/* Search argv for options. Once found, subsequent matches ignored. */
	for (int i = 2; i < argc; i++) {
		if (!state.infile_name && streq("-f", argv[i]))
			state.infile_name = argv[++i];
		else if (!state.padfile_name && streq("-p", argv[i]))
			state.padfile_name = argv[++i];
		else if (!state.outfile_name && streq("-o", argv[i]))
			state.outfile_name = argv[++i];

		if (state.infile_name && state.padfile_name && state.outfile_name)
			break;				
	}

	if (state.infile_name) {
		if (!( state.infile = fopen(state.infile_name, "rb") ))
			P_SYSE_X("Could not open file \"%s\".\n", state.infile_name);
	} else 
		state.infile = stdin;

	if (state.padfile_name) {
		state.padfile = fopen(state.padfile_name, "rb");
		state.is_generating_pad = false;

		if (state.is_encrypting && !state.padfile) {
			state.padfile = fopen(state.padfile_name, "wb");
			state.is_writing_to_pad = true;
			state.is_generating_pad = true;
		}
		if (!state.padfile)
			P_SYSE_X("Could not open pad \"%s\".\n", state.padfile_name);
	} else {
		if (state.is_encrypting) {
			state.is_writing_to_default_pad = true;

			if (!(
				state.padfile_name = strdupcat(state.infile_name?
					state.infile_name : DEFAULT_PAD_FILENAME, DEFAULT_PAD_EXTENSION)
			))
				P_SYSE_X("Could not malloc for pad filename.\n");

			if (!( state.padfile = fopen(state.padfile_name, "wb") )) {
				P_SYSE_DO(
					free(state.padfile_name); exit(EXIT_FAILURE),
					"Could not open \"%s\" to save pad.\n", state.padfile_name
				);
			}
			state.tofree = state.padfile_name;
		} else if (state.infile_name)
			state.padfile = stdin;
		else
			P_EMSG_X("No pad specified.\n");
	}

	if (state.outfile_name) {
		state.is_writing_to_outfile = true;

		if (!( state.outfile = fopen(state.outfile_name, "wb") ))
			P_SYSE_DO(
				state.outfile_name = NULL; errexit(&state),
				"Could not open \"%s\" to write output.\n", state.outfile_name
			);
	} else
		state.outfile = stdout;

	return state;
}

typedef unsigned char byte;

static inline bool getpad(xotpState *state, byte *restrict dst, FILE *src, size_t nbytes)
{
	if (state->is_generating_pad) {
		#ifdef OS_WIN
       		return RtlGenRandom(dst, nbytes);
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
		FILE *randf = NULL;
		
		#ifdef OS_NIX	
		if (!( randf = fopen(NIX_RAND_DEV, "rb") ))
			P_SYSE_DO(errexit(&state), "Could not open "NIX_RAND_DEV" to generate pad.\n");
		#elif defined OS_OTHER
		srand(time(0));
		#endif

		
		while (!feof(state.infile)) {
			size_t nread = fread(buf1, 1, sizeof(buf1), state.infile);

			if (ferror(state.infile))
				P_SYSE_DO(errexit(&state), "Failed reading input.\n");
			
			if (!getpad(&state, buf2, randf, nread))
				P_SYSE_DO(errexit(&state), "Failed getting pad data.\n");
			
			if (state.is_generating_pad && fwrite(buf2, 1, nread, state.padfile) != nread)		
				P_SYSE_DO(errexit(&state), "Failed writing pad data.\n");

			for (size_t i = 0; i < nread; i++)
				buf1[i] ^= buf2[i];

			if (fwrite(buf1, 1, nread, state.outfile) != nread)
				P_SYSE_DO(errexit(&state), "Failed writing output.\n"); 
		}

		free(state.tofree);
		if (fclose(state.outfile) == EOF)
			P_SYSE_X("Failed saving output.\n");
		if (state.is_generating_pad && fclose(state.padfile) == EOF)
			P_SYSE_X("Failed saving pad.\n");
		else
			exit(EXIT_SUCCESS);
	} else
		P_EMSG_X(
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
		"See https://github.com/a-p-jo/XOTP for more information.\n"
		);
}
