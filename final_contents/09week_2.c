/*검토 완료*/

#define FOSC 16000000UL
#define F_CPU 16000000UL
#define BAUD_RATE 103

#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdarg.h> // → Printf를 위한 헤더
#include <string.h>


/* UART 전송 함수 ----------------------------------------------------------------------------------*/
/* 한 문자씩 전송하는 함수 */
void tx_char(unsigned char txChar){
	while (!(UCSR0A & (1 << UDRE0)));  // 전송 버퍼가 빌 때까지 대기
	UDR0 = txChar;
}
void Printf(char *fmt, ...) {
/* 함수 콜 스택 프레임(stack frame)
Printf("ID=%d, Temp=%.1f, Status=%s", 27, 36.5, "OK");
1. [fmt] ["ID=%d, Temp=%.1f, Status=%s"]
2. [int 27]
3. [double 36.5]
4. ["OK"]
*/
	va_list arg_ptr; // 가변 인자 목록을 가리키는 포인터
	uint8_t i, len;
	
	char sText[128];
	for (i = 0; i < 128; i++) sText[i] = 0; // Buffer 초기화

	va_start(arg_ptr, fmt); // arg_ptr : fmt 다음을 가리킴
	vsprintf(sText, fmt, arg_ptr); // [저장 장소 : sText], [가변인자를 담는 형식 : fmt], [arg_ptr : 가변 인자 목록을 가리키는 포인터]
	va_end(arg_ptr); // 가변 인자 포인터 메모리 정리

	len = strlen(sText);
	for (i = 0; i < len; i++) tx_char(sText[i]);
}
/* 문자열 전송 함수 */
void tx_str(char *txStr, int len){ // *txstr : 문자 배열 주소, len : 문자 배열 길이
    if (len < 0) while (*txStr) tx_char((unsigned char)*txStr++);   // 문자열 길이가 없을 때 사용, [\0 = 0 → \0일 때 멈춤]
	else for (int i = 0; i < len; i++) tx_char((unsigned char)txStr[i]);   // 지정된 길이만큼 전송
}

/* 포트 제어----------------------------------------------------------------------------------*/
// UCSR0A : [☆RXCn | ☆TXCn | ☆UDREn | FEn | DORn | UPEn | U2Xn | MPCMn]
	// 수신 완료 | 송신 완료 | 송신버퍼 비어 있음 | 프레임 에러 | 데이터 오버런 | 패리티 에러 | 2배 속도 모드 | 멀티프로세서 모드
// UCSR0B : [☆RXCIEn | ☆TXCIEn | ☆UDRIEn | ☆RXENn | ☆TXENn | ☆UCSZn2 | RXB8n | TXB8n]
	// [ 0-2 수신/송신/데이터레지스터비움 완료 인터럽트 허용] | [3-4 수신기/송신기 활성화] | 5 문자크기 비트 |  [6-7 수신/송신 데이터 9비트]
// UCSR0C : [ UMSELn1 | UMSELn0 | UPMn1 | UPMn0 | USBSn | ☆UCSZn1 | ☆UCSZn0 | UCPOLn ]
	// [0-1 동기/비동기 모드 선택] | [2-3 패리티 모드 비트] | [4 스톱비트 설정] | [5-6 문자크기 비트] | [7 클록 폴라리티]
void PortSetting(){
	DDRD |= 0x02;    // PD1 : TX → OUTPUT
	DDRD &= ~(0x0D); // PD0 : RX, PD2,3 : 스위치 입력
}
void UARTSetting(){
	UBRR0H = (unsigned char)(BAUD_RATE >> 8);  // Baud Rate Setting
	UBRR0L = (unsigned char)(BAUD_RATE);
	UCSR0B = (1 << TXEN0) | (1 << RXEN0);      // RX, TX Enable
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);    // Character Size : 8-bit setting
}
void INTSetting(){
	// INT0~1 = PD2~3
	EICRA = 0x0A; // [– – – – ISC11 ISC10 ISC01 ISC00] → INT0/1 Falling Edge Setting
	EIFR = 0x03; // INT0/1 Flag Clear
	EIMSK = 0x03; // INT0/1 Enable Setting
}

int main() {
	cli();
	PortSetting();
	INTSetting();
	UARTSetting();
	unsigned char rx_char = 0;
	sei();

	while(1) {
		while(!(UCSR0A & (1 << RXC0)));       // 전송받을 때까지 대기 [RX (Blocking)]
		rx_char = UDR0;                       // 전송 받은 값을 변수에 저장

		// TX
		while(!(UCSR0A & (1 << UDRE0)));      // UDR0이 빌 때까지 대기 [TX (Blocking)]
		UDR0 = rx_char;                       // 변수를 다시 UDR0에 써서 전송
	}
}

/* 인터럽트 동작 설정 ----------------------------------------------------------------------------------*/
ISR(INT0_vect){
	Printf("****************************\n");
	Printf("Clock Speed = %lu\n", F_CPU);
	Printf("Interrupts = 0x%02X\n", 0xFF);
	Printf("****************************\n");
}

ISR(INT1_vect){
	const char msg[] = "INT1 PUSHED\n";
	tx_str((char *)msg, (int)(sizeof(msg) - 1));
}