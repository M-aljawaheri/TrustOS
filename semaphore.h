#ifndef __SEMAPHORE_H
#define __SEMAPHORE_H
typedef volatile unsigned int semaphore_t;

void OS_WaitNaive(semaphore_t* s);
void OS_SignalNaive(semaphore_t* s);

#endif