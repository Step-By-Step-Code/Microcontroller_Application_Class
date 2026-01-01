#define FOSC 16000000UL
#define F_CPU 16000000UL
#define BAUD_RATE 103

#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdarg.h> // → Printf를 위한 헤더
#include <string.h>

#define MAX_TXBUF_SIZE 128
char tx_buf[MAX_TXBUF_SIZE];
volatile int buf_len = 0;
volatile unsigned char sint_flag = 0;



#define LED_ONOFF  0x01
#define LED_ALT    0x02
#define LED_SHIFT  0x04
#define STX        0x02
#define ETX        0x03

// decode -> 0 : Normal, 1 : Data Reception, 2 : Termination state
volatile int decode = 0;
volatile uint8_t _command = 0;

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

/* LED 함수 제어----------------------------------------------------------------------------------*/
void led_all_onoff(int _n){ //  LED 전체 ON/OFF, [_n : 반복횟수], [_ms : 반복주기]
	for (volatile int i=0; i<_n; i++)
	{
		PORTD = 0x00;
		PORTC = 0x00;
		for(volatile int j = 0; j < 500; j++) _delay_ms(1);
		PORTD = (0xF0) ;
		PORTC = (0x0F) ;
		for(volatile int j = 0; j < 500; j++) _delay_ms(1);
	}
}


void led_alternating(int LED_MASK, int _n){ // LED 동작 제어
	for(int i=0; i < _n; i++){		// 상위 4비트 → PORTD[3:0], 하위 4비트 → PORTC[3:0]
		PORTD = ((LED_MASK & 0xF0)); // LED 상위 4 Bit 추출하여 4번 오른쪽으로 쉬프트
		PORTC = (LED_MASK & 0x0F);		// LED 하위 4 Bit 추출
		for(volatile int j = 0; j < 500; j++) _delay_ms(1);
		
		PORTD = 0xF0 - ((LED_MASK & 0xF0));  // 추출한 값에서 켜진 걸 꺼지도록 꺼진 걸 켜지도록 설정
		PORTC = 0x0F - (LED_MASK & 0x0F);		// 추출한 값에서 켜진 걸 꺼지도록 꺼진 걸 켜지도록 설정
		for(volatile int j = 0; j < 500; j++) _delay_ms(1);
	}
}

void led_shift(int _n){ //  [_n : 반복횟수], [_ms : 반복주기]
	for (int i=0;i<_n;i++)
	{
		for (int j=0;j<7;j++){ // 0 → 6번째 핀까지
			PORTC = ((~(0x01 << j) & 0x0F));
			PORTD = (~(0x01 << j) & 0xF0);
			for(volatile int j = 0; j < 500; j++) _delay_ms(1);
		}
		for (int k=0;k<8;k++) // 7번째 핀에서
		{
			PORTC = (~(0x80 >> k) & 0x0F);
			PORTD = (~(0x80 >> k) & 0xF0);
			for(volatile int j = 0; j < 500; j++) _delay_ms(1);
		}
	}
	PORTD = (0xF0); // LED 모두 끄기
	PORTC = (0x0F); // LED 모두 끄기
}

/* 포트 제어----------------------------------------------------------------------------------*/
void PortSetting(){
	DDRD = 0xF2; // [RX[PD0] : INPUT], [TX[PD1] = OUTPUT], [PD2-3[INT0/1]] : INPUT], [PD4-7 : OUTPUT] → 0b11110010;
	PORTD = 0xF0; // 11111111 → PD4~7[LED연결] 끄기, 입력핀은 모두 풀업저항
	DDRC  |= 0x0F; // 00011110 → PC0~3 모두 출력 설정
	PORTC |= 0x0F; // PC와 연결된 LED 끄기
}
void UARTSetting(){
	UBRR0H = (unsigned char)(BAUD_RATE >> 8);  // Baud Rate Setting
	UBRR0L = (unsigned char)(BAUD_RATE);
	UCSR0A = 0x00;
	UCSR0B = (1 << TXEN0) | (1 << RXEN0) | (1 << RXCIE0);     // RX, TX Enable, RX Complete Interrupt Enable
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);    // Character Size : 8-bit setting
}
void INTSetting(){
	// INT0~1 = PD2~3
	EICRA = 0x0A; // [– – – – ISC11 ISC10 ISC01 ISC00] → INT0/1 Falling Edge Setting
	EIFR = 0x03; // INT0/1 Flag Clear
	EIMSK = 0x03; // INT0/1 Enable Setting
}

/* 동작 제어 함수----------------------------------------------------------------------------------*/
void hex_command(uint8_t cd){
	if(cd == LED_ONOFF) led_all_onoff(3);
	else if(cd == LED_ALT) led_alternating(0xAA, 3);
	else if(cd == LED_SHIFT) led_shift(3);
	else Printf("Invalid Command\n");
}

/* MAIN 제어----------------------------------------------------------------------------------*/
int main(){
	cli();
	PortSetting();
	UARTSetting();
	INTSetting();
	sei();

	while(1){
		if(sint_flag){
			hex_command(_command); // 지정된 동작 진행
			sint_flag = 0; // 지정된 동작 후 초기화
			_command = 0;
			PORTC |= 0x0F; 
			PORTD |= 0xF0;
			
		}
	}
}


ISR(USART_RX_vect){
	cli();
	
	unsigned char _usart_rcv = 0;
	_usart_rcv = UDR0;
	
	/* 동작 과정 : decode = 0 → STX → decode = 1 → COMMAND → decode = 2 → ETX → decode = 0 */
	if(decode == 0 && _usart_rcv == STX) decode = 1;
	
	else if(decode == 1){
		_command = _usart_rcv;
		decode = 2;
	}
	
	else if(decode == 2){
		if(_usart_rcv == ETX) sint_flag = 1;
		else Printf("RX error\n");
		decode = 0;
	}
	sei();
}

