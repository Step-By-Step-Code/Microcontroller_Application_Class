#include <avr/io.h>
#define FOSC 16000000UL
#define F_CPU 16000000UL
#include <util/delay.h>
#include <avr/interrupt.h>

ISR(PCINT0_vect)
{
	/* 플래그 클리어: PCIF0 비트에 1을 쓰면 클리어됨 (PORTB용) */
	cli();
	PCIFR |= 0x01;    // PCIF0 = 1 → 전에 들어온 인터럽트 벡터 실행 제거
	sei();

	/* 간단 디바운스 */
	_delay_ms(15);

	/* 현재 PORTB 입력 읽기 */
	unsigned char now_b = PINB;
	
	/* PB0(PCINT0) 확인: 마스크 0x01 (PB0 비트) */
	if ((now_b & 0x01) == 0) {           // 눌림=Low이면 실행
		for (int j = 0; j < 2; j++) {    /* 0 → 7 */
			for (int i = 0; i < 8; i++) {
				unsigned char mask = (unsigned char)(0x01 << i); /* i번째 LED만 ON(Active-Low이므로 반전값을 씀) */
				PORTC = (PORTC & 0xF0) | (unsigned char)((~mask) & 0x0F);/* PC0~3(하위 4비트)만 갱신: ~mask & 0x0F */
				PORTD = (PORTD & 0x0F) | (unsigned char)((~mask) & 0xF0); /* PD4~7(상위 4비트)만 갱신: ~mask & 0xF0 */
				_delay_ms(100);
			}

			/* 6 → 0 (끝점 중복 피함) */
			for (int i = 6; i >= 0; i--) {
				unsigned char mask = (unsigned char)(0x01 << i);
				PORTC = (PORTC & 0xF0) | (unsigned char)((~mask) & 0x0F);
				PORTD = (PORTD & 0x0F) | (unsigned char)((~mask) & 0xF0);
				_delay_ms(100);
			}
		}

		/* ALL OFF (Active-Low + 1) */
		PORTD = (PORTD & 0x0F) | 0xF0;   // PD7~4=1111(끄기)
		PORTC = (PORTC & 0xF0) | 0x0F;   // PC3~0=1111(끄기)
		_delay_ms(100);
	}

	/* PB1(PCINT1) 확인: 마스크 0x02 (PB1 비트) */
	if ((now_b & 0x02) == 0) {           // 눌림=Low이면 실행
		for (int k = 0; k < 3; k++) {    // 기존 INT1 등록(3회) 유지
			/* ALL ON (Active-Low 0) */
			PORTD = (PORTD & 0x0F) | 0x00;   // PD7~4=0000(켜기)
			PORTC = (PORTC & 0xF0) | 0x00;   // PC3~0=0000(켜기)
			_delay_ms(500);

			/* ALL OFF (Active-Low 1) */
			PORTD = (PORTD & 0x0F) | 0xF0;   // PD7~4=1111(끄기)
			PORTC = (PORTC & 0xF0) | 0x0F;   // PC3~0=1111(끄기)
			_delay_ms(500);
		}
}

int main(void)
{
	/* --- LED 핀 방향 설정 --- */
	DDRD = (DDRD & 0x0F) | 0xF0;    // PD7~4=출력(1), PD3~0=입력(0)
	DDRC = (DDRC & 0xF0) | 0x0F;    // PC3~0=출력(1), PC7~4=입력(0)

	/* --- 스위치 핀 (PB0, PB1) 입력 + 내부 풀업 ON --- */
	DDRB &= (unsigned char)~0x03;   // PB1..PB0=입력(00)

	/* --- LED 초기: 모두 OFF (Active-Low + 1) --- */
	PORTD = (PORTD & 0x0F) | 0xF0;  // PD7~4=1111
	PORTC = (PORTC & 0xF0) | 0x0F;  // PC3~0=1111

	_delay_ms(20);                  // 전원/핀 안정 대기

	/* --- 핀체인지 인터럽트 설정 (PORTB용) --- */
	PCIFR |= 0x01;                  // PCIF0 플래그 클리어 (PORTB)
	PCMSK0 = 0x03;                  // PCINT0(0x01)=PB0, PCINT1(0x02)=PB1 허용
	PCICR |= 0x01;                  // PCIE0=1 PORTB 핀체인지 인터럽트 enable

	sei();                          // 전역 인터럽트 허용

	while (1) {
		PORTD = ~0xAF;
		PORTC = ~0xFA;
		_delay_ms(500);
		PORTD = ~0x5F;
		PORTC = ~0x5F;
		_delay_ms(500);
	}
}
