
#include "staticMalloc.h"

// choose a convenient alignment for all malloced addresses for testing purposes
#define ALIGNMENT_REQ 16

char *mallocArray;
char *memPointer;

void initMalloc(char *start) {
    mallocArray = start;
    memPointer = mallocArray;
    while ((unsigned int)memPointer % ALIGNMENT_REQ != 0) memPointer++;
}

void *Malloc(int size) {
    void *res = (void *)memPointer;
    memPointer += size;
    while ((unsigned int)memPointer % ALIGNMENT_REQ != 0) memPointer++;
    return res;
}