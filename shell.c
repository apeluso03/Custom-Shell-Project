#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <pwd.h>
#include <glob.h>
#include <limits.h>
#include <utmpx.h>
#include <dirent.h>
#include <assert.h>
// Sys
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
// My Header Files
#include "shell.h"
#include "linkedlist.h"
#include "watchuser.h"
// Globals
#define BUFFER_SIZE 1000

void handle_sigchild(int sig) {
    while (waitpid((pid_t) (-1), 0, WNOHANG) > 0) {}
}

pthread_mutex_t lock;
struct UserNode *watchuser = NULL;

void *watchuser_thread(void *arg) {
    struct utmpx *up;
    // Based on example code provided (Added Locks)
    while(1) {
        setutxent(); // Start at beginning
        while (up = getutxent()) { // Get entry
            if (up->ut_type == USER_PROCESS ) { // Only care about Users
                pthread_mutex_lock(&lock);
                struct UserNode* user = findUser(watchuser, up->ut_user);
                if (user != NULL) {
                    if (user->isLoggedOn == 0) {
                        printf("\n%s has logged on %s from %s\n", up->ut_user, up->ut_line, up ->ut_host);
                        user->isLoggedOn = 1;
                    }
                }
                pthread_mutex_unlock(&lock); 
            }
        }
        sleep(1);
    }
}
// List of commands
typedef enum commands {
        EXIT,
        WHICH,
        WHERE,
        CD,
        GET_CWD,
        PWD,
        LIST,
        PID,
        KILL,
        PROMPT,
        PRINT_ENV,
        SET_ENV,
        WATCHUSER,
        NOCLOBBER,
	command_count
    } commands;
