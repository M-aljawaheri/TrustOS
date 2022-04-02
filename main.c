#include <stdint.h>
#include <stdbool.h>
#include "15348.h"
#include "timer.h"
#include "serial.h"
#include <stddef.h>
#include "scheduler.h"
#include "lists.h"
#include "staticMalloc.h"

#define OS_SystickHandler SysTick_Handler
#define OS_PendSVHandler PendSV_Handler
#define configCPU_CLOCK_HZ (80000000)	// 80 Mhz clock frequency
#define configTICK_RATE_HZ (1000)       // 1000 hz tick rate
#define INITIAL_XPSR					( 0x01000000 )
#define INITIAL_EXC_RETURN				( 0xfffffff9 )
//(0xFFFFFFB8)
// (0xFFFFFFBC)
// or d? or 9
#define MAX_SYSCALL_INTERRUPT_PRIORITY (1)

#define DISABLE_INTERRUPTS()     \
{								 \
	__set_BASEPRI( MAX_SYSCALL_INTERRUPT_PRIORITY );   			 \
	__asm("DSB			\n");	 \
	__asm("ISB			\n");	 \
}	

#define ENABLE_INTERRUPTS()			__set_BASEPRI(0)
#define WORD_SIZE (4)

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
};
typedef struct taskControlBlock TCB_t;
char sparemem[1024];

TCB_t* tmpThread1 = NULL;
TCB_t* tmpThread2 = NULL;
TCB_t* pxCurrentTCB = NULL;
bool schedulerStarted = false;
uint32_t pendcounter = 0;

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

void thread1() {
	while (1) {
		ENABLE_INTERRUPTS();
		GPIO_PORTB_DATA_R = 0x1;
		GPIO_PORTB_DATA_R &= (~0x1UL);
	}
}

void thread2() {
	while (1) {
		ENABLE_INTERRUPTS();
		GPIO_PORTB_DATA_R = 0x2;
		GPIO_PORTB_DATA_R &= (~0x2UL);
	}
}

/*
 * REQUIRES: addresses returned by MALLOC are (at least) 8 byte aligned
 *
 */
void OS_spawnThread(void (*program)(void), uint32_t tid, 
					uint32_t stack_size, uint32_t priority) {
	// initializing new TCB
	void* stack = MALLOC(stack_size);
	TCB_t* newTCB = (TCB_t*)MALLOC(sizeof(TCB_t));
	newTCB->uxPriority = priority;
	newTCB->uxThreadId = tid;
	newTCB->pxTopOfStack = stack;
	newTCB->pxStack = &((uint8_t*)stack)[stack_size];
						
	// add it to the circular READY list	
	if (tmpThread1 == NULL) tmpThread1 = newTCB;
	else tmpThread2 = newTCB;
	pxCurrentTCB = tmpThread1;
						
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
	*((uint32_t*)newTCB->pxStack+5) = 0;	// LR = 0
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
	
	
	// NOTE: above could have been replaced by
	// for i <= 13: *(--sp) = i;
}

void OS_startScheduler(void) {
	schedulerStarted = true;
	
	/*
	
	// start popping off registers
	__asm volatile
	(
	"   LDR R3, =pxCurrentTCB                \n"    // load address of current TCB
	"	LDR R2, [R3]						  \n"
	"	LDR R0, [R2]						  \n"
	"	LDMIA R0!, {R4-R11, R14}			  \n"
	"	MOV SP, R0							  \n"
	"   ISB									  \n"
	"										  \n"
	"	bx R14								  \n"
	);
	
	*/
}

// int memthread1[100];
// OS_addThread(memthread1);
// OS_addThread(pointerToMem)

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
	initMalloc(sparemem);
	
	// test OS
	DISABLE_INTERRUPTS();
	OS_spawnThread(&thread1, 0, 100, 1);
	OS_spawnThread(&thread2, 1, 100, 1);
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
	// SerialWrite("Systick timer hit\n");
	// if (tmpThread1 == NULL || tmpThread2 == NULL) return;
	// TODO: do we really need to disable interrupts here
	
	// DISABLE_INTERRUPTS();
	
	// PendSV will only run when all current 
	NVIC_INT_CTRL_R = NVIC_INT_CTRL_PEND_SV;	// TODO: abstract away the regisiters for this step
	
	// ENABLE_INTERRUPTS();
}

void OS_switchToNextTask(void) {
	//pxCurrentTask = pxCurrentTask->xListEntry->next;
	if (tmpThread1 == NULL || tmpThread2 == NULL) return;
	if (pxCurrentTCB == tmpThread1) pxCurrentTCB = tmpThread2;
	else if (pxCurrentTCB == tmpThread2) pxCurrentTCB = tmpThread1;
}

void OS_PendSVHandler(void) {
	//SerialWrite("Pend SV timer hit\n");
	if (!schedulerStarted) return;
	
	if (pendcounter == 0) {
		__asm volatile
		(
			"   LDR R3, =pxCurrentTCB                \n"    // load address of current TCB
		);
	} else {
		__asm volatile 
		(
		"	MOV R0, SP                           \n"	// load current SP for a future store
		"   LDR R3, =pxCurrentTCB                \n"    // load address of current TCB
		"	LDR R2, [R3]						 \n"	// R2 = currentTCB
		"	STMDB R0!, {R4-R11, R14} 			 \n"	// push software-saved registers
		"   STR R0, [R2]						 \n" 	// save stack pointer in current TCB
		);
	}
	
	
	pendcounter = 1;
	DISABLE_INTERRUPTS();
	OS_switchToNextTask();
	ENABLE_INTERRUPTS();
	
	// start popping off registers
	__asm volatile
	(
	"   LDR R3, =pxCurrentTCB                \n"    // load address of current TCB
	"	LDR R2, [R3]						  \n"
	"	LDR R0, [R2]						  \n"
	"	LDMIA R0!, {R4-R11, R14}			  \n"
	"	MOV SP, R0							  \n"
	"   ISB									  \n"
	"										  \n"
	"	bx R14								  \n"
	);
}


