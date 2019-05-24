#include    <stdio.h>
#include    <string.h>
#include    <unistd.h>
#include    <stdlib.h>
#include    <fcntl.h>

#define     MAXBUFFER       2048
#define     MAXARGS         512
#define     ARGLENGTH       128
#define     MAXCHILDREN     16


/***************** PRE EXECUTION *****************/
void tryWaiting(int backgroundChildren[MAXCHILDREN]);
void clearData(char* buffer, char array[MAXARGS][ARGLENGTH]);
void removeNewLine(char* string);
void getCommand(char *buffer);
void split(char *buffer, char array[MAXARGS][ARGLENGTH]);
int isComment(char *command);


/***************** EXECUTION *********************/
void execute(char commands[MAXARGS][ARGLENGTH], int backgroundChildren[MAXCHILDREN], int statusInfo[2]);


/***************** BUILT IN **********************/
int isBuiltIn(char *array);
void builtInExit(int backgroundChildren[MAXCHILDREN]);
void builtInCD(char *path);
void builtInStatus(int statusInfo[2]);
void runBuiltIn(char commands[MAXARGS][ARGLENGTH], int ID, int backgroundChildren[MAXCHILDREN], int statusInfo[2]);


/***************** NON BUILT IN ******************/
void runCommannd(char commands[MAXARGS][ARGLENGTH], int backgroundChildren[MAXCHILDREN], int statusInfo[2]);
int isBackground(char commands[MAXARGS][ARGLENGTH]);


/***************** REDIRECTING IO ****************/
void ioRedirect(char commands[MAXARGS][ARGLENGTH], int backgroundFlag);
void getFiles(char commands[MAXARGS][ARGLENGTH], char *outputFile, char *inputFile);
void redirectInput(char *inputFile, int backgroundFlag);
void redirectOutput(char *outputFile, int backgroundFlag);


/***************** NON BUILT IN (CHILD) **********/
void childProcess(char commands[MAXARGS][ARGLENGTH]);
char ** buildExecArgs(char commands[MAXARGS][ARGLENGTH]);
void copyExecArgs(char commands[MAXARGS][ARGLENGTH], char **temp);
void expandPID(char* currentCommand, char* destination);
void freeExecArgs(char ** nullCommands);


/***************** NON BUILT IN (PARENT) *********/
void addToBackground(int childPID, int backgroundChildren[MAXCHILDREN]);
void collectData(int exitMethod, int statusInfo[2]);




/***************** MAIN **************************/
int main() {

    /* Variable Declaration */
    char commandBuffer[MAXBUFFER];                  // buffer for reading commands
    char commandArray[MAXARGS][ARGLENGTH];          // array for breaking up commands
    int backgroundChildren[MAXCHILDREN] = { 0 };    // array for tracking background processes
    int statusInfo[2] = { 0 };                      // array to hold exit status information

    while (1) {

        // try waiting on active bg processes
        tryWaiting(backgroundChildren);

        // clear the strings
        clearData(commandBuffer, commandArray);

        // get command
        getCommand(commandBuffer);

        // split the command at whitespace
        split(commandBuffer, commandArray);

        // run the given command
        execute(commandArray, backgroundChildren, statusInfo);
    }
    return 0;
}




/***************** PRE EXECUTION *****************/
void tryWaiting(int backgroundChildren[MAXCHILDREN]) {
    int i;
    int exitMethod;
    int res;

    // loop over all possible children
    for (i=0; i<MAXCHILDREN; i++) {

        // check if there is an active process in that index
        if (backgroundChildren[i] != 0) {

            // attempt to wait on the process (dont block)
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

                // flush stdout
                fflush(stdout);

                // clear the childs PID from array
                backgroundChildren[i] = 0;
            }
        }
    }
}

void clearData(char* buffer, char array[MAXARGS][ARGLENGTH]) {

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

int isComment(char *command) {

    // compare the commmand to the comment character
    if (strcmp(command, "#") == 0) {
        return 1;
    }
    return 0;
}




/***************** EXECUTION *********************/
void execute(char commands[MAXARGS][ARGLENGTH], int backgroundChildren[MAXCHILDREN], int statusInfo[2]) {

    int builtInID;

    // ignore comments
    if (isComment(commands[0])) {
        return;
    }

    // check if the given command is a built in command
    if ((builtInID = isBuiltIn(commands[0]))) {
        runBuiltIn(commands, builtInID, backgroundChildren, statusInfo);
    }

    // run other commands
    else {
        runCommannd(commands, backgroundChildren, statusInfo);
    }
}




/***************** BUILT IN **********************/
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

    fflush(stdout);


}




