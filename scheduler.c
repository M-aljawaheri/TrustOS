#include <stdint.h>
#include <stdbool.h>
#include "15348.h"
#include "timer.h"
#include "serial.h"
#include <stddef.h>

#include "lists.h"
#include "scheduler.h"
typedef void TCB_t;
extern TCB_t* pxCurrentTCB;
extern TCB_t* tmpThread1;
extern TCB_t* tmpThread2;

void OS_switchToNextTask(void) {
	//pxCurrentTask = pxCurrentTask->xListEntry->next;
	if (tmpThread1 == NULL || tmpThread2 == NULL) return;
	if (pxCurrentTCB == tmpThread1) pxCurrentTCB = tmpThread2;
	if (pxCurrentTCB == tmpThread2) pxCurrentTCB = tmpThread1;
}