#include <avr/interrupt.h>
#include <avr/io.h>
#define FOSC 16000000UL // 오실레이터 주파수(클럭 속도), _delay_ms에 필요한 파라미터
#define F_CPU 16000000UL // CPU 클럭 주파수, _delay_ms에 필요한 파라미터
#include <util/delay.h> // _delay_ms 라이브러리


unsigned int Total_delay = 500;
unsigned int STATUS = 0; // 0, 1 : Default, [2 : 양방향], 3 : Dual-Shift, 4: STACK, 5 : Final
unsigned int START_STOP = 0; // 시작 정지 비트 : 1일 때 정지


// 1번째 동작
unsigned int Default_LED_ARR[8] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};
unsigned int Default_LED_ORDER = 0;
void LED_Default(){
	if (STATUS == 1 && START_STOP == 1) return;
	
	unsigned int LED_MASK = Default_LED_ARR[Default_LED_ORDER];
	
	PORTD = ~(LED_MASK & 0xF0);
	PORTC = ~(LED_MASK & 0x0F);
	
	Default_LED_ORDER = (Default_LED_ORDER + 1) % 8;
}

// 2번째 동작
unsigned int Default_LED_ARR_2[15] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02};
unsigned int Default_LED_ORDER_2 = 0;
void LED_Double(){
	if (START_STOP == 1) return;
	
	unsigned int LED_MASK = Default_LED_ARR_2[Default_LED_ORDER_2];
	PORTD = ~(LED_MASK & 0xF0);
	PORTC = ~(LED_MASK & 0x0F);
	
	Default_LED_ORDER_2 = (Default_LED_ORDER_2 + 1) % 14;
}

// 3번째 동작
unsigned int Default_LED_ARR_3[8] = { 0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81 };
unsigned int Default_LED_ORDER_3 = 0;
void LED_Dual(){
	if (START_STOP == 1) return;
	
	unsigned int LED_MASK = Default_LED_ARR_3[Default_LED_ORDER_3];
	PORTD = ~(LED_MASK & 0xF0);
	PORTC = ~(LED_MASK & 0x0F);
	
	Default_LED_ORDER_3 = (Default_LED_ORDER_3 + 1) % 8;
}


// 4번째 동작
unsigned int Default_LED_ARR_4[] = {
	0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
	0x81, 0x82, 0x84, 0x88, 0x90, 0xA0, 0xC0,
	0xC1, 0xC2, 0xC4, 0xC8, 0xD0, 0xE0,
	0xE1, 0xE2, 0xE4, 0xE8, 0xF0,
	0xF1, 0xF2, 0xF4, 0xF8,
	0xF9, 0xFA, 0xFC,
	0xFD, 0xFE,
	0xFF
	};
	
unsigned int Default_LED_ORDER_4 = 0;
void LED_Stack(){
	if (START_STOP == 1) return;
	
	unsigned int LED_MASK = Default_LED_ARR_4[Default_LED_ORDER_4];
	PORTD = ~(LED_MASK & 0xF0);
	PORTC = ~(LED_MASK & 0x0F);
	
	Default_LED_ORDER_4 = (Default_LED_ORDER_4 + 1) % 35;
}



// Final 동작
unsigned int Default_LED_ORDER_5_1 = 0;
unsigned int Default_LED_ORDER_5_2 = 0;
unsigned int Default_LED_ORDER_5_3 = 0;
void LED_Final(){
	if (START_STOP == 1) return;
	unsigned int LED_MASK = 0x02;
	
	
	if(Default_LED_ORDER_5_1 < 28){
		LED_MASK = Default_LED_ARR_2[Default_LED_ORDER_5_1 % 14];
		Default_LED_ORDER_5_1++;
	}
	
	if(Default_LED_ORDER_5_1 >= 28 && Default_LED_ORDER_5_2 < 16){
		LED_MASK = Default_LED_ARR_3[Default_LED_ORDER_5_2 % 8];
		Default_LED_ORDER_5_2++;
	}
	 
	if(Default_LED_ORDER_5_2 >= 16 && Default_LED_ORDER_5_3 < 70){
		LED_MASK = Default_LED_ARR_4[Default_LED_ORDER_5_3 % 35];
		Default_LED_ORDER_5_3++;
	}
	
	if(Default_LED_ORDER_5_3 >= 70){ 
		for (volatile int i=0; i<2; i++)
		{
			PORTD = 0x00;
			PORTC = 0x00;
			for(volatile int j = 0; j < Total_delay; j++) _delay_ms(1);
			PORTD = (0xF0) ;
			PORTC = (0x0F) ;
			for(volatile int j = 0; j < Total_delay; j++) _delay_ms(1);
		}
		STATUS = 1; 
	}
	
	
	
	PORTD = ~(LED_MASK & 0xF0);
	PORTC = ~(LED_MASK & 0x0F);
}

