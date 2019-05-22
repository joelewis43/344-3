#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

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

    /*
        CONFIRM THAT CHILD PROCESSES ARE TERMINATED
    */

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

    if (strcmp(path, "") == 0) {
        flag = chdir(getenv("HOME"));
    }
    else {
        flag = chdir(path);
    }

    if (flag == -1) {
        printf("%s: No such file or directory.\n", path);
    }
}

void builtInStatus() {}

void runBuiltIn(char commands[MAXARGS][ARGLENGTH], int ID, int backgroundChildren[MAXCHILDREN]) {
    switch (ID) {
        case 1:
            builtInExit(backgroundChildren);
            break;
        case 2:
            builtInCD(commands[1]);
            break;
        case 3:
            printf("printing status\n");
            break;
        default:
            printf("Error in runBuiltIn\n");
            break;
    }
}

char ** buildArgs(char commands[MAXARGS][ARGLENGTH]) {
    
    int j, i = 0;

    // allocate a new array
    char **temp = malloc(MAXARGS * sizeof(char *));

    for (j=0; j<MAXARGS; j++) {
        temp[j] = malloc(ARGLENGTH * sizeof(char));
    }

    // copy commands into new array
    while (strcmp(commands[i], "") != 0) {
        strcpy(temp[i], commands[i]);
        i++;
    }
    
    // add null to the end
    temp[i] = NULL;

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

    // build an array of the arguments, with NULL at the end
    args = buildArgs(commands);

    // execute command (checking for error)
    if(execvp(commands[0], args) == -1)
        printf("%s: no such file or directory\n", commands[0]);
    
    freeArgs(args);

    // kill the child process
    exit(0);


}

void runCommannd(char commands[MAXARGS][ARGLENGTH], int backgroundChildren[MAXCHILDREN]) {

    int exitMethod;
    int currentChildPID;

    // create a fork for the non-built in command
    currentChildPID = fork();

    // differentiate between child and parent
    switch(currentChildPID) {

        case -1:
            printf("ERROR IN FORK\n");
            exit(1);
            break;

        case 0: //child
            childProcess(commands);
            break;
        
        default: //parent
            wait(&exitMethod); // blocks parent until child completes
            break;
    }
}

void execute(char commands[MAXARGS][ARGLENGTH], int backgroundChildren[MAXCHILDREN]) {

    int currentChildPID;
    int exitMethod;
    int builtInID;

    // ignore comments
    if (isComment(commands[0])) {
        return;
    }

    // check if the given command is a built in command
    if (!(builtInID = isBuiltIn(commands[0]))) {
        runCommannd(commands, backgroundChildren);
    }

    //built in commands
    else {
        runBuiltIn(commands, builtInID, backgroundChildren);
    }
}






int main() {

    /* Variable Declaration */
    char commandBuffer[MAXBUFFER];
    char commandArray[MAXARGS][ARGLENGTH];
    int backgroundChildren[MAXCHILDREN] = { 0 };
    int running = 1;

    while (running) {

        // clear the strings
        clearStrings(commandBuffer, commandArray);

        // get command
        getCommand(commandBuffer);

        // split the command at whitespace
        split(commandBuffer, commandArray);

        // run the given command
        execute(commandArray, backgroundChildren);
    }
    return 0;
}