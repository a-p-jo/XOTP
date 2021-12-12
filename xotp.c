#if !defined __STDC_HOSTED__ || __STDC_VERSION__ < 199901L
	#error "Hosted C99 implementation required !" 
#elif defined _WIN32
	#define WIN
	#define _CRT_SECURE_NO_WARNINGS
	#define _WIN32_WINNT _WIN32_WINNT_WINXP

#elif defined __unix__ || (defined __APPLE__ && defined __MACH__)
	#define NIX
	#define NIX_RAND_DEV "/dev/urandom"
#else
	#define OTHER
	#include <time.h> /* srand(time(0)) */
#endif

#include <stdio.h>   /* FILE, fopen(), fclose(), fprintf(), stdin,
                      * stdout, stderr, fread(), fwrite(), ferror(), feof() */
#include <string.h>  /* strlen(), strcmp(), memcpy(), strerror() */
#include <stdlib.h>  /* EXIT_SUCCESS, EXIT_FAILURE, malloc(), free(),
                      * exit(), rand(), srand() */
#include <errno.h>   /* errno */
#include <stdbool.h> /* bool, true, false */
#include <limits.h>  /* UCHAR_MAX */

/* Default pad name, extension and io blocksize */
#define DEF_PAD_NAME "pad"
#define DEF_PAD_EXT  ".pad"
enum { BLKSZ = 64*1024 };

/* Global state variables */
bool genpad = true,  /* generating a new pad ? */
encrypting  = false, /* encrypting ?           */
wpad        = false, /* wrote to pad ?         */
wdefpad     = false, /* wrote to default pad ? */
woutf       = false; /* wrote to outfile       */
char *fname = NULL,  /* infile                 */
     *pname = NULL,  /* padfile                */
     *oname = NULL;  /* outfile                */
FILE *plain, *cipher, *pad;
void *tofree = NULL; /* free before exit       */

char *combine(const char *s1, const char *s2)
{
	size_t l1 = strlen(s1), l2 = strlen(s2);
	char *res = malloc(l1+l2 + sizeof('\0'));
	if(res != NULL) {
		memcpy(res, s1, l1);
		memcpy(res+l1, s2, l2);
		res[l1+l2] = '\0';
	}
	return res;
}

void errexit(void)
{
	if(pname && (wpad || wdefpad))
		remove(pname);
	if(oname && woutf)
		remove(oname);
	free(tofree);
	exit(EXIT_FAILURE);
}

#define streq(s1, s2) (strcmp(s1, s2) == 0)
#define getsrc()      (encrypting? plain : cipher)
#define setsrc(f)     (encrypting? (plain = f) : (cipher = f))
#define getdst()      (encrypting? cipher : plain)
#define setdst(f)     (encrypting? (cipher = f) : (plain = f))

void init(int argc, char **argv)
{
	encrypting = streq("e", argv[1]);
	if(!encrypting) {
		if(streq("d", argv[1])) 
			genpad = false; 
		else {
			fprintf(stderr, "Unrecognised argument : \"%s\".\n" 
				"Only \"e\" and \"d\" are valid modes.\n", argv[1]);
			exit(EXIT_FAILURE);
		}
	}

	/* Search argv for options. Once found, subsequent matches ignored. */
	for(int i = 2; i < argc; i++) {
		if(!fname && streq("-f", argv[i]))
			fname = argv[++i];
		else if(!pname && streq("-p", argv[i]))
			pname = argv[++i];
		else if(!oname && streq("-o", argv[i]))
			oname = argv[++i];
	
		if(fname && oname && pname)
			break;				
	}

	if(fname) {
		setsrc(fopen(fname, "rb"));
		if(getsrc() == NULL) {
			fprintf(stderr,"Error : Could not open file \"%s\"%s%s\n",
					fname, errno? " : " : ".",
					errno? strerror(errno) : "");
			exit(EXIT_FAILURE);
		}	
	} else 
		setsrc(stdin);

	if(pname) {
		pad = fopen(pname, "rb");
		genpad = false;
		
		if(encrypting && pad == NULL) {
			pad = fopen(pname, "wb");
			wpad = true;
			genpad = true;
		}
		if(pad == NULL) {
			fprintf(stderr, "Error : Could not open pad \"%s\"%s%s\n",
					pname, errno? " : " : ".",
					errno? strerror(errno) : "");
			exit(EXIT_FAILURE);
		}
	} else {
		if(encrypting) {
			wdefpad = true;
			pname = combine(fname? fname : DEF_PAD_NAME, DEF_PAD_EXT);

			if(pname == NULL) {
				fprintf(stderr, "Error : Could not malloc for pad filename.\n");
				exit(EXIT_FAILURE);
			}

			pad = fopen(pname, "wb");
			if(pad == NULL) {
				fprintf(stderr, "Error : Could not open \"%s\" to save pad%s%s\n",
						pname, errno? " : " : ".",
						errno? strerror(errno) : "Unknown error");
				free(pname);
				exit(EXIT_FAILURE);
			}
			tofree = pname;
		} else if(fname)
			pad = stdin;
		else {
			fprintf(stderr, "Error : No pad specified.\n");
			exit(EXIT_FAILURE);
		}
	}

	if(oname) {
		woutf = true;
		setdst(fopen(oname, "wb"));
		if(getdst() == NULL) {
			fprintf(stderr,"Error : Could not open \"%s\" to write output%s%s\n",
					oname, errno? " : " : ".",
					errno? strerror(errno) : "");
			oname = NULL; /* errexit() won't remove(oname) if NULL */
			errexit();
		}
	} else
		setdst(stdout);
}

