// SLAVE : Master로부터 숫자를 받음 → 숫자만큼 LED 킴 → 성공 여부를 SPI 통신으로 보냄

#include <avr/io.h>
#define FOSC 16000000UL
#define F_CPU 16000000UL
#include <avr/interrupt.h>
#include <util/delay.h>

volatile uint8_t spi_recv_data = 0;
volatile uint8_t sorf = 0;
volatile uint8_t temp = 0;
volatile uint8_t PRESENT_STATE = 0;

/* 포트 제어----------------------------------------------------------------------------------*/
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

/* SPI 제어----------------------------------------------------------------------------------*/
	// SPCR : SPIE | SPE | DORD | MSTR | CPOL | CPHA | SPR1 | SPR0
	// 7-SPI Interrupt Enable, 6-SPI Enable, 5-Data Order
	// 4-Master/Slave Select, 3-Clock Polarity, 2-Clock Phase
	// SPR1, SPR0: SPI Clock Rate Select 1 and 0
	// SPSR : ☆SPIF | WCOL | – | – | – | – | – | SPI2X
	// SPDR : MSB | - | LSB

void SPI_SlaveInit(){
	SPCR = (1<<SPE) | (1<<SPIE); // SPCR 설정 : SPE=1 (SPI Enable), MSTR=0 (Slave 모드)
	sei();
	SPDR = 0x4E; // 'N'
}

/* 동작 함수 ----------------------------------------------------------------------------------*/
void led_all_onoff(int _n){ //  LED 전체 ON/OFF, [_n : 반복횟수], [_ms : 반복주기]
	for (volatile int i=0; (i < _n) && (PRESENT_STATE == 1) ; i++)
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
	for(int i=0; (i < _n) && (PRESENT_STATE == 2) ; i++){		// 상위 4비트 → PORTD[3:0], 하위 4비트 → PORTC[3:0]
		PORTD = (LED_MASK & 0xF0); // LED 상위 4 Bit 추출하여 4번 오른쪽으로 쉬프트
		PORTC = (LED_MASK & 0x0F); // LED 하위 4 Bit 추출
		for(volatile int j = 0; j < 500; j++) _delay_ms(1);
		
		PORTD = 0xF0 - (LED_MASK & 0xF0);  // 추출한 값에서 켜진 걸 꺼지도록 꺼진 걸 켜지도록 설정
		PORTC = 0x0F - (LED_MASK & 0x0F);		// 추출한 값에서 켜진 걸 꺼지도록 꺼진 걸 켜지도록 설정
		for(volatile int j = 0; j < 500; j++) _delay_ms(1);
	}
}

void led_shift(int _n){ //  [_n : 반복횟수], [_ms : 반복주기]
	for (int i=0;(i < _n) && (PRESENT_STATE == 3);i++)
	{
		for (int j=0;(j<7) && (PRESENT_STATE == 3);j++){ // 0 → 6번째 핀까지
			PORTC = ((~(0x01 << j) & 0x0F));
			PORTD = (~(0x01 << j) & 0xF0);
			for(volatile int j = 0; j < 500; j++) _delay_ms(1);
		}
		
		for (int k=0;(k<8) && (PRESENT_STATE == 3);k++) // 7번째 핀에서
		{
			PORTC = (~(0x80 >> k) & 0x0F);
			PORTD = (~(0x80 >> k) & 0xF0);
			for(volatile int j = 0; j < 500; j++) _delay_ms(1);
		}
	}
	PORTD = (0xF0); // LED 모두 끄기
	PORTC = (0x0F); // LED 모두 끄기
}

/* 메인 함수 ----------------------------------------------------------------------------------*/
int main(){
	PortSetting();
	SPI_SlaveInit();

	while(1){
		if(PRESENT_STATE == 0) {
			PORTC &= ~(0xF0);
			PORTD &= ~(0x0F);
		}
		if(PRESENT_STATE == 1) led_all_onoff(3);
		if(PRESENT_STATE == 2) led_alternating(0xAA,3);
		if(PRESENT_STATE == 3) led_shift(3);
	}
	
}

/* 인터럽트 제어 ----------------------------------------------------------------------------------*/
ISR(SPI_STC_vect){
	int sorf = PRESENT_STATE;
	spi_recv_data = SPDR; // Master가 보낸 데이터가 SPDR에 저장됨
	if(spi_recv_data >=0 && spi_recv_data <=3) PRESENT_STATE = spi_recv_data;
	SPDR = sorf;
}