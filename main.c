#include "shell.h"
#include <signal.h>
#include <stdio.h>

void sig_handler(int signal);

int main(int argc, char **argv, char **envp) {
    return shellMain(argc, argv, envp);
}

