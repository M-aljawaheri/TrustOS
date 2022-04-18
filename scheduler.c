#include <stdint.h>
#include <stdbool.h>
#include "15348.h"
#include "timer.h"
#include "serial.h"
#include <stddef.h>

#include "lists.h"
#include "scheduler.h"
#include "configconsts.h""
#include "staticMalloc.h""

TCB_t* pxCurrentTCB = NULL;
TCB_t* pxNextTCB = NULL;
list_t readyLists[NUM_PRIORITIES];
bool schedulerStarted = false;

void initReadyLists() {
    for (int i = 0; i < NUM_PRIORITIES; i++)
        readyLists[i] = NULL;
}


void OS_switchToNextTask(void) {
	/* 
        do any policies like priority upgrades here
    */
	
	// go over all the real time tasks, decrement their time 
	// by SWAP_TIME (time between context switches. 12.5ns*0x00FFFFFF
	if (readyLists[0] != 0) {
		list_t current_list = readyLists[0];
		list_t new_list = create_circular_list(current_list->data);
		((TCB_t*)current_list->data)->xTaskTime -= SWAP_TIME;
		while (current_list->next != readyLists[0]) {
			((TCB_t*)current_list->data)->xTaskTime -= SWAP_TIME;
			list_add_by_deadline(new_list, current_list->data);
			current_list = current_list->next;
		}
		readyLists[0] = new_list;
	}
	
	
    //round robin scheduler among threads of same priority, 
    // going through priorities in ascending order
    int i;
    bool found = false;
    for (i = 0; i < NUM_PRIORITIES; i++) {
        if (readyLists[i] != NULL) {			
			if (i == 0) {
				list_t current_list = readyLists[0];
				while (current_list->next != readyLists[0]) {
					// task deadline is "close enough" for some definition of close enough
					if (((TCB_t*)current_list->data)->xTaskTime <= DELTA_REALTIME) {
						pxNextTCB = (TCB_t*)readyLists[i]->data;
						pxNextTCB->xTaskTime = pxNextTCB->xTimeInterval;
						found = true;
						break;
					}
				}
			} else {
				pxNextTCB = (TCB_t *)readyLists[i]->data;
				readyLists[i] = readyLists[i]->next;
				found = true;
				break;
			}            
        }
    }
    if (!found) pxNextTCB = pxCurrentTCB; //or perhaps, a default idle thread's TCB
}


void OS_startScheduler(void) {
	schedulerStarted = true;
}


/**
* REQUIRES: current list is already sorted by deadlines
* Adds to list based on closest to deadline (smallest uxTime)
*/
list_t list_add_by_deadline(list_t lst, void *data) {
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
		if (((TCB_t*)(current_node->data))->xTaskTime <= ((TCB_t*)data)->xTaskTime) {
			add_as_next(current_node, data);
			return lst;
		}
		current_node = current_node->prev;
	}
	return add_to_front(lst, data);
}