#include <stdint.h>
#include <stdbool.h>
#include "15348.h"
#include "timer.h"
#include "serial.h"
#include <stddef.h>

#include "lists.h"
#include "scheduler.h"
//extern struct taskControlBlock* pxCurrentTCB;
//extern struct taskControlBlock* tmpThread1;
//extern struct taskControlBlock* tmpThread2;
/*
void OS_switchToNextTask(void) {
	//pxCurrentTask = pxCurrentTask->xListEntry->next;
	if (tmpThread1 == NULL || tmpThread2 == NULL) return;
	if (pxCurrentTCB == tmpThread1) pxCurrentTCB = tmpThread2;
	if (pxCurrentTCB == tmpThread2) pxCurrentTCB = tmpThread1;
}
*/