/* 검토완료 */
/*02 COMMAND 횟수 03*/
#define FOSC 16000000UL
#define F_CPU 16000000UL

#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdarg.h>
#include <string.h>

#define BAUD_RATE 103
#define LED_ONOFF  0x01
#define LED_ALT    0x02
#define LED_SHIFT  0x03
#define STX        0x02
#define ETX        0x03

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

/* LED 제어-----------------------------------------------------------------------------------*/
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
		PORTD = (LED_MASK & 0xF0); // LED 상위 4 Bit 추출하여 4번 오른쪽으로 쉬프트
		PORTC = (LED_MASK & 0x0F);		// LED 하위 4 Bit 추출
		for(volatile int j = 0; j < 500; j++) _delay_ms(1);
		
		PORTD = 0xF0 - (LED_MASK & 0xF0);  // 추출한 값에서 켜진 걸 꺼지도록 꺼진 걸 켜지도록 설정
		PORTC = 0x0F - (LED_MASK & 0x0F);		// 추출한 값에서 켜진 걸 꺼지도록 꺼진 걸 켜지도록 설정
		for(volatile int j = 0; j < 500; j++) _delay_ms(1);
	}
}

void led_shift(int _n){ //  [_n : 반복횟수], [_ms : 반복주기]
	for (int i=0;i<_n;i++)
	{
		for (int j=0;j<7;j++){ // 0 → 6번째 핀까지
			PORTD = (~(0x01 << j) & 0xF0);
			PORTC = (~(0x01 << j) & 0x0F);
			for(volatile int j = 0; j < 500; j++) _delay_ms(1);
		}
		for (int k=0;k<8;k++) // 7번째 핀에서
		{
			PORTD = (~(0x80 >> k) & 0xF0);
			PORTC = (~(0x80 >> k) & 0x0F);
			for(volatile int j = 0; j < 500; j++) _delay_ms(1);
		}
	}
	PORTD = 0xF0; // LED 모두 끄기
	PORTC = 0x0F; // LED 모두 끄기
}


/* 포트 제어----------------------------------------------------------------------------------*/
// [PD0:RXD → INPUT], [PD1:TXD → OUTPUT], [PD2~3:SWITCH INPUT], [PD4~7:OUTPUT] → [1, 4~7 : OUTPUT], [0, 2~3 : INPUT]
// [PC0~3:OUTPUT]
void PortSetting(){
	DDRD |= 0xF2;   // [1, 4~7 : OUTPUT]
	DDRD &= ~(0x0D); // [0, 2~3 : INPUT]
	
	DDRC |= 0x0F;   // PC0~3 : LED 출력
}
void UARTSetting(){
	UBRR0H = (unsigned char)(BAUD_RATE >> 8);   // Baud Rate Setting → 9600
	UBRR0L = (unsigned char)(BAUD_RATE);
	
	// UCSR0A : Flag 관리 → 0, 1은 모드 설정이므로 제외
	// Write 핀만 적용 → 모든 Flag 초기화 [6 : TXC0], [1,0: 사용하지 않는 기능]  
	UCSR0A = 0x00;
	
	// UCSR0B : 송수신기, 인터럽트, 9번째 비트 읽기, 문자크기 결정
	UCSR0B = (1 << TXEN0) | (1 << RXEN0); // 송수신기 활성화
	UCSR0B |= (1 << RXCIE0); // 수신 완료 인터럽트 활성화
	
	// UCSR0C : 극성 설정, 문자크기 결정, 정지비트 설정, 패리티/모드 설정
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); // 문자 크기 : 8 bit 설정
}
void INTSetting(){
	// INT0~1 = PD2~3
	EIFR  = 0x03; // 인터럽트 플래그 초기화
	EIMSK = 0x03; // 인터럽트 활성화
}

/* 수신 후 동작 함수----------------------------------------------------------------------------------*/

// decode -> [0 : Normal], [1 : Data Reception], [2 : Data Reception], [3 : Termination state]
volatile int decode = 0;
volatile uint8_t _command_1 = 0;
volatile uint8_t _command_2 = 0;
volatile unsigned char sint_flag = 0;


void hex_command(uint8_t cd_1, uint8_t cd_2){
	if(cd_1 == LED_ONOFF) led_all_onoff(cd_2);
	else if(cd_1 == LED_ALT) led_alternating(cd_2, 1);
	else if(cd_1 == LED_SHIFT) led_shift(cd_2);
	else Printf("Invalid Command\n");
}

/* 메인문 ----------------------------------------------------------------------------------*/
int main(){
	cli();
	PortSetting();
	UARTSetting();
	INTSetting();
	// LED OFF 시작
	PORTC = 0x0F;
	PORTD = 0xF0;
	sei();

	while(1){
		if(sint_flag){
			hex_command(_command_1, _command_2);
			sint_flag = 0;
			_command_1 = 0;
			_command_2 = 0;
			// 커맨드 종료 후 LED 모두 종료
			PORTC |= 0x0F;
			PORTD |= 0xF0;
		}
	}
}
/* 인터럽트 동작 설정 ----------------------------------------------------------------------------------*/

// 1. 수신 완료 인터럽트
ISR(USART_RX_vect){
	cli();
	unsigned char _usart_rcv = 0;
	_usart_rcv = UDR0;
	
	if(decode == 0 && _usart_rcv == STX) decode = 1;
	
	else if(decode == 1){ // change state into 2
		_command_1 = _usart_rcv;
		decode = 2;
	}
	else if(decode == 2){ // change state into 3
		_command_2 = _usart_rcv;
		decode = 3;
	}
	else if(decode == 3){
		if(_usart_rcv == ETX) sint_flag = 1;
		else Printf("RX error\n");
		decode = 0; // return to normal state
	}
	sei();
}