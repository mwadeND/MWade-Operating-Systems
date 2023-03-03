/*
fractal.c - Mandelbrot fractal generation
Starting code for CSE 30341 Project 3 - Spring 2023
*/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <complex.h>
#include <pthread.h>
#include <ctype.h>

#include "bitmap.h"
#include "fractal.h"

/*
Compute the number of iterations at point x, y
in the complex space, up to a maximum of maxiter.
Return the number of iterations at that point.

This example computes the Mandelbrot fractal:
z = z^2 + alpha

Where z is initially zero, and alpha is the location x + iy
in the complex plane.  Note that we are using the "complex"
numeric type in C, which has the special functions cabs()
and cpow() to compute the absolute values and powers of
complex values.
*/

#define MAX_THREADS 10
#define TASK_SIZE 20

// mutex lock
pthread_mutex_t lock;
// task counter
int nTasks;

// struct which holds all of the info needed by the thread functions
struct ThreadInfo {
    double  fMinX;
    double  fMaxX;
    double  fMinY;
    double  fMaxY;
    int     nMaxIter;
    int     yStart;
    int     yEnd;
    int     xStart;
    int     xEnd;
    /* Size of the image to generate */
    int     nPixelWidth;
    int     nPixelHeight;

    pthread_t ThreadID;
    struct bitmap * pBitmap;
};

// a list of threads
struct ThreadInfo threads[MAX_THREADS];

// struct which holds all of the info needed for a task
struct Task {
    int yStart;
    int yEnd;
    int xStart;
    int xEnd;
};

// list of Tasks
struct Task * tasks;

// counter for the next task to be done (will be locked with a mutex)
int nextTask = 0;



static int compute_point( double x, double y, int max )
{
	double complex z = 0;
	double complex alpha = x + I*y;

	int iter = 0;

	while( cabs(z)<4 && iter < max ) {
		z = cpow(z,2) + alpha;
		iter++;
	}

	return iter;
}

/*
Compute an entire image, writing each point to the given bitmap.
Scale the image to the range (xmin-xmax,ymin-ymax).

HINT: Generally, you will want to leave this code alone and write your threaded code separately

*/

void compute_image_singlethread ( struct FractalSettings * pSettings, struct bitmap * pBitmap)
{
	int i,j;

	// For every pixel i,j, in the image...

	for(j=0; j<pSettings->nPixelHeight; j++) {
		for(i=0; i<pSettings->nPixelWidth; i++) {

			// Scale from pixels i,j to coordinates x,y
			double x = pSettings->fMinX + i*(pSettings->fMaxX - pSettings->fMinX) / pSettings->nPixelWidth;
			double y = pSettings->fMinY + j*(pSettings->fMaxY - pSettings->fMinY) / pSettings->nPixelHeight;

			// Compute the iterations at x,y
			int iter = compute_point(x,y,pSettings->nMaxIter);

			// Convert a iteration number to an RGB color.
			// (Change this bit to get more interesting colors.)
			int gray = 255 * iter / pSettings->nMaxIter;

            // Set the particular pixel to the specific value
			// Set the pixel in the bitmap.
			bitmap_set(pBitmap,i,j,gray);
		}
	}
}

// function to compute a part of the image (run as a child thread in the case of rowbased threading)
void * compute_image_part(void * pData){
    struct ThreadInfo * pThreadInfo;
    pThreadInfo = (struct ThreadInfo *) pData;

	int i,j;
	// For every pixel i,j, in the image...

	for(j=pThreadInfo->yStart; j<pThreadInfo->yEnd; j++) {
        // printf("j=%i\n",j);
		for(i=pThreadInfo->xStart; i<pThreadInfo->xEnd; i++) {

			// Scale from pixels i,j to coordinates x,y
			double x = pThreadInfo->fMinX + i*(pThreadInfo->fMaxX - pThreadInfo->fMinX) / pThreadInfo->nPixelWidth;
			double y = pThreadInfo->fMinY + j*(pThreadInfo->fMaxY - pThreadInfo->fMinY) / pThreadInfo->nPixelHeight;

			// Compute the iterations at x,y
			int iter = compute_point(x,y,pThreadInfo->nMaxIter);

			// Convert a iteration number to an RGB color.
			// (Change this bit to get more interesting colors.)
			int gray = 255 * iter / pThreadInfo->nMaxIter;

            // Set the particular pixel to the specific value
			// Set the pixel in the bitmap.
			bitmap_set(pThreadInfo->pBitmap,i,j,gray);
		}
	}
    return NULL;
}

