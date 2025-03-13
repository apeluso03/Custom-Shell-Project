#include<stdio.h> 
#include<stdlib.h> 
#include<string.h>
#include "watchuser.h"

struct UserNode* userAppend(struct UserNode* head, char* user) {
    struct UserNode* curr = head;
    if (head != NULL) {
        while (curr->next != NULL) {
            curr = curr->next;
        }
    }
    struct UserNode* new = (struct UserNode*)malloc(sizeof(struct UserNode));
    new->user = user;
    new->isLoggedOn = 0;
    new->next = NULL;
    strcpy(new->user, user);
    if (head != NULL) {
        curr->next = new;
    }
    else {
        head = new;
    }
    return head;
}

struct UserNode* findUser(struct UserNode* head, char* user) {
    struct UserNode* curr = head;
    while (curr != NULL) {
        if (strcmp(user, curr->user) == 0) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

struct UserNode* removeUserNode(struct UserNode* head, char* user) {
    struct UserNode* curr = head;
    if (strcmp(curr->user, user) == 0) {
        head = curr->next;
        freeUserNode(curr);
        return head;
    }
    while(curr != NULL){
        struct UserNode* next = curr->next;
        if (next != NULL) {
            if (strcmp(next->user, user) == 0) {
                curr->next = next->next;
                freeUserNode(next);
            }
        }
        curr = next;
    }
    return head;
}

void freeUserNode(struct UserNode* node) {
    free(node->user);
    free(node);
}

void userFreeAll(struct UserNode* head) {
    struct UserNode* curr = head;
    while (curr != NULL) {
        struct UserNode* val = curr;
        curr = curr->next;
        free(val->user);
        free(val);
    }
}
