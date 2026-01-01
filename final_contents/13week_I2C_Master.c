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


 
 /* I²C Master Transmitter Mode
 | 상태 코드    | 의미                 | 다음 동작  |
 | -------- | ------------------ | ---------------------- |
 | **0x08** | START 전송됨          | SLA+W 또는 SLA+R 전송      |
 | **0x10** | Repeated START 전송됨 | SLA+W 또는 SLA+R 전송      |
 | **0x18** | SLA+W 전송, ACK 수신됨  | 데이터 전송 계속              |
 | **0x20** | SLA+W 전송, NACK 수신됨 | STOP 또는 Repeated START |
 | **0x28** | 데이터 전송, ACK 수신됨    | 다음 데이터 전송              |
 | **0x30** | 데이터 전송, NACK 수신됨   | STOP 또는 Repeated START |
 | **0x38** | 버스 중재 상실[멀티 마스터 시스템] | 버스 재시도 (START 대기)      |
 */

 /* I²C Master Receiver Mode
 | 상태 코드    | 의미                 | 다음 동작                      |
 | -------- | ------------------ | -------------------------- |
 | **0x08** | START 전송됨          | SLA+R 또는 SLA+W 전송          |
 | **0x10** | Repeated START 전송됨 | SLA+R 또는 SLA+W 전송          |
 | **0x38** | Arbitration lost [멀티 마스터 시스템]   | 버스 해제, 다시 START 대기         |
 | **0x40** | SLA+R 전송, ACK 수신됨  | 데이터 수신 시작 (ACK 또는 NACK 선택) |
 | **0x48** | SLA+R 전송, NACK 수신됨 | STOP 또는 반복 START로 재시도      |
 | **0x50** | 데이터 수신됨, ACK 반환함   | 다음 바이트 계속 수신               |
 | **0x58** | 데이터 수신됨, NACK 반환함  | 읽기 종료 → STOP 전송            |
 */
 
 
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

/* I2C 함수 MASTER 기준 ----------------------------------------------------------------------------------*/
// TWBR : TWBR7-0 → [마스터 모드] SCL 클록 주파수를 생성하는 주파수 분주기
// TWSR : [TWS7-3 :	TWI Status] | – | [TWPS1-0 : TWI Prescaler Bits]
// TWDR : TWDR7-0
// TWAR : TWA6-0[MASTER Mode에서 필요 X, SLAVE 송수신기로 동작할 때 사용 또는 멀티 마스터 시스템] | TWGCE[SET → General Call 인식]
// TWAMR[TWI Address Mask] : TWAM[6:0] | – 
	// 모든 주소 무시 : 0b1111111
	// TWAMR = 0b0001111 → TWAR = 0b1010000 → 0b101xxxx(0x50~0x57) 모두 응답
// TWCR : TWINT | TWEA | TWSTA | TWSTO | TWWC | TWEN | – | TWIE
// 7. TWINT : TWI Interrupt Flag [Condition : SREG-I, TWIE]
	// TWI 작업 완료 후 Set → SCL LOW → 소프트웨어에서 CLEAR [TW[A/S/D]R모든 접근 완료되어야 함] [TWI 동작 다시 시작]
	// 자동 CLEAR 안됨
// 6. TWEA : Set → 조건을 만족할 때 TWI 버스에서 ACK 생성 [지속되는 속성]
	// ① 장치 고유의 슬레이브 주소 수신됐을 때
	// ② General Call 수신 → TWAR의 TWGCE Bit Set
	// ③ 마스터 리시버 모드 또는 슬레이브 리시보 모드에서 데이터 바이트 수신된 경우
// 5. TWSTA : Set → 마스터가 되길 원함 → 버스가 비었는지 확인[STOP 조건 감지될 때까지] → START Condition 생성 → 마스터 지위 획득
	// 전송 완료 후 소프트웨어가 반드시 클리어해야함
