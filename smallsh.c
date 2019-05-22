#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include<fcntl.h>

#define     MAXBUFFER       2048
#define     MAXARGS         512
#define     ARGLENGTH       128
#define     MAXCHILDREN     16


void clearStrings(char* buffer, char array[MAXARGS][ARGLENGTH]) {

    memset(buffer, '\0', MAXBUFFER*sizeof(char));

    int i;
    for (i=0; i<MAXARGS; i++) {
        memset(array[i], '\0', ARGLENGTH*sizeof(char));
    }

}

void removeNewLine(char* string) {
    int len = strlen(string);

    if (string[len-1] == '\n')
        string[len-1] = '\0';
}

void getCommand(char *buffer) {
    printf(": ");
    fflush(stdout);
    fgets(buffer, 2048, stdin);
    removeNewLine(buffer);
}

void split(char *buffer, char array[MAXARGS][ARGLENGTH]) {

    char *token;

    // get first string up to whitespace
    token = strtok(buffer, " ");

    // loop over the rest of the string
    int i=0;
    while (token != NULL) {

        // copy string into the command array
        strcpy(array[i], token);
        token = strtok(NULL, " ");
        i++;

    }
}

void printCommands(char array[MAXARGS][ARGLENGTH]) {

    int i=0;
    while(array[i][0] != '\0') {
        printf("%d: %s\n", i, array[i]);
        fflush(stdout);
        i++;
    }
}

int isBuiltIn(char *array) {

    if (strcmp(array, "exit") == 0) {
        return 1;
    }
    else if (strcmp(array, "cd") == 0) {
        return 2;
    }
    else if (strcmp(array, "status") == 0) {
        return 3;
    }
    else {
        return 0;
    }

}

void builtInExit(int backgroundChildren[MAXCHILDREN]) {

    int tempPID;
    int status;
    char childPID[8];

    // terminate active background children
    int i;
    for (i=0; i<MAXCHILDREN; i++) {

        // check if there is a PID at the index
        if (backgroundChildren[i] != 0) {
            
            // fork the process to terminate a child
            tempPID = fork();

            switch (tempPID) {

                case 0: //child

                    printf("Closing process %d!\n", backgroundChildren[i]);
                    fflush(stdout);

                    sprintf(childPID, "%d", backgroundChildren[i]);
                    execlp("kill", "kill", "-KILL", childPID, NULL);
                    break;

                default: //parent
                    waitpid(tempPID, &status, 0);
                    break;
            }
        }
    }

    exit(0);
}

void builtInCD(char *path) {

    int flag = 0;

    if (strcmp(path, "&") == 0) {
        strcpy(path, "");
    }

    if (strcmp(path, "") == 0) {
        flag = chdir(getenv("HOME"));
    }
    else {
        flag = chdir(path);
    }

    if (flag == -1) {
        printf("%s: No such file or directory.\n", path);
        fflush(stdout);
    }
}

void builtInStatus(int statusInfo[2]) {

    /*
        status info
            [0] 0:exit 1:signal
            [1] value
    */

    if (statusInfo[0] == 0) {
        printf("exit value %d\n", statusInfo[1]);
    }
    else {
        printf("terminating by signal %d\n", statusInfo[1]);
    }


}

void runBuiltIn(char commands[MAXARGS][ARGLENGTH], int ID, int backgroundChildren[MAXCHILDREN], int statusInfo[2]) {
    switch (ID) {
        case 1:
            builtInExit(backgroundChildren);
            break;
        case 2:
            builtInCD(commands[1]);
            break;
        case 3:
            builtInStatus(statusInfo);
            fflush(stdout);
            break;
        default:
            printf("Error in runBuiltIn\n");
            fflush(stdout);
            break;
    }
}

void tryWaiting(int backgroundChildren[MAXCHILDREN]) {
    int i;
    int exitMethod;
    int res;

    // loop over all possible children
    for (i=0; i<MAXCHILDREN; i++) {

        // check if there is an active process in that index
        if (backgroundChildren[i] != 0) {

            // attempt to wait on the process
            res = waitpid(backgroundChildren[i], &exitMethod, WNOHANG);

            // if the process returned non zero, it is done
            if (res != 0) {

                printf("background pid %d is done: ", backgroundChildren[i]);

                // check exit status or termination signal
                if (WIFEXITED(exitMethod)) {
                    printf("exit value %d\n", WEXITSTATUS(exitMethod));
                }
                else {
                    printf("terminated by signal %d\n", WTERMSIG(exitMethod));
                }
                fflush(stdout);

                // clear the childs PID from array
                backgroundChildren[i] = 0;
            }
        }
    }
}

char ** buildArgs(char commands[MAXARGS][ARGLENGTH]) {
    
    int j, i;

    // allocate a new array
    char **temp = malloc(MAXARGS * sizeof(char *));

    for (j=0; j<MAXARGS; j++) {
        temp[j] = malloc(ARGLENGTH * sizeof(char));
    }

    i = 0;
    j = 0;
    // copy commands into new array
    while (strcmp(commands[i], "") != 0) {

        if (strcmp(commands[i], "<") == 0) {
            i+=2;
        }
        else if (strcmp(commands[i], ">") == 0) {
            i+=2;
        }
        else {
            strcpy(temp[j], commands[i]);
            i++;
            j++;
        }
    }
    
    // add null to the end
    if (strcmp(temp[j-1], "&") == 0)
        temp[j-1] = NULL;
    else
        temp[j] = NULL;

    return temp;
    
}

int isComment(char *command) {

    // compare the commmand to the comment character
    if (strcmp(command, "#") == 0) {
        return 1;
    }
    return 0;
}

