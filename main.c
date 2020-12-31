#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_PRESSES 5
#define MAX_LINE 80 /* 80 chars per line, per command, should be enough. */

typedef struct {
    pid_t id;
    struct backgroundProcess *nextBackgroundProcess;
}backgroundProcess;

typedef struct {
    char* command;
    struct bookmark *nextBookmark;
}bookmark;

backgroundProcess *headRunningBackgroundProcess = NULL;
backgroundProcess *headFinishedBackgroundProcess = NULL;
bookmark *headBookmark = NULL;

pid_t foregroundProcessID;
int foreground = 0;

void child_process(char *pString[41]);

void parent_process(pid_t child, int background, char *pString[41]);

void fillPath();

void executeArgument(char **pString);

void createNewBackgroundProcess(pid_t child);

void addNewBackgroundProcess(backgroundProcess *root, backgroundProcess *process);

void moveBackgroundProcessToFinished(backgroundProcess *process);

void printProcesses();

void initLinkedList();

void control_z(int signal);

void checkAndExit();

void bookmarkCommands(char **pString);

void listBookmarks(bookmark *root);

void deleteBookmark(bookmark *root, int index);

char *prepareCommand(char **pString);

void addNewBookmark(bookmark *root, char* command);

/* The setup function below will not return any value, but it will just: read
in the next command line; separate it into distinct arguments (using blanks as
delimiters), and set the args array entries to point to the beginning of what
will become null-terminated, C-style strings. */

void setup(char inputBuffer[], char *args[],int *background)
{
    int length, /* # of characters in the command line */
    i,      /* loop index for accessing inputBuffer array */
    start,  /* index where beginning of next command parameter is */
    ct;     /* index of where to place the next parameter into args[] */

    ct = 0;

    /* read what the user enters on the command line */
    length = read(STDIN_FILENO,inputBuffer,MAX_LINE);

    /* 0 is the system predefined file descriptor for stdin (standard input),
       which is the user's screen in this case. inputBuffer by itself is the
       same as &inputBuffer[0], i.e. the starting address of where to store
       the command that is read, and length holds the number of characters
       read in. inputBuffer is not a null terminated C-string. */

    start = -1;
    if (length == 0)
        exit(0);            /* ^d was entered, end of user command stream */

/* the signal interrupted the read system call */
/* if the process is in the read() system call, read returns -1
  However, if this occurs, errno is set to EINTR. We can check this  value
  and disregard the -1 value */
    if ( (length < 0) && (errno != EINTR) ) {
        perror("error reading the command");
        exit(-1);           /* terminate with error code of -1 */
    }

    //printf(">>%s<<",inputBuffer);
    for (i=0;i<length;i++){ /* examine every character in the inputBuffer */

        switch (inputBuffer[i]){
            case ' ':
            case '\t' :               /* argument separators */
                if(start != -1){
                    args[ct] = &inputBuffer[start];    /* set up pointer */
                    ct++;
                }
                inputBuffer[i] = '\0'; /* add a null char; make a C string */
                start = -1;
                break;

            case '\n':                 /* should be the final char examined */
                if (start != -1){
                    args[ct] = &inputBuffer[start];
                    ct++;
                }
                inputBuffer[i] = '\0';
                args[ct] = NULL; /* no more arguments to this command */
                break;

            default :             /* some other character */
                if (start == -1)
                    start = i;
                if (inputBuffer[i] == '&'){
                    *background  = 1;
                    inputBuffer[i-1] = '\0';
                }
        } /* end of switch */
    }    /* end of for */
    args[ct] = NULL; /* just in case the input line was > 80 */


    for (i = 0; i <= ct; i++)
        printf("args %d = %s\n",i,args[i]);

} /* end of setup routine */

