#include "otp.h"

/* Encryption : xotp e plainfile (argc >= 3) OR otp e plainfile keyfile (argc >= 4)
 * Decryption : xotp d cipherfile keyfile (argc >= 4)
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

                char * filename_copy_for_cipher = duplicate_str_by_value(argv[2]);
                // v1.02 : As filename strcat'd with extension , malloc sufficient buffer instead of trusting argv[2] to have space

                if(filename_copy_for_cipher != NULL)
                {
                    if(argc == 3)
                    {
                        char * filename_copy_for_generate_keys = duplicate_str_by_value(argv[2]);

                        if(filename_copy_for_generate_keys != NULL)
                        {
                            generate_keys(filename_copy_for_generate_keys);
                            free(filename_copy_for_generate_keys);

                            cipher(filename_copy_for_cipher);
                            free(filename_copy_for_cipher);

                            printf("Encryption Successful.\n");
                        }
                        else
                        {
                            printf("Failed : Error buffering filename for generating keyfile name.\n");
                            free(file_bytes);
                            exit(-1);
                        }
                    }
                    else
                    {
                        buffer_keys(argv[3]);
                        cipher(argv[2]);
                        free(filename_copy_for_cipher);
                        printf("Encryption Successful.\n");
                    }
                }
                else
                {
                    printf("Failed : Error buffering filename for generating cipherfile name.\n");
                    free(file_bytes);
                    exit(-1);
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
            perror("Failed : Error opening source file ");
            exit(-1);
        }
    }
    else
    {
        printf("Bad arguments.\n");
    }

    return 0;
}
