#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>


char buffer[1000];
char *words[100];
int nWords = 0;
extern int errno;
int children[100];
int nChild = 0;

void controlCHandler(int signo);

// exit function
void exitF(){
    printf("ndshell: Exiting shell immediatly\n");
    exit(0);
}

int startF(){
    int rc = fork();

    if (rc < 0){
        printf("ndshell: Error forking\n");
        return -1;
    } else if (rc == 0){
        // child
        int ret;
        // printf("words[1]: %s", words[1]);
        ret = execvp(words[1], words + 1);
        // handle error
        if (ret < 0){
            printf("ndshell: Error calling execvp\n");
            return ret;
        }

    } else {
        // parent 
        printf("ndshell: process %i started\n", rc);
        children[nChild] = rc;
        nChild ++;
        return rc;
    }
    return 1;
}


// wait function
int waitF(){
    int status;
    int pid = wait(&status);
    if (pid < 0){
        printf("ndshell: No children.\n");
        return pid;
    } else if (WIFEXITED(status)){
        printf("ndshell: process %i exited normally with status %i\n", pid, WEXITSTATUS(status));
        return 0;
    } else {
        printf("ndshell: process %i exited abnormally with signal %i\n", pid, WTERMSIG(status));
        return -1;

    }
}

// waitfor function
int waitforF(int pid) {
    int status;
    
    int ret = waitpid(pid, &status, 0);
    if (ret < 0){
        printf("ndshell: Error waiting for PID %i, process may not exist\n", pid);
        return -1;
    } else if (WIFEXITED(status)){
        printf("ndshell: process %i exited normally with status %i\n", pid, WEXITSTATUS(status));
        return 0;
    } else {
        printf("ndshell: process %i exited abnormally with signal %i\n", pid, WTERMSIG(status));
        return -1;

    }
}

// run function
int runF(){
    int rc = fork();

    if (rc < 0){
        printf("ndshell: Error forking\n");
        return -1;
    } else if (rc == 0){
        // child
        int ret;
        ret = execvp(words[1], words + 1);
        // handle error
        if (ret < 0){
            printf("ndshell: Error calling execvp\n");
            return ret;
        }

    } else {
        // parent 
        printf("ndshell: process %i started\n", rc);
        children[nChild] = rc;
        nChild ++;
        waitforF(rc);
        return rc;
    }
    return 1;
}

// kill function
int killF(int pid){
    int ret = kill(pid, SIGKILL);
    if (ret < 0){
        printf("ndshell: Error killing process %i\n", pid);
        return -1;
    }
    return waitforF(pid);
}


// quit function
void quitF(){
    while (nChild > 0){
        int pid = children[nChild - 1];
        killF(pid);
        nChild --;
    }
    printf("All child processes complete - exiting the shell.\n");
    exit(0);
}


//bound function
void boundF(int timeLimit){
    int rc = fork();

    if (rc < 0){
        printf("ndshell: Error forking for bound\n");
    } else if (rc == 0){
        // child
        int ret;
        // printf("words[1]: %s", words[1]);
        ret = execvp(words[2], words + 2);
        // handle error
        if (ret < 0){
            printf("ndshell: Error calling execvp for bound\n");
        }

    } else {
        // parent 
        printf("ndshell: process %i started\n", rc);
        children[nChild] = rc;
        nChild ++;
        int result = sleep (timeLimit);
        if (result == 0){
            printf("ndshell: process %i exceeded the time limit, killing it...\n",rc);
            killF(rc);
        }
    }




}

// get instructions
int getInput(){
    // input
    printf("ndshell> ");
    while(!feof(stdin)){
        while(fgets(buffer, 1000, stdin) != NULL){
            nWords = 0;
            // split line into words
            char *token;
            token = strtok(buffer, " \t\n");
            while (token != NULL){
                // printf("%s ", token);
                words[nWords] = token;
                nWords ++;
                token = strtok(0, " \t\n");
            }
            words[nWords] = 0;
            fflush(stdout);

            // trigger commands
            if(nWords != 0){
                if (strcmp(words[0], "exit") == 0){
                    exitF();
                } else if (strcmp(words[0], "start") == 0){
                    startF();
                } else if (strcmp(words[0], "wait") == 0){
                    waitF();
                } else if (strcmp(words[0], "waitfor") == 0) {
                    if(words[1] == NULL){
                        printf("incorrect usage\n");
                    } else {
                        int pid = atoi(words[1]);
                        if (pid <= 0){
                            printf("Error, invalid PID\n");
                        } else{
                            waitforF(pid);
                        }
                    }

                } else if (strcmp(words[0], "run") == 0) {
                    runF();
                } else if (strcmp(words[0], "kill") == 0) {
                    if(words[1] == NULL){
                        printf("incorrect usage\n");
                    } else {
                        int pid = atoi(words[1]);
                        killF(pid);
                    }
                } else if (strcmp(words[0], "quit") == 0) {
                    quitF();
                } else if (strcmp(words[0], "bound") == 0) {
                    if(words[1] == NULL){
                        printf("incorrect usage\n");
                    } else {
                        int timeLimit = atoi(words[1]);
                        boundF(timeLimit);
                    }
                }
            }
            printf("ndshell> ");

            fflush(stdin);
        }

    }
    return 1;
}


void controlCHandler(int signo) {
    if (nChild > 0 ){
        killF(children[nChild - 1]);
        children[nChild -1] = 0;
        nChild -= 1;
    }
    // getInput();
}

void childHandler(int signo){
    // does not have to do anything
}

int main(int argc, char *argv[]){
    // custom sigactions

    struct sigaction new_action, old_action;
    new_action.sa_handler = controlCHandler;
    sigemptyset(&new_action.sa_mask);
    new_action.sa_flags = 0;
    sigaction(SIGINT, NULL, &old_action);
    if (old_action.sa_handler != SIG_IGN){
        sigaction(SIGINT, &new_action, NULL);
    }

    struct sigaction new_action2;
    new_action2.sa_handler = controlCHandler;
    sigemptyset(&new_action2.sa_mask);
    new_action2.sa_flags = 0;
    sigaction(SIGCHLD, &new_action2, NULL);

    // run program
    getInput();
    return 1;
}