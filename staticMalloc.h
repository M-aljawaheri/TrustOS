
#ifndef STATIC_MALLOC
#define STATIC_MALLOC
#define MALLOC(size) Malloc(size)

// call this function with the pointer to the start of static array
void initMalloc(char *start);

// similar to malloc
void *Malloc(int size);

#endif