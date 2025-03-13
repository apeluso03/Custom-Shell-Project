#include "getpath.h"

struct pathelement *get_path() {
    char *path, *p;
    struct pathelement *temp, *pathlist = NULL;
    p = getenv("PATH");
    path = malloc((strlen(p) + 1) * sizeof(char));
    strncpy(path, p, strlen(p));
    path[strlen(p)] = '\0';
    p = strtok(path, ":");
    do {
        if (!pathlist) {
            temp = calloc(1, sizeof(struct pathelement));
            pathlist = temp;
        } 
	else {
            temp->next = calloc(1, sizeof(struct pathelement));
            temp = temp->next;
        }
        temp->element = p;
        temp->next = NULL;
    } 
    while (p = strtok(NULL, ":"));
    return pathlist;
}
