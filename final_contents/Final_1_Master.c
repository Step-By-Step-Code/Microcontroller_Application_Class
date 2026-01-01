#define FOSC 16000000UL
#define F_CPU 16000000UL

#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdarg.h>
#include <string.h>

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
/*-------------------------------------------------------------------------------------------------------------------------------------------*/
#define SLAVE_ADDR 0x20

#define FREQ(x) (unsigned int)(16000000/(2*8*(1+x)))
volatile unsigned int note_freq[8] = {956, 1014, 1136, 1277, 1433, 1517, 1704, 1912};
volatile int note_idx = 0;

#define BAUD_RATE 25
#define LED_STOP_STATE			0
#define BUZZER_STOP_STATE		0


#define STX					0x02
#define ETX					0x03

#define LED_OP				0x01
#define LED_ALL_ON_OFF      0x01
#define LED_SHIFT			0x02
#define LED_SHIFT_BOTH		0x04

#define BUZZER_ON			0x02

#define BUZZER_SET			0x04
#define BUZZER_UP			0x01
#define BUZZER_DOWN			0x02

#define GET_ADC				0x08


volatile unsigned char sint_flag = 0;
volatile unsigned char LED_STATUS = 0;
volatile unsigned char LED_Conversion = 0;
volatile unsigned char BUZZER_STATE = 0;
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
void led_all_onoff(){ //  LED 전체 ON/OFF, [_n : 반복횟수], [_ms : 반복주기]
	if(LED_STOP_STATE == 0){
		PORTD = 0x00;
		PORTC = 0x00;
		for(volatile int j = 0; j < 500; j++) _delay_ms(1);
	}
	if(LED_STOP_STATE == 0){
		PORTD = (0xF0) ;
		PORTC = (0x0F) ;
		for(volatile int j = 0; j < 500; j++) _delay_ms(1);
	}
}

void led_shift(){ //  [_n : 반복횟수], [_ms : 반복주기]
	for (int j=0;j<7 && (LED_STOP_STATE == 0);j++){ // 0 → 6번째 핀까지
		if(LED_Conversion == 1){
			LED_Conversion = 0;
			return;
		}
		PORTD = (~(0x01 << j) & 0xF0);
		PORTC = (~(0x01 << j) & 0x0F);
		for(volatile int j = 0; j < 500; j++) _delay_ms(1);
	}
	
	if(LED_STOP_STATE == 0){
		PORTD = (0xF0);
		PORTC = (0x0F);
	}
}

void led_shift_both(){ //  [_n : 반복횟수], [_ms : 반복주기]
	for (int j=0;j<7 && (LED_STOP_STATE == 0);j++){ // 0 → 6번째 핀까지
		if(LED_Conversion == 1){
			LED_Conversion = 0;
			return;
		}
		PORTD = (~(0x01 << j) & 0xF0);
		PORTC = (~(0x01 << j) & 0x0F);
		for(volatile int j = 0; j < 500; j++) _delay_ms(1);
	}
	for (int k=0;k<8 && (LED_STOP_STATE == 0);k++){
		if(LED_Conversion == 1){
			LED_Conversion = 0;
			return;
		}
		PORTD = (~(0x80 >> k) & 0xF0);
		PORTC = (~(0x80 >> k) & 0x0F);
		for(volatile int j = 0; j < 500; j++) _delay_ms(1);
	}
	
	if (!LED_STOP_STATE){
		PORTD = 0xF0; // LED 모두 끄기
		PORTC = 0x0F; // LED 모두 끄기
	}
}
/*BUZZER 동작 함수---------------------------------------*/
void play(unsigned int y){
	DDRB |= 0x02; // PB1 출력
	TCCR1A = (1<<COM1A0); // [OC1A Toggle Setting]
	TCCR1B = (1<<WGM13) | (1<<WGM12) | (1<<CS11); // [CTC Mode → TOP : ICR1], [Prescaler 8]

	unsigned int x;
	x = FREQ(note_freq[y % 8]);

	ICR1 = x; // 주파수에 맞는 COUNT 값 저장
	PORTD = 0x0f; // LED 켜기
	OCR1A = 0; // ICR1보다 낮은 값으로 유지 (토글 보장) [없어도 되는 부분]
}
void stop(){
	TCCR1A = 0x00;
	TCCR1B = 0x00;
	TCNT1 = 0;
	ICR1 = 0;
	DDRB &= ~(0x02);
	PORTD = 0xff;
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
void PortSetting(){
	DDRD |= 0xF0;   // [4~7 : OUTPUT]
	DDRD &= ~(0x01); // PD0 INPUT
	DDRD |= 0x02; // PD1 OUTPUT
	DDRC |= 0x0F;   // PC0~3 : LED 출력
	
	PORTC |= 0x0F;
	PORTD |= 0xF0;
}
void UARTSetting(){
	UBRR0H = (unsigned char)(BAUD_RATE >> 8);
	UBRR0L = (unsigned char)(BAUD_RATE);
	UCSR0A = 0x00;
	UCSR0B = (1 << TXEN0) | (1 << RXEN0); // 송수신기 활성화
	UCSR0B |= (1 << RXCIE0); // 수신 완료 인터럽트 활성화
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00) | (1 << USBS0); // 문자 크기 : 8 bit , Stop 2 bit
}