bool getpad(unsigned char *restrict buf, size_t bytes, FILE *randf)
{
	if(genpad) {
		#ifdef WIN
       			return RtlGenRandom(buf, bytes)
		#elif defined NIX
			return fread(buf, 1, bytes, randf) == bytes;
		#else
			for(size_t i = 0; i < bytes; i++)
				buf[i] = rand() % (UCHAR_MAX+1);
			return true;
		#endif
	} else
		return fread(buf, 1, bytes, pad) == bytes;
}

int main(int argc, char **argv)
{
	if(argc >= 2 && !streq("-h", argv[1]) && !streq("--help", argv[1]))
	{
		init(argc, argv);

		unsigned char buf1[BLKSZ], buf2[BLKSZ];
		FILE *src = getsrc(), *dst = getdst(), *randf = NULL;
		
		#ifdef NIX
			randf = fopen(NIX_RAND_DEV, "rb");
			if(randf == NULL) {
				fprintf(stderr, "Error : Could not open "NIX_RAND_DEV" to generate pad%s%s\n",
						errno? " : " : ".",
						errno? strerror(errno) : "");
				errexit();
			}
		#elif defined OTHER
			srand(time(0));
		#endif

		for(size_t frd_src, fwrt_dst, fwrt_pad; !feof(src); ) {
			frd_src = fread(buf1, 1, BLKSZ, src);
			
			if(ferror(src))
			{
				fprintf(stderr,"Error reading input%s%s\n",
						errno? " : " : ".",
						errno? strerror(errno) : "");
				errexit();
			}
			
			if(!getpad(buf2, frd_src, randf))
			{
				fprintf(stderr, "Error getting sufficient pad data%s%s\n",
						errno? " : " : ".",
						errno? strerror(errno) : "");
				errexit();
			}
			
			if(genpad && (fwrt_pad = fwrite(buf2, 1, frd_src, pad)) != frd_src)
			{		
				fprintf(stderr,"Error saving pad data%s%s\n",
						errno? " : " : ".",
						errno? strerror(errno) : "");
				errexit();
			}

			for(size_t i = 0; i < frd_src; i++)
				buf1[i] ^= buf2[i];

			fwrt_dst = fwrite(buf1, 1, frd_src, dst);
			if(fwrt_dst != frd_src)
			{
				fprintf(stderr, "Error writing output%s%s\n",
						errno? " : " : ".",
						errno? strerror(errno) : "");
				errexit();
			}
		}
		
		free(tofree);
		if(fclose(dst) == EOF)
		{
			fprintf(stderr,"Error saving output%s%s\n",
					errno? " : " : ".",
					errno? strerror(errno) : "");
			exit(EXIT_FAILURE);
		}
		if(genpad && fclose(pad) == EOF)
		{
			fprintf(stderr,"Error saving pad%s%s\n",
					errno? " : " : ".",
					errno? strerror(errno) : "");
			exit(EXIT_FAILURE);
		}
		else
			exit(EXIT_SUCCESS);
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
		"See https://github.com/a-p-jo/XOTP for more information.\n");
		exit(EXIT_SUCCESS);
	}
}
