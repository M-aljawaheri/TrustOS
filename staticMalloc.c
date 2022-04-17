
#include "staticMalloc.h"
#include <stdlib.h>
#include <stdio.h>

// choose a convenient alignment for all malloced addresses for testing purposes
#define ALIGNMENT_REQ 16
// all malloc requests will
#define MIN_BLOCK_SIZE 16

struct freeList {
    struct freeList *next;
    struct freeList *prev;
};

typedef struct freeList freeListNode_t;
typedef freeListNode_t* freeList_t;

char *mallocArrayStart;
char *memPointer;
int heapSize;
freeList_t freeList;
void removeFromFreeList(freeList_t node);
int getSize(void *addr);

void initMalloc(char *start, int heap_size) {
    mallocArrayStart = start;
    //printf("mallocArray Init %i\n", (unsigned int)mallocArrayStart);
    memPointer = mallocArrayStart;
    heapSize = heap_size;
    freeList = NULL;
    while ((unsigned int)memPointer % ALIGNMENT_REQ != 12) memPointer++;
}


void *Malloc(int size) {
    void *res;
    freeList_t cur = freeList;
    while (cur != NULL) {
        //printf("cur: 0x%x\n", (unsigned int)cur);
        //printf("header size: %i\n", (unsigned int)getSize((void *)cur));
        //printf("malloc size: %i\n", (unsigned int)size);
        if (getSize((void *)cur) == size) {

            res = (void *)cur;
            removeFromFreeList(cur);
            //printf("returned malloc from FreeList\n");
            return (void *)res;
        }
        cur = cur->next;
    }

    res = (void *)memPointer;
    *(int *)res = size;
    res += 4;
    memPointer += 4 + size;
    while ((unsigned int)memPointer % ALIGNMENT_REQ != 12) memPointer++;
    if ((unsigned long)memPointer - (unsigned long)mallocArrayStart > heapSize) {
        //printf("memPointer %i\n", (unsigned int)memPointer);
        //printf("mallocArray %i\n", (unsigned int)mallocArrayStart);
        //printf("heap overflow!");
        //exit(1);
    }
    //printf("malloc returned %x\n", (unsigned int)res);
    return res;
}


int getSize(void *addr) {
    char *sizeAddr = (char *)addr - 4;
    return *((int *)sizeAddr);
}


void removeFromFreeList(freeList_t node) {
    if (node->prev == NULL) {
        freeList = node->next;
        if (freeList != NULL)
            freeList->prev = NULL;
    }
    else {
        node->prev->next = node->next;
        if (node->next != NULL)
            node->next->prev = node->prev;
    }
}


void Free(void *addr)
{
    if (freeList == NULL) {
        freeList = (freeList_t)addr;
        freeList->next = NULL;
        freeList->prev = NULL;
    }
    else {
        freeList_t new_node = (freeList_t)addr;
        new_node->next = freeList;
        new_node->prev = NULL;
        freeList->prev = new_node;
        freeList = new_node;
    }
}