// noclobber set to OFF by default
int noclobber = 0;
// Variables to be used later
int i, uid, status, argsct;
int run = 1;
struct passwd *password;
struct pathelement *pathlist;
char *homedirectory;
char *arg, *command, *commandpath, *cwd, *owd, *p, *pwd;
int len;
char BUFFER[BUFFER_SIZE];
int watching = 0;
pthread_t watchuser_id;
char *prefix = NULL;
// Main shell function
int shellMain(int argc, char **argv, char **envp) {
    prefix = (char *) malloc(0);
    char *prompt = calloc(PROMPTMAX, sizeof(char));
    char **args = calloc(MAXARGS, sizeof(char *));
    uid = getuid();
    password = getpwuid(uid);
    homedirectory = password->pw_dir;
    if ((pwd = getcwd(BUFFER, BUFFER_SIZE + 1)) == NULL) {
        perror("getcwd");
        exit(2);
    }
    // Current working directory
    cwd = calloc(strlen(pwd) + 1, sizeof(char));
    owd = calloc(strlen(pwd) + 1, sizeof(char));
    memcpy(owd, pwd, strlen(pwd));
    memcpy(cwd, pwd, strlen(pwd));
    prompt[0] = ' ';
    prompt[1] = '\0';
    // Put PATH into linked list
    pathlist = get_path();
    int len;
    char *str_input;
    // Signals
    sigignore(SIGTERM);
    sigignore(SIGTSTP);
    sigignore(SIGINT);
    signal(SIGCHLD, handle_sigchild);
    // List of commands
    char *commands[] = {
            "exit",
            "which",
            "where",
            "cd",
            "getcwd",
            "pwd",
            "list",
            "pid",
            "kill",
            "prompt",
            "printenv",
            "setenv",
            "watchuser",
            "noclobber"
    };
    // Loop for shell to run
    while (run) {
        for (int j = 0; j < MAXARGS; j++) {
            args[j] = NULL;
        }
        printf("%s%s $ ", prefix, cwd);
        fgets(BUFFER, BUFFER_SIZE, stdin);
        len = (int) strlen(BUFFER);
        if (len >= 2) {
            BUFFER[len - 1] = '\0';
            str_input = (char *) malloc(len);
            strcpy(str_input, BUFFER);
	    char *tok = strtok(str_input, " ");
            int arg_num = 0;
            while (tok) {
                // Wildcard (? and *)
                if (strstr(tok, "?") != NULL || strstr(tok, "*") != NULL) {
                    glob_t paths;
                    int val;
                    val = glob(tok, 0, NULL, &paths);
                    char **p;
                    if (val == 0) {
                        for (p = paths.gl_pathv; *p != NULL; p++) {
                            len = (int) strlen(*p);
                            args[arg_num] = (char *) malloc(len);
                            strcpy(args[arg_num], *p);
                            arg_num++;
                        }
                        globfree(&paths);
                    }
                } 
		else {
                    len = (int) strlen(tok);
                    args[arg_num] = (char *) malloc(len);
                    strcpy(args[arg_num], tok);
                }
                tok = strtok(NULL, " ");
                arg_num++;
            }
            int piped = 0;
            int pipe_err = 0;
            int pipeind = -1;
            // Pipes
            for (int i = 0; i < arg_num; i++) {
                if (strcmp(args[i], "|") == 0 || strcmp(args[i], "|&") == 0) {
                    if (strcmp(args[i], "|&") == 0) {
                        pipe_err = 1;
                    }
                    piped = 1;
                    pipeind = i;
                    break;
                }
            }
            char** piped_args = NULL;
            int piped_arg_nums = 0;
            int pfds[2];
            int first_child = -1;
            int second_child = -1;
            int commandind = 0;
            int flag = 0;
            for (commandind = 0; commandind < command_count; commandind++) {
                if (strcmp(args[0], commands[commandind]) == 0) {
                    break;
                }
            }
            if (piped) {
                char** piped_args = calloc(MAXARGS, sizeof(char *));
                for (int i = 0; i < MAXARGS; i++) {
                    piped_args[i] = NULL;
                }
                free(args[pipeind]);
                for (int i = pipeind + 1; i < MAXARGS; i++) {
                    if (args[i] != NULL) {
                        piped_args[i - pipeind - 1] = args[i];
                        piped_arg_nums++;
                    }
		    else {
                        break;
                    }       
                }
                for (int j = pipeind; j < MAXARGS; j++) {
                    if (args[j] == NULL) {
                        break;
                    }
                    args[j] = NULL;
                    arg_num--;
                }
                pipe(pfds);
                first_child = fork();
                if (first_child == 0) {
                    if (pipe_err) {
                        close(STDERR_FILENO);
                        dup(pfds[1]);
                    }
                    close(STDOUT_FILENO);
                    dup(pfds[1]);
                    close(pfds[0]);
                    run_command(commandind, args, pathlist, arg_num, envp, 0);
                    exit(0);
                }
		else {
                    second_child = fork();
                    if (second_child == 0) {
                        close(STDIN_FILENO);
                        dup(pfds[0]);
                        close(pfds[1]);
                        executeCommand(piped_args, pathlist, piped_arg_nums, envp, 1);
                        exit(0);
                    }
		    else {
                        close(pfds[0]);
                        close(pfds[1]);
                        int c_status;
                        waitpid(second_child, &c_status, 0);
                        for (int j = 0; j < MAXARGS; j++) {
                            if (piped_args[j] != NULL) {
                                free(piped_args[j]);
                            }
                        }
                        free(piped_args);
                    }
                }
            }                
            if (piped == 0) {
                run_command(commandind, args, pathlist, arg_num, envp, 1);
            }
            free(tok);
            // Null array
            for (int j = 0; j < MAXARGS; j++) {
                if(args[j] != NULL){
                    free(args[j]);
                    args[j] = NULL;
                }
                
            }
            free(str_input);
        }
    }
    userFreeAll(watchuser);
    if (watching == 1) {
        pthread_cancel(watchuser_id);
        pthread_join(watchuser_id, NULL);
    }
    free(args);
    free(cwd);
    free(owd);
    free(prefix);
    free(prompt);
    struct pathelement *curr;
    curr = pathlist;
    free(curr->element);
    while (curr != NULL) {
        free(curr);
        curr = curr->next;
    }
    return 0;
}
// Command Implementations (Cases)
void run_command(int command_index, char** args, char* pathlist, int arg_num, char** envp, int suppress_output){
    switch (command_index) {
	    // Exits shell exe
        case EXIT:
            run = 0;
            break;
	    // Prints path to given command
        case WHICH:
            if (args[1] == NULL) {
                printf("%s", "[Which]: Too few args\n");
            } 
	    else {
                for (int i = 1; i < MAXARGS; i++) {
                    if (args[i] != NULL) {
                        char *result = which(args[i], pathlist);
                        if (result != NULL) {
                            printf("%s\n", result);
                            free(result);
                        } 
			else {
                            printf("[%s] not found\n", args[i]);
                        }
                    } 
		    else {
                        break;
                    }
                }
            }
            break;
	    // Prints path to given command
        case WHERE:
            if (args[1] == NULL) {
                printf("%s", "[Where]: Too few args\n");
            } 
	    else {
                for (int i = 1; i < MAXARGS; i++) {
                    if (args[i] != NULL) {
                        char *result = where(args[i], pathlist);
                        if (result != NULL) {
                            printf("%s\n", result);
                            free(result);
                        } 
			else {
                            printf("[%s] not found\n", args[i]);
                        }
                    } 
		    else {
                        break;
                    }
                }
            }
            break;
	    // Changes the cwd
        case CD:
            printf("");
            char *cd_path = args[1];
            if (arg_num > 2) {
                perror("[cd]: Too many args");
            } 
	    else {
                if (arg_num == 1) {
                    cd_path = homedirectory;
                } 
		else if (arg_num == 2) {
                    cd_path = args[1];
		}
                if ((pwd = getcwd(BUFFER, BUFFER_SIZE + 1)) == NULL) {
                    perror("getcwd");
                    exit(2);
                }
                if (cd_path[0] == '-') {
                    if (chdir(owd) < 0) {
                        printf("Invalid Directory: [%d]\n", errno);
                    } 
		    else {
                        free(cwd);
                        cwd = malloc((int) strlen(owd));
                        strcpy(cwd, owd);
                        free(owd);
                        owd = malloc((int) strlen(BUFFER));
                        strcpy(owd, BUFFER);
                    }
                } 
		else {
                    if (chdir(cd_path) < 0) {
                        printf("Invalid Directory: [%d]\n", errno);
                    } 
		    else {
                        free(owd);
                        owd = malloc((int) strlen(BUFFER));
                        strcpy(owd, BUFFER);

                        if ((pwd = getcwd(BUFFER, BUFFER_SIZE + 1)) == NULL) {
                            perror("getcwd");
                            exit(2);
                        }
                        free(cwd);
                        cwd = malloc((int) strlen(BUFFER));
                        strcpy(cwd, BUFFER);
                    }
                }
            }
            break;
	    // Prints the cwd
        case PWD:
            printf("%s\n", cwd);
            break;
	    // Lists all the elements in the cwd, or given directory
        case LIST:
            if (arg_num == 1) {
                list(cwd);
            } 
	    else {
                for (int i = 1; i < MAXARGS; i++) {
                    if (args[i] != NULL) {
                        printf("[%s]:\n", args[i]);
                        list(args[i]);
                    }
                }
            }
            break;
	    // Prints the PID of the shell
        case PID:
	    printf("PID: [%d]\n", getpid());
            break;
	    // Kills (ends) execution of a given process by its PID
        case KILL:
            if (arg_num == 3) {
                char *pid_str = args[2];
                char *signal_str = args[1];
                char *end;
                long pid_num;
                long sig_num;
                pid_num = strtol(pid_str, &end, 10);
                if (end == pid_str) {
                    printf("%s\n", "Cannot convert string to number");
                }
                signal_str[0] = ' ';
                sig_num = strtol(signal_str, &end, 10);
                if (end == signal_str) {
                    printf("%s\n", "Cannot convert string to number");
                }
                int id = (int) pid_num;
                int sig = (int) sig_num;
                kill(id, sig_num);
            } 
	    else if (arg_num == 2) {
                char *pid_str = args[1];
                char *end;
                long num;
                num = strtol(pid_str, &end, 10);
                if (end == pid_str) {
                    printf("%s\n", "Cannot convert string to number");
                }
                int id = (int) num;
                kill(id, SIGTERM);
            } 
	    else {
                printf("%s\n", "[Kill]: Incorrect number of args");
            }
            break;
	    // Allows for change to prompt prefix
        case PROMPT:
            free(prefix);
            if (arg_num == 1) {
                fgets(BUFFER, BUFFER_SIZE, stdin);
                len = (int) strlen(BUFFER);
                BUFFER[len - 1] = '\0';
                prefix = (char *) malloc(len);
                strcpy(prefix, BUFFER);
            } 
	    else if (arg_num == 2) {
                prefix = (char *) malloc(strlen(args[1]));
                strcpy(prefix, args[1]);
            }
            break;
	    // Prints all the environment variables
        case PRINT_ENV:
            printenv(arg_num, envp, args);
            break;
	    // Sets a new environment variable if it doesn't already exist
        case SET_ENV:
            if (arg_num == 1) {
                printenv(arg_num, envp, args);
            } 
	    else if (arg_num == 2) {
                setenv(args[1], "", 1);
            } 
	    else if (arg_num == 3) {
                setenv(args[1], args[2], 1);
                if (strcmp(args[1], "HOME") == 0) {
                    homedirectory = getenv("HOME");
                } 
		else if (strcmp(args[1], "PATH") == 0) {
                    pathlist = get_path();
                }
            } 
	    else {
                printf("%s\n", "[setenv]: Incorrect number of args");
            }
            break;
	    // Adds the given user to the watchuser list and prints when they log in
        case WATCHUSER:
            if (arg_num == 2) {
                printf("Watching user [%s]\n", args[1]);
                char* user = (char *)malloc(strlen(args[1]));
                strcpy(user, args[1]);
                pthread_mutex_lock(&lock);
                watchuser = userAppend(watchuser, user);
                pthread_mutex_unlock(&lock);
                if (watching == 0) {
                    pthread_create(&watchuser_id, NULL, watchuser_thread, NULL);
                    watching = 1;
                }
            }
	    else if (arg_num == 3) {
		    if (strcmp("off", args[2]) == 0) {
			    printf("Stopped watching user [%s]\n", args[1]);
			    char* user = (char *)malloc(strlen(args[1]));
			    strcpy(user, args[1]);
			    pthread_mutex_lock(&lock);
			    watchuser = removeUserNode(watchuser, user);
			    pthread_mutex_unlock(&lock);
			    watching -= 1;
		    }
	    }
	    else {
                printf("[Watchuser]: Not enough args\n");
            }
            break;
	    // Toggles the noclobber variable, allowing for file creation/redirection
        case NOCLOBBER:
            if (noclobber == 0) {
                printf("Noclobber [ON]\n");
                noclobber = 1;
            }
	    else {
                printf("Noclobber [OFF]\n");
                noclobber = 0;
            }
            break;
	    // Default case, should no valid command be found
        default:
            executeCommand(args, pathlist, arg_num, envp, suppress_output);
    }
}

