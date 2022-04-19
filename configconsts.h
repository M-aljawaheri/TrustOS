#ifndef __CONFIG_H
#define __CONFIG_H

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
#define NUM_PRIORITIES 4

#define DISABLE_INTERRUPTS()     \
{								 \
	__set_BASEPRI( MAX_SYSCALL_INTERRUPT_PRIORITY );   			 \
	__asm("DSB			\n");	 \
	__asm("ISB			\n");	 \
}	

#define ENABLE_INTERRUPTS()			__set_BASEPRI(0)
#define WORD_SIZE (4)

# define SWAP_TIME (210) // (209715187)
#define SWAP_TIME_MS (210)
#define DELTA_REALTIME (10)		// 10 ms

#endif