/* 검토완료 */
#include <avr/io.h>
#define FOSC 16000000UL
#define F_CPU 16000000UL
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define BAUD_RATE 103
#define MAX_TXBUF_SIZE 128
#define FREQ(x) (unsigned int)(16000000/(2*8*(1+x)))
#define ON 1
#define OFF 0

volatile unsigned int note_freq[8] = {956, 1014, 1136, 1277, 1433, 1517, 1704, 1912};
volatile unsigned char on_off = 0;
volatile int note_idx = 0;

/* 동작 함수 정의----------------------------------------------------------------------------------*/
// TCCR1A : COM1A1 | COM1A0 | COM1B1 | COM1B0 | - | - | WGM11 | WGM10
	// COM0[A/B]동작 제어 → Compare Match 시에 [Disconnected, Toggle, Clear, Set]
	// WGM[0/1] → Normal/Phase Correct/CTC/Fast PWM
	
// TCCR1B : ICNC1 | ICES1 | - | WGM13 | WGM12 | CS12 | CS11 | CS10
	// [ICNC1[ICP1핀] : 입력 캡처 노이즈 제거 [필요없음]], [ICES[ICP1핀] : 입력 캡처 에지 선택 [0:Falling Edge][0:Rising Edge]]
	// CS : Prescaler, 넣으면 타이머 시작

// TCCR1C : FOC1A | FOC1B | – | – | – | – | – | –
	// 강제로 OC1A/B 출력 제어

// TCNT1H/L, OCR1AH/L, OCR1BH/L, ICR1H/L
// OCR1[A/B][H/L]카운터 값(TCNT1)와 지속적 비교 [Output Compare 인터럽트를 발생, OC1A/B 핀 파형 생성]
// ICR1H/L : ICP1 핀에서 이벤트마다 TCNT1 값으로 갱신 [입력 캡처 값 저장] [카운터 TOP 값 정의]
// TIFR1 : – | – | ICF1 | – | – | OCF1B | OCF1A | TOV1 [FLAG]
// TIMSK1 : – | – | ICIE1 | – | – | OCIE1B | OCIE1A | TOIE1 [Interrupt Enable]

/* 동작 함수 정의----------------------------------------------------------------------------------*/
void play(unsigned int y){
	DDRB |= 0x02; // PB1 출력
	TCCR1A = (1<<COM1A0); // [OC1A Toggle Setting]
	TCCR1B = (1<<WGM13) | (1<<WGM12) | (1<<CS11); // [CTC Mode → TOP : ICR1], [Prescaler 8]

	unsigned int x;
	x = FREQ(note_freq[y % 8]);

	ICR1 = x; // 주파수에 맞는 COUNT 값 저장
	PORTD = 0x0f; // LED 켜기
	OCR1A = 0; // ICR1보다 낮은 값으로 유지 (토글 보장) [없어도 되는 부분]
}

void stop(){
	TCCR1A = 0x00;
	TCCR1B = 0x00;
	TCNT1 = 0;
	ICR1 = 0;
	DDRB &= ~(0x02);
	PORTD = 0xff;
}

/* UART 전송 함수 ----------------------------------------------------------------------------------*/
void tx_char(unsigned char txChar){
	while (!(UCSR0A & (1 << UDRE0)));  // 전송 버퍼가 빌 때까지 대기
	UDR0 = txChar;
}

void Printf(char *fmt, ...) { // ... → 인자 개수 가변적
	va_list arg_ptr;  // 가변 인자 목록을 가리킬 포인터
	uint8_t i, len;
	char sText[128];

	// Text Buffer
	for (i = 0; i < 128; i++) sText[i] = 0; // Buffer 초기화

	va_start(arg_ptr, fmt); // 가변 인자 읽기
	vsprintf(sText, fmt, arg_ptr); // 문자열을 만들어 sText에 저장
	va_end(arg_ptr); // 가변 인자 포인터 메모리 정리

	len = strlen(sText);
	for (i = 0; i < len; i++) tx_char(sText[i]); // 문자열 전송
}

/* 포트 제어----------------------------------------------------------------------------------*/
void PortSetting(){
	DDRD |= 0xf0;     // PD4~7 : LED 출력
	DDRC |= 0x0f;     // PC0~3 : LED 출력
	PORTC |= 0x0f; // LED = OFF 상태로 시작
	PORTD |= 0xf0; // LED = OFF 상태로 시작
}
void UARTSetting(){
	UBRR0H = (unsigned char)(BAUD_RATE >> 8);   // Baud Rate Setting → 9600
	UBRR0L = (unsigned char)(BAUD_RATE);
	UCSR0A = 0x00;
	UCSR0B = (1 << TXEN0) | (1 << RXEN0);      // RX, TX Enable
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);    // Character Size : 8-bit setting
}

// PCIFR : – | – | – | – | – | PCIF2 | PCIF1 | PCIF0
// PCICR : – | – | – | – | – | PCIE2 | PCIE1 | PCIE0
// PCMSK0 : PCINT7 | PCINT6 | PCINT5 | PCINT4 | PCINT3 | PCINT2 | PCINT1 | PCINT0
void INTSetting(){
	/* --- 핀체인지 인터럽트 설정 (PORTB용) --- */
	PCMSK0 |= 0x04;       // PCINT2(0x04)=PB2 허용
	PCIFR |= 0x01;        // PCIF0 플래그 클리어 (PORTB)
	PCICR |= 0x01;        // PCIE0=1 → PORTB 핀체인지 인터럽트 enable

	// 외부 인터럽트 설정
	EICRA = 0x0A;
	EIFR = 0x03;
	EIMSK = 0x03;
}

/* MAIN 함수----------------------------------------------------------------------------------*/
int main(){
	cli();
	PortSetting();
	INTSetting();
	UARTSetting();
	sei();

	while(1){
		if (on_off == ON) play(note_idx);
		else stop();
	}
}


ISR(INT0_vect){
	cli();
	_delay_ms(20);
	if(++note_idx > 7) note_idx = 7; // LIMIT 걸기
	Printf("Note Up to %d\n", note_idx); // 현재 단계 출력
	sei();
}

ISR(INT1_vect){
	cli();
	_delay_ms(20); 
	if(--note_idx < 0) note_idx = 0; // LIMIT 걸기
	Printf("Note Down to %d\n", note_idx); // 다운된 단계 출력
	sei();
}

ISR(PCINT0_vect){
	cli();
	PCIFR |= 0x01;
	_delay_ms(20);

	unsigned char now_b = PINB;
	if((now_b & 0x04) == 0){
		on_off = !on_off;
		Printf("Buzzer : %d\n", on_off);
	}
	sei();
}
