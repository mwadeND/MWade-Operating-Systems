
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>

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

        /* If the command is quit - join any active threads and finish up gracefully */
    

        /* If the command is chime, the second argument is the chime number (integer) and the 
           third number is the new interval (floating point). If necessary, start the thread
           as needed */

        /* Optionally, provide appropriate protection against changing the
           chime interval and reading it in a thread at the same time by using a
           mutex.  Note that it is not strictly necessary to do that */
    }
}

