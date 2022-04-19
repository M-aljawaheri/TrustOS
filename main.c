#include <stdint.h>
#include <stdbool.h>
#include "15348.h"
#include "timer.h"
#include "serial.h"
#include <stddef.h>
#include "scheduler.h"
#include "lists.h"
#include "staticMalloc.h"
#include "semaphore.h"
#include "mutex.h"
#include "scheduler.h"
#include "configconsts.h"

char sparemem[1024];
mutex_t globalMutex;

uint32_t currentSP = 0;
uint32_t nextSP = 0;

extern TCB_t* pxCurrentTCB;
extern TCB_t* pxNextTCB;
extern list_t readyLists[NUM_PRIORITIES];
extern bool schedulerStarted;


/* Important assumptions
 * 1) Real-time tasks finish in T < 200 ms.
 **/

void PLLInit();
void portBSetup();

/**
 * This sets the basepri register to have the priority
 * setting, usually called with either 0 (all exceptions are unmasked)
 * or 1, all exceptions are masked. 
 * https://www.ti.com/lit/ds/spms376e/spms376e.pdf?ts=1646645911650&ref_url=https%253A%252F%252Fwww.ti.com%252Ftool%252FEK-TM4C123GXL
 * consult page 87 of the TI datasheet above for options.
 *
 */
static inline void __set_BASEPRI(uint32_t priority) {
	priority = (priority << 5);
	__asm("MSR basepri, %[priority]\t\n" :: [priority] "r" (priority));
}


void OS_SetupTimerInterrupt(void) {
	SerialWrite("Setting up systick timer..\n");
	// Disable until configuration is done
	NVIC_ST_CTRL_R = 0;
    NVIC_ST_CURRENT_R = 0;
    
	NVIC_ST_RELOAD_R = ( configCPU_CLOCK_HZ / configTICK_RATE_HZ ) - 1UL;
    NVIC_ST_CTRL_R = 0x00000007;
}

semaphore_t GLOBAL_SEMAPHORE = 1;
void SEMAPHORES_Thread1(void);
void SEMAPHORES_Thread2(void);
void SEMAPHORES_Thread3(void);
void SEMAPHORES_Thread4(void);
void SEMAPHORES_REALTIME(void);

/*
 * REQUIRES: addresses returned by MALLOC are (at least) 8 byte aligned
 *
 */
void OS_spawnThread(void (*program)(void), uint32_t tid, 
					uint32_t stack_size, uint32_t priority) {
	__asm("CPSID I");
	// initializing new TCB
	void* stack = MALLOC(stack_size);
	TCB_t* newTCB = (TCB_t*)MALLOC(sizeof(TCB_t));
	newTCB->uxPriority = priority;
	newTCB->uxThreadId = tid;
	newTCB->pxTopOfStack = stack;
	newTCB->pxStack = &((uint8_t*)stack)[stack_size];
						
    // add the thread to readyList
    if (readyLists[priority] == NULL) {
        readyLists[priority] = create_circular_list((void *)newTCB);
    }
    else {
        add_as_next(readyLists[priority], (void *)newTCB);
    }
    pxNextTCB = (pxNextTCB == NULL) ? newTCB : pxNextTCB;
						
	// set up the initial state	
	uint32_t pushed_registers_size = 8*WORD_SIZE;
	uint8_t* temp = ((uint8_t*)(newTCB->pxStack)) - (pushed_registers_size);
	newTCB->pxStack = temp;
						
	// write the pushed registers in reverse order
	// The numbers are just for debugging, can be optimized out
	*((uint32_t*)newTCB->pxStack) = 0;		// R0 = 0
	*((uint32_t*)newTCB->pxStack+1) = 1;	// R1 = 1
	*((uint32_t*)newTCB->pxStack+2) = 2;	// R2 = 2
	*((uint32_t*)newTCB->pxStack+3) = 3;	// R3 = 3
	*((uint32_t*)newTCB->pxStack+4) = 12;	// R12 = 12
	*((uint32_t*)newTCB->pxStack+5) = INITIAL_EXC_RETURN;	// LR = 0
	*((uint32_t*)newTCB->pxStack+6) = (uint32_t)program;
	*((uint32_t*)newTCB->pxStack+7) = INITIAL_XPSR;		
	// {R4-R11, R14}
	(newTCB->pxStack)--;
	*((uint32_t*)newTCB->pxStack) = INITIAL_EXC_RETURN;	// R14
	
	(newTCB->pxStack)--;
	*((uint32_t*)newTCB->pxStack) = 11;	// R11
	//(newTCB->pxStack) -= 7;				// skipping registers R5,R6,R7,R8,R9,R10
	
	(newTCB->pxStack)--;
	*((uint32_t*)newTCB->pxStack) = 10;
	
	(newTCB->pxStack)--;
	*((uint32_t*)newTCB->pxStack) = 9;
	
	(newTCB->pxStack)--;
	*((uint32_t*)newTCB->pxStack) = 8;
	
	(newTCB->pxStack)--;
	*((uint32_t*)newTCB->pxStack) = 7;
	
	(newTCB->pxStack)--;
	*((uint32_t*)newTCB->pxStack) = 6;
	
	(newTCB->pxStack)--;
	*((uint32_t*)newTCB->pxStack) = 5;
	
	(newTCB->pxStack)--;
	*((uint32_t*)newTCB->pxStack) = 4;	// R4	
	
	__asm("CPSIE I");
	// NOTE: above could have been replaced by
	// for i <= 13: *(--sp) = i;
}

