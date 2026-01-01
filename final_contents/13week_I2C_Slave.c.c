#include <avr/io.h>
#define FOSC 16000000UL
#define F_CPU 16000000UL
#include <util/delay.h>
#include <avr/interrupt.h>
// ------------------------------------------------------------
// I2C (TWI) Status Codes – Slave Receiver Mode
// ------------------------------------------------------------
/* I²C Slave Receiver Mode
| 상태 코드    | 의미                                 | 다음 동작                         |
| -------- | ---------------------------------- | ----------------------------- |
| **0x60** | 자신의 SLA+W 수신, ACK 반환               | 데이터 수신(ACK 또는 NACK) 시작        |
| **0x68** | Arbitration lost 후 자신의 SLA+W 수신    | 데이터 수신 시작                     |
| **0x70** | General Call 수신됨, ACK 반환           | 데이터 수신 시작                     |
| **0x78** | Arbitration lost 후 General Call 수신 | 데이터 수신 시작                     |
| **0x80** | Own SLA+W 주소로 데이터 수신, ACK 반환  | 다음 데이터 계속 수신 (ACK 또는 NACK 선택) |
| **0x88** | Own SLA+W 주소로 데이터 수신, NACK 반환      | 슬레이브 모드 종료로 이동                |
| **0x90** | General Call로 데이터 수신, ACK 반환       | 다음 데이터 계속 수신                  |
| **0x98** | General Call로 데이터 수신, NACK 반환      | 슬레이브 모드 종료                    |
| **0xA0** | STOP 또는 Repeated START 수신          | 슬레이브 모드 종료 & 대기 상태로 복귀        |
*/

/* I²C Transmitter Mode
| 상태 코드    | 의미                               | 다음 동작                 |
| -------- | -------------------------------- | --------------------- |
| **0xA8** | Master SLA+R 수신됨, ACK 반환됨           | 첫 번째 데이터 바이트 전송       |
| **0xB0** | Arbitration lost 후 Own SLA+R 수신됨 | 첫 번째 데이터 바이트 전송       |
| **0xB8** | 데이터 바이트 전송됨, ACK 수신됨             | 다음 데이터 계속 전송          |
| **0xC0** | 데이터 바이트 전송됨, NACK 수신됨            | 전송 종료 → 다음 통신 대기      |
| **0xC8** | 마지막 바이트(TWEA=0) 전송됨, ACK 수신됨     | 슬레이브 모드 종료(대기 상태로 복귀) |
*/
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



// ------------------------------------------------------------

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
	// 자동 CLEAR 안됨 ★★★
// 6. TWEA : Set → 조건을 만족할 때 TWI 버스에서 ACK 생성 [지속되는 속성]
	// ① 장치 고유의 슬레이브 주소 수신됐을 때
	// ② General Call 수신 → TWAR의 TWGCE Bit Set
	// ③ 마스터 리시버 모드 또는 슬레이브 리시보 모드에서 데이터 바이트 수신된 경우
// 5. ★★★TWSTA : Set → 마스터가 되길 원함 → 버스가 비었는지 확인[STOP 조건 감지될 때까지] → START Condition 생성 → 마스터 지위 획득
	// ★★★전송 완료 후 소프트웨어가 반드시 클리어해야함 
// 4. ★★★TWSTO : Set → Stop Condition 생성 → 자동 CLEAR[Master]
	// ★★★SLAVE : STOP 조건 생성 X → 비주소 슬레이브 모드로 복귀 → High Impedance 상태 
// 3. TWWC [TWWC: TWI Write Collision Flag] : TWINT가 LOW(0)일 때 TWDR에 쓰기를 시도하면 SET
	// TWINT가 HIGH일 때 TWDR 레지스터에 값을 기록하면 CLEAR
// 2. TWEN : TWI Enable Bit
// 0. TWIE : TWIE: TWI Interrupt Enable

#define SLAVE_ADDR 0x20
volatile char twi_data = 0;   // 마스터로부터 수신한 데이터 저장

void TWI_Slave_Init(uint8_t addr) {
	TWAR = (addr << 1); // 슬레이브 주소 설정
	TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN) | (1 << TWIE); 
	// TWINT: 작업 할 수 있는 상태로 전환
	// TWEA: 주소 일치 시 ACK
	// TWEN: TWI Enable
	// TWIE: TWI 인터럽트 인에이블
}

int main(void) {
	cli();
	TWI_Slave_Init(SLAVE_ADDR);
	sei(); // 인터럽트 활성화

	while (1) {};
}

 ISR(TWI_vect) {
	 uint8_t twi_status = TWSR & 0xF8; // ★★★ TWI 동작 한 차례 끝 → STATUS 코드 읽음

	 switch (twi_status) {
		 /* 마스터가 슬레이브에게 데이터를 쓰는 경우 (수신) */
		 case SLAVE_RECEIVE_SLA_W_SEND_ACK:  //★★★ SLA+W 수신하고 ACK 전송 → 데이터 받을 준비 코드 필요
		 case SLAVE_RECEIVE_SLA_W_ARB_LOST_SEND_ACK:  // Arbitration Lost (SLA+W), ACK 전송
		 TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN) | (1 << TWIE); // SLAVE의 데이터 수신 준비
		 break;


		 case SLAVE_RECEIVE_DATA_SEND_ACK:  // 데이터 바이트 수신, ACK 전송 (데이터 저장)
		 twi_data = TWDR;
		 TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN) | (1 << TWIE); // 다음 데이터 수신 또는 STOP/Repeated START 대기
		 break;


		 case SLAVE_RECEIVE_STOP_OR_RESTART  :  // STOP 또는 Repeated START 수신 → 슬레이브 대기 모드로 돌아감
		 TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN) | (1 << TWIE);
		 break;


		 /* 마스터가 슬레이브로부터 데이터를 읽는 경우 (전송) */
		 case SLAVE_RECEIVE_SLA_R_SEND_ACK:  // SLA+R 수신, ACK 전송
		 case SLAVE_RECEIVE_SLA_R_ARB_LOST_SEND_ACK:  // Arbitration Lost (SLA+R), ACK 전송
		 TWDR = twi_data; // 수신한 데이터를 마스터에게 전송 준비
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
