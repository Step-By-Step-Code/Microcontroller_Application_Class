#include <avr/io.h>
#define FOSC 16000000UL
#define F_CPU 16000000UL
#include <util/delay.h>
#include <avr/interrupt.h>

int main(void)
{
	DDRD = 0xF0;
	DDRC = 0x0F;
	PORTD = 0xF0;
	PORTC = 0x0F;

	// ===== 외부 인터럽트 설정 =====
	// INT0: PD2 하강엣지, INT1: PD3 하강엣지
	EICRA = 0x0A;   // ISC01=1,ISC00=0 => FALLING / ISC11=1,ISC10=0 => FALLING
	EIFR = 0x03;    // 과거 플래그 클리어
	EIMSK = 0x03;   // INT0/INT1 허용

	sei();          // 전역 인터럽트 허용

	_delay_ms(20);

	while(1){
		PORTD = (uint8_t)~0xAF;
		PORTC = (uint8_t)~0xFA;
		_delay_ms(500);
		
		PORTD = (uint8_t)~0x5F;
		PORTC = (uint8_t)~0xF5;
		_delay_ms(500);
	}

}
// --- PD2: INT0 (SW1) → LED 왕복 SHIFT 2회 ----------------
ISR(INT0_vect)
{
	// 디바운스: 디바운스는 버튼 신호의 불안정한 튐을 제거해 입력을 한 번만 인식하도록 만드는 과정
	// 타이밍 구조 : 누름 + 채터링 10ms + LOW + HIGH
	// 채터링이 10ms 뒤에 HIGH가 되어 있으면, 조건에 걸려서 가짜 입력으로 판단되어 무시됨.

	_delay_ms(10);
	if (PIND & 0x04) return;   // 이미 떼었으면 무시

	cli();
	EIFR |= 0x01; // 대기 중 쌓였을 수도 있는 INT0 대기 플래그를 싹 지움 → 중복 ISR 재진입 차단.
	sei();
	
	for (int j = 0; j < 2; j++) {
		// → 방향
		for (int i = 0; i < 8; i++) {
			PORTC = (~(0x01 << i)) & 0x0F;  // PC0~3
			PORTD = (~(0x01 << i)) & 0xF0;  // PD4~7
			_delay_ms(100);
		}
		// ← 방향 (끝점 중복 제거: 7은 이미 직전 단계에서 켜졌음)
		for (int i = 6; i >= 0; i--) {
			PORTC = (~(0x01 << i)) & 0x0F;
			PORTD = (~(0x01 << i)) & 0xF0;
			_delay_ms(100);
		}
		// 전체 OFF (Active-Low → 1)
		PORTD = (PORTD & 0x0F) | 0xF0;
		PORTC = (PORTC & 0xF0) | 0x0F;
		_delay_ms(100);
	}
}

// --- PD3: INT1 (SW2) → LED ALL ON/OFF 2회 ----------------
ISR(INT1_vect)
{
	// 디바운스 제거
	// 만약 한 번 눌렀지만 2개를 받아들임
	_delay_ms(15);
	// 처음 입력은 15ms 지난 후에도 0의 값이라 Continue, 
	if (PIND & 0x08) return; // 버튼을 누른 상태면 Continue, 버튼을 뗀 상태라면 Return

	cli();
	EIFR |= 0x02; // 인터럽트 인식 가능하게 함
	sei(); // 인터럽트 인식 가능하게 함

	for (int k = 0; k < 3; k++) {
		// ALL ON (Active-Low → 0)
		PORTD = (PORTD & 0x0F) | 0x00;
		PORTC = (PORTC & 0xF0) | 0x00;
		_delay_ms(500);

		// ALL OFF (Active-Low → 1)
		PORTD = (PORTD & 0x0F) | 0xF0;
		PORTC = (PORTC & 0xF0) | 0x0F;
		_delay_ms(500);
	}
}