/***************** NON BUILT IN ******************/
void runCommannd(char commands[MAXARGS][ARGLENGTH], int backgroundChildren[MAXCHILDREN], int statusInfo[2]) {

    /* Variable Declaration */
    int exitMethod;                 // stores info about how the child exited
    int currentChildPID;            // stores PID of forked child
    int backgroundFlag;             // 1 if a background command, other 0

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

                // tell user the background PID
                printf("background pid is %d\n", currentChildPID);
                fflush(stdout);

                // add the PID to the array of active background commands
                addToBackground(currentChildPID, backgroundChildren);

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

int isBackground(char commands[MAXARGS][ARGLENGTH]) {
    int i = 0;

    // loop until end of commands array is reached
    while (strcmp(commands[i], "") != 0) {
        i++;
    }

    // check if the last command is an "&"
    if (strcmp(commands[i-1], "&") == 0)
        return 1;

    return 0;
}




/***************** REDIRECTING IO ****************/
void ioRedirect(char commands[MAXARGS][ARGLENGTH], int backgroundFlag) {

    /* Variable Declaration */
    char outputFile[ARGLENGTH] = "";    // handle for output file
    char inputFile[ARGLENGTH] = "";     // handle for input file

    // parse the command array for IO files
    getFiles(commands, outputFile, inputFile);

    // perform redirection
    redirectInput(inputFile, backgroundFlag);
    redirectOutput(outputFile, backgroundFlag);

}

void getFiles(char commands[MAXARGS][ARGLENGTH], char *outputFile, char *inputFile) {

    int i=0;

    // loop until end of commands array is reached
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
}

void redirectInput(char *inputFile, int backgroundFlag) {

    int inputID, devNull;

    // check for input redirect
    if (strcmp(inputFile, "") != 0) {

        // attempt to open file and redirect stdin to file
        if ((inputID = open(inputFile, O_RDONLY)) != -1 ) {
            dup2(inputID, 0);
        }

        // open error
        else {
            printf("cannot open %s for input\n", inputFile);
            fflush(stdout);
            exit(1);
        }
    }

    // no output redirect, check for background, redirect to devnull
    else if (backgroundFlag) {
        devNull = open("/dev/null", O_RDONLY);
        dup2(devNull, 0);
    }

    else {
        // no redirection needed
    }

}

void redirectOutput(char *outputFile, int backgroundFlag) {

    int outputID, devNull;
    
    // check for output redirect
    if (strcmp(outputFile, "") != 0) {

        // try to open existing file
        outputID = open(outputFile, O_WRONLY | O_TRUNC);

        // open successful, redirect stdout to file
        if (outputID != -1) {
            dup2(outputID, 1);
        }

        // file does not exist, open one write only
        else {

            // attempt to create and open a new file
            outputID = open(outputFile, O_WRONLY | O_CREAT);
            
            // open successful, redirect stdout to file
            if (outputID != -1) {
                dup2(outputID, 1);
            }

            // open error
            else {
                printf("cannot open %s for output\n", outputFile);
                fflush(stdout);
                exit(1);
            }
        }
    }

    // no output redirect, check for background, redirect to devnull
    else if (backgroundFlag) {
        devNull = open("/dev/null", O_WRONLY);
        dup2(devNull, 1);
    }

    else {
        // do nothing
    }
}




/***************** NON BUILT IN (CHILD) **********/
void childProcess(char commands[MAXARGS][ARGLENGTH]) {

    /* Variable Declaration */
    char **args;                    // dynamic array of strings, holds arguments for EXEC
    int status = 0;                 // holds status of child's exit

    // build a NULL terminated array of arguments
    args = buildExecArgs(commands);

    // execute command (checking for error)
    if(execvp(commands[0], args) == -1) {
        printf("%s: no such file or directory\n", commands[0]);
        fflush(stdout);
        status = 1;                 // set exit status to 1
    }
    
    // free dynamic array
    freeExecArgs(args);

    // kill the child process
    exit(status);
}

char ** buildExecArgs(char commands[MAXARGS][ARGLENGTH]) {
    
    /* Variable Declaration */
    int i;
    char PID[8];

    // allocate an array of char* for arguments
    char **temp = malloc(MAXARGS * sizeof(char *));
    for (i=0; i<MAXARGS; i++) {
        temp[i] = malloc(ARGLENGTH * sizeof(char));
    }

    // copy arguments into temp, including NULL termination
    copyExecArgs(commands, temp);

    return temp;
    
}

void copyExecArgs(char commands[MAXARGS][ARGLENGTH], char **temp) {

    /* Variable Declarations */
    int i = 0;                       // counter for commands array
    int j = 0;                      // counter for temp array

    // loop until end of commands array is reached
    while (strcmp(commands[i], "") != 0) {

        // skip IO redirect parameters
        if (strcmp(commands[i], "<") == 0) {
            i+=2;
        }

        // skip IO redirect parameters
        else if (strcmp(commands[i], ">") == 0) {
            i+=2;
        }

        // check if $$ is in the command
        else if (strstr(commands[i], "$$")) {

            // expand $$ into PID of the shell
            expandPID(commands[i], temp[j]);

            i++;
            j++;
        }

        // copy the argument into the new array
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

}

void expandPID(char* currentCommand, char* destination) {

    /* Variable Declarations */
    int i = 0;                      // counter for current command   
    int j = 0;                      // counter for destination
    int x;                          // counter for PID
    char temp[8] = { '\0' };        // temp array for PID

    // run while there is still part of the current command to copy
    while (currentCommand[i]) {

        // check if "$$" has been encountered
        if (currentCommand[i] == '$' && currentCommand[i+1] == '$') {

            // copy the PID into temp
            sprintf(temp, "%d", getppid());

            // copy pid into destination
            x = 0;
            while (temp[x]) {
                destination[j] = temp[x];
                j++;
                x++;
            }

            // skip over the $$ in the current command
            i += 2;
        }

        // otherwise copy character
        else {
            destination[j] = currentCommand[i];
            i++;
            j++;
        }
    }
}

void freeExecArgs(char ** commands) {

    int i;
    for (i=0; i<MAXARGS; i++) {
        free(commands[i]);
    }
    free(commands);

}




/***************** NON BUILT IN (PARENT) *********/
void addToBackground(int childPID, int backgroundChildren[MAXCHILDREN]) {

    int i;

    // loop over the backgroundChildren array
    for (i=0; i<MAXCHILDREN; i++) {

        // check if current index is clear
        if (backgroundChildren[i] == 0) {

            // add child PID to array
            backgroundChildren[i] = childPID;

            // break the loop
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

    // store signal data
    else {
        statusInfo[1] = 1;
        statusInfo[1] = WTERMSIG(exitMethod);
    }

}