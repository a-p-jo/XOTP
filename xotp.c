/* ---------------------------
 * Preprocessor system checks |
 * ---------------------------
 */

/* Reject compilation on freestanding or older than C99 compiler */
#if !defined __STDC_HOSTED__ || __STDC_VERSION__ < 199901L
	#error "Hosted C99 implementation required !"

/* Identify OS/API for PRNG used in password generation */
#elif defined _WIN32
	#define _CRT_SECURE_NO_WARNINGS         /* Shut up, MSVC */
	#define _WIN32_WINNT _WIN32_WINNT_WINXP /* Support XP and newer */
	#define _CRT_RAND_S                     /* Required before #includes, see MS docs */
	#define WIN

#elif defined __unix__ || (defined __APPLE__ && defined __MACH__)
	#define NIX_RAND_DEV "/dev/urandom"     /* Change if you need */
	#define NIX
#else
	#include <time.h> /* srand(time(0)) */
	#define OTHER
#endif

/* ------------------------------------------------
 * Dependencies on the hosted C99 standard library |
 * ------------------------------------------------
 */

#include <stdio.h>   /* fprintf(), FILE * , fopen(), fclose(), 
		       stdin , stdout, stderr, fread(), fwrite(), strerror()  */
#include <string.h>  /* strcpy() , strcat(), strcmp()                         */
#include <stdlib.h>  /* malloc() , free(), exit() , ferror(), feof(), srand(),
		       rand()                                                 */
#include <errno.h>   /* errno                                                 */

/* ----------
 * Constants |
 * ----------
 */

typedef _Bool bool;
enum {false,true};

typedef unsigned char byte;
enum {BYTE_MAX = (byte)-1};

/* main()'s exit codes */
typedef enum {
	SUCCESS = EXIT_SUCCESS,
	NO_ARG_FOR_OPT,
	FOPEN_ERR,
	MALLOC_ERR,
	MISSING_ARG,
	MID_IO_ERR,
	FCLOSE_ERR
} retval;

/* Defaults */
#define DEFAULT_PAD_EXTENSION ".pad"    /* Used if no "-p" when encrypting, appended to "-f" arg.                  */
#define DEFAULT_PAD_NAME      "pad"     /* Used if no "-f" and no "-p" when encrypting, is used as name for pad.   */
#define CLEANUP 		        /* if defined, we remove() files that we were writing to if writing fails. */
enum {BUFSZ = 64 * 1024};               /* Block size for sequential file cloning,  64KiB by default.		   */

/* -----------------
 * Global Variables |
 * -----------------
 */

/* State variables */
bool generate_pad    = true,
     encrypting      = false,
     writing2pad     = false,
     writing2outfile = false,
     writing2defpad  = false;

/* Index of command line options if found in argv 
 * f : index for option "-f" or file.
 * p : index for option "-p" or pad.
 * o : index for option "-o" or output.
 */
size_t f,p,o; 

/* Streams used */
FILE *plain, *cipher, *pad;
/* One particular pointer cannot be free()'d until after it goes out of scope */
void *restrict tofree;

/* Functions : */
char *combine(const char *restrict str1, const char *restrict str2)
{
	size_t len1 = strlen(str1), len2 = strlen(str2);
	char *restrict ret = malloc(len1 + len2 + 1);
	if(ret) {
		memcpy(ret, str1, len1);
		memcpy(ret+len1, str2, len2);
		ret[len1+len2] = '\0';
		return ret;
	} else
		return NULL;
}

/* A tragic exit, after a fatal error. Handles free(), fclose() & remove() as needed */
void exunt(char **argv,retval err)
{
	#ifdef CLEANUP
		if(p && writing2pad)
			remove(argv[p+1]);
		else if(writing2defpad)
			remove(tofree);

		if(o && writing2outfile)
			remove(argv[o+1]);
	#endif
	free(tofree);
	exit(err);
}