void executeCommand(char** args, char* pathlist, int arg_num, char** envp, int command){
    char *cmd_path;
    if (args[0][0] == '/' || args[0][0] == '.') {
        cmd_path = (char *) malloc(strlen(args[0]));
        strcpy(cmd_path, args[0]);
    } 
    else {
        cmd_path = which(args[0], pathlist);
    }
    int result = access(cmd_path, F_OK | X_OK);
    struct stat path_stat;
    stat(cmd_path, &path_stat);
    int background = strcmp(args[arg_num - 1], "&");
    if (background == 0){
        args[arg_num - 1] = NULL;
    }
    if (result == 0 && S_ISREG(path_stat.st_mode)) {
        if (cmd_path != NULL) {
            if (command) {
                printf("Executing built-in [%s]\n", args[0]);
            }
            pid_t child_pid = fork();
            if (child_pid == 0) {
                int redirect_ind = -1;
                for (int i = 0; i < arg_num; i++) {
                    if (strcmp(args[i], ">") == 0 || strcmp(args[i], ">>") == 0 || strcmp(args[i], ">&") == 0 
				    || strcmp(args[i], ">>&") == 0 || strcmp(args[i], "<") == 0) {
                        redirect_ind = i;
                    }
                }
                int execute = 1;
                if (redirect_ind != -1) {
                    char* file_dest = args[redirect_ind + 1];
                    char* redirect_str = args[redirect_ind];
                    int permissions = 0666;
                    int exists = 0;
                    struct stat st;
                    if (stat(file_dest, &st) == 0) {
                        exists = 1;
                    }
                    if (strcmp(redirect_str, ">") == 0) {
                        if (noclobber == 1 && exists == 1) {
                            printf("Noclobber ON; Cannot overwrite file [%s]\n", file_dest);
                            execute = 0;
                        }
			else {
                            int fid = open(file_dest, O_WRONLY|O_CREAT|O_TRUNC, permissions);
                            close(STDOUT_FILENO);
                            dup(fid);
                            close(fid);
                        }
                    } 
		    else if (strcmp(redirect_str, ">&") == 0) {
                        if (noclobber == 1 && exists == 1) {
                            printf("Noclobber ON; Cannot overwrite file [%s]\n", file_dest);
                            execute = 0;
                        }
			else {
                            int fid = open(file_dest, O_WRONLY|O_CREAT|O_TRUNC, permissions);
                            close(STDOUT_FILENO);
                            dup(fid);
                            close(STDERR_FILENO);
                            dup(fid);
                            close(fid);
                        }
                    }
		    else if (strcmp(redirect_str, ">>") == 0) {
                        if (noclobber == 1 && exists == 0) {
                            printf("Noclobber ON; File [%s] does not exist\n", file_dest);
                            execute = 0;
                        }
			else {
                            int fid = open(file_dest, O_WRONLY|O_CREAT|O_APPEND, permissions);
                            close(STDOUT_FILENO);
                            dup(fid);
                            close(fid);
                        }
                    }
		    else if (strcmp(redirect_str, ">>&") == 0) {
                        if (noclobber == 1 && exists == 0) {
                            printf("Noclobber ON; File [%s] does not exist\n", file_dest);
                            execute = 0;
                        }
			else {
                            int fid = open(file_dest, O_WRONLY|O_CREAT|O_APPEND, permissions);
                            close(STDOUT_FILENO);
                            dup(fid);
                            close(STDERR_FILENO);
                            dup(fid);
                            close(fid);
                        }
                    }
		    else if (strcmp(redirect_str, "<") == 0) {
                        if (stat(file_dest, &st) == -1) {
                            printf("Could not open file [%s]\n", file_dest);
                            execute = 0;
                        }
			else {
                            int fid = open(file_dest, O_RDONLY);
                            close(STDIN_FILENO);
                            dup(fid);
                            close(fid);
                        }
                    }

                    for (int i = redirect_ind; i < arg_num; i++) {
                            args[i] = NULL;
                    }
                }

                if (execute == 1) {
                    int ret = execve(cmd_path, args, envp);
                }
		else {
                    exit(0);
                }
            }
            int child_status;
            if (background == 0) {
                waitpid(child_pid, &child_status, WNOHANG);
            } 
	    else {
                waitpid(child_pid, &child_status, 0);
            }
        } 
	else {
            printf("[%s]: Command not found\n", args[0]);
        }
    } 
    else {
        printf("[%s]\n", "Invalid Command");
        printf("Access Error: [%i]\n", errno);
    }
    free(cmd_path);
}
/*
 * Prints the current environment.
 * If it is specified what to print, will only print that
 * */
