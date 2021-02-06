// ID the environment 
#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
#define UNIX //"/dev/urandom" will be used for pad generaion

#elif defined _WIN32
#define WINDOWS // rand_s() will be used for pad generation
#define _CRT_RAND_S 
// as per https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/rand-s?view=msvc-160 
// this macro must be defined to use srand() from stdlib.h

#else
#define OTHER // the insecure rand() from stdlib.h will be used for pad generation
#endif

// Includes
#include <stdio.h>  // FILE I/O , printf()
#include <stdlib.h> // standard ints, malloc() & free() , perror(), srand() & rand() / rand_s() on MSVC
#include <string.h> // strcat(), strcpy()
#include <stdint.h>

#ifdef OTHER
#include <time.h>   // time(NULL) for srand() , only needed if not using rand_s() or /dev/urandom
#endif

// Configurable defaults :
#define KEY_FILE_SUFFIX ".key" // extension added to filename for keyfile generated for encryption (if own pad is not supplied)
#define ENCRYPTED_FILE_SUFFIX ".otp" // extension added to filename for encrypted file generated
#define LENGTH_OF_ENCRYPTION_SUFFIX 4 // number of letters removed from filename for output file when decrypting a file
#define UNIX_RANDOM_DEVICE "/dev/urandom"

// for clairty , renamed the 8-bit (1-byte) block as a byte.
typedef uint8_t byte;

// Important GLOBAL variables
byte * file_bytes; // byte-wise buffer of plain/encrypted file
byte * key_bytes; // byte-wise buffer of key
uint64_t buffer_size; // size of the buffers in bytes

byte encrypting; // byte storing state/mode of wether we are encrypting or decrypting
#define YES (1) // Assigned to encrypting to set mode for encryption or decryption : "Are we encrypting ?" "YES/NO"
#define NO (0)

char * duplicate_str_by_value(char * source)
{
    // returns pointer to a malloced string which is same as source by value but differs in memory location
    // required to save/buffer cmdln arguments because string.h functions modify strings in-place , destroying it's original value.

    char * dest = malloc(strlen(source)+1); // dest MUST later be freed !

    if(dest != NULL)
    {
        return strcpy(dest,source);
    }
    else
    {
        return NULL;
    }
    
}

void bufferise(FILE * file)
{
    fseek(file,0,SEEK_END); // go to end of file
    buffer_size = ftell(file); // get position (no. of bytes from start)
    rewind(file); // go back to start of file

    file_bytes = malloc(buffer_size); // allocate a file buffer as big as the file. If successful, free this at every possible exit.

    if(file_bytes != NULL)
    {
        if((fread(file_bytes,sizeof(*file_bytes),buffer_size,file)) == buffer_size); // store the entire file in the buffer . If able to read all bytes, exit.

        else
        {
            // if couldn't read all bytes , print the error, free the buffer and exit with -1
            printf("Failed : Error buffering source file.\n");
            free(file_bytes);
            exit(-1);
        }
    }
    else
    {
        // if malloc fails,print error, close the file and exit with -1
        printf("Failed : Error allocating buffer for source file.\n");
        fclose(file);
        exit(-1);
    }
}