// 4. TWSTO : Set → Stop Condition 생성 → 자동 CLEAR[Master]
	// SLAVE : STOP 조건 생성 X → 비주소 슬레이브 모드로 복귀 → High Impedance 상태 
// 3. TWWC [TWWC: TWI Write Collision Flag] : TWINT가 LOW(0)일 때 TWDR에 쓰기를 시도하면 SET
	// TWINT가 HIGH일 때 TWDR 레지스터에 값을 기록하면 CLEAR
// 2. TWEN : TWI Enable Bit
// 0. TWIE : TWIE: TWI Interrupt Enable

void TWI_Master_Init(void){ /* I2C 초기화 (Master) */
	// SCL 클럭주파수 설정: TWI Clock Frequency = F_CPU / (16 + 2 * TWBR + 4^TWPS)
	// // 100kHz 설정: TWBR = 72, TWPS = 0 (Prescaler = 1) → (16000000 / (16 + 2 * 72)) / 1 = 100000 Hz
	
	TWBR = 72; // TWBR = 72
	TWSR = 0x00; // Prescaler = 1
}

// TWCR : ★★★TWINT | ★★★TWEA | ★★★TWSTA | ★★★TWSTO | TWWC | ★★★TWEN | – | ★★★TWIE

/*------------TWI_START_STOP----------------*/
uint8_t TWI_Start(void){// I2C 시작 조건 전송
	TWCR = (1<<TWINT) | (1<<TWSTA) | (1<<TWEN); // START 조건 전송, TWINT 플래그 클리어
	while (!(TWCR & (1<<TWINT))); // TWINT 플래그가 셋될 때까지 대기
	return (TWSR & 0xF8);         // 상태 코드 반환
}
void TWI_Stop(void){ // I2C 정지 조건 전송
	TWCR = (1<<TWINT) | (1<<TWSTO) | (1<<TWEN); // STOP 조건 전송, TWINT 플래그 클리어
	// TWSTO가 클리어될 때까지 기다릴 필요 X
}

/*------------TWI_WRITE_DATA----------------*/
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

/*------------TWI_READ_DATA----------------*/
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

/* 메인문 ----------------------------------------------------------------------------------*/
int main(void){
	char tx_data, rx_data;
	UARTSetting();
	TWI_Master_Init();
	sei();

	Printf("ATmega328P Master I2C Loopback Test Start!\r\n");
	Printf("Slave Address: 0x%x\r\n", SLAVE_ADDR);

	// 마스터가 데이터를 보내고 슬레이브로부터 ACK 받음
	// 마스터가 데이터를 받고 슬레이브로부터 NACK 받음
	while (1) {
		Printf("\r\n>> Enter 1 char (will be sent to slave): ");
		tx_data = rx_char(); // UART로 문자 1개 입력받기 [함수 내부에서 받을 때까지 대기]
		Printf("\r\nInput Data : %c\r\n", tx_data); // 입력받은 문자 에코

		// 1. 슬레이브에게 데이터 전송 (Write)
		if (TWI_Start() == START_ACK) { // Start Condition → 전송 후 TWINT 기다림 → TWINT Set 후 Status Code 확인 [START CONDITION 전송]
			if (TWI_Write_Address((SLAVE_ADDR << 1) | 0x00) == MASTER_SEND_SLA_W_RECEIVE_ACK) { // ② SLA+W 전송 후 Status Code 확인 [SLA+W 전송 후 ACK 확인]
				if (TWI_Write_Data(tx_data) == MASTER_SEND_DATA_RECEIVE_ACK) Printf(" - Data sent: %c\n", tx_data); // ③ 받은 데이터 UART 전송[데이터 전송 후 ACK 확인]
			}
		}
		TWI_Stop();
		_delay_ms(10); // 통신 안정화 대기


		// 2. 슬레이브로부터 데이터 수신 (Read)
		if (TWI_Start() == START_ACK) { // Start Condition 전송 후 TWINT 기다림 → TWINT Set 후 Status Code 확인 [START CONDITION 전송]
			if (TWI_Write_Address((SLAVE_ADDR << 1) | 0x01) == MASTER_SEND_SLA_R_RECEIVE_ACK) { // ② SLA+R 전송 후 Status Code 확인 [ SLA+R 전송 후 ACK 확인]
				rx_data = TWI_Read_Data_NACK(); // 마스터가 보내는 신호 [ACK (TWEA = 1) : 다음 바이트 보내] [NACK (TWEA = 0) 읽기 끝 → STOP 보냄]
				if ((TWSR & 0xF8) == MASTER_RECEIVE_DATA_SEND_NACK) Printf(" - Data received from slave: %c\n", rx_data);
			}
		}
		TWI_Stop();

		// 3. Loopback 결과 출력
		if (tx_data == rx_data) Printf("LOOPBACK SUCCESS!\r\n");
		else Printf("LOOPBACK FAIL! (Sent: %c, Received: %c)\r\n", tx_data, rx_data);
	}
}

