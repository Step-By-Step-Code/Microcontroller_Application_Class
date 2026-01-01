/* 
MASTER MCU
1. SPI Master
2. I2C Slave

SLAVE MCU
1. SPI Slave
2. I2C Master

1. 스위치 입력 : SLAVE MCU에서 MASTER MCU로 I2C로 신호 보냄  
2. MASTER MCU가 I2C로 1을 받음 [UART : I2C Data Receive!]
3. MASTER MCU가 SLAVE MCU에 SPI로 1을 전송 [UART : SPI Transmit!]
4. SLAVE MCU가 받고 2를 전송 [UART : SPI Transmit SUCCESS!]
*/ 

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

// ------------------------------------------------------------
// I2C (TWI) Status Codes – Slave Receiver Mode
// ------------------------------------------------------------
#define SLAVE_RECEIVE_SLA_W_SEND_ACK            0x60    // [SLAVE] SLA+W 수신 → [SLAVE] ACK 전송
#define SLAVE_RECEIVE_SLA_W_ARB_LOST_SEND_ACK   0x68    // Arbitration lost 이후 SLA+W 수신 → ACK 전송  [필요없음]

#define SLAVE_RECEIVE_GCALL_SEND_ACK            0x70    // General Call 수신 → [SLAVE] ACK 전송
#define SLAVE_RECEIVE_GCALL_ARB_LOST_SEND_ACK   0x78    // Arbitration lost 이후 GCALL 수신 → ACK 전송 [필요없음]

// 데이터 수신
#define SLAVE_RECEIVE_DATA_SEND_ACK             0x80    // [MASTER → SLAVE] 데이터 수신 → [SLAVE] ACK 전송
#define SLAVE_RECEIVE_DATA_SEND_NACK            0x88    // 데이터 수신 → [SLAVE] NACK 전송(슬레이브 종료)

#define SLAVE_RECEIVE_GCALLDATA_SEND_ACK       0x90    // General Call 데이터 수신 → ACK 전송
#define SLAVE_RECEIVE_GCALLDATA_SEND_NACK      0x98    // GCALL 데이터 수신 → NACK 전송(종료)

// STOP / Repeated START
#define SLAVE_RECEIVE_STOP_OR_RESTART           0xA0    // STOP or Repeated START 수신 → 종료 후 대기

// ------------------------------------------------------------
// I2C (TWI) Status Codes – Slave Transmitter Mode
// ------------------------------------------------------------

// 주소 수신(SLA+R)
#define SLAVE_RECEIVE_SLA_R_SEND_ACK               0xA8    // [SLAVE] SLA+R 수신 → [SLAVE] ACK 전송
#define SLAVE_RECEIVE_SLA_R_ARB_LOST_SEND_ACK      0xB0    // Arbitration lost 후 SLA+R 수신 → ACK 전송[필요없음]

// 데이터 전송(SLAVE → MASTER)
#define SLAVE_SEND_DATA_RECEIVE_ACK             0xB8    // [SLAVE → MASTER] 데이터 전송 → [MASTER] ACK 수신
#define SLAVE_SEND_DATA_RECEIVE_NACK            0xC0    // 데이터 전송 → [MASTER] NACK → 송신 종료

// 마지막 바이트 전송(TWEA=0)
#define SLAVE_SEND_LAST_DATA_RECEIVE_ACK        0xC8    // 마지막 바이트 전송, [MASTER] ACK → 종료



#define SLAVE_ADDR 0x20
volatile char twi_data = 0;   // 마스터로부터 수신한 데이터 저장


/* TWI Slave Init ----------------------------------------------------------------------------------*/
void TWI_Slave_Init(uint8_t addr) {
	TWAR = (addr << 1); // 슬레이브 주소 설정
	TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN) | (1 << TWIE); // [TWEA: 주소 일치 시 ACK 응답], [TWEN: TWI Enable], [TWIE: TWI 인터럽트 인에이블]
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

/* 동작 함수 ----------------------------------------------------------------------------------*/
uint8_t SPI_MasterTransmit(uint8_t data){
	PORTB &= ~(1<<PORTB2);  // SS LOW
	SPDR = data;
	while(!(SPSR & (1<<SPIF)));
	uint8_t recv = SPDR;
	PORTB |= (1<<PORTB2);   // SS HIGH
	return recv;
}




/* MAIN 함수----------------------------------------------------------------------------------*/
volatile int Action_State_1 = 0;
volatile int Action_State_2 = 0;
int main(void){
	cli();
	UARTSetting();
	SPI_MasterInit(); // SPI Master 설정
	TWI_Slave_Init(SLAVE_ADDR);
	sei();   // 전역 인터럽트 Enable

	while(1){
		if(Action_State_1 == 1) {
			Printf("1\r\n");	
			SPI_MasterTransmit(1);
			Action_State_1 = 0;
		}
	}
}

ISR(TWI_vect) {
	uint8_t twi_status = TWSR & 0xF8; // TWI 동작 한 차례 끝→ STATUS 코드 읽음

	switch (twi_status) {
		/* 마스터가 슬레이브에게 데이터를 쓰는 경우 (수신) */
		case SLAVE_RECEIVE_SLA_W_SEND_ACK:  // SLA+W 수신, ACK 전송
		case SLAVE_RECEIVE_SLA_W_ARB_LOST_SEND_ACK:  // Arbitration Lost (SLA+W), ACK 전송
		TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN) | (1 << TWIE); // SLAVE의 데이터 수신 준비
		break;


		case SLAVE_RECEIVE_DATA_SEND_ACK:  // 데이터 바이트 수신, ACK 전송 (데이터 저장)
		twi_data = TWDR;
		if(twi_data == 1) Action_State_1 = 1;
		if(twi_data == 2) Printf("2\r\n");
		TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN) | (1 << TWIE);
		break;


		case SLAVE_RECEIVE_STOP_OR_RESTART  :  // STOP 또는 Repeated START 수신 → 슬레이브 대기 모드로 돌아감
		TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN) | (1 << TWIE);
		break;


		/* 마스터가 슬레이브로부터 데이터를 읽는 경우 (전송) */
		case SLAVE_RECEIVE_SLA_R_SEND_ACK:  // SLA+R 수신, ACK 전송
		case SLAVE_RECEIVE_SLA_R_ARB_LOST_SEND_ACK:  // Arbitration Lost (SLA+R), ACK 전송
		TWDR = 0;
		TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN) | (1 << TWIE);
		break;

		case SLAVE_SEND_DATA_RECEIVE_ACK:  // 데이터 전송, ACK 수신 (더 전송할 것이 있다면)
		// 반복적으로 데이터를 계속 전송 후 종료
		// 더 전송할 데이터가 없으므로 다음 데이터 요청 시 NACK로 응답하도록 설정 [1Byte 밖에 없으므로 한 번 전송하고 종료함]
		TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWIE);   // TWEA 비활성화 (NACK 전송)
		break;

		case SLAVE_SEND_DATA_RECEIVE_NACK:  // 데이터 전송, NACK 수신 (마스터가 마지막 데이터를 읽음)
		case SLAVE_SEND_LAST_DATA_RECEIVE_ACK:  // 마지막 바이트 전송, ACK 전송 후 TWEA 비활성화 상태 → 대기 모드로 돌아감
		TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN) | (1 << TWIE);
		break;

		default: // 에러 상황 또는 예약 상태
		TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN) | (1 << TWIE);
		break;
	}
}
