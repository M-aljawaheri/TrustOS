#include "lists.h"
#include "staticMalloc.h"
#include "semaphore.h"
#include <stdlib.h>


struct mutex
{
    list_t queue;
    bool acquired;
};

typedef int queue_item_t;
typedef struct mutex *mutex_t;
int priority(queue_item_t x);

semaphore_t queue_mutex = 1;
list_t add_by_priority(list_t Queue, void *data);
queue_item_t queue_item(int id, int priority);
int id(queue_item_t x);

mutex_t create_mutex() {
    mutex_t res = MALLOC(sizeof(struct mutex));
    if (!res) return NULL;
    res->queue = create_list();
    res->acquired = false;
	return res;
}

void free_mutex(mutex_t mutex) {
    list_t Q = mutex->queue;
    while (Q)
        Q = delete_node(Q);
    FREE(mutex);
}

void acquire_mutex(mutex_t mutex, int id, int priority) {
    queue_item_t data = queue_item(id, priority);
    OS_WaitNaive(queue_mutex);
    mutex->queue = add_by_priority(mutex->queue, data);
    OS_SignalNaive(queue_mutex);
	
	OS_WaitNaive(queue_mutex);
    while (mutex->acquired == true || 
		mutex->queue->data != data) {
		OS_SignalNaive(queue_mutex);
		__asm("NOP");		
		__asm("NOP");
		__asm("NOP");
		OS_WaitNaive(queue_mutex);	
	}
	mutex->acquired = true;
	mutex->queue = delete_node(mutex->queue);
	OS_SignalNaive(queue_mutex);
}

void release_mutex(mutex_t mutex, int id, int priority) {
    mutex->acquired = false;
}

list_t add_by_priority(list_t lst, void *data) {
	list_t node =  MALLOC(sizeof(list_node));
    if (!node) return NULL;
    node->data = data;
    list_t current_node = lst;
    //special case: adding to an empty list: add in front of dummy node
    if (!(current_node->next)) {
        node->next = lst;
        node->prev = NULL;
        current_node->prev = node;
        return node;
    }
    //loop invariant: current_node->next is never null
    while (current_node->next->next) {
        current_node = current_node->next;
    }
    //current node is the node before the dummy node
	while (current_node != NULL) {
		if (priority((queue_item_t)(current_node->data)) <= priority(data)) {
			add_as_next(current_node, data);
			return lst;
		}
		current_node = current_node->prev;
	}
	return add_to_front(lst, data);
}

int queue_item(int id, int priority) {
    return (id<<16) + priority;
}

int priority(queue_item_t x) {
    return x & 0xFFFF;
}

int id(queue_item_t x) {
    return (x >> 16) & 0xFFFF;
}

