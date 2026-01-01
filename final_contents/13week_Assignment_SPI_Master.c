/* ============================================================
 * UART 기반 Master–Slave LED Control 요구사항 정리
 * ============================================================
 *
 * [Master → Slave 명령]
 *   - '0' : 모든 LED OFF, 동작 없음(정지)
 *   - '1' : LED_ALL_ON_OFF 패턴 무한 반복
 *   - '2' : LED_ALT 패턴 무한 반복
 *   - '3' : LED_SHIFT 패턴 무한 반복
 *
 * [Slave → Master 응답]
 *   - Master는 명령 전송 후 즉시 Slave의 "이전 상태"를 UART로 수신
 *   - 출력 형식:
 *        Before : <이전 상태 또는 X/None>
 *        After  : <새로운 상태>
 *
 * [패턴 전환 규칙]
 *   - LED 동작이 변경될 때 즉시 바뀌면 안 됨
 *   - 현재 패턴의 한 싸이클을 완전히 끝낸 뒤 다음 패턴 적용
 *     ex) SHIFT → ALT :
 *         (SHIFT 마지막 싸이클 수행) → delay → ALT 시작
 *
 * [기타 조건]
 *   - Delay, blinking 주기 등은 자유롭게 설정
 *   - Master/Slave는 UART 통신 기반
 *
 * ============================================================ */

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

void SPI_MasterInit(void){
	DDRB |= (1<<PORTB3) | (1<<PORTB5) | (1<<PORTB2); // MOSI, SCK, SS 출력
	PORTB |= (1<<PORTB2);   // SS HIGH -> Slave 비활성화
	SPCR = (1<<SPE) | (1<<MSTR) | (1<<SPR0); // Enable, Master, Fosc/16
}

/* SPI 전송 함수 ----------------------------------------------------------------------------------*/
uint8_t SPI_MasterTransmit(uint8_t data){
	PORTB &= ~(1<<PORTB2);  // SS LOW
	SPDR = data;
	while(!(SPSR & (1<<SPIF)));
	uint8_t recv = SPDR;
	PORTB |= (1<<PORTB2);   // SS HIGH
	return recv;
}

/* MAIN 함수----------------------------------------------------------------------------------*/
int main(void)
{
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
			if(uart_recv >= '0' && uart_recv <= '3'){
				if(uart_recv == '0'){ 
					reply = SPI_MasterTransmit(0);// 데이터 전송 목적
					_delay_ms(100);
					reply = SPI_MasterTransmit(0); // 데이터 수신 목적
				}
				if(uart_recv == '1'){
					reply = SPI_MasterTransmit(1);// 데이터 전송 목적
					_delay_ms(100);
					reply = SPI_MasterTransmit(1); // 데이터 수신 목적
				}
				if(uart_recv == '2'){
					reply = SPI_MasterTransmit(2);// 데이터 전송 목적
					_delay_ms(100);
					reply = SPI_MasterTransmit(2); // 데이터 수신 목적
				}
				if(uart_recv == '3'){
					reply = SPI_MasterTransmit(3);// 데이터 전송 목적
					_delay_ms(100);
					reply = SPI_MasterTransmit(3); // 데이터 수신 목적
				}
				Printf("Before : %d\n", reply);
				Printf("After : %c\n", uart_recv);
			}
			else Printf("\nCheck it is 0~9 Numbers\n");
		}
	}
	
	
}

ISR(USART_RX_vect){
	uart_recv = UDR0;   // 수신된 문자 저장
	uart_flag = 1;      // 새로운 문자 수신 플래그
}