/* Initialize FILE *'s and state vars by parsing command line args */
void init(char **argv)
{
	encrypting = !strcmp(argv[1],"e");
	bool decrypting = !strcmp(argv[1],"d");

	if(decrypting) 
		generate_pad = false;
	else if(!encrypting) {
		fprintf(stderr, "Unrecognised argument : \"%s\".\n" 
			"Only \"e\" and \"d\" are valid modes.\n"
			"\n", argv[1]);
		int main(int,char **); /* Prototype for main() so below line is compiled */
		main(1,NULL);          /* Go to main's else case to print usage. */
	}

	/* Search argv for 1st occurrence of options position-independency.
	 * Once an option is found, any subsequent occurrences are ignored.
	 * NOTE : argv[0] and argv[1] are reserved, search argv[2] onwards.
	 */
	for(size_t idx = 2; argv[idx]; idx++) {
		if(!f && !strcmp("-f", argv[idx]))
			f = idx;
		else if(!p && !strcmp("-p", argv[idx]))
			p = idx;
		else if(!o && !strcmp("-o", argv[idx]))
			o = idx;
		else if(f && o && p)
			break; /* do not iterate wastefully */				
	}

	/* Assign FILE *'s depending on given options.
	 * Any argument immediately after an option is taken as its file argument. 
	 * Unrecognised options are ignored.
	 */
	if(f) {
		const char *fname = argv[f+1];
		if(fname && *fname) {
			if(encrypting)
				plain = fopen(fname, "rb");
		        else
				cipher = fopen(fname, "rb");
			if(encrypting? !plain : !cipher) {
				fprintf(stderr,"Error : Could not open file \"%s\"%s%s\n",
						fname, errno? " : " : ".",
						errno? strerror(errno) : "");
				exit(FOPEN_ERR);
			}
		} else {
			fprintf(stderr,"Error : No argument given for option \"-f\"\n");
			exit(NO_ARG_FOR_OPT);
		}	
	} else {
		/* Input is sdtin by default */
		if(encrypting)
			plain = stdin;
		else
			cipher = stdin;
	}

	/* If "-p" has been given :
	 * 1. If encrypting , open file arg to write if it does not pre-exist. Else, read.
	 *    If no arg for "-p", but "-f" given, save pad to it's file arg + DEFAULT_PAD_EXTENSION.
	 *    If no "-f", save pad to DEFAULT_PAD_NAME + DEFAULT_PAD_EXTENSION.
	 *
	 * 2. If decrypting , open file arg to read (the pad from). 
	 *    If no arg for "-p", and "-f" is given, read pad from stdin.
	 */
	if(p) {
		const char *pname = argv[p+1];

		if(pname && *pname) {
			pad = fopen(pname,"rb");
			generate_pad = false;
			
			if(encrypting && !pad) {
				pad = fopen(pname, "wbx");
				writing2pad = true;
				generate_pad = true;
			}
			if(!pad) {
				fprintf(stderr, "Error : Could not open pad \"%s\"%s%s\n",
						pname, errno? " : " : ".",
						errno? strerror(errno) : "");
				exit(FOPEN_ERR);
			}
		} else {
			fprintf(stderr, "Error : No argument for option \"-p\"\n");
			exit(NO_ARG_FOR_OPT);
		}
	} else {
		if(encrypting) {
			/* If filename is given, concat it with ".pad", else use "pad.pad" */
			char *pname = combine(f? argv[f+1] : DEFAULT_PAD_NAME, DEFAULT_PAD_EXTENSION);

			if(!pname) {
				fprintf(stderr, "Error : Could not malloc for pad filename.\n");
				exit(MALLOC_ERR);
			}

			pad = fopen(pname, "wb");
			writing2defpad = true;

			if(!pad) {
				fprintf(stderr, "Error : Could not open \"%s\" to save pad%s%s\n",
						pname, errno? " : " : ".",
						errno? strerror(errno) : "Unknown error");
				free(pname);
				exit(FOPEN_ERR);
			}
			tofree = pname;
		} else {
			if(f)
				pad = stdin;
			else {
				fprintf(stderr, "Error : No pad specified.\n");
				exit(MISSING_ARG);
			}
		}

	}

	if(o) {
		const char *oname = argv[o+1];

		if(oname && *oname) {
			writing2outfile = true;

			if(encrypting)
				cipher = fopen(oname, "wb");
			else
				plain = fopen(oname, "wb");

			if(encrypting? !cipher : !plain) {
				fprintf(stderr,"Error : Could not open \"%s\" to write output%s%s\n",
						oname, errno? " : " : ".",
						errno? strerror(errno) : "");
				o = false; /* exunt won't attempt remove() on -o file if o == FALSE. */
				exunt(argv, FOPEN_ERR);
			}
		} else {
			fprintf(stderr, "Error : No argument for option \"-o\"\n");
			o = false;
			exunt(argv, NO_ARG_FOR_OPT);
		}
	} else {
		encrypting? (cipher = stdout) : (plain = stdout);
	}

}