void freeArgs(char ** nullCommands) {

    int i;
    for (i=0; i<MAXARGS; i++) {
        free(nullCommands[i]);
    }
    free(nullCommands);

}

void childProcess(char commands[MAXARGS][ARGLENGTH]) {

    char **args;
    int status = 0;

    // build an array of the arguments, with NULL at the end
    args = buildArgs(commands);

    // execute command (checking for error)
    if(execvp(commands[0], args) == -1) {
        printf("%s: no such file or directory\n", commands[0]);
        fflush(stdout);
        status = 1;
    }
    
    freeArgs(args);

    // kill the child process
    exit(status);


}

int isBackground(char commands[MAXARGS][ARGLENGTH]) {
    int i = 0;

    while (strcmp(commands[i], "") != 0) {
        i++;
    }

    if (strcmp(commands[i-1], "&") == 0)
        return 1;

    return 0;
}

void ioRedirect(char commands[MAXARGS][ARGLENGTH], int backgroundFlag) {

    char outputFile[ARGLENGTH] = "";
    char inputFile[ARGLENGTH] = "";
    int outputID = 0, inputID = 0;
    int devNull;

    int i=0;
    while (strcmp(commands[i], "") != 0) {
        
        // check for output redirect
        if (strcmp(commands[i], ">") == 0) {
            strcpy(outputFile, commands[i+1]);
        }

        // check for input redirect
        else if (strcmp(commands[i], "<") == 0) {
            strcpy(inputFile, commands[i+1]);
        }

        // advance the loop
        i++;
    }

    // check for input redirect
    if (strcmp(inputFile, "") != 0) {
        if ((inputID = open(inputFile, O_RDONLY)) != -1 ) {
            dup2(inputID, 0);
        }
        else {
            printf("cannot open %s for input\n", inputFile);
            exit(1);
        }
    }
    // no input redirect, check for background
    else if (backgroundFlag) {
        devNull = open("/dev/null", O_RDONLY);
        dup2(devNull, 0);
    }


    // check for output redirect
    if (strcmp(outputFile, "") != 0) {

        // try to open existing file
        outputID = open(outputFile, O_WRONLY | O_TRUNC);

        // open successful
        if (outputID != -1) {
            dup2(outputID, 1);
        }

        // file does not exist, open one write only
        else {
            outputID = open(outputFile, O_WRONLY | O_CREAT);
            
            if (outputID != -1) {
                dup2(outputID, 1);
            }
            else {
                printf("cannot open %s for output\n", outputFile);
                exit(1);
            }
        }
    }
    // no output redirect, check for background
    else if (backgroundFlag) {
        devNull = open("/dev/null", O_WRONLY);
        dup2(devNull, 1);
    }
}

void addBackground(int childPID, int backgroundChildren[MAXCHILDREN]) {

    int i;
    for (i=0; i<MAXCHILDREN; i++) {
        if (backgroundChildren[i] == 0) {
            backgroundChildren[i] = childPID;
            break;
        }
    }
}

void collectData(int exitMethod, int statusInfo[2]) {

    // store exit data
    if (WIFEXITED(exitMethod)) {
        statusInfo[0] = 0;
        statusInfo[1] = WEXITSTATUS(exitMethod);
    }
    else {
        statusInfo[1] = 1;
        statusInfo[1] = WTERMSIG(exitMethod);
    }

}

void runCommannd(char commands[MAXARGS][ARGLENGTH], int backgroundChildren[MAXCHILDREN], int statusInfo[2]) {

    int exitMethod;
    int currentChildPID;

    int backgroundFlag;

    // create a fork for the non-built in command
    currentChildPID = fork();

    // check if the command is a background command
    backgroundFlag = isBackground(commands);

    // differentiate between child and parent
    switch(currentChildPID) {

        case -1:
            printf("ERROR IN FORK\n");
            fflush(stdout);
            exit(1);
            break;

        case 0: //child

            // perform appropriate IO redirection
            ioRedirect(commands, backgroundFlag);

            // run the command
            childProcess(commands);
            break;
        
        default: //parent

            // check if this is a background command
            if (backgroundFlag) {
                printf("background pid is %d\n", currentChildPID);
                fflush(stdout);
                addBackground(currentChildPID, backgroundChildren);
                break;
            }
            else { // foreground command

                // blocks parent until child completes
                waitpid(currentChildPID, &exitMethod, 0);

                // collect data about the child process exit
                collectData(exitMethod, statusInfo);
                break;
            }
    }
}

void execute(char commands[MAXARGS][ARGLENGTH], int backgroundChildren[MAXCHILDREN], int statusInfo[2]) {

    int currentChildPID;
    int exitMethod;
    int builtInID;

    // ignore comments
    if (isComment(commands[0])) {
        return;
    }

    // check if the given command is not a built in command
    if (!(builtInID = isBuiltIn(commands[0]))) {
        runCommannd(commands, backgroundChildren, statusInfo);
    }

    //built in commands
    else {
        runBuiltIn(commands, builtInID, backgroundChildren, statusInfo);
    }
}






int main() {

    /* Variable Declaration */
    char commandBuffer[MAXBUFFER];
    char commandArray[MAXARGS][ARGLENGTH];
    int backgroundChildren[MAXCHILDREN] = { 0 };
    int running = 1;
    int statusInfo[2] = { 0 };

    while (running) {

        // try waiting on active bg processes
        tryWaiting(backgroundChildren);

        // clear the strings
        clearStrings(commandBuffer, commandArray);

        // get command
        getCommand(commandBuffer);

        // split the command at whitespace
        split(commandBuffer, commandArray);

        // run the given command
        execute(commandArray, backgroundChildren, statusInfo);
    }
    return 0;
}