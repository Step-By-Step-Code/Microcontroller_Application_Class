#include <avr/io.h>
#define FOSC 16000000UL
#define F_CPU 16000000UL
#include <util/delay.h>
#include <avr/interrupt.h>
// ------------------------------------------------------------
// I2C (TWI) Status Codes – Slave Receiver Mode
// ------------------------------------------------------------

// 주소 수신
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
volatile char PRESENT_STATE = 1;

void TWI_Slave_Init(uint8_t addr) {
	TWAR = (addr << 1); // 슬레이브 주소 설정
	TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN) | (1 << TWIE); // [TWEA: 주소 일치 시 ACK 응답], [TWEN: TWI Enable], [TWIE: TWI 인터럽트 인에이블]
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
/* 포트 제어----------------------------------------------------------------------------------*/
void PortSetting(){
	DDRD |= 0xF0; // [RX[PD0] : INPUT], [TX[PD1] = OUTPUT], [PD2-3[INT0/1]] : INPUT], [PD4-7 : OUTPUT] → 0b11110010;
	PORTD |= 0xFF; // 11111111 → PD4~7[LED연결] 끄기, 입력핀은 모두 풀업저항
	DDRC  |= 0x0F; // 00011110 → PC0~3 모두 출력 설정
	PORTC |= 0x0F; // PC와 연결된 LED 끄기
}
/* MAIN 함수 ----------------------------------------------------------------------------------*/
int main(void) {
	cli();
	PortSetting();
	TWI_Slave_Init(SLAVE_ADDR);
	sei(); // 인터럽트 활성화

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

volatile char temporary_data;

ISR(TWI_vect) {
	uint8_t twi_status = TWSR & 0xF8; // TWI 동작 한 차례 끝→ STATUS 코드 읽음

	switch (twi_status) {
		/* 마스터가 슬레이브에게 데이터를 쓰는 경우 (수신) */
		case SLAVE_RECEIVE_SLA_W_SEND_ACK:  // SLA+W 수신, ACK 전송
		case SLAVE_RECEIVE_SLA_W_ARB_LOST_SEND_ACK:  // Arbitration Lost (SLA+W), ACK 전송
		TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN) | (1 << TWIE); // SLAVE의 데이터 수신 준비
		break;

		case SLAVE_RECEIVE_DATA_SEND_ACK:  // 데이터 바이트 수신, ACK 전송 (데이터 저장)
		temporary_data = PRESENT_STATE;
		twi_data = TWDR;
		PRESENT_STATE = twi_data;
		// 다음 데이터 수신 또는 STOP/Repeated START 대기
		TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN) | (1 << TWIE);
		break;

		case SLAVE_RECEIVE_STOP_OR_RESTART  :  // STOP 또는 Repeated START 수신 → 슬레이브 대기 모드로 돌아감
		TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN) | (1 << TWIE);
		break;


		/* 마스터가 슬레이브로부터 데이터를 읽는 경우 (전송) */
		case SLAVE_RECEIVE_SLA_R_SEND_ACK:  // SLA+R 수신, ACK 전송
		case SLAVE_RECEIVE_SLA_R_ARB_LOST_SEND_ACK:  // Arbitration Lost (SLA+R), ACK 전송
		TWDR = temporary_data; // 수신한 데이터를 마스터에게 전송 준비
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