/* Save BLOCK pad bytes to given buffer and return count of bytes saved.
 *
 * If we must generate pad :
 * 1. "FILE * randf" is unique to *NIX OSes; needs an fopen() + fclose() for the  random device.
 * 2. For Windows call RtlGenRandom(), a microsoft function that has no includes and generates
 *    "cryptographically secure" random bytes.
 * 3. For non-windows, non-unix platform fallback on portable rand() from stdlib.h - INSECURE !
 * 4. Then, save to pad.
 *
 * Else, fread(pad).
 */
size_t getpad(byte *restrict buf, size_t bytes, FILE *randf)
{
	if(generate_pad) {
		#if defined WIN
       			if(RtlGenRandom(buf, bytes))
				return bytes;
			else
				return 0;
		#elif defined NIX
			return fread(buf, 1, bytes, randf);
		#else
			size_t idx;
			for(idx = 0; idx < bytes; idx++)
				buf[idx] = rand() % (BYTE_MAX + 1);
			return idx;
		#endif
	} else
		return fread(buf, 1, bytes, pad);
}

int main(int argc, char **argv)
{
	if(argc >= 2 && strcmp(argv[1],"-h") && strcmp(argv[1],"--help"))
	{
		init(argv);

		byte buf1[BUFSZ], buf2[BUFSZ];
		FILE *src = (encrypting ? plain : cipher),
		     *dest = (encrypting ? cipher : plain),
		     *randf = NULL;
		/* Counters for error checking fread/fwrite */
		size_t frd_src, frd_pad, fwrt_dest, fwrt_pad;
	
		#if defined NIX
			randf = fopen(NIX_RAND_DEV, "rb");
			if(!randf) {
				fprintf(stderr, "Error : Could not open "NIX_RAND_DEV" to generate pad%s%s\n",
						errno? " : " : ".",
						errno? strerror(errno) : "");
				exunt(argv, FOPEN_ERR);
			}
		#elif defined OTHER
			srand(time(0));
		#endif

		while(true) {
			frd_src = fread(buf1, 1, BUFSZ, src);
			
			if(ferror(src))
			{
				fprintf(stderr,"Error reading input%s%s\n",
						errno? " : " : ".",
						errno? strerror(errno) : "");
				exunt(argv, MID_IO_ERR);
			}

			frd_pad = getpad(buf2, frd_src, randf);
			
			if(frd_pad != frd_src)
			{
				fprintf(stderr, "Error getting sufficient pad data%s%s\n",
						errno? " : " : ".",
						errno? strerror(errno) : "");
				exunt(argv, MID_IO_ERR);
			}
			
			if(generate_pad && (fwrt_pad = fwrite(buf2, 1, frd_src, pad)) != frd_src)
			{
				
				fprintf(stderr,"Error saving pad data%s%s\n",
						errno? " : " : ".",
						errno? strerror(errno) : "");
				exunt(argv, MID_IO_ERR);
			}

			for(size_t idx = 0; idx < frd_src; idx++)
				buf1[idx] ^= buf2[idx];

			fwrt_dest = fwrite(buf1, 1, frd_src, dest);

			if(fwrt_dest != frd_src)
			{
				fprintf(stderr, "Error writing output%s%s\n",
						errno? " : " : ".",
						errno? strerror(errno) : "");
				exunt(argv, MID_IO_ERR);
			}
			if(feof(src))
				break;
		}

		free(tofree);
		int rval = SUCCESS;
		if(fclose(dest))
		{
			rval = FCLOSE_ERR;
			fprintf(stderr,"Error saving output%s%s\n",
					errno? " : " : ".",
					errno? strerror(errno) : "");
		}
		if(generate_pad && fclose(pad))
		{
			rval = FCLOSE_ERR;
			fprintf(stderr,"Error saving pad%s%s\n",
					errno? " : " : ".",
					errno? strerror(errno) : "");
		}
		exit(rval);
	} else {
		fprintf(stderr,
		"Invalid arguments, see usage below for help.\n\n"
		"Simple Usage :\n"
		"-> \"xotp e -f x\"\n"
		"\tEncrypt \"foo\", pad generated and saved to foo.pad, ciphertext to foo.xotp\n"
		"-> \"xotp d -f foo.xotp -p foo.pad -o foo\"\n"
		"\tDecrypts \"foo.xotp\" with pad \"foo.pad\" and saves to \"foo\"\n"
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
		"\tWithout \"-o\", output is written to stdout\n"
		"\n"
		"See https://github.com/a-p-jo/XOTP for more information.\n"
		);
		exit(SUCCESS);
	}
}