int main(void)
{
    char inputBuffer[MAX_LINE]; /*buffer to hold command entered */
    int background; /* equals 1 if a command is followed by '&' */
    char *args[MAX_LINE/2 + 1]; /*command line arguments */

    // Control<z>
    signal(SIGTSTP, control_z);


    // Init head of background process linked list
    initLinkedList();

    // Read Path Variables and Fill Path Array
    fillPath();


    while (1){
        background = 0;

        // Print our shell to the screen and wait for the user input
        printf("myshell> ");
        fflush(NULL);

        /*setup() calls exit() when Control-D is entered */
        setup(inputBuffer, args, &background);

        if(strcmp(args[0], "bookmark") == 0){
            bookmarkCommands(args);
        }

        int counter = 0;
        while(args[counter] != NULL){
            if (!strcmp(args[counter], "&")){
                args[counter] = NULL;
            }
            counter++;
        }

        if (strcmp(args[0], "exit") == 0){
            checkAndExit();
        }

        // Fork a child from parent process
        pid_t child = fork();


        // Set ID of Foregorund Process
        if (background == 0){
            foregroundProcessID = child;
            foreground = 1;
        }

        // Handle problems during fork
        if (child == -1) {
            perror("Error occured during forking child.\n");
            return -1;
        }

        // Child Code
        if (child == 0){
            //printf("In child code.\n");
            child_process(args);
        }
        // Parent Code
        else{
            //printf("In parent code.\n");
            parent_process(child, background, args);
        }
        /** the steps are:
        (1) fork a child process using fork()
        (2) the child process will invoke execv()
        (3) if background == 0, the parent will wait,
        otherwise it will invoke the setup() function again. */
    }
}

void initLinkedList() {
    headRunningBackgroundProcess = malloc(sizeof(backgroundProcess));
    headRunningBackgroundProcess->id = -1;
    headRunningBackgroundProcess->nextBackgroundProcess = NULL;

    headFinishedBackgroundProcess = malloc(sizeof(backgroundProcess));
    headFinishedBackgroundProcess->id = -1;
    headFinishedBackgroundProcess->nextBackgroundProcess = NULL;

    headBookmark = malloc(sizeof (bookmark));
    headBookmark->command = "start";
    headBookmark->nextBookmark = NULL;
}

void fillPath() {

}

void parent_process(pid_t child, int background, char *pString[41]) {
    // If it is a foreground process, wait for the child.
    if(background == 0){
        wait(child);
    }
    /*
     * If it is a background process,
     * create a new struct for the child process
     * add the process to the background process linked list
     *
    */
    else{
        printf("child id    : %ld\n", child);
        createNewBackgroundProcess(child);
        waitpid(-1, NULL, WNOHANG);
    }
}

void createNewBackgroundProcess(pid_t child) {
    backgroundProcess *newBackgroundProcess = malloc(sizeof (backgroundProcess));
    newBackgroundProcess->id = child;
    newBackgroundProcess->nextBackgroundProcess = NULL;
    addNewBackgroundProcess(headRunningBackgroundProcess, newBackgroundProcess);
}

void addNewBackgroundProcess(backgroundProcess *root, backgroundProcess *process) {
    backgroundProcess *iter = root;
    while (iter->nextBackgroundProcess != NULL){
        iter = iter->nextBackgroundProcess;
    }
    iter->nextBackgroundProcess = process;
}

void moveBackgroundProcessToFinished(backgroundProcess *process) {
    // Find the process
    backgroundProcess *iter = headRunningBackgroundProcess;
    while (iter->nextBackgroundProcess != process){
        iter = iter->nextBackgroundProcess;
    }
    // Remove process from the list
    iter = iter->nextBackgroundProcess;
    iter->nextBackgroundProcess = process->nextBackgroundProcess;
    addNewBackgroundProcess(headFinishedBackgroundProcess, process);
}

void child_process(char *pString[41]) {
    // Print situtations of processes
    if (strcmp(pString[0], "ps_all") == 0){
        printProcesses();
    }
    // Search PATH Variable and run execute argument
    executeArgument(pString);

}

void bookmarkCommands(char **pString) {
    // Add new bookmark
    if(pString[1][0] == '"'){
        char* command =prepareCommand(pString);
        addNewBookmark(headBookmark, command);
    }
    // Delete a bookmark
    else if(strcmp(pString[1], "-d") == 0){
        deleteBookmark(headBookmark, atoi(pString[2]));
    }
    // List bookmarks
    else if(strcmp(pString[1], "-l") == 0){
        listBookmarks(headBookmark);
    }
}