/* Repeated Start is Available after ACK is received */
/* 역할이 서로 바껴도 서로 주소 달라야 함*/
/* 마스터 = START 신호 만드는 쪽*/
/*I2C에서는 마스터는 보통 폴링 방식으로 동작하고
슬레이브는 버스 이벤트를 즉시 처리해야 해서 인터럽트 기반으로 동작한다.*/

/* I2C 단일 메시지 전송 로직 (Master 기준)
1) START 조건 발생
2) Slave 주소 + R/W 비트 전송 (W = 0)
3) Slave ACK 확인
4) 데이터 1바이트 전송
5) Slave ACK 확인
6) STOP 조건 발생
*/

/*I2C 단일 메시지 수신 로직 (Slave 기준)
1) Slave는 자신의 주소(SLA+W)가 오기를 기다림
2) START + SLA+W 수신 → Slave가 ACK 전송
3) Master가 보낸 데이터 1바이트 수신
4) Slave가 ACK 전송
5) STOP 또는 Repeated START 감지 → 수신 종료
6) 받은 데이터 처리

*/



/* I²C에서 마스터가 여러 개 데이터를 보낼 때 로직
1) START 조건 전송
→ “통신 시작한다”는 신호

2) SLA+W(슬레이브 주소 + Write) 전송
→ 어떤 슬레이브에게 보낼지 지정
→ 슬레이브가 ACK 보냄 → 준비됨

3) 데이터 바이트 전송
→ 첫 번째 데이터 전송 → 슬레이브가 ACK 응답 → 두 번째 데이터 전송 → 슬레이브가 ACK 응답 → N번째 데이터 전송 → 슬레이브가 NACK 응답 전송 중단

4) STOP 조건 전송
→ STOP 조건을 보내 버스를 해제
*/



/* I²C에서 마스터가 여러 개 데이터를 받을 때 로직
1) START 조건 전송
→ 통신 시작

2) SLA + R 전송
→ 슬레이브에게 “읽겠다(Read)”라고 알림
→ 슬레이브가 ACK로 응답

3) 데이터 수신 루프
①첫 번째 ~ 마지막 전 바이트: 마스터가 ACK
②마지막 바이트: 마스터가 NACK(끝 신호)

4) STOP 조건 전송
→ 통신 완전 종료
→ 버스를 IDLE 상태로 되돌림
*/



/* I²C에서 마스터가 여러 개 데이터를 받고 보낼 때 로직
1) START 전송
통신 시작

2) SLA+W 전송
슬레이브에게 "쓸 거다" 알림

3) 여러 데이터 전송
각 데이터마다 슬레이브가 ACK
→ 계속 전송 가능

4) 읽기 모드로 전환
①STOP 후 START 또는
②Repeated START(정석)

5) SLA+R 전송
슬레이브에게 "읽을 거다" 알림

6) 여러 데이터 수신
①첫 번째 ~ 마지막 전 바이트: 마스터가 ACK
②마지막 바이트: 마스터가 NACK(끝 신호)

7) STOP 전송
전체 통신 종료
*/