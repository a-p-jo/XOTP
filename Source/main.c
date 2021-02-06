#include "otp.h"

/* Encryption : ./otp e plainfile (argc >= 3)
 * Decryption : ./otp d cipherfile keyfile (argc >= 4)
*/

int main(int argc, char * argv[])
{
    /*
    * Conditions for running :
    * 
    * 1) There must be at least 3 arguments
    * 2) 1st arg's 1st char must be 'e' or 'd'
    * 3) If 1st arg is 'e' then there should be at least 3 arguments , otherwise ('d') 4.
    */

    if( (argc >= 3) && ((*argv[1]) == 'e' || (*argv[1]) == 'd') && (argc >= ((*argv[1]) == 'd' ? 4 : 3) ) ) 
    {
        FILE * file = fopen(argv[2],"rb");

        if ( file != NULL)
        {
            bufferise(file);
            fclose(file);
            
            if(*argv[1] == 'e')
            {
                encrypting = YES;

                if(argc == 3)
                {
                    char * filename = duplicate_str_by_value(argv[2]);
                    if(filename != NULL)
                    {
                        generate_keys(filename);
                        free(filename);
                        cipher(argv[2]);

                        printf("Encryption Successful.\n");
                    }
                    else
                    {
                        printf("Failed : Memory Allocation Error.\n");
                        free(file_bytes);
                        exit(-1);
                    }
                }
                else
                {
                    buffer_keys(argv[3]);
                    cipher(argv[2]);
                    printf("Encryption Successful.\n");
                }
            }
            else
            {               
                encrypting = NO;
                buffer_keys(argv[3]);
                cipher(argv[2]);
                printf("Decryption Successful.\n");
            }           
        }
        else
        {
            perror("Failed ");
            exit(-1);
        }
    }
    else
    {
        printf("Bad arguments.\n");
    }

    return 0;
}