// function to handle rowbased thread creation
void compute_image_rowthread ( struct FractalSettings * pSettings, struct bitmap * pBitmap)
{
    int nThreads = pSettings->nThreads;
    printf("nThreads = %i\n", nThreads);
    int step = pSettings->nPixelHeight/nThreads;
    // create threads
    for (int i = 0; i < nThreads - 1; i ++){
        threads[i].fMinY = pSettings->fMinY;
        threads[i].fMaxY = pSettings->fMaxY;
        threads[i].fMinX = pSettings->fMinX;
        threads[i].fMaxX = pSettings->fMaxX;
        threads[i].nMaxIter = pSettings->nMaxIter;
        threads[i].nPixelWidth = pSettings->nPixelWidth;
        threads[i].nPixelHeight = pSettings->nPixelHeight;
        threads[i].yStart = step*i;
        threads[i].yEnd = step*(i+1);
        threads[i].xStart = 0;
        threads[i].xEnd = pSettings->nPixelWidth;
        threads[i].pBitmap = pBitmap;
        pthread_create(&threads[i].ThreadID, NULL, compute_image_part, &threads[i]);
        printf("starting thread %li for rows %i-%i\n", threads[i].ThreadID, threads[i].yStart, threads[i].yEnd);
    }
    int i = nThreads - 1;
    threads[i].fMinY = pSettings->fMinY;
    threads[i].fMaxY = pSettings->fMaxY;
    threads[i].fMinX = pSettings->fMinX;
    threads[i].fMaxX = pSettings->fMaxX;
    threads[i].nMaxIter = pSettings->nMaxIter;
    threads[i].nPixelWidth = pSettings->nPixelWidth;
    threads[i].nPixelHeight = pSettings->nPixelHeight;
    threads[i].yStart = step*i;
    threads[i].yEnd = pSettings->nPixelHeight;
    threads[i].xStart = 0;
    threads[i].xEnd = pSettings->nPixelWidth;
    threads[i].pBitmap = pBitmap;
    pthread_create(&threads[i].ThreadID, NULL, compute_image_part, &threads[i]);
    printf("starting thread %li for rows %i-%i\n", threads[i].ThreadID, threads[i].yStart, threads[i].yEnd);

    printf("All Threads Created\n");

    // join threads
    for (int i = 0; i < nThreads; i ++){
        pthread_t id = threads[i].ThreadID;
        printf("Joining Thread %li\n", id);
        pthread_join(id, NULL);
    }

    printf("All Threads Completed\n");
}




// child thread function for taskbased threading
void * taskThread(void * pData) {

    struct ThreadInfo * pThreadInfo;
    pThreadInfo = (struct ThreadInfo *) pData;

    // holds the index of the task to be completed
    int myTask;

    while(1){
        // lock mutex to get the number of the next task
        pthread_mutex_lock(&lock);
        myTask = nextTask;
        nextTask += 1;
        pthread_mutex_unlock(&lock);

        // return if there are no more tasks to complete;
        if(myTask >= nTasks){
            return NULL;
        }

        // set variables to values from task
        pThreadInfo->yStart = tasks[myTask].yStart;
        pThreadInfo->yEnd = tasks[myTask].yEnd;
        pThreadInfo->xStart = tasks[myTask].xStart;
        pThreadInfo->xEnd = tasks[myTask].xEnd;

        // get the image
        compute_image_part(pThreadInfo);
    }
}

