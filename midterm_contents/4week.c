 #include <avr/io.h>
#define FOSC 16000000UL // 오실레이터 주파수(클럭 속도), _delay_ms에 필요한 파라미터
#define F_CPU 16000000UL // CPU 클럭 주파수, _delay_ms에 필요한 파라미터
#include <util/delay.h> // _delay_ms 라이브러리

void LED_ALL_ON_OFF(int _n, int _ms){ //  LED 전체 ON/OFF, [_n : 반복횟수], [_ms : 반복주기]
	for (volatile int i=0; i<_n; i++)
	{
		PORTD = 0x00;
		PORTC = 0x00;
		for(volatile int j = 0; j < _ms; j++) _delay_ms(1);
		PORTD = (0xF0) ;
		PORTC = (0x0F) ;
		for(volatile int j = 0; j < _ms; j++) _delay_ms(1);
	}
}


void led_alternating_onoff(int LED_MASK, int _onoff, int _ms){ // LED 동작 제어
	for(int i=0; i<_onoff; i++){		// 상위 4비트 → PORTD[3:0], 하위 4비트 → PORTC[3:0]
		PORTD = ((LED_MASK & 0xF0)>>4); // LED 상위 4 Bit 추출하여 4번 오른쪽으로 쉬프트  
		PORTC = (LED_MASK & 0x0F);		// LED 하위 4 Bit 추출
		for(volatile int j = 0; j < _ms; j++) _delay_ms(1);
		
		PORTD = 0x0F - ((LED_MASK & 0xF0)>>4);  // 추출한 값에서 켜진 걸 꺼지도록 꺼진 걸 켜지도록 설정
		PORTC = 0x0F - (LED_MASK & 0x0F);		// 추출한 값에서 켜진 걸 꺼지도록 꺼진 걸 켜지도록 설정
		for(volatile int j = 0; j < _ms; j++) _delay_ms(1);
	}
}

void LED_SHIFT(int _n, int _ms){ //  [_n : 반복횟수], [_ms : 반복주기]
	for (int i=0;i<_n;i++)
	{
		for (int j=0;j<7;j++){ // 0 → 6번째 핀까지		
			PORTC = ((~(0x01 << j) & 0x0F));
			PORTD = (~(0x01 << j) & 0xF0);
			for(volatile int j = 0; j < _ms; j++) _delay_ms(1);
		}
		for (int k=0;k<8;k++) // 7번째 핀에서 
		{
			PORTC = (~(0x80 >> k) & 0x0F);
			PORTD = (~(0x80 >> k) & 0xF0);
			for(volatile int j = 0; j < _ms; j++) _delay_ms(1);
		}
	}
	PORTD = (0xF0); // LED 모두 끄기
	PORTC = (0x0F); // LED 모두 끄기
}


void LED_ACROSS(int _n) {
	uint8_t pattern[] = {0x81, 0x42, 0x24, 0x18, 0x24, 0x42, 0x81};
	int n = sizeof(pattern) / sizeof(pattern[0]);
	unsigned int LED_MASK;
	
	for (int i=0; i<_n; i++)
	{
		for (int i = 0; i < n; i++) {
			LED_MASK = pattern[i];  // 패턴 출력
			PORTD = ~(LED_MASK & 0xF0) | (PORTD & 0x0F); // LED 상위 4 Bit 추출하여 4번 오른쪽으로 쉬프트
			PORTC = ~(LED_MASK & 0x0F) | (PORTC & 0xF0);		// LED 하위 4 Bit 추출
			_delay_ms(300);      // 간격 (조절 가능)
		}
	}
	PORTD |= (0xF0); // LED 모두 끄기
	PORTC |= (0x0F); // LED 모두 끄기
	
	// ((0x08) >> i) | ((0x08) < i) i = [0,1,2,3] [2,1,0]
}


void PORTsetting(void){
	DDRD = 0xF0; // 11110000 → PD3 ~ PD0 입력, PD4 ~ PD7 출력
	PORTD = 0xFF; // 11111111 → PD4~7[LED연결] 끄기, 입력핀은 모두 풀업저항
	DDRC = 0x0F; // 00011110 → PC1~4 모두 출력 설정
	PORTC = 0x0F; // PC와 연결된 LED 끄기
}

int main(void)
{
    PORTsetting();
	while (1) {
		if (!(PIND & 0x04)){ // PD2 읽기
			LED_ACROSS(2);
		}
		else if (!(PIND & 0x08)){ // PD3 읽기
			LED_ALL_ON_OFF(3, 1000);
		}
	}
}