char *prepareCommand(char **pString) {
        int counter = 1;
        char *command = "";
        int temp;
        char* name_with_extension;
        while(pString[counter] != '\0'){
            temp = counter + 1;
            if(pString[temp] != '\0'){
                name_with_extension = malloc(strlen(command)+1+4);
                strcpy(name_with_extension, command); /* copy name into the new var */
                strcat(name_with_extension, pString[counter]); /* add the extension */
                strcat(name_with_extension, " "); /* add the extension */
                counter++;
            } else {
                strcat(name_with_extension, pString[counter]); /* add the extension */
                counter++;
            }
        }
    return name_with_extension;

    /*
    int counter = 1;
    char *command = "";
    int temp;
    while(pString[counter] != NULL){
        temp = counter + 1;
        if(pString[temp] != NULL){
            sprintf(command,"%s ",pString[counter]);
        } else {
            sprintf(command,"%s",pString[counter]);
        }
    }*/
}

void checkAndExit() {
    // Check if there is any running background procecess, if any notify the user
    if (headRunningBackgroundProcess->nextBackgroundProcess != NULL){
        printf("There is still background processes running!\nPlease terminate all background processes to exit from shell.\n");
    }else{
        exit(1);
    }
}

void executeArgument(char **pString) {
    const char* string = getenv("PATH");
    // Split string code taken from https://www.educative.io/edpresso/splitting-a-string-using-strtok-in-c
    // Extract the first token
    char * token = strtok(string, ":");
    char tempToRun[80];
    // loop through the string to extract all other tokens
    while( token != NULL ) {
        strcpy(tempToRun, token);
        sprintf(tempToRun,"%s/%s",token,pString[0]);
        execv(tempToRun, pString);
        strcpy(tempToRun, "");
        token = strtok(NULL, ":");
    }
}

void printProcesses() {
    // 1. Iterate all processes and move if any of them has finished

    /*
    backgroundProcess *iter = headRunningBackgroundProcess->nextBackgroundProcess;
    while (iter != NULL){
        if(kill(iter->id, 0) != -1) {
            printf("Kapattığım: %d\n", iter->id);
            //printf("%d\n",waitpid(iter->id, NULL, WNOHANG));
           // backgroundProcess *temp = iter;
            //iter = iter->nextBackgroundProcess;
           // moveBackgroundProcessToFinished(temp);
            //continue;
        }
        iter = iter->nextBackgroundProcess;
    }
    */

    // 2. Print running processes
    printf("\nRunning\n");

    backgroundProcess *iterRunning = headRunningBackgroundProcess->nextBackgroundProcess;

    if(iterRunning == NULL){
        printf("There is no running processes!\n");
    }
    int counter = 1;
    while (iterRunning != NULL){
        printf("%d. (Pid=%d)\n", counter, iterRunning->id);
        iterRunning = iterRunning->nextBackgroundProcess;
        counter++;
    }

    /*
    // 3. Print finished processes
    printf("Finished\n");
    backgroundProcess *iterFinished = headFinishedBackgroundProcess->nextBackgroundProcess;
    if(iterFinished == NULL){
        printf("There is no finished processes!\n");
    }
    counter = 1;
    while (iterRunning != NULL){
        printf("%d. (Pid=%d)\n", counter,iterFinished->id);
        iterFinished = iterFinished->nextBackgroundProcess;
        counter++;
    }*/

}

void control_z(int signal) {
    // Check if there is any foreground process
    if(foreground==1){
        // Kill the process
        kill(foregroundProcessID, SIGKILL);
        printf("\nCurrent foreground processes successfully killed.\n");
        // Update the global variable to check later
        foreground=0;
    }else{
        // Print to user that there is no running foreground process.
        printf("\nThere is no running foreground process.\nmyshell> ");
    }
    fflush(stdout);
}

void addNewBookmark(bookmark *root, char* command){
    bookmark *newBookmark = malloc(sizeof (bookmark));
    newBookmark->command = command;
    newBookmark->nextBookmark = NULL;

    bookmark *iter = root;
    while (iter->nextBookmark != NULL){
        iter = iter->nextBookmark;
    }
    iter->nextBookmark = newBookmark;
}

void deleteBookmark(bookmark *root, int index){
    bookmark *iter = root;
    while(index != 0){
        iter = iter->nextBookmark;
        index--;
    }
    bookmark *temp = iter->nextBookmark;
    iter->nextBookmark = temp->nextBookmark;
    free(temp);
}

void listBookmarks(bookmark *root){
    bookmark *iter = root->nextBookmark;
    int counter = 0;
    while (iter != NULL){
        printf("%d %s\n",counter, iter->command);
        counter++;
        iter = iter->nextBookmark;
    }
}