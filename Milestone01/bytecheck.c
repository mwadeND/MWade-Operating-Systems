#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

int bytecheck(char* fn, char* byte);

int main( int argc, char *argv[]) {
    // verify that the correct number of arguments are present 
    if(argc == 3){ 

        // ensure the last argument is a hex number
        if(argv[2][0] == '0' && argv[2][1] == 'x' && isxdigit(argv[2][2]) && isxdigit(argv[2][3])){
            // execute the bytecheck
            return bytecheck(argv[1], argv[2]);
        } else {
            printf("Please ensure that the 2nd argument is a hexidecimal byte\n");
            return -1;
        }
    } else {
        printf("error %i args, expect 2\n", argc - 1);
        return -1;
    }
}


// function to check the binary file 
int bytecheck (char* fn, char* byteStr){
    // pointer to the file
    FILE *fp;
    // buffer to hold the data read in (not this is larger than 25 kB to ensure that any file that does not fit should be rejected by the program). This is not an efficient use of storage but it is the simplest solution.
    char buffer[25100];
    // open the file
    fp = fopen(fn, "r");
    // convert the string byte into a long integer 
    long byte = strtol(byteStr, NULL, 0);
    // count variable to store the number of times the byte is found
    int count = 0;
    // ensure that the file opened correctly
    if (fp) {
        // read the file into the buffer
        unsigned long s = fread(buffer, 1, 25100, fp);
        // handle files too large for the program
        if (s > 25000) {
            printf("Error: File over 25 kilobytes\n");
            return -1;
        } else if (s) {
            // compare each byte with the target byte, increment count if the byte is found
            for (int i = 0; i <= s; i ++){
                if(buffer[i] == byte){
                    count ++;
                }
            }
            // output final count of target byte
            printf("%i\n", count);

        } else {
            printf("Error reading file\n");
        }
        // close the file and return
        fclose(fp);
        return 0;
    } else {
        printf("Error opening file\n");
        return -1;
    }
}