void generate_keys(char * filename)
{
    key_bytes = malloc(buffer_size); // malloc as much space for keys as is taken by the buffer

    if(key_bytes != NULL)
    {
        char * keyfile_name = strcat(filename,KEY_FILE_SUFFIX);
        FILE * keyfile = fopen(keyfile_name,"wb");

        if ( keyfile != NULL)
        {
            /*If UNIX system, get random bytes from /dev/urandom
            * Otherwise, loop for number of bytes needed and :
            * If Windows system , use rand_s()
            * If some other system , use Cstdlib's srand() and ran() 
            * Convert the raw number into a number within the range of a single byte.
            */

            #ifdef UNIX
            FILE * urandom = fopen(UNIX_RANDOM_DEVICE,"rb");

            if(urandom != NULL)
            {
                fread(key_bytes,sizeof(*key_bytes),buffer_size,urandom);
                fclose(urandom);
            }
            else
            {
                printf("Failed : Error opening /dev/urandom.\n");
                free(file_bytes);
                free(key_bytes);
                fclose(keyfile);
                remove(keyfile_name);
                exit(-1);
            }
            #endif

           #if defined (WINDOWS) || (OTHER)
           uint64_t raw;

           #ifdef OTHER
           srand(time(NULL)); // seed rand() with time now
           #endif

           for(size_t i = 0; i < buffer_size; i++ ) // generate as many random keys as bytes in buffer
            {
                #ifdef WINDOWS
                rand_s(&raw);                
                #endif

                #ifdef OTHER
                raw = rand();
                #endif

                key_bytes[i] = (raw % (UINT8_MAX + 1)); // For range : rand() % (max_number + 1 - minimum_number) + minimum_number
            }
            #endif

            if(fwrite(key_bytes,sizeof(*key_bytes),buffer_size,keyfile) == buffer_size) // write keys to file; if written fully ,fclose the file
                fclose(keyfile);
            else
            {   // not all bytes written to file, then print error, free buffer and key buffer , fclose the file and delete it, exit with -1         
                printf("Failed : Error writing required number of bytes to keyfile.\n");
                free(file_bytes);
                free(key_bytes);
                fclose(keyfile);
                remove(keyfile_name);
                exit(-1);
            }
            
        }
        else
        {
            // file could not be openend to write , then free the buffer and key buffer and exit with -1
            perror("Failed : Error opening a keyfile ");
            free(file_bytes);
            free(key_bytes);
            exit(-1);
        }
        
    }
    else
    {
        printf("Failed : Error allocating buffer for key bytes.\n");
        exit(-1);
    }
    
}

void buffer_keys(char * filename)
{
    FILE * file = fopen(filename,"rb");
    if ( file != NULL)
    {
        key_bytes = malloc(buffer_size); // allocate a file buffer as big as the file. If successful, free this at every possible exit.

        if(key_bytes != NULL)
        {
            if((fread(key_bytes,sizeof(*key_bytes),buffer_size,file)) == buffer_size); // store the entire file in the buffer

            else
            {
                // if couldn't read all bytes , print the error, free the buffer and exit with -1
                printf("Failed : Error Reading Key.\n");
                free(file_bytes);
                free(key_bytes);
                fclose(file);
                exit(-1);
            }
        }
        else
        {
            // if malloc fails,print error, close the file and exit with -1
            printf("Failed : Memory Allocation Error.\n");
            free(file_bytes);
            fclose(file);
            exit(-1);
        }
    }
    else
    {
        // keyfile could not be opened to read , then print error , free the file buffer, close the key file.
        perror("Failed ");
        free(file_bytes);
        fclose(file);
        exit(-1);
    }
    
}

void cipher(char * filename)
{
    char * cipherfile_name;

    if(encrypting)
    {
        cipherfile_name = strcat(filename,ENCRYPTED_FILE_SUFFIX);
    }
    else
    {
        filename[(strlen(filename) - LENGTH_OF_ENCRYPTION_SUFFIX)] = '\0';
        cipherfile_name =  filename;
    }

    FILE * cipherfile = fopen(cipherfile_name,"wb");

    if (cipherfile != NULL) // "cipher()" NOT "encypher()" or "decypher()" because the same function can do both.
    {
        byte * cipher_bytes = malloc(buffer_size); // malloc as many bytes for the cipher buffer as for the original buffer

        if(cipher_bytes != NULL)
        {
            for(size_t i = 0; i < buffer_size; i++ )
            {
                cipher_bytes[i] = (file_bytes[i] ^ key_bytes[i]); // for all bytes, cipher byte =  plain byte XOR key byte
            }

            free(file_bytes);
            free(key_bytes);

            if(fwrite(cipher_bytes,sizeof(*cipher_bytes),buffer_size,cipherfile) == buffer_size) // write cipher buffer to file
            {
                free(cipher_bytes); // free the cipher buffer and close the file.
                fclose(cipherfile);
            }
            else
            {
                // couldn't complete write : print error, free pending buffer , close the file and delete it , exit with -1
                printf("Failed : Error writing %s data.\n", (encrypting == YES) ? "encrypted" : "decrypted");

                free(cipher_bytes);
                fclose(cipherfile);
                remove(cipherfile_name);
                exit(-1);
            }
            
        }
        else
        {
            // coldn't allocate memeory : print error, free buffers, close file and remove it
            printf("Failed : Memory Allocation Error.\n");
            free(file_bytes);
            free(key_bytes);
            fclose(cipherfile);
            remove(cipherfile_name);

        }
        
    }
    else
    {   // couldn't open file to save data : print error , clear buffers and exit with -1
        perror("Failed ");
        free(file_bytes);
        free(key_bytes);
        exit(-1);
    }
    
}
