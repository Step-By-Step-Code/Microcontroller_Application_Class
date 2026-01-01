/*검토 완료*/
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
// ADMUX : [REFS1-0 : 참조 전압 선택 비트] | [ADLAR : ADC 변환 결과를 데이터 레지스터에 어떻게 정렬할지] | – | [MUX3-0]
	// REFS : ①AREF 기준, 내부 기준전압(Vref) 꺼짐
	// ②AVCC 사용 (AREF 핀에 외부 캐패시터 필요)
	// ④내부 1.1V 기준전압 사용 (AREF 핀에 외부 캐패시터 필요)
// ADCSRA : [ADEN : 활성화] | [ADSC : 변환 시작] | [ADATE : 자동 트리거 활성화] | [ADIF : 인터럽트 플래그] | [ADIE : 인터럽트 활성화] | [ADPS2-0 : ADC Prescaler Select]
	// ADIF : ADC 변환이 완료, 데이터 레지스터가 업데이트되면 SET [1을 써서 클리어][인터럽트 벡터 실행하면 클리어]
	// ADATE : 선택된 트리거 신호의 상승 에지에서 ADC 변환이 자동으로 시작 → ADCSRB의 ADTS에서 트리거 소스 설정
unsigned int GetADCData(unsigned char aIn){
	volatile unsigned int result;
	ADMUX = aIn;                     // Channel Selection
	ADMUX |= (1 << REFS0);           // AVCC Option (외부 5V 공급)

	// ADC Enable, ADC prescaler : 128
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
	ADCSRA |= (1 << ADSC);           // ADC 변환 시작

	while(!(ADCSRA & (1 << ADIF)));  // ADIF is set when ADC completed
	_delay_us(300);

	result = ADCL + (ADCH << 8);
	ADCSRA = 0x00;
	return result;
}

void disp_led(unsigned int _value){
	PORTC |= 0x0f;
	PORTD |= 0xf0;
	for(int i = 0; i<=(_value/128); i++){
		if(i<4) PORTC &= ~(0x01<<i);
		else PORTD &= ~(0x01<<i);
	}
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
	PortSetting();
	UARTSetting();

	while(1){
		unsigned int adc_value = GetADCData(5);
		
		Printf("ADC : %d\n", GetADCData(5));
		disp_led(adc_value);
		_delay_ms(100);
	}
}