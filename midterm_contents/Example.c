#include <avr/io.h>
#define F_CPU 16000000UL
#define FOSC 16000000UL
#include <util/delay.h>
#include <avr/interrupt.h>

/*
 * 개요
 * - 입력: PINK의 하위 3비트(K0,K1,K2, 즉 PINK&0x07). PCINT2 그룹(Port K)에 핀체인지 인터럽트 사용.
 *   * K0(비트0): 'inverse' 모드 토글 스위치(짧게 누를 때 단계 증가, 0→1→2→0).
 *   * K1(비트1): 마스크 set_num = 2로 설정(하위 4비트만 사용).
 *   * K2(비트2): 마스크 set_num = 1로 설정(상위 4비트만 사용).
 *   * K1,K2를 모두 떼면(set_num 임시로 2/1 쓰다가) set_num = 0(전체 8비트)로 복귀.
 *
 * - 출력: PORTA(하위 8개), PORTD의 상위 4개(PD4~PD7). 액티브-로우 LED 가정.
 * - 표시 데이터 경로: led_data → (inverse/특수조건에 따른 반전) → set[set_num] 마스킹 → 최종 반전 → 포트 출력
 *
 * - inverse 모드:
 *   inverse == 0 : 기본 반전 출력(액티브-로우 보정용)  ← set_led()의 첫 반전(if(inverse==0))
 *   inverse == 1 : 기본 반전을 하지 않음
 *   inverse == 2 : 특별 모드 (ISR 내부 루프에서 K0/K1/K2 의 '누름-뗌' 시퀀스를 감지해 일회성 동작 수행)
 *
 * 주의
 * - ISR 내부에서 sei()로 중첩 인터럽트를 허용하고 있음 → 바운스/재진입으로 복잡해질 수 있음.
 * - check1/2/3 플래그는 '누름-뗌' 시퀀스(펄스)를 감지해서 한 번만 동작시키려는 용도.
 */

static inline void custom_delay(unsigned int t){
    for (int i = 0; i < (int)t; i++) _delay_ms(1);
}

/* LED 패턴(액티브-로우 가정이라 ~처리된 값 사용)
 * arr[]는 메인 루프에서 순환 재생되는 4개 프레임
 */
unsigned char arr[4]   = { ~0x81, ~0x18, ~0x42, ~0x24 };

/* 출력 마스크 세트
 * set[0]=0xFF (전체 8비트), set[1]=0xF0 (상위 4비트만), set[2]=0x0F (하위 4비트만)
 * set_led()에서 AND로 걸어 필요한 쪽만 켬
 */
unsigned char set[3]   = { 0xFF, 0xF0, 0x0F };
unsigned int  set_num  = 0;  // 0:전체, 1:상위4, 2:하위4

/* 표시용 데이터와 변형 버퍼 */
unsigned char led_data = 0x01;
unsigned char led_data_ifinverse;

/* inverse 모드 상태(0/1/2 순환) */
unsigned char inverse = 0;

/* 디버그/지연 등 */
int delay_time = 0;

/* 직전 입력 스냅샷(핀체인지 에지 검출용) */
unsigned char prev_in = 0xFF;

/* 특별 모드(inverse==2)에서 각 스위치의 '완전한 클릭(누름→뗌)'을 감지하기 위한 플래그 */
int check1 = 0; // K0(비트0) 클릭 시퀀스 상태
int check2 = 0; // K1(비트1) 클릭 시퀀스 상태
int check3 = 0; // K2(비트2) 클릭 시퀀스 상태

/* 실제 포트에 값을 내보내는 함수
 * - inverse==0이면 먼저 전체 반전(액티브-로우 보정)
 * - check2/3==2(완전 클릭됨)면 일시적으로 한 번 더 반전(특수 효과)
 * - set[set_num]으로 마스킹
 * - 마지막에 한 번 더 반전해서 액티브-로우 LED로 출력(결과적으로 '반전 횟수' 조합으로 패턴이 바뀜)
 */
void set_led(void){
    led_data_ifinverse = led_data;

    // 기본 반전: inverse==0 (액티브-로우 보정 겸 기본 모드)
    if (inverse == 0) led_data_ifinverse = ~led_data_ifinverse;

    // 특별 클릭 효과: K1/K2가 '누르고 떼는' 완전한 클릭 시퀀스를 완료하면 한 번 더 뒤집기
    if (check2 == 2) led_data_ifinverse = ~led_data_ifinverse;
    if (check3 == 2) led_data_ifinverse = ~led_data_ifinverse;

    // 마스크 적용 (상/하위 4비트만 쓰거나 전체 8비트)
    led_data_ifinverse = led_data_ifinverse & set[set_num];

    // 최종 반전(액티브-로우 LED 구동을 위해 0이 켬)
    led_data_ifinverse = ~led_data_ifinverse;

    // 포트 출력: PORTA(하위 8비트), PORTD 상위 4비트(PD4~PD7)
    PORTA = led_data_ifinverse;
    PORTD = led_data_ifinverse >> 4;
}

