/* main.c : Main file for redextract */

#include <stdio.h>
#include <stdlib.h>

/* for strdup due to C99 */
char * strdup(const char *s);

#include <string.h>
#include <sys/time.h>

#include "pcap-read.h"
#include "pcap-process.h"

int threads = 0;
int prodThreads = 2;
int consThreads = 1;

int filesToRead = 0;
int filesRead = 0;
char *files[16];
pthread_mutex_t filesReadLock;

struct timeval start, end;

pthread_t *     pThreadProducers;
pthread_t *     pThreadConsumers;

pthread_mutex_t KeepGoingLock;


void * thread_consumer(void *pData){
    struct Packet * pPacket;
    while(1){
        pthread_mutex_lock(&StackLock);
        // printf("StackSize = %i\n", StackSize);
        while(StackSize == 0){
            // printf("StackSize = %i\n", StackSize);
            pthread_mutex_lock(&KeepGoingLock);
            if (!KeepGoing) {
                pthread_cond_broadcast(&NotEmptyCond); 
                pthread_mutex_unlock(&KeepGoingLock);
                pthread_mutex_unlock(&StackLock);   
                return NULL;
            }
            pthread_mutex_unlock(&KeepGoingLock);
            // printf("Stack Empty\n");
            pthread_cond_wait(&NotEmptyCond, &StackLock);
        }
        pPacket = StackItems[StackSize-1];
        StackSize --;
        pthread_cond_signal(&NotFullCond);
        pthread_mutex_unlock(&StackLock);
        processPacket(pPacket);

    }
}


void * thread_producer(void *pData){

    while(1){
        // printf("In producer (locking)\n");
        pthread_mutex_lock(&filesReadLock);
        if(filesRead >= filesToRead){
            pthread_mutex_lock(&KeepGoingLock);
            KeepGoing -= 1;
            
            pthread_mutex_unlock(&KeepGoingLock);
            pthread_cond_broadcast(&NotEmptyCond);
            pthread_mutex_unlock(&filesReadLock);
            // printf("AAAAAAA KG = %i\n",KeepGoing);
            return NULL;
        }
        int index = filesRead;
        filesRead ++;
        pthread_mutex_unlock(&filesReadLock);
        // printf("Producer unlock\n");
        struct FilePcapInfo     theInfo;
        theInfo.FileName = strdup(files[index]);
        theInfo.EndianFlip = 0;
        theInfo.BytesRead = 0;
        theInfo.Packets = 0;
        theInfo.MaxPackets = 0;

        // printf("MAIN: Attempting to read in the file named %s\n", theInfo.FileName);
        readPcapFile(&theInfo);
    }
}



// read file in
int getFileNames(char *fileName){

    // printf("File: %s\n", fileName);

    if(strstr(fileName, ".pcap")){
        // add file name to list
        pthread_mutex_lock(&filesReadLock);
        // files[filesToRead] = fileName;
        // files[filesToRead] = malloc(128*sizeof(char));
        files[filesToRead] = strdup(fileName);
        filesToRead ++;
        pthread_mutex_unlock(&filesReadLock);
    } else {
        // if file list of pcap files
        FILE * pTheFile;
        char buffer[128] = "../";
        pTheFile = fopen(fileName, "r");
        if(!pTheFile){
            printf("error reading file\n");
            return -1;
        }
        while(fgets(buffer + 3, 125, pTheFile)) {
            // new file name 
            buffer[strcspn(buffer, "\n")] = 0;
            getFileNames(buffer);
        }
    }
    return 0;
}

