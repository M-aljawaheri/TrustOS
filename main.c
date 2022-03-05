#include <stdint.h>
#include <stdbool.h>
#include "15348.h"
#include "timer.h"
#include "serial.h"

#define OS_SystickHandler SysTick_Handler
#define configCPU_CLOCK_HZ (80000000)	// 80 Mhz clock frequency
#define configTICK_RATE_HZ (1000)       // 1000 hz tick rate

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
 * Systick handler for the device.
 * In an effort to replicate freeRTOS's implementation, 
 * The systick handler simply pends a pendSV interrupt
 * which is the lowest priority interrupt that can also
 * be pended on demand. The pendSV handler is what handles
 * the context switch.
 */
void OS_SystickHandler(void) {
	SerialWrite("Systick timer hit\n");
}

void OS_SetupTimerInterrupt(void) {
	SerialWrite("Setting up systick timer..\n");
	// Disable until configuration is done
	NVIC_ST_CTRL_R = 0;
    NVIC_ST_CURRENT_R = 0;
    
	NVIC_ST_RELOAD_R = ( configCPU_CLOCK_HZ / configTICK_RATE_HZ ) - 1UL;
    NVIC_ST_CTRL_R = 0x00000007;
}


/*
 * main.c
 */
int main(void) 
{
	SerialWrite("Starting OS..\n");
    PLLInit();
	portBSetup();
	SetupSerial();
    
	OS_SetupTimerInterrupt();
	
	while (1) {
		GPIO_PORTB_DATA_R = 0x1;
		GPIO_PORTB_DATA_R = 0x0;
	}

}