/* Port K(PCINT2 그룹) 핀체인지 인터럽트
 * - 눌림/뗌의 '에지'를 prev_in과 비교해서 감지
 * - K0: inverse 모드 0→1→2→0 순환
 * - K1 눌림: set_num=2(하위4), K2 눌림: set_num=1(상위4)
 * - K1,K2 모두 떼면 set_num=0(전체8) 복귀
 * - inverse==2일 때는 루프 돌면서 K0/K1/K2의 '완전 클릭(누름→뗌)'을 개별 감지해서
 *   일회성 set_led() 호출로 효과를 내고, K0 클릭 완성되면 inverse=0으로 복귀
 */
ISR(PCINT2_vect)
{
    // 짧은 크리티컬 섹션: 입력 샘플링만 원자적으로
    cli();
    unsigned char in = PINK & 0x07;   // K0,K1,K2만 사용
    sei();

    // K0: 1→0 하강 에지(누름)에서 inverse 상태 순환
    if (!(in & 0x01) && (prev_in & 0x01)) {
        inverse = (inverse + 1) % 3;  // 0→1→2→0
    }

    // K1: 0→1 상승 에지(뗌)에서 하위 4비트 모드
    if ((in & 0x02) && !(prev_in & 0x02)) {
        set_num = 2; // 0x0F
    }
    // K2: 0→1 상승 에지(뗌)에서 상위 4비트 모드
    if ((in & 0x04) && !(prev_in & 0x04)) {
        set_num = 1; // 0xF0
    }

    // 직전에 K1=0, K2=1 이었고 → 현재 K1=0,K2=0 이 되면 전체 모드로 복귀 (특정 패턴용)
    if (!(prev_in & 0x02) && (prev_in & 0x04)) {
        if (!(in & 0x02) && !(in & 0x04)) {
            set_num = 0; // 0xFF
        }
    }

    // inverse==2: '특수 시퀀스 모드' 진입
    if (inverse == 2) {
        unsigned char prev = in;

        while (check1 != 2) { // K0 클릭(누름→뗌) 완성될 때까지 루프
            in = PINK & 0x07;

            // K1 완전 클릭(누름→뗌) 감지 시: set_num=2 적용 후 즉시 렌더링 1회
            if (check2 == 2) {
                set_num = 2;
                set_led();
                check2 = 0; // 소모
                }
            // K2 완전 클릭(누름→뗌) 감지 시: set_num=1 적용 후 즉시 렌더링 1회
            if (check3 == 2) {
                set_num = 1;
                set_led();
                check3 = 0; // 소모
            }

            // K0: 하강 에지(누름) → 상태1
            if ((!(in & 0x01)) && (prev & 0x01)) {
                check1 = 1;
            }
            // K0: 상승 에지(뗌) & 이전에 눌림이 있었으면 → 클릭 완성(상태2), inverse 종료
            if (((in & 0x01) && !(prev & 0x01))) {
                if (check1 == 1) {
                    inverse = 0;  // 기본 모드 복귀
                    check1  = 2;  // 루프 탈출 조건
                }
            }

            // K1 클릭 시퀀스(누름→뗌) 추적
            if ((!(in & 0x02)) && (prev & 0x02)) {
                check2 = 1; // 누름 감지
            }
            if (((in & 0x02) && !(prev & 0x02))) {
                if (check2 == 1) {
                    check2 = 2; // 클릭 완성 플래그
                }
            }

            // K2 클릭 시퀀스(누름→뗌) 추적
            if ((!(in & 0x04)) && (prev & 0x04)) {
                check3 = 1; // 누름 감지
            }
            if (((in & 0x04) && !(prev & 0x04))) {
                if (check3 == 1) {
                    check3 = 2; // 클릭 완성 플래그
                }
            }

            prev = in; // 다음 에지 검출 대비
        }
        // 여기서 inverse는 0으로 돌아왔고, while이 종료됨
    }

    // 현재 입력을 스냅샷으로 저장(다음 에지 비교용)
    prev_in = in;

    // PCINT2 인터럽트 플래그 클리어(쓰기 1로 클리어)
    PCIFR |= 0x04;
}

int main(void)
{
    cli();

    /* 핀체인지 인터럽트(PCINT2: Port K) 설정 */
    PCICR  = 0x04; // PCIE2 enable
    PCIFR  = 0x04; // PCIF2 플래그 클리어
    PCMSK2 = 0x07; // K0,K1,K2만 인터럽트 감지

    /* 출력 포트: PORTA[7:0], PORTD[7:4] 사용 */
    DDRD = 0xFF;   // PD0~PD7 출력(여기선 상위4만 사용)
    DDRA = 0xFF;   // PA0~PA7 출력

    /* 입력 포트: PORTK[2:0] 입력 (풀업 여부는 외부 회로에 맞게 필요 시 PORTK에 1 세팅) */
    DDRK &= ~(0x07); // K0,K1,K2 입력

    set_led(); // 초기 프레임 출력

    sei();

    /* 메인 루프: arr[]의 4패턴을 왕복(0→3→0) 순환 재생 */
    while (1) {
        for (int i = 0; i < 4; i++) {
            led_data = arr[i];
            set_led();
            custom_delay(250);
        }
        for (int i = 0; i < 3; i++) { // 끝점 중복 방지(3을 두 번 쓰지 않도록 3→0 사이 3 제외)
            led_data = arr[3 - i];
            set_led();
            custom_delay(250);
        }
    }
}
