/*
MASTER MCU
1. SPI Master
2. I2C Slave

SLAVE MCU
1. SPI Slave
2. I2C Master

1. 스위치 입력 : SLAVE MCU에서 MASTER MCU로 I2C로 1을 신호 보냄
2. MASTER MCU가 I2C로 1을 받음 [UART : I2C Data Receive!]
3. MASTER MCU가 SLAVE MCU에 SPI로 1을 전송 [UART : SPI Transmit!]
4. SLAVE MCU가 SPI로 1을받고 I2C로 2를 전송 [UART : SPI Transmit SUCCESS!]
*/

// MASTER : UART로 문자를 받음 → 받은 값을 숫자로 변환 → SLAVE에게 SPI를 통해 메세지 전송 → SLAVE한테 SPI를 통해 메세지를 받고 UART로 컴퓨터한테 전송

#include <avr/io.h>
#define FOSC 16000000UL
#define F_CPU 16000000UL
#include <avr/interrupt.h>
#include <util/delay.h>

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


#define SLAVE_ADDR 0x20
volatile uint8_t spi_received_flag = 0;
volatile uint8_t spi_recv_data = 0;
volatile uint8_t led_len = 0;
volatile uint8_t sorf = 0;
volatile uint8_t temp = 0;
void TWI_Master_Init(void){ /* I2C 초기화 (Master) */
	// SCL 클럭주파수 설정: TWI Clock Frequency = F_CPU / (16 + 2 * TWBR + 4^TWPS)
	// // 100kHz 설정: TWBR = 72, TWPS = 0 (Prescaler = 1) → (16000000 / (16 + 2 * 72)) / 1 = 100000 Hz
	
	TWBR = 72; // TWBR = 72
	TWSR = 0x00; // Prescaler = 1
}

/* 동작 함수----------------------------------------------------------------------------------*/
void disp_led(unsigned int _value){ // _value 개수만큼 Low로 만들어 LED 켬
	PORTC |= 0x0f;
	PORTD |= 0xf0;

	for(int i = 0; i < _value; i++) {
		if(i < 4) PORTC &= ~(0x01<<i);
		else PORTD &= ~(0x01<<i);
	}
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

/* I2C 데이터 ★송신하기★ ★마스터★ ----------------------------------------------------------------------------------*/
void I2C_Transmit(char tx_data){
	if (TWI_Start() == START_ACK) {
		if (TWI_Write_Address((SLAVE_ADDR << 1) | 0x00) == MASTER_SEND_SLA_W_RECEIVE_ACK) {
			if (TWI_Write_Data(tx_data) == MASTER_SEND_DATA_RECEIVE_ACK) {}
		}
	}
	TWI_Stop();
	_delay_ms(20);
}


void SPI_SlaveInit(){
	// SPCR 설정 : SPE=1 (SPI Enable), MSTR=0 (Slave 모드)
	SPCR = (1<<SPE) | (1<<SPIE);
	sei();
}

/* 포트 제어----------------------------------------------------------------------------------*/
void PortSetting(){
	DDRD |= 0xf0;     // PD4~7 : LED 출력
	DDRC |= 0x0f;     // PC0~3 : LED 출력
	PORTC |= 0x0f; // LED = OFF 상태로 시작
	PORTD |= 0xf0; // LED = OFF 상태로 시작
}
void INTSetting(){
	/* --- 핀체인지 인터럽트 설정 (PORTB용) --- */
	PCMSK0 |= 0x04;       // PCINT2(0x04)=PB2 허용
	PCIFR |= 0x01;        // PCIF0 플래그 클리어 (PORTB)
	PCICR |= 0x01;        // PCIE0=1 → PORTB 핀체인지 인터럽트 enable

	// 외부 인터럽트 설정
	EICRA = 0x0A;
	EIFR = 0x03;
	EIMSK = 0x03;
}

/* 메인 함수 ----------------------------------------------------------------------------------*/
int main(){
	cli();
	PortSetting();
	INTSetting();
	SPI_SlaveInit();
	TWI_Master_Init();
	sei();
	
	while (1) {
		if(spi_received_flag) {
			I2C_Transmit(2);
			spi_received_flag = 0;
		}		
	}
}



/* 인터럽트 제어 ----------------------------------------------------------------------------------*/


ISR(SPI_STC_vect)
{
	spi_recv_data = SPDR;
	if(spi_recv_data == 1) spi_received_flag = 1;
}


ISR(INT0_vect){
	cli();
	I2C_Transmit(1);
	sei();
}
