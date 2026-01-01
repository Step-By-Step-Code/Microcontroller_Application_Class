/* 검토완료 */
#include <avr/io.h>
#define FOSC 16000000UL
#define F_CPU 16000000UL
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define BAUD_RATE 103
#define MAX_TXBUF_SIZE 128
#define ON 1
#define OFF 0

volatile unsigned char on_off = 0;

/* UART 관련 제어-----------------------------------------------------------------------------------*/
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



// [16MHz의 주기 0.0000000625초], [16MHz / 8 = 2 MHz의 주기 0.0000005초] → [ 주기  = 0.0000005초 * ICR1 * 2 = 0.002273 → 440Hz "라"]
// f_OCnA = f_clk_I/O / ( 2 * N * (1 + OCRnA) )
void play(unsigned int _Hz){
	DDRB |= 0x02; // PB1 출력
	TCCR1A = (1 << COM1A0); // [OC1A Toggle Setting]
	TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS11); // [CTC Mode → TOP : ICR1], [Prescaler 8]
	ICR1 = _Hz;
	// OCR1A = 0; 없어도 잘 작동
}

void stop(){
	DDRB &= ~(0x02); // PB1 입력
	TCCR1A = 0x00;
	TCCR1B = 0x00;
	TCNT1 = 0;
	ICR1 = 0;
}

/* 포트 제어----------------------------------------------------------------------------------*/
void UARTSetting(){
	UBRR0H = (unsigned char)(BAUD_RATE >> 8);  // Baud Rate Setting
	UBRR0L = (unsigned char)(BAUD_RATE);
	UCSR0A = 0x00;
	UCSR0B = (1 << TXEN0) | (1 << RXEN0);     // RX, TX Enable
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);    // Character Size : 8-bit setting
}
void INTSetting(){
	// INT0~1 = PD2~3
	EICRA = 0x0A; // [– – – – ISC11 ISC10 ISC01 ISC00] → INT0/1 Falling Edge Setting
	EIFR = 0x03; // INT0/1 Flag Clear
	EIMSK = 0x03; // INT0/1 Enable Setting
}

/* MAIN 함수----------------------------------------------------------------------------------*/
int main(){
	cli();
	INTSetting();
	UARTSetting();
	sei();

	while(1){
		if (on_off == ON) play(2273);
		else stop();
	}
}
/*  Interrupt 제어----------------------------------------------------------------------------------*/
ISR(INT0_vect){
	cli();
	_delay_ms(20);
	
	on_off = ON;
	Printf("Play Button Pressed\n");
	sei();
}

ISR(INT1_vect){
	cli();
	_delay_ms(20);
	
	on_off = OFF;
	Printf("Stop Button Pressed\n");
	sei();
}