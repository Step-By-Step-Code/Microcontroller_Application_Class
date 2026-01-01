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

volatile unsigned int note_freq[8] = {956, 1014, 1136, 1277, 1433, 1517, 1704, 1912};
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
unsigned int GetADCData(unsigned char aIn){
	volatile unsigned int result;
	ADMUX = aIn;                     // Channel Selection
	ADMUX |= (1 << REFS0);           // AVCC Option (외부 5V 공급)

	// ADC Enable, ADC prescaler : 128
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
	ADCSRA |= (1 << ADSC);           // ADC Start Conversion

	while(!(ADCSRA & (1 << ADIF)));  // ADIF is set when ADC completed
	_delay_us(300);

	result = ADCL + (ADCH << 8);
	ADCSRA = 0x00;
	return result;
}

void play(unsigned int y){
	DDRB |= 0x02; // PB1 출력
	TCCR1A = (1<<COM1A0); // [OC1A Toggle Setting]
	TCCR1B = (1<<WGM13) | (1<<WGM12) | (1<<CS11); // [CTC Mode → TOP : ICR1], [Prescaler 8]

	unsigned int x;
	x = FREQ(note_freq[y % 8]);

	ICR1 = x; // 주파수에 맞는 COUNT 값 저장
	OCR1A = 0; // ICR1보다 낮은 값으로 유지 (토글 보장) [없어도 되는 부분]
}

/* 포트 제어----------------------------------------------------------------------------------*/
void PortSetting(){
	DDRD |= 0xf0;// PD4~7 : LED 출력
	DDRC |= 0x0f;// PC0~3 : LED 출력
	PORTC |= 0x0f;
	PORTD |= 0xf0;// LED는 일단 끈 상태로 시작
}
void UARTSetting(){
	UBRR0H = (unsigned char)(BAUD_RATE >> 8);   // Baud Rate Setting → 9600
	UBRR0L = (unsigned char)(BAUD_RATE);
	UCSR0A = 0x00;
	UCSR0B = (1 << TXEN0) | (1 << RXEN0);      // RX, TX Enable
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);    // Character Size : 8-bit setting
}

/* MAIN 함수----------------------------------------------------------------------------------*/
int main(){
	UARTSetting();
	PortSetting();

	while(1){
		unsigned int adc_value = GetADCData(5);
		Printf("ADC : %d\n", adc_value);
		play((int)(adc_value/128));
		_delay_ms(100);
	}
}