void printenv(int arg_num, char **envp, char **args) {
    if (arg_num == 1) {
        int i = 0;
        while (envp[i] != NULL) {
            printf("%s\n", envp[i]);
            i++;
        }
    } 
    else if (arg_num == 2) {
        char *str = getenv(args[1]);
        if (str != NULL) {
            printf("%s\n", str);
        }
    }
}
/*
 * Searches the PATH environment variable and returns 
 * the absolute path of the comand given as an arg
 * */
char *which(char *command, struct pathelement *pathlist) {
    char CBUFFER[BUFFER_SIZE];
    struct pathelement *current = pathlist;
    DIR *dir;
    struct dirent *stream;
    while (current != NULL) {
        char *path = current->element;
        dir = opendir(path);
        if (dir) {
            while ((stream = readdir(dir)) != NULL) {
                if (strcmp(stream->d_name, command) == 0) {
                    strcpy(CBUFFER, path);
                    strcat(CBUFFER, "/");
                    strcat(CBUFFER, stream->d_name);
                    int len = (int) strlen(CBUFFER);
                    char *p = (char *) malloc(len);
                    strcpy(p, CBUFFER);
                    closedir(dir);
                    return p;
                }
            }
        }
        closedir(dir);
        current = current->next;
    }
    return NULL;
}
/*
 * Similar to which
 * */
