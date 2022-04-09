/**
  * Semaphore implementation 
  *
  */
#include "semaphore.h""

void OS_WaitNaive(semaphore_t* s) {
	__asm("CPSID I");
	while ((*s) == 0) {
		__asm("CPSIE I");
		__asm("NOP");
		__asm("CPSID I");
	}
	(*s)--;
	__asm("CPSIE I");
}

void OS_SignalNaive(semaphore_t* s) {
	__asm("CPSID I");
	(*s)++;
	__asm("CPSIE I");
}
