/*검토 완료*/
#define FOSC 16000000UL
#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#define BAUD_RATE 103

volatile unsigned char send_char = 0;

void PortSetting(){
	DDRD = 0b00000010;  // PD1 = TX
	DDRD &= ~(0b00001100);  // PD2, PD3 = 스위치 입력
}
void UARTSetting(){
	UBRR0H = (unsigned char)(BAUD_RATE >> 8);   // Baud Rate Setting → 9600
	UBRR0L = (unsigned char)(BAUD_RATE);
	
	// UCSR0A : [☆RXCn | ☆TXCn | ☆UDREn | FEn | DORn | UPEn | U2Xn | MPCMn]
		// 수신 완료 Flag | 송신 완료 Flag | 송신버퍼 비어 있음 | 프레임 에러 | 데이터 오버런 | 패리티 에러 | 2배 속도 모드 | 멀티프로세서 모드
	UCSR0A = 0x00;
	
	// UCSR0B : [☆RXCIEn | ☆TXCIEn | ☆UDRIEn | ☆RXENn | ☆TXENn | ☆UCSZn2 | RXB8n | TXB8n]
		// [ 0-2 수신/송신/데이터레지스터비움 완료 인터럽트 허용] | [3-4 수신기/송신기 활성화] | 5 문자크기 비트 |  [6-7 수신/송신 데이터 9비트]
	UCSR0B = (1 << TXEN0); // 송신기 활성화
	
	// UCSR0C : [ UMSELn1 | UMSELn0 | UPMn1 | UPMn0 | USBSn | ☆UCSZn1 | ☆UCSZn0 | UCPOLn ]
		// [0-1 동기/비동기 모드 선택] | [2-3 패리티 모드 비트] | [4 스톱비트 설정] | [5-6 문자크기 비트] | [7 클록 폴라리티]
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); // 문자 크기 : 8 bit 설정
}
void INTSetting(){
	// INT0~1 = PD2~3
	EICRA = 0x0A; // [– – – – ISC11 ISC10 ISC01 ISC00] → INT0/1 Falling Edge Setting
	EIFR = 0x03; // INT0/1 Flag Clear
	EIMSK = 0x03; // INT0/1 Enable Setting
}

int main(){
	cli();
	PortSetting();
	INTSetting();
	UARTSetting();
	sei();

	while(1){
		if(send_char){
			while(!(UCSR0A & (1 << UDRE0)));     // UDR이 비워질 때까지 대기
			UDR0 = send_char;                  // 송신 버퍼에 문자 전송
			send_char = 0;
		}
	}
}

// ===== INT0 인터럽트 =====
ISR(INT0_vect){
	cli();
	_delay_ms(20);
	send_char = 'A';
	// EIFR |= 0x01; → 자동으로 CLEAR
	sei();
}

// ===== INT1 인터럽트 =====
ISR(INT1_vect){
	cli();
	_delay_ms(20);
	send_char = 'B';
	// EIFR |= 0x02; → 자동으로 CLEAR
	sei();
}