// function to handle creating taskbased threads
void compute_image_taskthread ( struct FractalSettings * pSettings, struct bitmap * pBitmap) {

    // make tasks 
    int remainingRows = pSettings->nPixelHeight;
    int remainingCols = pSettings->nPixelWidth;
    int taskRows = ceil(remainingRows/TASK_SIZE);
    int taskCols = ceil(remainingCols/TASK_SIZE);
    nTasks = taskRows * taskCols;
    printf("nTasks=%i\n", nTasks);

    tasks = malloc(sizeof(struct Task) * nTasks);

    for (int i=0; i <taskRows; i++){
        for(int j=0; j <taskCols; j++){
            tasks[i * taskCols + j].xStart = j*TASK_SIZE;
            if(j < taskCols - 1){
                tasks[i * taskCols + j].xEnd = (j + 1)*TASK_SIZE;
            } else {
                tasks[i * taskCols + j].xEnd = remainingCols;
            }
            tasks[i * taskCols + j].yStart = i*TASK_SIZE;
            if(i < taskRows - 1){
                tasks[i * taskCols + j].yEnd = (i + 1)*TASK_SIZE;
            } else {
                tasks[i * taskCols + j].yEnd = remainingRows;
            }
        }
    }


    // create threads
    int nThreads = pSettings->nThreads;
    printf("nThreads = %i\n", nThreads);
    for (int i = 0; i < nThreads; i ++){
        threads[i].fMinY = pSettings->fMinY;
        threads[i].fMaxY = pSettings->fMaxY;
        threads[i].fMinX = pSettings->fMinX;
        threads[i].fMaxX = pSettings->fMaxX;
        threads[i].nMaxIter = pSettings->nMaxIter;
        threads[i].nPixelWidth = pSettings->nPixelWidth;
        threads[i].nPixelHeight = pSettings->nPixelHeight;
        threads[i].pBitmap = pBitmap;
        pthread_create(&threads[i].ThreadID, NULL, taskThread, &threads[i]);
        printf("starting thread %li\n", threads[i].ThreadID);

    }
    printf("All Threads Created\n");

    // join threads
    for (int i = 0; i < nThreads; i ++){
        pthread_t id = threads[i].ThreadID;
        printf("Joining Thread %li\n", id);
        pthread_join(id, NULL);
    }

    printf("All Threads Completed\n");

    // clean up memory
    free(tasks);
}

void displayHelp(){
    printf("Usage: ./fractal\n");
    printf("\t-help\tDisplay the help information\n");
    printf("\t-xmin X\tNew value for x min\n");
    printf("\t-xmax X\tNew value for x max\n");
    printf("\t-ymin Y\tNew value for y min\n");
    printf("\t-ymax Y\tNew value for y max\n");
    printf("\t-maxiter N\tNew value for the maximum number of iterations (must be an integer)\n");
    printf("\t-width W\tNew width for the output image\n");
    printf("\t-height H\tNew height for the output image\n");
    printf("\t-output F\tNew name for the output file\n");
    printf("\t-threads N\tNumber of threads to use for processing (default is 1) (only applies if using -row or -task)\n");
    printf("\t-row\tRun using a row-based approach\n");
    printf("\t-task\tRun using a thread-based approach\n");
    return;
}

/* Process all of the arguments as provided as an input and appropriately modify the
   settings for the project 
   @returns 1 if successful, 0 if unsuccessful (bad arguments) */
