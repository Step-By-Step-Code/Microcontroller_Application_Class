// SLAVE : Master로부터 숫자를 받음 → 숫자만큼 LED 킴 → 성공 여부를 SPI 통신으로 보냄

#include <avr/io.h>
#define FOSC 16000000UL
#define F_CPU 16000000UL
#include <avr/interrupt.h>
#include <util/delay.h>

volatile uint8_t spi_recv_data = 0;
volatile uint8_t led_len = 0;
volatile uint8_t sorf = 0;
volatile uint8_t temp = 0;

/* 동작 함수----------------------------------------------------------------------------------*/
void disp_led(unsigned int _value){ // _value 개수만큼 Low로 만들어 LED 켬
	PORTC |= 0x0f;
	PORTD |= 0xf0;

	for(int i = 0; i < _value; i++) {
		if(i < 4) PORTC &= ~(0x01<<i);
		else PORTD &= ~(0x01<<i);
	}
}

/* 포트 제어----------------------------------------------------------------------------------*/
// SPCR : SPIE | SPE | DORD | MSTR | CPOL | CPHA | SPR1 | SPR0
	// 7-SPI Interrupt Enable, 6-SPI Enable, 5-Data Order
	// 4-Master[1]/Slave[0] Select, 3-Clock Polarity, 2-Clock Phase
	// SPR1, SPR0: SPI Clock Rate Select 1 and 0
// SPSR : ☆SPIF | WCOL | – | – | – | – | – | SPI2X
// SPDR : MSB | - | LSB

void SPI_SlaveInit(){
	// SPCR 설정 : SPE=1 (SPI Enable), MSTR=0 (Slave 모드), f_osc/4
	SPCR = (1<<SPE) | (1<<SPIE);
	sei();
	SPDR = 0x4E;
}

void PortSetting(){
	// PD4~7, PC0~3 : LED 출력
	DDRD = 0xf0;
	DDRC = 0x0f;
	PORTC |= 0x0f; // LED는 끈 상태
	PORTD |= 0xf0; // LED는 끈 상태
	
	// SPI Communication Port Setting
	// PB3(MOSI), PB5(SCK), PB2(SS)는 모두 Input
	DDRB |= (1<<PORTB4); // [PB4 : MISO] →  Slave OUTPUT
	DDRB &= ~(1<<PORTB2); // [PB2 : SS] → ALWAYS INPUT
	DDRB &= ~(1<<PORTB5); // [PB5 : SCK] → ALWAYS INPUT
	PORTB &= ~(1<<PORTB2); // [PB2 : SS] → PULL-DOWN INPUT
}

/* 메인 함수 ----------------------------------------------------------------------------------*/
int main(){
	PortSetting();
	SPI_SlaveInit();

	while(1) disp_led(led_len);
}

/* 인터럽트 제어 ----------------------------------------------------------------------------------*/
ISR(SPI_STC_vect)
{
	spi_recv_data = SPDR; // Master가 보낸 데이터가 SPDR에 저장됨
	
	if(spi_recv_data <= 8){ // LED개수 유효성 검사
		led_len = spi_recv_data;
		sorf = 'S'; // Success
	}
	else sorf = 'F'; // Fail

	// Slave가 다음에 Master에게 보낼 데이터를 미리 SPDR에 저장해둠 (Master가 다음 클럭 바이트를 보내면 이 값이 전송됨)
	SPDR = sorf;
}