/**
 * Special spawn thread for real time 
 * tasks. period T must be big enough to allow the system
 * to work and T_program < T_systickInterrupt
 * returns: thread that was spawned
 */
TCB_t* OS_spawnPeriodicThread(void (*program)(void), uint32_t tid, 
							uint32_t stack_size, int32_t period) {
	
	__asm("CPSID I");	// disable interrupts
	// initializing new TCB
	void* stack = MALLOC(stack_size);
	uint32_t priority = 0;
	TCB_t* newTCB = (TCB_t*)MALLOC(sizeof(TCB_t));
	newTCB->uxPriority = priority;
	newTCB->uxThreadId = tid;
	newTCB->pxTopOfStack = stack;
	newTCB->pxStack = &((uint8_t*)stack)[stack_size];
	newTCB->xTaskTime = period;
	newTCB->xTimeInterval = period;
						
	    // add the thread to readyList
    if (readyLists[priority] == NULL) {
        readyLists[priority] = create_circular_list((void *)newTCB);
    }
    else {
        add_as_next(readyLists[priority], (void *)newTCB);
    }
    pxNextTCB = (pxNextTCB == NULL) ? newTCB : pxNextTCB;
						
	// set up the initial state	
	uint32_t pushed_registers_size = 8*WORD_SIZE;
	uint8_t* temp = ((uint8_t*)(newTCB->pxStack)) - (pushed_registers_size);
	newTCB->pxStack = temp;
						
	// write the pushed registers in reverse order
	// The numbers are just for debugging, can be optimized out
	*((uint32_t*)newTCB->pxStack) = 0;		// R0 = 0
	*((uint32_t*)newTCB->pxStack+1) = 1;	// R1 = 1
	*((uint32_t*)newTCB->pxStack+2) = 2;	// R2 = 2
	*((uint32_t*)newTCB->pxStack+3) = 3;	// R3 = 3
	*((uint32_t*)newTCB->pxStack+4) = 12;	// R12 = 12
	*((uint32_t*)newTCB->pxStack+5) = INITIAL_EXC_RETURN;	// LR = 0
	*((uint32_t*)newTCB->pxStack+6) = (uint32_t)program;
	*((uint32_t*)newTCB->pxStack+7) = INITIAL_XPSR;		
	// {R4-R11, R14}
	(newTCB->pxStack)--;
	*((uint32_t*)newTCB->pxStack) = INITIAL_EXC_RETURN;	// R14
	
	(newTCB->pxStack)--;
	*((uint32_t*)newTCB->pxStack) = 11;	// R11
	//(newTCB->pxStack) -= 7;				// skipping registers R5,R6,R7,R8,R9,R10
	
	(newTCB->pxStack)--;
	*((uint32_t*)newTCB->pxStack) = 10;
	
	(newTCB->pxStack)--;
	*((uint32_t*)newTCB->pxStack) = 9;
	
	(newTCB->pxStack)--;
	*((uint32_t*)newTCB->pxStack) = 8;
	
	(newTCB->pxStack)--;
	*((uint32_t*)newTCB->pxStack) = 7;
	
	(newTCB->pxStack)--;
	*((uint32_t*)newTCB->pxStack) = 6;
	
	(newTCB->pxStack)--;
	*((uint32_t*)newTCB->pxStack) = 5;
	
	(newTCB->pxStack)--;
	*((uint32_t*)newTCB->pxStack) = 4;	// R4	
	
	__asm("CPSIE I");	// enable interrupts
	return newTCB;
}