char processArguments (int argc, char * argv[], struct FractalSettings * pSettings)
{

    for (int i = 1; i < argc; i ++){
        if(strcmp(argv[i], "-xmin") == 0) {
            if(argc > i + 1){
                i++;
                if(isdigit(argv[i][0])){
                    pSettings->fMinX = strtod(argv[i], NULL);
                    // printf("fMinX = %f\n", pSettings->fMaxX);
                } else {
                    // displayHelp();
                    return 0;
                }
            } else {
                // displayHelp();
                return 0;
            }
        } else if (strcmp(argv[i], "-xmax") == 0) {
            if(argc > i + 1){
                i++;
                if(isdigit(argv[i][0])){
                    pSettings->fMaxX = strtod(argv[i],NULL);
                    // printf("fMaxX = %f\n", pSettings->fMaxX);
                } else {
                    // displayHelp();
                    return 0;
                }
            } else {
                // displayHelp();
                return 0;
            }
        } else if (strcmp(argv[i], "-ymin") == 0) {
            if(argc > i + 1){
                i++;
                if(isdigit(argv[i][0])){
                    pSettings->fMinY = strtod(argv[i], NULL);
                    // printf("fMinY = %f\n", pSettings->fMinY);
                } else {
                    // displayHelp();
                    return 0;
                }
            } else {
                // displayHelp();
                return 0;
            }
        } else if (strcmp(argv[i], "-ymax") == 0) {
            if(argc > i + 1){
                i++;
                if(isdigit(argv[i][0])){
                    pSettings->fMaxY = strtod(argv[i], NULL);
                    // printf("fMaxY = %f\n", pSettings->fMaxY);
                } else {
                    // displayHelp();
                    return 0;
                }
            } else {
                // displayHelp();
                return 0;
            }
        } else if (strcmp(argv[i], "-maxiter") == 0) {
            if(argc > i + 1){
                i++;
                if(isdigit(argv[i][0])){
                    pSettings->nMaxIter = atoi(argv[i]);
                    // printf("nMaxIter = %i\n", pSettings->nMaxIter);
                } else {
                    // displayHelp();
                    return 0;
                }
            } else {
                // displayHelp();
                return 0;
            }
        } else if (strcmp(argv[i], "-width") == 0) {
            if(argc > i + 1){
                i++;
                if(isdigit(argv[i][0])){
                    pSettings->nPixelWidth = atoi(argv[i]);
                    // printf("nPixelWidth = %i\n", pSettings->nPixelWidth);
                } else {
                    // displayHelp();
                    return 0;
                }
            } else {
                // displayHelp();
                return 0;
            }
        } else if (strcmp(argv[i], "-height") == 0) {
            if(argc > i + 1){
                i++;
                if(isdigit(argv[i][0])){
                    pSettings->nPixelHeight = atoi(argv[i]);
                    // printf("nPixelHeight = %i\n", pSettings->nPixelHeight);
                } else {
                    // displayHelp();
                    return 0;
                }
            } else {
                // displayHelp();
                return 0;
            }
        } else if (strcmp(argv[i], "-output") == 0) {
            if(argc > i + 1){
                i++;
                strcpy(pSettings->szOutfile, argv[i]);
                // printf("szOutfile = %s\n", pSettings->szOutfile);

            } else {
                // displayHelp();
                return 0;
            }
        } else if (strcmp(argv[i], "-threads") == 0) {
            if(argc > i + 1){
                i++;
                if(isdigit(argv[i][0])){
                    int num = atoi(argv[i]);
                    if(num > 0 && num <= MAX_THREADS){
                        pSettings->nThreads = num;
                        // printf("nThreads = %i\n", pSettings->nThreads);
                    } else {
                        printf("Error: max thread count is %i\n", MAX_THREADS);
                        return 0;
                    }
                } else {
                    // displayHelp();
                    return 0;
                }
            } else {
                // displayHelp();
                return 0;
            }
        } else if (strcmp(argv[i], "-row") == 0) { 
            // will use whichever argument (-row or -task) is last
            pSettings->theMode = MODE_THREAD_ROW;
        } else if (strcmp(argv[i], "-task") == 0) {
            // will use whichever argument (-row or -task) is last
            pSettings->theMode = MODE_THREAD_TASK;
        } else {
            // HELP
            // displayHelp();
            return 0;
        }
    }
    /* If we don't process anything, it must be successful, right? */
    return 1;
}




