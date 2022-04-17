
#ifndef STATIC_MALLOC
#define STATIC_MALLOC
#define MALLOC(size) Malloc(size)
#define FREE(addr) Free(addr)

// call this function with the pointer to the start of static array
void initMalloc(char *start, int heap_size);

// similar to malloc
void *Malloc(int size);

// similar t free
void Free(void *addr);

#endif