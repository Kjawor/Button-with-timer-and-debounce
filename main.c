#include <inttypes.h>
#include <stdbool.h>

struct rcc {
  volatile uint32_t CR, PLLCFGR, CFGR, CIR, AHB1RSTR, AHB2RSTR, AHB3RSTR,
      RESERVED0, APB1RSTR, APB2RSTR, RESERVED1[2], AHB1ENR, AHB2ENR, AHB3ENR,
      RESERVED2, APB1ENR, APB2ENR, RESERVED3[2], AHB1LPENR, AHB2LPENR,
      AHB3LPENR, RESERVED4, APB1LPENR, APB2LPENR, RESERVED5[2], BDCR, CSR,
      RESERVED6[2], SSCGR, PLLI2SCFGR;
  //structure that holds all the RCC registers that are responsible for managing the clock and reset functionalities of peripherals
  //with some of these registers we are able to turn on peripherals that are turned off initially

};



#define RCC ((struct rcc*) 0x40023800)

struct gpio{
	volatile uint32_t MODER,OTYPER,OSPEEDR,PUPDR,IDR,ODR,BSRR,LCKR,AFR[2];
};

#define GPIOD ((struct gpio *) 0x40020C00)
#define GPIOA ((struct gpio *) 0x40020000)

struct nvic{
	volatile uint32_t ISER[8U],RESERVED0[24U],ICER[8U], RESERVED1[24U],  ISPR[8U], RESERVED2[24U],
	ICPR[8U], RESERVED3[24U], IABR[8U], RESERVED4[56U], IP[60U], RESERVED5[644U], STIR;
};

#define NVIC ((struct nvic *) 0xE000E100)

struct tim{
	volatile uint32_t CR1,CR2,SMCR,DIER,SR,EGR,CCMR1,CCMR2,CCER,CNT,PSC,ARR,RESERVED0,CCR1,CCR2,CCR3,CCR4,RESERVED1,DCR,DMAR,TIM2_OR,
	TIM5_OR;
};

#define TIM2 ((struct tim*) 0x40000000) //location of timer2 in memory
#define FREQ 16000000 //CPU clock frequency

void timer_init(){
	RCC->APB1ENR |=(1U<<0); //turn on timer clock
	NVIC->IP[7] |= (1U<<4); //set priority for the interrupt that will be generated by the timer
	//once the value in the CNT register reaches 0
	//each IP register holds 4 interrupts e.g. IP[0] holds interrupts 0-3
	//each IP register is split into 4 section each 8 bits long, each section corresponds to an interrupt
	//in the case of the STM32f411e-discovery board only the 4 most significant bits are used to set the priority
	//for an interrupt, the 4 least significant bits are read only.
	//tim2 interrupt index is 28 which is located in the 7th IP register


	NVIC->ISER[28>>5UL] = (1U<<28);
	//we want to turn on the interrupt using the ISER register
	//there is multiple ISER registers each 32 bits long
	//each bit in the registers corresponds to an interrupt
	//setting a bit in the register will enable an interrupt.
	//because each register is 32 bits long >>5UL is used to get the correct ISER register of the interrupt we want.
	//each multiple of 32 will cause us to have to move up a register and start from bit 0
	//e.g. interrupt 33 is located in ISER[1] at the 1st(not 0) bit.
	//in our case TIM2 interrupt is located in ISER[0] so we can easily pass the number we want without
	//needing to figure out which bit the interrupt would be on.

	TIM2->CR1 &=~(1U<<0);//make sure the timer is off
	RCC->APB1RSTR |= (1U<<0); //reset the timer
	RCC->APB1RSTR &= ~(1U<<0); //clear reset timer bit



	TIM2->PSC = 15; //set prescaler, 15 will result in 1 usec(1 microsecond) counter increment
	TIM2->ARR = 1000; //1 microseconds x 1000 = 1 milisecond
	//this means a timer interrupt will be generated each millisecond
	TIM2->EGR |=(1U<<0); //reset timer and update registers
	TIM2->DIER |=(1U<<0);//enable interrupt
	TIM2->CR1 |= (1U<<0);//enable timer



}



volatile uint32_t tick =0; //count that has to reach 50 in order for the debounce to be successful
volatile uint32_t laststate =0; //hold the state of the button from the previous interrupt
volatile uint32_t buttonstate =0; //holds the state of the button after a successful debouncing

//we debounce by incrementing a timer when the button is in the same state as it was when the previous interrupt
//when the current reading is not the same as of the previous interrupt we want to reset the counter as a bounce occurred.
//the only way for the button to fully register a state switch it needs to remain in that state for a certain period of time
//in this case its 50ms

//if the button state remains the same for 50ms we check to see if the current read state is the same as it was
//when we were successfully able to change it before

void TIM2_IRQHandler(void) {
	if(TIM2->SR & (1U<<0)) { //checks to see if the interrupt generated when the timer reached 0 has been generated.
		TIM2->SR &= ~(1U<<0); //if the interrupt has been generated we need to clear the flag so it can be triggered
		//again in the future
		uint32_t reading = GPIOA->IDR &(1U<<0); //get the current reading of the button

		if(reading!=laststate) { //if the current reading is not the same as the reading from the previous interrupt
			//we want to reset the timer as a bounce has occurred
			tick=0;
		}
		else{
			tick++; //if the state is the same as the previous state we want to increment as it could mean the button
			// has stabilized and is past its bouncing period
		}

		if(tick > 50) {
			//this checks to see if after the 50ms of the same state have passed (meaning the button has been debounced)
			//the button state is different to what it was when the 50ms were previously successful

			if(reading != buttonstate) {
				buttonstate = reading; //if the current reading is different to what it previously was when the debounce
				//was successful we want to update it to the current reading
				if(buttonstate){//we want to check if the button reading is high
					//this means the button is pressed
					//if the button is pressed meaning the IDR reading from GPIOA is set to 1
					//we want to change the state of the LED

					GPIOD->ODR ^= (1U<<13); //set/clear bit of LED
				}
			}
		}
		laststate = reading; //update the last saved reading at the end of each interrupt so that in the next interrupt
		//we can see if the current reading is the same as the previous if not we reset the tick

	}
}






int main(void){
	RCC->AHB1ENR |= (1U << 0); //enable GPIOA clock for button
	RCC->AHB1ENR |= (1U<<3); //enable GPIOD clock for LED
	GPIOD->MODER |= (1U<<26); //set orange LED to output


	GPIOA->MODER &=~(1U<<0); //makes sure the button pin is input
	GPIOA->PUPDR &= ~(3U<<0); //clear the pullup/pulldown register for the button pin
	GPIOA->PUPDR |= (2U<<0); //set the button pin to a pulldown register.

	timer_init();
	for(;;);
}
