/*
   - LED0: P2_0  = GPIO2[2]
   - LED1: P2_1  = GPIO2[2]
   - LED2: P2_2  = GPIO2[2]
   - LED3: P2_3  = GPIO2[3]
   - LED4: P2_4  = GPIO2[4]
   - LED5: P2_5  = GPIO2[5]
   - LED6: P2_6  = GPIO2[6]
   - LED7: P2_7  = GPIO2[7]
*/

#include <Board_LED.h>
#include "LPC17xx.h" 
#include "GPIO_LPC17xx.h"
#include "PIN_LPC17xx.h"
//#include "GPDMA_LPC17xx.h"

volatile uint32_t msTicks = 0;
void SysTick_Handler(void)  {msTicks++;}

#define UART LPC_UART1
void UART_Init() {
	PIN_Configure(0,15,1,0,0);
	PIN_Configure(0,16,1,0,0);
	UART->LCR = (UART->LCR & 0xFFFFFF00) | 0b10000111;
	UART->DLL = 163;
	UART->LCR = (UART->LCR & 0xFFFFFF00) | 0b00000111;	
}

void UART_Char_Send(char c) {
	while(!(UART->LSR & 1<<5));
	UART->THR = c;
}

void UART_Send(const char* str) {
	for(int i = 0; str[i]; i++) {
		UART_Char_Send(str[i]);
	}
}
	
void Timer1_Init() {
	int ms = 500;
	LPC_TIM1->MCR = 3; // bitowo SRI = 0b011 na Timer0
	LPC_TIM1->MR0 = 100e6/4/2 * ms / 1000; // co sekunde = 1e8/4/2
	LPC_TIM1->TCR |= 1; // Counter Enable
	//NVIC_EnableIRQ(TIMER1_IRQn);
}

int g_i = 0;
void TIMER1_IRQHandler(void) {
	LPC_TIM1->IR |= 1; // reset
	uint32_t LED_state = 1 << (g_i++ % 8);
	//LED_SetOut(LED_state);
	//GPIO_PortWrite(2, (1<<8) - 1,	~LED_state);
	LPC_GPIO2->FIOPIN0 = ~LED_state;
}

void KEY2_Interrupt_Init() {
	LPC_SC->EXTMODE = 1;
	LPC_SC->EXTPOLAR = 0;
	LPC_SC->EXTINT = 1;
	PIN_Configure(2,10,1,0,0);
	NVIC_EnableIRQ(EINT0_IRQn);
}

// Handler EINT0_IRQn zwiazany z przyciskiem KEY2
void EINT0_IRQHandler(void) {
	LPC_SC->EXTINT = 1;
}

#define N 8
uint8_t diody[N];

struct {
  __IO uint32_t DMACCSrcAddr;
  __IO uint32_t DMACCDestAddr;
  __IO uint32_t DMACCLLI;
  __IO uint32_t DMACCControl;
} DMA_LLI_zapetlone_diody = {
	&diody[0],
	&LPC_GPIO2->FIOPIN0,
	&DMA_LLI_zapetlone_diody,
	N | // TransferSize - rozmiar tablicy zrodlowej a jak sie skonczy tablica to generuje przerwanie DMA
	(0b000 << 12) | // 000 - 1 burst zrodlowy
	(0b000 << 15) | // 000 - 1 burst docelowy
	(0b000 << 18) | // 000 - burst zrodlowy to bajt
	(0b000 << 21) | // 000 - burst docelowy to bajt
	(0b1 << 26)		| // 1 - inkrementacja zrodla
	(0b0 << 27) 	| // 0 - brak inkrementacji celu
	(0b1 << 31) // 1 - przerwania wlaczone
};

void DMA_Init() {
	LPC_SC->PCONP |= 1 << 29; // set PCGPDMA bit
	LPC_GPDMA->DMACConfig |= 1;
	LPC_SC->DMAREQSEL = 1 << 2; // select Timer1 match 0
	LPC_GPDMA->DMACIntTCClear = 1; // channel 0
	LPC_GPDMA->DMACIntErrClr = 1; // channel 0
	
	// init array
	for(int i = 0; i < N; ++i) {
		diody[i] = ~(1 << (i % 8));
	}
	//
	LPC_GPDMACH0->DMACCSrcAddr = &diody[0];
	LPC_GPDMACH0->DMACCDestAddr = &LPC_GPIO2->FIOPIN0;
	LPC_GPDMACH0->DMACCLLI = &DMA_LLI_zapetlone_diody;
	// DMACCControl:
	// no increment od destination
	// 8 bits
	// irq enabled
	LPC_GPDMACH0->DMACCControl = 
	N | // TransferSize - rozmiar tablicy zrodlowej a jak sie skonczy tablica to generuje przerwanie DMA
	(0b000 << 12) | // 000 - 1 burst zrodlowy
	(0b000 << 15) | // 000 - 1 burst docelowy
	(0b000 << 18) | // 000 - burst zrodlowy to bajt
	(0b000 << 21) | // 000 - burst docelowy to bajt
	(0b1 << 26)		| // 1 - inkrementacja zrodla
	(0b0 << 27) 	| // 0 - brak inkrementacji celu
	(0b1 << 31); // 1 - przerwania wlaczone
	
	LPC_GPDMACH0->DMACCConfig = 
	1 | // enable
	(10 << 1) | // source channel 10, id timer 1, match 0, 
	// destination channel skipped
	(0b010 << 11) | // transfer type: peripherial to memory
	0b1 << 15; // ITC
	//NVIC_EnableIRQ(DMA_IRQn);
}

void DMA_IRQHandler(void) {
	DMA_Init();
}

void Setup(void) {
	//uint32_t returnCode = SysTick_Config(SystemCoreClock / 1000);
	for(int i = 0; i < 8; ++i) {
		PIN_Configure(2, i, 0, PIN_PINMODE_PULLDOWN, PIN_PINMODE_NORMAL);
		LPC_GPIO2->FIODIR0 = 0xff;
	}
	LPC_GPIO2->FIOMASK = 0xffffff00;
	LPC_GPIO2->FIOPIN0 = ~0;
	Timer1_Init();
	DMA_Init();
	UART_Init();
	KEY2_Interrupt_Init();
}

void Loop(void) {
	UART_Send("Ide spac\n");
	__WFI();
}
int main() { Setup(); while(1) Loop(); /* implicit shut down */ }
