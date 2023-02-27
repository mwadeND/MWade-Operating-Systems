
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>


char g_bKeepLooping = 1;

#define MAX_THREADS 5

struct ChimeThreadInfo
{
    int        nIndex;
    float      fChimeInterval;
    char       bIsValid;
    pthread_t  ThreadID;
};

struct ChimeThreadInfo  TheThreads[MAX_THREADS];

void * ThreadChime (void * pData)
{
    struct ChimeThreadInfo  * pThreadInfo;

    /* Which chime are we? */
    pThreadInfo = (struct ChimeThreadInfo *) pData;

    while(g_bKeepLooping)
    {
        sleep(pThreadInfo->fChimeInterval);
        printf("Ding - Chime %d with an interval of %f s!\n", pThreadInfo->nIndex, pThreadInfo->fChimeInterval);
    }

    return NULL;
}

#define BUFFER_SIZE 1024

int main (int argc, char *argv[])
{
    char szBuffer[BUFFER_SIZE];

    /* Set all of the thread information to be invalid (none allocated) */
    for(int j=0; j<MAX_THREADS; j++)
    {
        TheThreads[j].bIsValid = 0;
    }

    while(1)
    {
        /* Prompt and flush to stdout */
        printf("CHIME>");
        fflush(stdout);

        /* Wait for user input via fgets */
        fgets(szBuffer, BUFFER_SIZE, stdin);


        // handle input
        char *words[10];
        int nWords = 0;
        // split line into words
        char *token;
        token = strtok(szBuffer, " \t\n");
        while (token != NULL){
            // printf("%s \n", token);
            words[nWords] = token;
            nWords ++;
            token = strtok(0, " \t\n");
        }
        words[nWords] = 0;
        

        /* If the command is quit - join any active threads and finish up gracefully */
        if (strcmp(words[0], "exit") == 0 || strcmp(words[0], "quit") == 0) {
            g_bKeepLooping = 0;
            for(int j=0; j<MAX_THREADS; j++)
            {
                if(TheThreads[j].bIsValid) {
                    pthread_t id = TheThreads[j].ThreadID;
                    printf("Joining Chime %i (Thread %li)\n", j, id);
                    pthread_join(id, NULL);
                    printf("Join Complete for Chime %i!\n", j);
                }

            }
            printf("Exit Chime Program...\n");
            return 0;
        }




        /* If the command is chime, the second argument is the chime number (integer) and the 
           third number is the new interval (floating point). If necessary, start the thread
           as needed */
        char validInput = 1;
        int threadIndex;
        if (nWords == 3){


            if(isdigit(words[1][0])){
                threadIndex = atoi(words[1]);
                if (0 > threadIndex || threadIndex >= MAX_THREADS){
                    validInput = 0;
                    printf("Cannot adjust chime %i, out of range\n", threadIndex);
                }
            } else {
                validInput = 0;
                printf("Invalid input for chime number\n");
            }
            // handle chime interval input
            float chimeInterval = atof(words[2]);
            if(!chimeInterval){
                validInput = 0;
                printf("Invalid input for chime interval\n");
            }



            if (strcmp(words[0], "chime") == 0 && validInput) {
                if(TheThreads[threadIndex].bIsValid){
                    // if thread already created
                    TheThreads[threadIndex].fChimeInterval = chimeInterval;
                } else {
                    // if new thread 
                    TheThreads[threadIndex].nIndex = threadIndex;
                    TheThreads[threadIndex].fChimeInterval = chimeInterval;
                    TheThreads[threadIndex].bIsValid = 1;

                    pthread_create(&TheThreads[threadIndex].ThreadID, NULL, ThreadChime, &TheThreads[threadIndex]);
                    printf("Starting thread %li for chime %i, interval of %f s\n", TheThreads[threadIndex].ThreadID, TheThreads[threadIndex].nIndex, TheThreads[threadIndex].fChimeInterval);
                }
            }




        } else {
            printf("Incorrect usage of chime\n");
        }
        



        /* Optionally, provide appropriate protection against changing the
           chime interval and reading it in a thread at the same time by using a
           mutex.  Note that it is not strictly necessary to do that */
    }
}

