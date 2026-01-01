#include <avr/io.h>
#define FOSC 16000000UL
#define F_CPU 16000000UL
#include <util/delay.h>
#include <avr/interrupt.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#define BAUD_RATE 103       // 9600bps
#define SLAVE_ADDR 0x20

// ------------------------------------------------------------
// I2C (TWI) Status Codes for Master Mode
// ------------------------------------------------------------
// START & REPEATED START
#define START_ACK                     0x08    // START 전송됨
#define REP_START_ACK                 0x10    // Repeated START 전송됨

// -------------------------------------------------------------------
// MASTER → SLAVE : Transmitter Mode (Write)
// -------------------------------------------------------------------
#define MASTER_SEND_SLA_W_RECEIVE_ACK     0x18    // [MASTER] SLA+W 전송, [SLAVE로부터] ACK 수신
#define MASTER_SEND_SLA_W_RECEIVE_NACK    0x20    // [MASTER] SLA+W 전송, [SLAVE로부터] NACK 수신

#define MASTER_SEND_DATA_RECEIVE_ACK      0x28    // [MASTER] 데이터 전송, [SLAVE로부터] ACK 수신
#define MASTER_SEND_DATA_RECEIVE_NACK     0x30    // [MASTER] 데이터 전송, [SLAVE로부터] NACK 수신

#define MASTER_ARBITRATION_LOST_TX        0x38    // [MASTER] Arbitration Lost (송신 도중 버스 중재 상실)

// -------------------------------------------------------------------
// SLAVE → MASTER : Receiver Mode (Read)
// -------------------------------------------------------------------
#define MASTER_SEND_SLA_R_RECEIVE_ACK     0x40    // [MASTER] SLA+R 전송, [SLAVE로부터] ACK 수신
#define MASTER_SEND_SLA_R_RECEIVE_NACK    0x48    // [MASTER] SLA+R 전송, [SLAVE로부터] NACK 수신

#define MASTER_RECEIVE_DATA_SEND_ACK      0x50    // [SLAVE→MASTER] 데이터 수신, [MASTER] ACK 전송
#define MASTER_RECEIVE_DATA_SEND_NACK     0x58    // [SLAVE→MASTER] 데이터 수신, [MASTER] NACK 전송 (마지막 바이트)

#define MASTER_ARBITRATION_LOST_RX        0x38    // [MASTER] Arbitration Lost (수신 도중 버스 중재 상실)


/* UART 송수신 함수 ----------------------------------------------------------------------------------*/
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

char rx_char(void){
	while(!(UCSR0A & (1<<RXC0)));
	return UDR0;
}

/* I2C 초기화 함수 ----------------------------------------------------------------------------------*/
void TWI_Master_Init(void){ /* I2C 초기화 (Master) */
	// SCL 클럭주파수 설정: TWI Clock Frequency = F_CPU / (16 + 2 * TWBR + 4^TWPS)
	// // 100kHz 설정: TWBR = 72, TWPS = 0 (Prescaler = 1) → (16000000 / (16 + 2 * 72)) / 1 = 100000 Hz
	
	TWBR = 72; // TWBR = 72
	TWSR = 0x00; // Prescaler = 1
}

/* I2C 제어 로직 ★마스터★ ----------------------------------------------------------------------------------*/
uint8_t TWI_Start(void){// I2C 시작 조건 전송
	TWCR = (1<<TWINT) | (1<<TWSTA) | (1<<TWEN); // START 조건 전송, TWINT 플래그 클리어
	while (!(TWCR & (1<<TWINT))); // TWINT 플래그가 셋될 때까지 대기
	return (TWSR & 0xF8);         // 상태 코드 반환
}
void TWI_Stop(void){ // I2C 정지 조건 전송
	TWCR = (1<<TWINT) | (1<<TWSTO) | (1<<TWEN); // STOP 조건 전송, TWINT 플래그 클리어
	// TWSTO가 클리어될 때까지 기다릴 필요 X
}
uint8_t TWI_Write_Address(uint8_t address){ // SLA+R/W 전송
	TWDR = address; // SLA+R/W (address << 1) | R/W
	TWCR = (1<<TWINT) | (1<<TWEN); // TWINT 플래그 클리어
	while (!(TWCR & (1<<TWINT))); // 상태값을 받기 위해 기다림
	return (TWSR & 0xF8); // 상태값 반환
}
uint8_t TWI_Write_Data(uint8_t data){ // 데이터 전송
	TWDR = data;
	TWCR = (1<<TWINT) | (1<<TWEN); // TWINT 플래그 클리어
	while (!(TWCR & (1<<TWINT))); // 상태값을 받기 위해 기다림
	return (TWSR & 0xF8); // 상태값 반환
}

