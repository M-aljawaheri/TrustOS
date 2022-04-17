#include "lists.h"
#include "staticMalloc.h"
#include <stdlib.h>

#ifndef MUTEXES
#define MUTEXES
struct mutex
{
    list_t queue;
    bool acquired;
};

typedef int queue_item_t;
typedef struct mutex *mutex_t;

mutex_t create_mutex();
void free_mutex(mutex_t mutex);
void acquire_mutex(mutex_t mutex, int id, int priority);
void release_mutex(mutex_t mutex, int id, int priority);

#endif