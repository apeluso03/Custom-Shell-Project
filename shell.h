#include "getpath.h"

#define MAXARGS 100
#define PROMPTMAX 32

int pid;

int shellMain(int argc, char **argv, char **envp);

void printenv(int num_args, char **envp, char **args);

void executeCommand(char** args, char* pathlist, int num_args, char** envp, int message);

char *which(char *command, struct pathelement *pathlist);

char *where(char *command, struct pathelement *pathlist);

void list(char *dir);