char *where(char *command, struct pathelement *pathlist) {
    char CBUFFER[BUFFER_SIZE];
    struct pathelement *current = pathlist;
    DIR *dir;
    struct dirent *stream;
    strcpy(CBUFFER, "");
    while (current != NULL) {
        char *path = current->element;
        dir = opendir(path);
        if (dir) {
            while ((stream = readdir(dir)) != NULL) {
                if (strcmp(stream->d_name, command) == 0) {
                    strcat(CBUFFER, path);
                    strcat(CBUFFER, "/");
                    strcat(CBUFFER, stream->d_name);
                    strcat(CBUFFER, "\n");
                }
            }
        }
        closedir(dir);
        current = current->next;
    }
    int len = (int) strlen(CBUFFER);
    char *p = (char *) malloc(len);
    CBUFFER[len - 1] = '\0';
    strcpy(p, CBUFFER);
    return p;
}
/*
 * Without arguments, returns every element in the current
 * working directory
 * When given a directory as an argument, instead returns
 * the contents of given directory
 * */
void list(char *dir) {
    DIR *dir2;
    struct dirent *stream;
    dir2 = opendir(dir);
    if (dir2 == NULL) {
        printf("Cannot open directory [%s]\n", dir);
    } 
    else {
        while ((stream = readdir(dir2)) != NULL) {
            printf("%s\n", stream->d_name);
        }
    }
    closedir(dir2);
}