uint8_t TWI_Read_Data_ACK(void){ // 데이터 수신 (ACK)
	TWCR = (1<<TWINT) | (1<<TWEN) | (1<<TWEA); // ACK 전송 (TWEA = 1)
	while (!(TWCR & (1<<TWINT))); // 데이터 받을때까지 기다림
	return TWDR; // 데이터 반환
}
uint8_t TWI_Read_Data_NACK(void){ // 데이터 수신 (NACK) - 마지막 바이트 수신 시 사용
	TWCR = (1<<TWINT) | (1<<TWEN); // NACK 전송 (TWEA = 0)
	while (!(TWCR & (1<<TWINT))); // 데이터 받을때까지 기다림
	return TWDR; // 데이터 반환
}

/* 포트 제어----------------------------------------------------------------------------------*/
void UARTSetting(){
	UBRR0H = (unsigned char)(BAUD_RATE >> 8);   // Baud Rate Setting → 9600
	UBRR0L = (unsigned char)(BAUD_RATE);
	UCSR0A = 0x00;
	UCSR0B = (1 << TXEN0) | (1 << RXEN0);      // RX, TX Enable
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);    // Character Size : 8-bit setting
}



/* I2C 데이터 ★송신하기★ ★마스터★ ----------------------------------------------------------------------------------*/
void I2C_Transmit(char tx_data){
	if (TWI_Start() == START_ACK) {
		if (TWI_Write_Address((SLAVE_ADDR << 1) | 0x00) == MASTER_SEND_SLA_W_RECEIVE_ACK) {
			if (TWI_Write_Data(tx_data) == MASTER_SEND_DATA_RECEIVE_ACK) Printf(" - Data sent: %d\r\n", tx_data);
		}
	}
	TWI_Stop();
	_delay_ms(10);
}

/* I2C 데이터 ★수신하기★ ★마스터★ ----------------------------------------------------------------------------------*/
char I2C_Receive(){
	char rx_data;
	if (TWI_Start() == START_ACK) {
		if (TWI_Write_Address((SLAVE_ADDR << 1) | 0x01) == MASTER_SEND_SLA_R_RECEIVE_ACK) {
			rx_data = TWI_Read_Data_NACK(); 
			if ((TWSR & 0xF8) == MASTER_RECEIVE_DATA_SEND_NACK) Printf(" - Data received from slave: %d\r\n", rx_data);
		}
	}
	Printf("\n");
	TWI_Stop();	
	return rx_data;
}

/* 메인문 ----------------------------------------------------------------------------------*/
int main(void){
	char uart_recv;
	char rx_data = 0;
	UARTSetting();
	TWI_Master_Init();
	sei();

	Printf("ATmega328P Master I2C Loopback Test Start!\r\n");
	Printf("Slave Address: 0x%x\r\n", SLAVE_ADDR);

	while (1) {
		/* UART로 문자 1개 입력받기 [함수 내부에서 받을 때까지 대기] → RXC0 인터럽트 사용 X */
		uart_recv = rx_char();
		
		
		if(uart_recv >= '0' && uart_recv <= '3'){
			if(uart_recv == '0'){
				I2C_Transmit(0);
				_delay_ms(10); // 통신 안정화 대기
				rx_data = I2C_Receive();
			}
			
			if(uart_recv == '1'){
				I2C_Transmit(1);
				_delay_ms(10); // 통신 안정화 대기
				rx_data = I2C_Receive();
			}
			if(uart_recv == '2'){
				I2C_Transmit(2);
				_delay_ms(10); // 통신 안정화 대기
				rx_data = I2C_Receive();
			}
			if(uart_recv == '3'){
				I2C_Transmit(3);
				_delay_ms(10); // 통신 안정화 대기
				rx_data = I2C_Receive();
			}
			Printf("Before : %d\r\n", rx_data);
			Printf("After : %c\r\n", uart_recv);
		}
		else Printf("\nCheck it is 0~9 Numbers\n");
	}
}