int main( int argc, char *argv[] )
{
    struct FractalSettings  theSettings;

	// The initial boundaries of the fractal image in x,y space.
    theSettings.fMinX = DEFAULT_MIN_X;
    theSettings.fMaxX = DEFAULT_MAX_X;
    theSettings.fMinY = DEFAULT_MIN_Y;
    theSettings.fMaxY = DEFAULT_MAX_Y;
    theSettings.nMaxIter = DEFAULT_MAX_ITER;

    theSettings.nPixelWidth = DEFAULT_PIXEL_WIDTH;
    theSettings.nPixelHeight = DEFAULT_PIXEL_HEIGHT;

    theSettings.nThreads = DEFAULT_THREADS - 1;
    theSettings.theMode  = MODE_THREAD_SINGLE;
    
    strncpy(theSettings.szOutfile, DEFAULT_OUTPUT_FILE, MAX_OUTFILE_NAME_LEN);

    /* TODO: Adapt your code to use arguments where the arguments can be used to override 
             the default values 

        -help         Display the help information
        //-xmin X       New value for x min
        //-xmax X       New value for x max
        //-ymin Y       New value for y min
        //-ymax Y       New value for y max
        //-maxiter N    New value for the maximum number of iterations (must be an integer)     
        //-width W      New width for the output image
        //-height H     New height for the output image
        //-output F     New name for the output file
        //-threads N    Number of threads to use for processing (default is 2) (only applies if using -row or -task)
        //-row          Run using a row-based approach        
        //-task         Run using a thread-based approach

        Support for setting the number of threads is optional

        You may also appropriately apply reasonable minimum / maximum values (e.g. minimum image width, etc.)
    */


   /* Are there any locks to set up? */


   if(processArguments(argc, argv, &theSettings))
   {
        /* Dispatch here based on what mode we might be in */
        if(theSettings.theMode == MODE_THREAD_SINGLE)
        {
            /* Create a bitmap of the appropriate size */
            struct bitmap * pBitmap = bitmap_create(theSettings.nPixelWidth, theSettings.nPixelHeight);

            /* Fill the bitmap with dark blue */
            bitmap_reset(pBitmap,MAKE_RGBA(0,0,255,0));

            /* Compute the image */
            compute_image_singlethread(&theSettings, pBitmap);

            // Save the image in the stated file.
            if(!bitmap_save(pBitmap,theSettings.szOutfile)) {
                fprintf(stderr,"fractal: couldn't write to %s: %s\n",theSettings.szOutfile,strerror(errno));
                return 1;
            }            
        }
        else if(theSettings.theMode == MODE_THREAD_ROW)
        {
            /* A row-based approach will not require any concurrency protection */

            /* Could you send an argument and write a different version of compute_image that works off of a
               certain parameter setting for the rows to iterate upon? */

            /* Create a bitmap of the appropriate size */
            struct bitmap * pBitmap = bitmap_create(theSettings.nPixelWidth, theSettings.nPixelHeight);

            /* Fill the bitmap with dark blue */
            bitmap_reset(pBitmap,MAKE_RGBA(0,0,255,0));

            /* Compute the image */
            compute_image_rowthread(&theSettings, pBitmap);

            // Save the image in the stated file.
            if(!bitmap_save(pBitmap,theSettings.szOutfile)) {
                fprintf(stderr,"fractal: couldn't write to %s: %s\n",theSettings.szOutfile,strerror(errno));
                return 1;
            }      

        }
        else if(theSettings.theMode == MODE_THREAD_TASK)
        {
            /* For the task-based model, you will want to create some sort of a way that captures the instructions
               or task (perhaps say a startX, startY and stopX, stopY in a struct).  You can have a global array 
               of the particular tasks with each thread attempting to pop off the next task.  Feel free to tinker 
               on what the right size of the work unit is but 20x20 is a good starting point.  You are also welcome
               to modify the settings struct to help you out as well.  
               
               Generally, it will be good to create all of the tasks into that array and then to start your threads
               with them in turn attempting to pull off a task one at a time.  
               
               While we could do condition variables, there is not really an ongoing producer if we create all of
               the tasks at the outset. Hence, it is OK whenever a thread needs something to do to try to access
               that shared data structure with all of the respective tasks.  
               */

            // init mutex
            if(pthread_mutex_init(&lock, NULL) != 0){
                printf("Mutex failed\n");
                return -1;
            }

            /* Create a bitmap of the appropriate size */
            struct bitmap * pBitmap = bitmap_create(theSettings.nPixelWidth, theSettings.nPixelHeight);

            /* Fill the bitmap with dark blue */
            bitmap_reset(pBitmap,MAKE_RGBA(0,0,255,0));

            /* Compute the image */
            compute_image_taskthread(&theSettings, pBitmap);

            // Save the image in the stated file.
            if(!bitmap_save(pBitmap,theSettings.szOutfile)) {
                fprintf(stderr,"fractal: couldn't write to %s: %s\n",theSettings.szOutfile,strerror(errno));
                return 1;
            }      

        }
        else 
        {
            /* Uh oh - how did we get here? */
            displayHelp();
            return -1;
        }
   }
   else
   {
        /* Probably a great place to dump the help */
        displayHelp();
        /* Probably a good place to bail out */
        exit(-1);
   }

    /* TODO: Do any cleanup as required */

	return 0;
}