/*
 * main.c
 */
int main(void) 
{
	SetupSerial();	
	SerialWrite("Starting OS..\n");
    
	PLLInit();
	portBSetup();
    
	
	OS_SetupTimerInterrupt();
	initMalloc(sparemem, 20000);
    initReadyLists(); //must be init before spawning threads
	
	globalMutex = create_mutex();
	
	// test OS
	DISABLE_INTERRUPTS();
	OS_spawnPeriodicThread(&SEMAPHORES_REALTIME, 0, 200, 2000);
	OS_spawnThread(&SEMAPHORES_Thread2, 1, 200, 1);
	OS_spawnThread(&SEMAPHORES_Thread3, 2, 200, 1);
	OS_spawnThread(&SEMAPHORES_Thread4, 3, 200, 1);
	ENABLE_INTERRUPTS();
	OS_startScheduler();
	while (1) {}
	//thread2();
}


/**
 * Systick handler for the device.
 * In an effort to replicate freeRTOS's implementation, 
 * The systick handler simply pends a pendSV interrupt
 * which is the lowest priority interrupt that can also
 * be pended on demand. The pendSV handler is what handles
 * the context switch. This allows us to easily pend a thread
 * yield on demand, and also lets our context switch only happen
 * when all interrupts are done executing
 */
void OS_SystickHandler(void) {
	// TODO: do we really need to disable interrupts here
	
	// DISABLE_INTERRUPTS();
	
	// PendSV will only run when all current 
	NVIC_INT_CTRL_R = NVIC_INT_CTRL_PEND_SV;	// TODO: abstract away the regisiters for this step
	
	// ENABLE_INTERRUPTS();
}


void OS_PendSVHandler(void) {
	//SerialWrite("Pend SV timer hit\n");
	__asm("POP {LR, R7}		\n");
	if (!schedulerStarted) return;

	__asm("CPSID I");
	
	if (pxCurrentTCB == NULL) {
		pxCurrentTCB = pxNextTCB;
		
		nextSP = (uint32_t)(pxCurrentTCB->pxStack);
		__asm volatile("LDR R0, =nextSP		\n");
		__asm volatile("LDR SP, [R0]		\n");
		__asm volatile("POP {R4-R11, R14}	\n");
		__asm volatile("ISB					\n");
		__asm volatile("CPSIE I				\n");
		__asm volatile("bx R14				\n");
	}
	
	// save software-saved registers
	__asm("PUSH {R4-R11, R14}");
	OS_switchToNextTask();
	
	// save current SP in currentTCP SP
	// TODO: can save a ton of unnecessary instructions here
	currentSP = (uint32_t)&(pxCurrentTCB->pxStack);
	__asm("LDR R0, =currentSP");
	__asm("LDR R0, [R0]");
	__asm("STR SP, [R0]");
	
	
	// update SP to nextThread SP
	nextSP = (uint32_t)(pxNextTCB->pxStack);
	__asm("LDR R0, =nextSP");
	__asm("LDR SP, [R0]");
	
	// pxCurrentTCB = pxNextTCB;
	__asm("LDR R0, =pxNextTCB");
	__asm("LDR R0, [R0]");
	__asm("LDR R1, =pxCurrentTCB");
	__asm("STR R0, [R1]");
	
	// restore context and jump to current PC
	__asm("POP {R4-R11, R14}");
	__asm("CPSIE I");
	__asm("bx R14");
}