int main (int argc, char *argv[])
{ 
    if(argc < 2)
    {
        printf("Usage: redextract FileX\n");
        printf("       redextract FileX\n");
        printf("  FileList        List of pcap files to process\n");
        printf("    or\n");
        printf("  FileName        Single file to process (if ending with .pcap)\n");
        printf("\n");
        printf("Optional Arguments:\n");
        /* You should handle this argument but make this a lower priority when 
           writing this code to handle this 
         */
        printf("  -threads N       Number of threads to use (2 to 8)\n");
        /* Note that you do not need to handle this argument in your code */
        printf("  -window  W       Window of bytes for partial matching (64 to 512)\n");
        printf("       If not specified, the optimal setting will be used\n");
        return -1;
    }


    for (int i = 0; i < argc; i ++){
        if(strcmp(argv[i], "-threads") == 0) {
            if(argc <= i + 1) {
                printf("Must have a number of threads after -thread\n");
                return -1;
            }
            threads = atoi(argv[i+1]);

            if (threads < 2 || threads > 8) {
                printf("Thread count must be an integer between 2 and 8\n");
                return -1;
            }
        }
    }

    pthread_mutex_init(&filesReadLock, 0);
    pthread_mutex_init(&StackLock, 0);
    pthread_mutex_init(&KeepGoingLock, 0);
    pthread_cond_init(&NotEmptyCond, NULL);
    pthread_cond_init(&NotFullCond, NULL);


    // printf("MAIN: Initializing the tables for redundancy extraction\n");
    initializeProcessing(DEFAULT_TABLE_SIZE/4);
    // printf("MAIN: Initializing the tables for redundancy extraction ... done\n");

    /* Note that the code as provided below does only the following 
     *
     * - Reads in a single file twice
     * - Reads in the file one packet at a time
     * - Process the packets for redundancy extraction one packet at a time
     * - Displays the end results
     */

    if(getFileNames(argv[1]) < 0) {
        return -1;
    }

    // printf("filesToRead: %i\n", filesToRead);

    // number of producer threads
    if(threads){
        prodThreads = filesToRead;
        if(prodThreads >= threads){
            prodThreads = threads - 1;
        }
        consThreads = threads - prodThreads;
    } else {
        prodThreads = filesToRead;
        if(prodThreads >= 8){
            prodThreads = 7;
        }
        consThreads = 1;
    }

    // printf("prodThreads: %i\n", prodThreads);
    pthread_mutex_lock(&KeepGoingLock);
    KeepGoing = prodThreads;
    pthread_mutex_unlock(&KeepGoingLock);

    pThreadProducers = (pthread_t *) malloc(sizeof(pthread_t *) * prodThreads); 
    pThreadConsumers = (pthread_t *) malloc(sizeof(pthread_t *) * consThreads); 

    gettimeofday(&start, NULL);


    // start producers
    for(int i=0; i<prodThreads; i++){
        pthread_create(pThreadProducers+i, 0, thread_producer, NULL);
    }

    // start consumers
    for(int i=0; i<consThreads; i++){
        pthread_create(pThreadConsumers+i, 0, thread_consumer, NULL);
    }

    // printf("All threads created!\n");

    // if all producers are done, and all consumers are done then the program should be done
    for(int i=0; i<prodThreads; i++){
        pthread_join(pThreadProducers[i], NULL);
        // printf("Prod Done: %i\n", i);
    }
    for(int i=0; i<consThreads; i++){
        pthread_join(pThreadConsumers[i], NULL);
        // printf("Cons Done: %i\n", i);
    }

    // struct FilePcapInfo     theInfo;

    // theInfo.FileName = strdup(argv[1]);
    // theInfo.EndianFlip = 0;
    // theInfo.BytesRead = 0;
    // theInfo.Packets = 0;
    // theInfo.MaxPackets = 5;

    // printf("MAIN: Attempting to read in the file named %s\n", theInfo.FileName);
    // readPcapFile(&theInfo);

    // printf("MAIN: Attempting to read in the file named %s again\n", theInfo.FileName);
    // readPcapFile(&theInfo);

    printf("Summarizing the processed entries\n");
    tallyProcessing();

    /* Output the statistics */

    printf("Parsing of file %s complete\n", argv[1]);

    printf("  Total Packets Parsed:    %d\n", gPacketSeenCount);
    printf("  Total Bytes   Parsed:    %lu\n", (unsigned long) gPacketSeenBytes);
    printf("  Total Packets Duplicate: %d\n", gPacketHitCount);
    printf("  Total Bytes   Duplicate: %lu\n", (unsigned long) gPacketHitBytes);

    float fPct;

    fPct = (float) gPacketHitBytes / (float) gPacketSeenBytes * 100.0;

    printf("  Total Duplicate Percent: %6.2f%%\n", fPct);

    gettimeofday(&end, NULL);
    double time = end.tv_sec + (1.0/1000000) * end.tv_usec - (start.tv_sec + (1.0/1000000) * start.tv_usec);
    // long mls = (end.tv_usec - start.tv_usec);
    printf("This program took %f seconds\n", time);

    return 0;
}
