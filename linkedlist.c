#include<stdio.h> 
#include<stdlib.h> 
#include<string.h>
#include "linkedlist.h"

struct Node* append(struct Node* head, char* str, char* key){
    struct Node* curr = head;
    if (head != NULL) {
        while (curr->next != NULL) {
            curr = curr->next;
        }
    }
    struct Node* new = (struct Node*)malloc(sizeof(struct Node));
    new->data = (char*)malloc(strlen(str));
    if (key == NULL) {
        new->key = NULL;
    }
    else {
       new->key = (char*)malloc(strlen(key));
       strcpy(new->key, key); 
    }
    new->next = NULL;
    strcpy(new->data, str);
    if (head != NULL) {
        curr->next = new;
    }
    else {
        head = new;
    }
    return head;
}

void traverse(struct Node* head, int num, int keys) {
    struct Node* curr = head;
    int i = 0;
    while (curr != NULL && i < num) {
        if (keys == 1) {
            printf("%s (%s)\n", curr->data, curr->key);
        }
	else {
            printf("%s\n", curr->data);
        }
        curr = curr->next;
        i++;
    }
}
/*
 * Returns a key given a Node value and a string
 * */
char* find(struct Node* head, char* str) {
    if (head != NULL) {
        struct Node* curr = head;
        while (curr != NULL) {
            if (strcmp(curr->data, str) == 0) {
                char* val = (char*)malloc(strlen(str));
                strcpy(val, curr->key);
                return val;
            }
            curr = curr->next;
        }
    }
    return NULL;
}
/*
 * Takes in a head Node and frees every value in the Node's list
 * */
void freeAll(struct Node* head) {
    struct Node* curr = head;
    while (curr != NULL) {
        struct Node* val = curr;
        curr = curr->next;
        if (val->key != NULL) {
            free(val->key);
        }
        free(val->data);
        free(val);
    }
}
