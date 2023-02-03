#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

int argumentO = 0;
long maxSize = 25000;
int argumentBFR = 0;
int argumentR = 0;
char outName[100] = "";
long fileSize = 0;
long written = 0;


// output for -help or incorrect usage
void correctUsage () {
    printf("Correct Use: ./bitflip <InputFile> (optional arguments)\n");
    printf("Optional Arguments:\n");
    printf("\t-o <XXX>      \t Change the output file name to XXX\n");
    printf("\t-maxsize <XXX>\t Change the maximum file size allowed to XXX bytes\n");
    printf("\t-bfr          \t Reverse the order of the bytes and bit-flip the bits in the file. Use the extension .bfr instead of .bf (May not be used with -r)\n");
    printf("\t-r            \t Reverse the order of the bytes in the file without doing a bit-flip. Use the extension .r instead of .bf (May not be used with -bfr)\n");
}


// deal with arguments beyond the required argument
int argumentHandler(int argsRemaining, char *argv[]){
    // counter for number of arguments read on this pass over argv
    int argsHandled = 1;
    if(strcmp(argv[0], "-o") == 0 && argsRemaining >= 2){
        // set state variables for -o input
        argumentO = 1;
        // pull additional argument which is the desired output name
        strcpy(outName, argv[1]);
        // increment counter (since 2 arguments used on this pass)
        argsHandled = 2;
    } else if (strcmp(argv[0], "-maxsize") == 0){
        // ensure that -maxsize is being used correctly
        if(argsRemaining >= 2){
           // get the max file size 
            maxSize = atoi(argv[1]);
            // handle incorrect second argument 
            if(maxSize == 0){
                printf("Error: Valid number not specified for -maxsize\n");
                return -1;
            }
            // increment counter (since 2 arguments used on this pass)
            argsHandled = 2;
        } else {
            printf("Error: Valid number not specified for -maxsize\n");
            return -1;
        }
    } else if (strcmp(argv[0], "-bfr") == 0){
        // set state variables for -brf input
        argumentBFR = 1;
        argumentR = 0;
    } else if (strcmp(argv[0], "-r") == 0){
        // set state variables for -r input
        argumentR = 1;
        argumentBFR = 0;
    } else {
        // if -help or unknown argument output help screen
        correctUsage();
        return -1;
    }
    // decerement remaining args by the args handled on this pass
    argsRemaining -= argsHandled;

    if(argsRemaining != 0){
        // increment the argument pointer
        argv += argsHandled;
        // call recursivly for each optional argument
        return argumentHandler (argsRemaining, argv);
    }
    return 0;
}


// bit-flip
int bitFlip(FILE *iFile, FILE *oFile) {
    // get the first byte
    char c = fgetc(iFile);
    // while there are bytes to pull from the infile flip them and write them to the outfile
    while (c != EOF){
        char nC = c ^ 0xFF;
        fwrite(&nC, 1, 1, oFile);
        written += 1;
        c = fgetc(iFile);
    }
    return 0;
}


// reverse 
int reverse (FILE *iFile, FILE *oFile) {
    // move to the last byte of the infile
    fseek(iFile, -1, SEEK_END);
    // pull the last bite
    char c = fgetc(iFile);
    // write the bytes from the end of the infile to the outfile
    while (written != fileSize){
        fwrite(&c, 1, 1, oFile);
        written += 1;
        fseek(iFile, -written - 1, SEEK_END);
        c = fgetc(iFile);
    }
    return 0;    
}


int main(int argc, char *argv[]){
    // handle arguments
    if (argc == 2) {
        // should contain InputFile
    } else if (argc > 2) {
        // contains InputFile and optional argument(s)
        int cont = argumentHandler(argc -2, argv+2);
        if (cont != 0) {
            return -1;
        }
    } else {
        // does not contain InputFile
        correctUsage();
        return -1;
    }
    // open input file
    FILE *iFile;
    if(!(iFile = fopen(argv[1], "r"))){
        printf("Error: The file %s does not exist or is not accessible.\n",argv[1]);
        return -1;
    } 
    // determine the size of the input file (code inspired by "geeksforgeeks.org/c-program-find-size-file/")
    fseek(iFile, 0, SEEK_END);
    fileSize = ftell(iFile);
    if(fileSize > maxSize) {
        printf("Error: Input file %s was too large (more than %li bytes)\n", argv[1], maxSize);
        return -1;
    }
    fseek(iFile, 0, SEEK_SET);
    // assign output file name
    if (!argumentO){
        strcpy(outName, argv[1]);
        if(argumentBFR){
            strncat(outName, ".bfr", 4);
        } else if (argumentR) {
            strncat(outName, ".r", 2);
        } else {
            strncat(outName, ".bf", 3);
        }
    }
    // check if output file already exists
    FILE *oFile;
    if((oFile = fopen(outName, "r"))) {
        printf("Error: %s already exists\n", outName);
        return -1;
    }
    // open output file for writing
    if(!(oFile = fopen(outName, "w"))){
        printf("Error: could not open %s for writing\n", outName);
        return -1;
    }
    // call bit-flip and/or reverse
    if (argumentBFR){
        // I use a temp file to store the results of the reverse which will then be bit-flipped
        // there are more efficient ways to do this but this will allow for any sized files
        FILE *tmpFile;
        if(!(tmpFile = fopen("temp.txt", "w+"))){
            printf("Error: could not create a temporary file\n");
            return -1;
        }
        // reverse 
        reverse(iFile, tmpFile);
        // return to the start of the temp file
        fseek(tmpFile, 0, SEEK_SET);
        // bit-flip
        bitFlip(tmpFile, oFile);
        // remove temp file
        remove("temp.txt");
    } else if (argumentR){
        // reverse
        reverse(iFile, oFile);
    } else {
        // bit-flip
        bitFlip(iFile, oFile);
    }
    // output results
    printf("Input: %s was %li bytes.\n", argv[1], fileSize);
    printf("Output: %s was output successfully.\n", outName);
}