void PLLInit()
{
    SYSCTL_RCC2_R |= 0x80000000;
    SYSCTL_RCC2_R |= 0x00000800;
    SYSCTL_RCC_R = (SYSCTL_RCC_R & ~0x000007C0) + 0x00000540;
    SYSCTL_RCC2_R &= ~0x00000070;
    SYSCTL_RCC2_R &= ~0x00002000;
    SYSCTL_RCC2_R |= 0x40000000;
    SYSCTL_RCC2_R = (SYSCTL_RCC2_R & ~0x1FC00000) + (4 << 22);
    while ((SYSCTL_RIS_R &0x00000040)==0){};
    SYSCTL_RCC2_R &= ~0x00000800;
}

void portBSetup() {
	// port B setup 
	SYSCTL_RCGCGPIO_R |= 0x02; 	    // enable clock for PORT F
	GPIO_PORTB_LOCK_R = 0x4C4F434B; // this value unlocks the GPIOCR register.
	GPIO_PORTB_CR_R = 0xFF;
	GPIO_PORTB_AMSEL_R = 0x00;      // disable analog functionality
	GPIO_PORTB_PCTL_R = 0x00000000; // Select GPIO mode in PCTL
	GPIO_PORTB_DIR_R = 0x0F;        // Port F0 & F4 are input, rest are output
	GPIO_PORTB_AFSEL_R = 0x00;      // Disable alternate functionality
	GPIO_PORTB_DEN_R = 0xFF;        // Enable digital ports	
}


/* Test functions */

void SEMAPHORES_Thread1(void){
  while(1){
    acquire_mutex(globalMutex, 0, 2); 
    // exclusive access to object

	for (int i = 0;i < 100; i++) {
		GPIO_PORTB_DATA_R = 0x1;
		for (int j = 0; j < 3; j++) {
			SerialWrite("Thread1 Stalling\n");
		}	
		GPIO_PORTB_DATA_R &= (~0x1UL);
	}			  
    release_mutex(globalMutex, 0, 2);
	//for (int i = 0; i < 5; i++) {
	//	SerialWrite("Thread1 Stalling\n");
	//}		
    // other processing
  }
}

void SEMAPHORES_REALTIME(void) {
	  while(1){
    // exclusive access to object

	for (int i = 0;i < 100; i++) {
		GPIO_PORTB_DATA_R = 0x1;
		for (int j = 0; j < 3; j++) {
			SerialWrite("Thread1 Stalling\n");
		}	
		GPIO_PORTB_DATA_R &= (~0x1UL);
	}			  
  }
}

void SEMAPHORES_Thread2(void){
  while(1){
    acquire_mutex(globalMutex, 1, 1);
    // exclusive access to object

	for (int i = 0;i < 100; i++) {
		GPIO_PORTB_DATA_R = 0x2;
		for (int j = 0; j < 3; j++) {
			SerialWrite("Thread2 Stalling\n");
		}
		GPIO_PORTB_DATA_R &= (~0x2UL);
	}		

    release_mutex(globalMutex, 1, 1);
    // other processing
	//for (int i = 0; i < 25; i++) {
	//	SerialWrite("Thread 2 Stalling\n");
	//}
  }
}

void SEMAPHORES_Thread3(void){
  while(1){
    acquire_mutex(globalMutex, 2, 1);
    // exclusive access to object

	for (int i = 0;i < 100; i++) {
		GPIO_PORTB_DATA_R = 0x4;
		for (int j = 0; j < 3; j++) {
			SerialWrite("Thread3 Stalling\n");
		}
		GPIO_PORTB_DATA_R &= (~0x4UL);
	}		

    release_mutex(globalMutex, 2, 1);
    // other processing
	//for (int i = 0; i < 625; i++) {
	//	SerialWrite("Thread 2 Stalling\n");
	//}
  }
}

void SEMAPHORES_Thread4(void) {
  while(1){
    acquire_mutex(globalMutex, 3, 1);
    // exclusive access to object

	for (int i = 0;i < 100; i++) {
		GPIO_PORTB_DATA_R = 0x8;
		for (int j = 0; j < 3; j++) {
			SerialWrite("Thread2 Stalling\n");
		}
		GPIO_PORTB_DATA_R &= (~0x8UL);
	}		

    release_mutex(globalMutex, 3, 1);
    // other processing
	//for (int i = 0; i < 3000; i++) {
	//	SerialWrite("Thread 2 Stalling\n");
	//}
  }
}
