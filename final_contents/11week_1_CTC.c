/*검토완료*/
#include <avr/io.h>
#define FOSC 16000000UL
#define F_CPU 16000000UL
#include <util/delay.h>
#include <avr/interrupt.h>

volatile unsigned char Tov_Val = 0;

/* 포트 제어----------------------------------------------------------------------------------*/
void PORTSetting(){
	DDRD |= 0xF0;  // PD4~7 : LED 출력
	DDRC |= 0x0F;  // PC0~3 : LED 출력
	PORTC |= 0x0F; // LED는 끈 상태로 시작
	PORTD |= 0xF0; // LED는 끈 상태로 시작
}

// TCCR0A : [COM0A[0/1] → OC0A 동작 제어]|[COM0B[0/1] → OC0B 동작 제어]|   -   |   -   | [WGM01-0 → 동작모드 설정]
	// COM0[A/B]동작 제어 → Compare Match 시에 [Disconnected, Toggle, Clear, Set]
	// WGM[0/1] → Normal/Phase Correct/CTC/Fast PWM
// TCCR0B : [FOC0A/B → SET하면 OC0A/B 출력이 즉시 변화] | - | - | [WGM02 → 동작모드 설정] | [CS02-0: Prescaler Setting → Setting하는 순간 Timer 동작]
// TCNT0/OCR0A/OCR0B
	// TCNT0
	// OCR0A : 카운터 값(TCNT0)와 지속적 비교 [Output Compare 인터럽트를 발생, OCOA 핀 파형 생성]
	// OCR0B : 카운터 값(TCNT0)와 지속적 비교 [Output Compare 인터럽트를 발생, OCOB 핀 파형 생성]
// TIFR0 : - | - | - | - | - | OCF0B | OCF0A | TOV0 [FLAG]
// TIMSK0 : - | - | - | - | - | OCIE0B | OCIE0A | TOIE0 [Interrupt Enable]

// TCNT0 = 100 → [16MHz의 주기 0.0000000625초], [16MHz/1024 = 15.625 kHz의 주기 0.000064초] → [0.000064초 * 156 = 0.009984초 ≒ 0.01초] → 100번에 1초
void TIMERSetting(){
	// Timer0: 8bit + CTC + prescaler=1024 → 107 Page 참고
	TCCR0A = (1<<WGM01); // [TOP = OCRA] [Update of OCRx at : Immediate] [TOV0 SET : MAX] → TOP마다 OCF0A SET 
	TCCR0B = (1<<CS02) | (1<<CS00);   // 1024 분주
	OCR0A  = 155;                     // 156 * 0.000064s ≈10ms
	TIFR0  = (1<<OCF0A);              // 기존 플래그 클리어
	TIMSK0 = (1<<OCIE0A);             // Output Compare 인터럽트 허용
}
/* 메인 동작----------------------------------------------------------------------------------*/
int main(){
	cli();
	PORTSetting();
	TIMERSetting();
	sei();
	
	while(1){}
}

/* 인터럽트 함수 ----------------------------------------------------------------------------------*/
ISR(TIMER0_COMPA_vect){
	Tov_Val++;
	if(Tov_Val >= 100){
		PORTC = 0xF0;
		PORTD = 0x0F;
		_delay_ms(500);
		PORTC = 0x0F;
		PORTD = 0xF0;

		Tov_Val = 0; // 카운트 초기화
	}
	sei();
}