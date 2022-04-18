
#ifndef __SCHEDULER_H
#define __SCHEDULER_H
#include <stdint.h>
#include <stdbool.h>
#include "lists.h"

/**
 * Pre-emtive scheduler for the OS
 * for now simply uses a round-robin scheduling algorithm
 */
 
 /* We adapt the freeRTOS naming convention:
 * Prefixes are as follows:
 * p: pointer
 * u: unsigned
 * l: long
 * x: size_t, 32 bit on the cortex m4 architecture we are targetting.
 *
 **/
 struct taskControlBlock {
	uint32_t* pxStack;			// base SP for this thread
	uint32_t* pxTopOfStack;		// current SP for this thread, TODO: should be volatile?
	list_t xListEntry;		// link back to the list item this TCB is in. This list item should include what list it's in
	uint32_t uxPriority;		// current priority of this thread
	uint32_t uxThreadId;		// ID for this thread
	int32_t xTimeInterval; 	// Every how many ms should this task run (within some delta DELTA_REALTIME)
	int32_t xTaskTime;		// Time since task last ran (ms)
};
typedef struct taskControlBlock TCB_t;

void OS_switchToNextTask(void);
void OS_startScheduler(void);
list_t list_add_by_deadline(list_t lst, void *data);
void initReadyLists();

#endif