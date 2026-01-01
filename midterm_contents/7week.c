#include <avr/interrupt.h>
#include <avr/io.h>
#define FOSC 16000000UL // 오실레이터 주파수(클럭 속도), _delay_ms에 필요한 파라미터
#define F_CPU 16000000UL // CPU 클럭 주파수, _delay_ms에 필요한 파라미터
#include <util/delay.h> // _delay_ms 라이브러리

#define SW1_EVENT 0x01
#define SW2_EVENT 0x02

unsigned int status;

void LED_ALL_ON_OFF(int _n, int _ms) {
	for (volatile int i = 0; i < _n; i++) {
		PORTD &= 0x0F; // LED ON (PD4~PD7, PC0~PC3만 0으로) 
		PORTC &= 0xF0; // LED ON (PD4~PD7, PC0~PC3만 0으로)
		for (volatile int j = 0; j < _ms; j++) _delay_ms(1);

		PORTD = (PORTD & 0x0F) | 0xF0; // LED OFF (PD4~PD7 = 1, PC0~PC3 = 1)
		PORTC = (PORTC & 0xF0) | 0x0F; // LED OFF (PD4~PD7 = 1, PC0~PC3 = 1)
		for (volatile int j = 0; j < _ms; j++) _delay_ms(1);
	}
}


void LED_SHIFT(int _n, int _ms) {
	for (int i = 0; i < _n; i++) {
		// 왼쪽으로 이동
		for (int j = 0; j < 7; j++) {
			unsigned int maskC = (~(0x01 << j)) & 0x0F;  // PC0~PC3 제어
			unsigned int maskD = (~(0x01 << j)) & 0xF0;  // PD4~PD7 제어

			PORTC = (PORTC & 0xF0) | maskC;
			PORTD = (PORTD & 0x0F) | maskD;

			for (volatile int t = 0; t < _ms; t++) _delay_ms(1);
		}

		// 오른쪽으로 이동
		for (int k = 0; k < 8; k++) {
			unsigned int maskC = (~(0x80 >> k)) & 0x0F;
			unsigned int maskD = (~(0x80 >> k)) & 0xF0;

			PORTC = (PORTC & 0xF0) | maskC;
			PORTD = (PORTD & 0x0F) | maskD;

			for (volatile int t = 0; t < _ms; t++) _delay_ms(1);
		}
	}

	// LED 모두 끄기 (해당 비트만 조작)
	PORTD = (PORTD & 0x0F) | 0xF0;
	PORTC = (PORTC & 0xF0) | 0x0F;
}


void INTsetting(void){
	// PD2, 3 입력 + 내부 풀업
	DDRD  &= ~((1 << DDD2) | (1 << DDD3));
	PORTD |=  (1 << PORTD2) | (1 << PORTD3);  // 내부 풀업 ON
	
	 // INT0, INT1 하강엣지 트리거
	 EICRA = (1 << ISC01) | (1 << ISC11);
	 
	 // 인터럽트 플래그 클리어
	 EIFR  = (1 << INTF0) | (1 << INTF1);
	 
	 // 인터럽트 허용
	 EIMSK = (1 << INT0) | (1 << INT1);
	 
	 sei();
 }

int main(void)
{
    /* Replace with your application code */
    DDRD  |= 0xF0; // 11110000 → PD3 ~ PD0 입력, PD4 ~ PD7 출력
	PORTD |= 0xF0; // 11111111 → PD4~7[LED연결] 끄기, 입력핀은 모두 풀업저항
    DDRC  |= 0x0F; // 00011110 → PC1~4 모두 출력 설정
	PORTC |= 0x0F; // PC와 연결된 LED 끄기

	INTsetting();
	
	while (1) {
		PORTD = (PORTD & 0x0F) | 0x50;
		PORTC = (PORTC & 0xF0) | 0x05;
		_delay_ms(500);
		PORTD = (PORTD & 0x0F) | 0xA0;
		PORTC = (PORTC & 0xF0) | 0x0A;
		_delay_ms(500);
		
		/* 이런 활용방법도 있음
		switch(status){
			case  SW1_EVENT:
			LED_SHIFT();
			case SW2_EVENT;
			LED_SHIFT();
		}
		status = 0;
		*/
	}
}

// INT0 인터럽트 (PD2)
ISR(INT0_vect) {
	sei(); // 전역 인터럽트 키기
	LED_SHIFT(2, 300);         // 즉시 실행
	
	// status = SW1_EVENT; // 이런 활용방법도 있음
}

// INT1 인터럽트 (PD3)
ISR(INT1_vect) {
	// cli(); // 전역 인터럽트 끄기
	sei(); // 전역 인터럽트 키기
	LED_ALL_ON_OFF(3, 1000);   // 즉시 실행
	
	// status = SW1_EVENT; // 이런 활용방법도 있음
}

