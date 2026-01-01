#include <avr/interrupt.h>
#include <avr/io.h>
#define FOSC 16000000UL // 오실레이터 주파수(클럭 속도), _delay_ms에 필요한 파라미터
#define F_CPU 16000000UL // CPU 클럭 주파수, _delay_ms에 필요한 파라미터
#include <util/delay.h> // _delay_ms 라이브러리

#define SW1_EVENT 0x01
#define SW2_EVENT 0x02
#define SW3_EVENT 0x03
#define SW4_EVENT 0x04

volatile unsigned int  status;
volatile uint32_t LED_period = 500;


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
	 PCMSK0 |= 0x03;                // PCINT0(0x01)=PB0, PCINT1(0x02)=PB1 허용
	 PCICR  |= 0x01;                // PCIE0=1 → PORTB 핀체인지 인터럽트 enable
	 // 전원/핀 안정 대기
	 _delay_ms(20);

	 sei(); // 전역 인터럽트 허용
 }



int main(void)
{
	PORTsetting();
	status = SW3_EVENT;
	unsigned int LED_Mask = 0xAA;
	
	while(1){
		PORTD = (PORTD & 0x0F) | (LED_Mask & 0xF0);
		PORTC = (PORTC & 0xF0) | (LED_Mask & 0x0F);
		for (int i=0;i < LED_period;i++) _delay_ms(1);
		PORTD = (PORTD & 0x0F) | (~LED_Mask & 0xF0);
		PORTC = (PORTC & 0xF0) | (~LED_Mask & 0x0F);
		for (int i=0;i < LED_period;i++) _delay_ms(1);
		
		switch(status){
			case  SW1_EVENT:
				LED_Mask = 0xF0;
				break;
			case SW2_EVENT:
				LED_Mask = 0x3C;
				break;
			case SW3_EVENT:
				LED_Mask = 0xAA;
				break;
		}
	}
	_delay_ms(500);
}

// INT0 인터럽트 (PD2)
ISR(INT0_vect) {
	status = SW1_EVENT;
}

// INT1 인터럽트 (PD3)
ISR(INT1_vect) {
	status = SW2_EVENT;
}


ISR(PCINT0_vect){
	cli();
	_delay_ms(10);
	if((PINB & 0x01) == 0){          // PB0 버튼 눌림 (0이면 눌림) → 토글과 유사해서 핀 인식이 필요.
		status = SW3_EVENT;
	}
	else if((PINB & 0x02) == 0){     // PB1 버튼 눌림 (0이면 눌림)
		if(LED_period == 100) LED_period = 500;
		else LED_period -= 50;
	}
	sei();
}