void SPI_MasterInit(void){
	DDRB |= (1<<PORTB3) | (1<<PORTB5) | (1<<PORTB2); // MOSI, SCK, SS 출력
	PORTB |= (1<<PORTB2);   // SS HIGH -> Slave 비활성화
	SPCR = (1<<SPE) | (1<<MSTR) | (1<<SPR0); // Enable, Master, Fosc/16
}

void TWI_Master_Init(void){ /* I2C 초기화 (Master) */
	// SCL 클럭주파수 설정: TWI Clock Frequency = F_CPU / (16 + 2 * TWBR + 4^TWPS)
	// // 100kHz 설정: TWBR = 72, TWPS = 0 (Prescaler = 1) → (16000000 / (16 + 2 * 72)) / 1 = 100000 Hz
	
	TWBR = 72; // TWBR = 72
	TWSR = 0x00; // Prescaler = 1
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




/* 메인문 ----------------------------------------------------------------------------------*/
int main(){
	cli();
	PortSetting();
	UARTSetting();
	SPI_MasterInit();
	sei();

	Printf("INIT Message 202101661\r\n");
	
	while(1){
		if(LED_STATUS == 1) led_all_onoff();
		if(LED_STATUS == 2) led_shift();
		if(LED_STATUS == 3) led_shift_both();
	}
}
/* 인터럽트 동작 설정 ----------------------------------------------------------------------------------*/
volatile int decode_0 = 0;
volatile int decode_1 = 0;
volatile int decode_2 = 0;
volatile int enter = 0;
volatile unsigned char LEN_1 = 0;
volatile unsigned char LEN_2 = 0;
volatile unsigned char _command_1 = 0;
volatile unsigned char _value_1 = 0;
volatile unsigned char _command_2 = 0;
volatile unsigned char _value_2 = 0;

ISR(USART_RX_vect){
	unsigned char _usart_rcv = 0;
	_usart_rcv = UDR0;
	
	/*decode 1----------------------------*/
	if(decode_0 == 0 && _usart_rcv == STX) {
		decode_0 = 1;
		enter = 1;
	}
	
	if(decode_0 == 1 && !(enter)){/*decode 2----------------------------*/
		LEN_1 = _usart_rcv;
		decode_0 = 2;
		decode_1 = 1;
		enter = 1;
	}
	
	if(decode_0 == 2  && !(enter)){/* decode 2 → 3----------------------------*/
		if(LEN_1 == 1 && !(enter)){
				_command_1 = _usart_rcv;
				decode_0 = 3;
				enter = 1;
		}
		
		if(LEN_1 == 2 && !(enter)){
			if(decode_1 == 1  && !(enter)){
				_command_1 = _usart_rcv;
				decode_1 = 2;
				enter = 1;
			}
			
			if(decode_1 == 2  && !(enter)){
					_value_1 = _usart_rcv;
					decode_0 = 3;
					
					enter = 1;
			}
		}
	}
	
	if(decode_0 == 3  && !(enter)){/*decode 3 → 4----------------------------*/
		LEN_2 =  _usart_rcv;
		decode_2 = 1;
		decode_0 = 4;
		enter = 1;
	}
	
	if(decode_0 == 4  && !(enter)){ /*decode 4 → 5----------------------------*/
		if(LEN_2 == 1 && !(enter)){
			_command_2 = _usart_rcv;
			decode_0 = 5;
			enter = 1;
		}
		
		if(LEN_2 == 2 && !(enter)){
			if(decode_2 == 1  && !(enter)){
				_command_2 = _usart_rcv;
				decode_2 = 2;
				enter = 1;
			}
			if(decode_2 == 2  && !(enter)){
				_value_2 = _usart_rcv;
				decode_0 = 5;
				enter = 1;
			}
		}
	}
	
	if(decode_0 == 5  && !(enter)){ /*decode 마지막----------------------------*/
		if(_usart_rcv == ETX) {
			/*LED 제어*/
			if(_command_1 == LED_OP){
				if(!((_value_1 == LED_ALL_ON_OFF) || (_value_1 == LED_SHIFT_BOTH) || (_value_1 == LED_SHIFT))) return;
				if(_value_1 == LED_ALL_ON_OFF) LED_STATUS = 1;
				if(_value_1 == LED_SHIFT) LED_STATUS = 2;
				if(_value_1 == LED_SHIFT_BOTH) LED_STATUS = 3;
				LED_Conversion = 1;
			}
			
			/*BUZZER ON*/
			if(_command_1 == BUZZER_ON) {
				if(BUZZER_STATE == 0){
					Printf("Note LEVEL : %d\r\n", note_idx);
					BUZZER_STATE = 1;
				}
				play(note_idx); // 0 : BUZZER_ON
			}

			/*BUZZER SET*/
			if(_command_1 == BUZZER_SET){
				if(_value_1 == BUZZER_UP) {
					note_idx++;
					if(note_idx>=7) note_idx = 7;
					play(note_idx);
					Printf("Note LEVEL : %d\r\n", note_idx);
				}
				if(_value_1 == BUZZER_DOWN) {
					note_idx--;
					if(note_idx<=0) note_idx = 0;
					play(note_idx);
					Printf("Note LEVEL : %d\r\n", note_idx);
				}
			}

			
			/*GET_ADC*/
			if(_command_1 == GET_ADC) I2C_Transmit(1); // 1 : GET_ADC
			if(_command_2 == GET_ADC) I2C_Transmit(1); // 1 : GET_ADC	
		}
		else {
			Printf("RX error\n");
		}
		decode_0 = 0; // return to normal state
	}
	enter = 0;
}