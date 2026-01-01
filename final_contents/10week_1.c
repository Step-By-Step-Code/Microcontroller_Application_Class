/* LED_SHIFT → HEX로 변환 → "\n" = 0A 추가  */
/* 검토완료 */
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

/* UART 전송 함수 ----------------------------------------------------------------------------------*/
void tx_char(unsigned char txChar){
	while (!(UCSR0A & (1 << UDRE0)));  // 전송 버퍼가 빌 때까지 대기
	UDR0 = txChar;
}
void tx_str(char *str, uint8_t len){
	for(uint8_t i = 0; i < len; i++) tx_char(str[i]);
}


/* 동작 함수 ----------------------------------------------------------------------------------*/
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
		PORTC = (LED_MASK & 0x0F); // LED 하위 4 Bit 추출
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
// UCSR0A : [☆RXCn | ☆TXCn | ☆UDREn | FEn | DORn | UPEn | U2Xn | MPCMn]
// 수신 완료 | 송신 완료 | 송신버퍼 비어 있음 | 프레임 에러 | 데이터 오버런 | 패리티 에러 | 2배 속도 모드 | 멀티프로세서 모드
// UCSR0B : [☆RXCIEn | ☆TXCIEn | ☆UDRIEn | ☆RXENn | ☆TXENn | ☆UCSZn2 | RXB8n | TXB8n]
// [ 0-2 수신/송신/데이터레지스터비움 완료 인터럽트 허용] | [3-4 수신기/송신기 활성화] | 5 문자크기 비트 |  [6-7 수신/송신 데이터 9비트]
// UCSR0C : [ UMSELn1 | UMSELn0 | UPMn1 | UPMn0 | USBSn | ☆UCSZn1 | ☆UCSZn0 | UCPOLn ]
// [0-1 동기/비동기 모드 선택] | [2-3 패리티 모드 비트] | [4 스톱비트 설정] | [5-6 문자크기 비트] | [7 클록 폴라리티]
void PortSetting(){ 
		DDRD = 0xF2; // [RX[PD0] : INPUT], [TX[PD1] = OUTPUT], [PD2-3[INT0/1]] : INPUT], [PD4-7 : OUTPUT] → 0b11110010;
		PORTD = 0xFF; // 11111111 → PD4~7[LED연결] 끄기, 입력핀은 모두 풀업저항
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
void command(char *buff, uint8_t len){
	char tmp[MAX_TXBUF_SIZE];
	memset(tmp, '\0', MAX_TXBUF_SIZE); // 메모리 특정 영역을 내가 지정한 값으로 모두 채우는 함수
	memcpy((char*)tmp, (char*)buff, len); // buff 데이터를 len 바이트만큼 tmp로 복사하는 함수
	
	/* strstr(a, b) : 문자열 a 안에 문자열 b가 포함되어 있는지 찾는 함수 → 받은 내용을 전송 → 동작 진행*/
	if(strstr((char*)tmp, "LED_ALL_ONOFF") != NULL){
		tx_str(tx_buf, buf_len); 
		led_all_onoff(3);
	}
	else if(strstr((char*)tmp, "LED_ALT") != NULL){
		tx_str(tx_buf, buf_len);
		led_alternating(0xAA, 3);
	}
	else if(strstr((char*)tmp, "LED_SHIFT") != NULL){
		tx_str(tx_buf, buf_len);
		led_shift(3);
	}
}

/* MAIN 제어----------------------------------------------------------------------------------*/
int main(){
	cli();
	PortSetting();
	INTSetting();
	UARTSetting();
	sei();

	while(1){
		if(sint_flag){
			tx_buf[buf_len] = '\0'; // 문자열 끝에 문자열 종료 표시 저장
			command(tx_buf, buf_len); // 
			buf_len = 0;
			memset(tx_buf, '\0', MAX_TXBUF_SIZE);
			
			// 동작 완료 후
			sint_flag = 0;
			PORTC |= 0x0F; // 동작 끝나면 LED 끄기
			PORTD |= 0xF0; // 동작 끝나면 LED 끄기
			
			
		}
	}
}

/*  RX Complete Interrupt 제어----------------------------------------------------------------------------------*/
ISR(USART_RX_vect){
	cli();
	unsigned char _usart_rcv = UDR0;
	if(_usart_rcv == '\n' || buf_len >= (MAX_TXBUF_SIZE - 1)){ // ① '\n' 받았을 때 ② 용량이 초과됐을 때
		tx_buf[buf_len++] = _usart_rcv; // 마지막 문자 저장
		sint_flag = 1; // 동작 명령 변수 SET 
	}
	else tx_buf[buf_len++] = _usart_rcv; // 도착한 데이터 저장
	sei();
}
