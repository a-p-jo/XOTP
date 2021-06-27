#include <stdio.h>  // fprintf(), FILE * , fopen(), fclose(), stdin , stdout, stderr, fread(), fwrite()
#include <stdint.h> // uint64_t , uint_fast8_t
#include <string.h> // strcpy() , strcat(), strcmp() , strerror()
#include <stdlib.h> // malloc() , free(), exit() , ferror(), feof(), srand(), rand()
#include <errno.h>  // errno

/* Preprocessor checks to identify OS, to use OS-provided random numbers if possible.
 * Windows : RtlGenRandom()
 * UNIX    : NIX_RAND_DEV ("/dev/urandom" by default)
 * Else    : rand() on "other" systems (portable, std C, not ideal)
 */
#if defined _WIN32
	#ifdef _MSC_VER
		#define _CRT_SECURE_NO_WARNINGS
		#define WIN
	#else
		#error "REQUIRED : Microsoft C compiler, for RtlGenRandom() on Windows." 
	#endif

#elif defined __unix__ || (defined __APPLE__ && defined __MACH__)
	#define NIX
#else
	#define OTHER
	#include <time.h> // time(0) , seed for srand()
#endif

/* Errors & their exit codes */
#define NO_ARGS_FOR_OPTION -1
#define FOPEN_ERR 	   -2
#define MALLOC_ERR 	   -3
#define MISSING_ARG 	   -4
#define MID_IO_ERR 	   -5
#define FCLOSE_ERR         -6

/* Defaults */
#define NIX_RAND_DEV 	      "/dev/urandom" /* Random device for getting random number on a UNIX/POSIX system.         */
#define DEFAULT_PAD_EXTENSION ".pad" 	     /* Used if no "-p" when encrypting, appended to "-f" arg.	                */
#define DEFAULT_PAD_NAME      "pad"          /* Used if no "-f" and no "-p" when encrypting, is used as name for pad.   */
#define CLEANUP 		             /* if defined, we remove() files that we were writing to if writing fails. */
#define BLOCK 1048576                        /* Block size for sequential file cloning,  1 MiB by default.		*/

/* For readability */
typedef uint_fast8_t byte;
#define YES (1)
#define TRUE (1)
#define NO (0)
#define FALSE (0)

/* Global Variables : */

/* 1. State variables */
byte generate_pad    = YES;
byte encrypting;
byte writing2pad     = NO;
byte writing2outfile = NO;
byte writing2defpad  = NO;

/* 2. Vars to save index of command line options if found in argv 
 * f : index for option "-f" or file.
 * p : index for option "-p" or pad.
 * o : index for option "-o" or output.
 */
int64_t f,p,o; 

/* 3. Vars for streams used */
FILE * plain, * cipher, * pad;

/* 4. Hold pointers to free that were allocated in other functions 
 * Only one is needed as yet.
 */
void * tofree[1+1]; // +1 for NULL terminator.

/* Functions : */

/* 1. Combine two null terminated strings */
char * combine(char * str1, char * str2)
{
	char * result = malloc(strlen(str1) + strlen(str2) + 1); // +1 for '\0'

	if(!result) return NULL;

	strcpy(result,str1);
	strcat(result,str2);

	return result;
}

/* Free as many ptrs as in tofree */
void freeall(void)
{
	for(uint64_t idx = 0; tofree[idx]; idx++)
		free(tofree[idx]);
}

/* A tragic exit, after a fatal error. Handles free(), fclose() & remove() as needed */
void exunt(char ** argv,int rval)
{
	#ifdef CLEANUP
		if(p && writing2pad)
			remove(argv[p+1]);
		else if(writing2defpad)
			remove(*tofree);

		if(o && writing2outfile)
			remove(argv[o+1]);
	#endif
	
	freeall();

	exit(rval);
}