void PORTsetting(void){
	// 1. 포트 설정
	DDRD  = 0xF0; // 11110000 → PD3 ~ PD0 입력, PD4 ~ PD7 출력
	PORTD = 0xF0; // 11111111 → PD4~7[LED연결] 끄기
	DDRD  &= ~((1 << DDD2) | (1 << DDD3)); // PD2, 3 입력
	PORTD |=  (1 << PORTD2) | (1 << PORTD3); // PD2, 3 내부 풀업
	
	DDRC  = 0x0F; // 00011110 → PC1~4 모두 출력 설정
	PORTC = 0x0F; // PC와 연결된 LED 끄기
	DDRB  &= ~0x03;  // PB1,PB0=입력(0)
	PORTB |= 0x03; // PB0~1 내부 풀업 저항
	
	// 2. 인터럽트 설정
	EICRA |= (1 << ISC01); // INT0[PD2] 하강엣지 트리거
	EIFR |= (1 << INTF0); // INT0 인터럽트 플래그 클리어
	EIMSK |= (1 << INT0); // INT0 인터럽트 허용

	EICRA |= (1 << ISC11); // INT1[PD3] 하강엣지 트리거
	EIFR |= (1 << INTF1); // INT1 인터럽트 플래그 클리어
	EIMSK |= (1 << INT1); // INT1 인터럽트 허용
	

	/* --- 핀체인지 인터럽트 설정 (PORTB용) --- */
	PCIFR  |= 0x01;                // PCIF0 플래그 클리어 (PORTB)
	PCMSK0 |= 0x07;                // PCINT0(0x01)=PB0, PCINT1(0x02)=PB1 허용
	PCICR  |= 0x01;                // PCIE0=1 → PORTB 핀체인지 인터럽트 enable
	// 전원/핀 안정 대기
	_delay_ms(20);

	sei(); // 전역 인터럽트 허용
}



int main(void)
{
	PORTsetting();
	
	while(1){
		if(STATUS == 0 || STATUS == 1) LED_Default();
		else Default_LED_ORDER = 0;
		
		if(STATUS == 2) LED_Double();
		else Default_LED_ORDER_2 = 0;
		
		if(STATUS == 3) LED_Dual();
		else Default_LED_ORDER_3 = 0;
		
		if(STATUS == 4) LED_Stack();
		else Default_LED_ORDER_4 = 0;
		
		if(STATUS == 5) LED_Final();
		else {
			Default_LED_ORDER_5_1 = 0;
			Default_LED_ORDER_5_2 = 0;
			Default_LED_ORDER_5_3 = 0;
		}
		
		for (volatile int j = 0; j < Total_delay; j++) _delay_ms(1);
		
	}
}

ISR(INT1_vect) {
	STATUS = 3;
}

// INT1 인터럽트 (PD3)
ISR(INT0_vect) {
	STATUS = 4;
}


ISR(PCINT0_vect){
	cli();
	_delay_ms(10);
	
	if((PINB & 0x01) == 0 && (PINB & 0x02) == 0){ // PB0 = SW1
		STATUS = 5;
		START_STOP = 0;
	}
	
	
	if((PINB & 0x01) == 0){ // PB0 = SW1
		if (START_STOP == 0) START_STOP = 1;
		else START_STOP = 0; 
	}
	
	if((PINB & 0x02) == 0){  // PB0 = SW1
		if(Total_delay == 100) Total_delay = 500;
		else Total_delay -= 50;
	}
	
	if((PINB & 0x04) == 0) STATUS = 2;
	
}