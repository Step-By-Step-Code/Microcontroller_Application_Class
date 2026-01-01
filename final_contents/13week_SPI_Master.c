// MASTER : UART로 문자를 받음 → 받은 값을 숫자로 변환 → SLAVE에게 SPI를 통해 메세지 전송 → SLAVE한테 SPI를 통해 메세지를 받고 UART로 컴퓨터한테 전송

#include <avr/io.h>
#define FOSC 16000000UL
#define F_CPU 16000000UL
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define BAUD_RATE 103

volatile uint8_t uart_recv = 0;
volatile uint8_t uart_flag = 0;

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

/* 포트 제어 ----------------------------------------------------------------------------------*/
void UARTSetting(){
	UBRR0H = (unsigned char)(BAUD_RATE >> 8);   // Baud Rate Setting → 9600
	UBRR0L = (unsigned char)(BAUD_RATE);
	UCSR0A = 0x00;
	UCSR0B = (1 << TXEN0) | (1 << RXEN0) | (1 << RXCIE0);     // RX, TX Enable
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);    // Character Size : 8-bit setting
}


// SPI에서 슬레이브는 마스터와 클럭을 맞출 필요 없음
	// 마스터가 클럭을 제공하는 구조라서 슬레이브는 따로 클럭을 생성하지 않는다.
void SPI_MasterInit(void){
	DDRB |= (1<<PORTB3) | (1<<PORTB5) | (1<<PORTB2); // MOSI, SCK, SS 출력
	PORTB |= (1<<PORTB2);   // SS HIGH -> Slave 비활성화
	SPCR = (1<<SPE) | (1<<MSTR) | (1<<SPR0); // Enable, Master, Fosc/16
}

/* 동작 함수 ----------------------------------------------------------------------------------*/
// SPCR : SPIE | SPE | DORD | MSTR | CPOL | CPHA | SPR1 | SPR0
// 7-SPI Interrupt Enable, 6-SPI Enable, 5-Data Order
// 4-Master[1]/Slave[0] Select, 3-Clock Polarity, 2-Clock Phase
// SPR1, SPR0: SPI Clock Rate Select 1 and 0
// SPSR : ☆SPIF | WCOL | – | – | – | – | – | SPI2X
// SPDR : MSB | - | LSB

uint8_t SPI_MasterTransmit(uint8_t data){
	PORTB &= ~(1<<PORTB2);  // SS LOW
	SPDR = data;
	while(!(SPSR & (1<<SPIF)));
	uint8_t recv = SPDR;
	PORTB |= (1<<PORTB2);   // SS HIGH
	return recv;
}

/* MAIN 함수----------------------------------------------------------------------------------*/
int main(void){
	cli();
	UARTSetting();
	SPI_MasterInit(); // SPI Master 설정
	sei();   // 전역 인터럽트 Enable

	uint8_t value = 0;
	uint8_t reply = 0;

	while(1){
		// UART로부터 새로운 문자가 들어오면 처리
		if(uart_flag){
			uart_flag = 0;

			if(uart_recv >= '0' && uart_recv <= '9'){ // 입력된 문자가 숫자인지 확인
				// 숫자로 변환 후 UART 전송
				value = uart_recv - '0';
				Printf("\n[Master] Send value: %d\n", value);

				reply = SPI_MasterTransmit(value); // 1) 명령 전송
				_delay_ms(100); // 2) Slave 응답 받기 위해 한번 더 전송 [만약 master의 전송이 slave의 처리속도보다 빠른 것을 방지]
				reply = SPI_MasterTransmit(value); // 명령 전송하면서 처리된 응답 수신받기

				// UART 응답 출력
				if(reply == 'S') Printf("[Master] Slave Response: SUCCESS\n");
				else if(reply == 'F') Printf("[Master] Slave Response: FAIL\n");
				else Printf("[Master] Unknown Response: %02X\n", reply);
				
				} 
				else Printf("\nCheck it is 0~9 Numbers\n");
		}
	}
}

ISR(USART_RX_vect){
	uart_recv = UDR0;   // 수신된 문자 저장
	uart_flag = 1;      // 새로운 문자 수신 플래그
}