/* Initialize FILE *'s and state vars by parsing command line args */
void init(char ** argv)
{
	encrypting = !strcmp(argv[1],"e");
	int decrypting = !strcmp(argv[1],"d");

	if(decrypting) generate_pad = NO;
	
	if(!encrypting && !decrypting)
	{
		fprintf(stderr,"Unrecognised argument : \"%s\".\n" 
			"Only \"e\" and \"d\" are valid modes.\n"
			"\n",argv[1]);
		int main(int argc,char ** argv); // Prototype for main() so below line is compiled
		main(1,NULL); 			 // Go to main's else case to print usage.
	}

	/* Search argv for 1st occurrence of options position-independency.
	 * Once an option is found, any subsequent occurrences are ignored.
	 * NOTE : argv[0] and argv[1] are reserved, search argv[2] onwards.
	 */
	for(uint64_t idx = 2; argv[idx]; idx++)
	{
		if(!f && !strcmp("-f",argv[idx]))
			f = idx;
		else if(!p && !strcmp("-p",argv[idx]))
			p = idx;
		else if(!o && !strcmp("-o",argv[idx]))
			o = idx;
		else if(f && o && p)
			break; // do not iterate wastefully
				
	}

	/* Assign FILE *'s depending on given options.
	 * Any argument immediately after an option is taken as its file argument. 
	 * Unrecognised options are ignored.
	 */
	if(f) 
	{
		char * fname = argv[f+1];

		if(fname && *fname) // In other words, if (fname != NULL && strlen(fname) != 0)
		{
			if(encrypting)
				plain = fopen(fname,"rb");
		        else
				cipher = fopen(fname,"rb");

			if(encrypting? !plain : !cipher)
			{
				fprintf(stderr,"Error : Could not open file \"%s\" : %s\n",fname,errno? strerror(errno) : "Unknown error");
				exit(FOPEN_ERR);
			}
		}
		else
		{
			fprintf(stderr,"Error : No argument given for option \"-f\"\n");
			exit(NO_ARGS_FOR_OPTION);
		}	
	}
	else
	{
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
	if(p)
	{
		char * pname = argv[p+1];

		if(pname && *pname)
		{
			pad = fopen(pname,"rb");
			generate_pad = NO;
			
			if(encrypting && !pad) 
			{
				pad = fopen(pname,"wbx");
				writing2pad = YES;
				generate_pad = YES;
			}
			
			if(!pad)
			{
				fprintf(stderr,"Error : Could not open pad \"%s\" : %s\n",pname,errno? strerror(errno) : "Unknown error");
				exit(FOPEN_ERR);
			}
		}
		else
		{
			fprintf(stderr,"Error : No argument for option \"-p\"\n");
			exit(NO_ARGS_FOR_OPTION);
		}
	}
	else
	{
		if(encrypting)
		{
			/* If filename is given, concat it with ".pad", else use "pad.pad" */
			char * pname = combine(f? argv[f+1] : DEFAULT_PAD_NAME, DEFAULT_PAD_EXTENSION);

			if(!pname)
			{
				fprintf(stderr,"Error : Could not malloc for pad filename.\n");
				exit(MALLOC_ERR);
			}

			tofree[0] = pname; // So we can free it later, in another function.
			pad = fopen(pname,"wb");
			writing2defpad = YES;

			if(!pad)
			{
				fprintf(stderr,"Error : Could not open \"%s\" to save pad : %s\n",pname,errno? strerror(errno) : "Unknown error");
				free(pname);
				exit(FOPEN_ERR);
			}

			
		}
		else
		{
			if(f)
				pad = stdin;
			else
			{
				fprintf(stderr,"Error : No pad specified.\n");
				exit(MISSING_ARG);
			}
		}

	}

	if(o)
	{
		char * oname = argv[o+1];

		if(oname && *oname)
		{
			writing2outfile = YES;

			if(encrypting)
				cipher = fopen(oname,"wb");
			else
				plain = fopen(oname,"wb");

			if(encrypting? !cipher : !plain)
			{
				fprintf(stderr,"Error : Could not open \"%s\" to write output : %s\n",oname,errno? strerror(errno) : "Unknown error");
				o = FALSE; // exunt wont attempt remove() on -o file if o == FALSE.
				exunt(argv,FOPEN_ERR);
			}
		}
		else
		{
			fprintf(stderr,"Error : No argument for option \"-o\"\n");
			o = FALSE;
			exunt(argv,NO_ARGS_FOR_OPTION);
		}
	}
	else
	{
		encrypting? (cipher = stdout) : (plain = stdout);
	}

}

/* Save BLOCK pad bytes to given stream and return count of bytes saved.
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
size_t getpad(byte * buf,size_t bytes, FILE * randf)
{

	if(generate_pad)
	{
		#if defined WIN
       			if(RtlGenRandom(buf,bytes))                  // RtlGenRandom() returns boolean.
				return bytes;
			else
				return 0;
		#elif defined NIX
			return fread(buf,1,bytes,randf);
		#else
			size_t idx;
			for(idx = 0; idx < bytes; idx++)
				buf[idx] = rand() % (UINT8_MAX + 1); // Get value in range of 1 unsigned byte (0-255).

			return idx;
		#endif
	}
	else
		return fread(buf,1,bytes,pad);
}

int main(int argc, char ** argv)
{
	if(argc >= 2 && strcmp(argv[1],"-h") && strcmp(argv[1],"--help"))
	{
		init(argv);

		byte buf1[BLOCK], buf2[BLOCK];		     // Buffers 
		FILE * from = (encrypting ? plain : cipher); // "from" stream
		FILE * to = (encrypting ? cipher : plain);   // "to" stream
		FILE * randf = NULL;
		size_t frd_frm, frd_pad, fwrt_to, fwrt_pad;  // Counters (for error checking fread/fwrite)
		
		#if defined NIX
			randf = fopen(NIX_RAND_DEV,"rb");
			if(!randf)
			{
				fprintf(stderr,"Error : Could not open %s to generate pad.\n",strerror(errno));
				exunt(argv,FOPEN_ERR);
			}
		#elif defined OTHER
			srand(time(NULL));
		#endif

		while(1)
		{
			frd_frm = fread(buf1, 1, BLOCK, from);
			
			if(ferror(from))
			{
				fprintf(stderr,"Error reading input : %s\n",errno? strerror(errno) : "unknown I/O error");
				exunt(argv,MID_IO_ERR);
			}

			frd_pad = getpad(buf2, frd_frm, randf);
			
			if(frd_pad != frd_frm)
			{
				fprintf(stderr, "Error getting sufficient pad data : %s\n",errno? strerror(errno) : "unknown error");
				exunt(argv,MID_IO_ERR);
			}
			
			if(generate_pad)
			{
				fwrt_pad = fwrite(buf2, 1, frd_frm, pad);
				
				if(fwrt_pad != frd_frm)
				{
					fprintf(stderr,"Error saving pad data : %s\n",errno? strerror(errno) : "unknown I/O error");
					exunt(argv,MID_IO_ERR);
				}
			}

			for(uint64_t idx = 0; idx < frd_frm; idx++)
				buf1[idx] ^= buf2[idx];

			fwrt_to = fwrite(buf1, 1, frd_frm, to);

			if(fwrt_to != frd_frm)
			{
				fprintf(stderr, "Error writing output : %s\n",errno? strerror(errno) : "unknown I/O errnor");
				exunt(argv,MID_IO_ERR);
			}

			if(feof(from))
				break;
		}

		freeall();
		
		int rval = 0;
		if(fclose(to))
		{
			rval = FCLOSE_ERR;
			fprintf(stderr,"Error saving output : %s\n",errno? strerror(errno) : "unknown error");
		}
		if(generate_pad && fclose(pad))
		{
			rval = FCLOSE_ERR;
			fprintf(stderr,"Error saving pad : %s\n",errno? strerror(errno) : "unknown error");
		}
		exit(rval);
	}
	else
	{

		fprintf(stderr,
		"Invalid arguments, see usage below for help.\n\n"
		"Simple Usage :\n"
		"-> \"xotp e -f x\"\n"
		"\tEncrypt \"foo\", pad generated and saved to foo.pad, ciphertext to foo.xotp\n"
		"-> \"xotp d -f foo.xotp -p foo.pad -o foo\"\n"
		"\tDecrypts \"foo.xotp\" with pad \"foo.pad\" and saves to \"foo\"\n"
		"\n"
		"Detailed Usage :\n"
		"-> xotp [command : e or d] [(optional) option] [(optional) option's argument] ...\n"
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
		
		exit(0); // init() calls main() when invalid mode given; "return" will go back to init, exit() instead.
	}
}
