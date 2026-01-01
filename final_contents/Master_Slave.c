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

/*****************************************************************
   UART SUPPORT
******************************************************************/
void tx_char(unsigned char txChar){
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = txChar;
}

void Printf(char *fmt, ...) {
    va_list arg_ptr;
    char sText[128] = {0};
    va_start(arg_ptr, fmt);
    vsprintf(sText, fmt, arg_ptr);
    va_end(arg_ptr);

    for (uint8_t i = 0; sText[i]; i++)
        tx_char(sText[i]);
}

char rx_char(void){
    while(!(UCSR0A & (1<<RXC0)));
    return UDR0;
}

void UARTSetting(){
    UBRR0H = (unsigned char)(BAUD_RATE>>8);
    UBRR0L = (unsigned char)BAUD_RATE;
    UCSR0A = 0x00;
    UCSR0B = (1<<TXEN0)|(1<<RXEN0);
    UCSR0C = (1<<UCSZ01)|(1<<UCSZ00);
}

/*****************************************************************
   I2C MASTER CONSTANTS
******************************************************************/
#define START_ACK                     0x08
#define REP_START_ACK                 0x10

#define MASTER_SEND_SLA_W_RECEIVE_ACK     0x18
#define MASTER_SEND_SLA_W_RECEIVE_NACK    0x20
#define MASTER_SEND_DATA_RECEIVE_ACK      0x28
#define MASTER_SEND_DATA_RECEIVE_NACK     0x30

#define MASTER_SEND_SLA_R_RECEIVE_ACK     0x40
#define MASTER_SEND_SLA_R_RECEIVE_NACK    0x48
#define MASTER_RECEIVE_DATA_SEND_ACK      0x50
#define MASTER_RECEIVE_DATA_SEND_NACK     0x58

/*****************************************************************
   I2C SLAVE CONSTANTS
******************************************************************/
#define SLAVE_RECEIVE_SLA_W_SEND_ACK       0x60
#define SLAVE_RECEIVE_DATA_SEND_ACK        0x80
#define SLAVE_RECEIVE_STOP_OR_RESTART      0xA0

#define SLAVE_RECEIVE_SLA_R_SEND_ACK       0xA8
#define SLAVE_SEND_DATA_RECEIVE_ACK        0xB8
#define SLAVE_SEND_DATA_RECEIVE_NACK       0xC0
#define SLAVE_SEND_LAST_DATA_RECEIVE_ACK   0xC8

/*****************************************************************
   GLOBAL VARIABLES
******************************************************************/
volatile char PRESENT_STATE = 1;
volatile char temporary_data = 0;

/*****************************************************************
   I2C MASTER FUNCTIONS
******************************************************************/
void TWI_Master_Init(void){
    TWBR = 72;
    TWSR = 0x00;
}

uint8_t TWI_Start(void){
    TWCR = (1<<TWINT)|(1<<TWSTA)|(1<<TWEN);
    while(!(TWCR & (1<<TWINT)));
    return (TWSR & 0xF8);
}

void TWI_Stop(void){
    TWCR = (1<<TWINT)|(1<<TWSTO)|(1<<TWEN);
}

uint8_t TWI_Write_Address(uint8_t address){
    TWDR = address;
    TWCR = (1<<TWINT)|(1<<TWEN);
    while(!(TWCR & (1<<TWINT)));
    return (TWSR & 0xF8);
}

uint8_t TWI_Write_Data(uint8_t data){
    TWDR = data;
    TWCR = (1<<TWINT)|(1<<TWEN);
    while(!(TWCR & (1<<TWINT)));
    return (TWSR & 0xF8);
}

uint8_t TWI_Read_Data_NACK(void){
    TWCR = (1<<TWINT)|(1<<TWEN);
    while(!(TWCR & (1<<TWINT)));
    return TWDR;
}

/*****************************************************************
   MASTER → SLAVE WRITE
******************************************************************/
void I2C_Transmit(char tx){
    if(TWI_Start()==START_ACK){
        if(TWI_Write_Address((SLAVE_ADDR<<1)|0)==MASTER_SEND_SLA_W_RECEIVE_ACK){
            if(TWI_Write_Data(tx)==MASTER_SEND_DATA_RECEIVE_ACK){
                Printf("Master sent: %d\r\n", tx);
            }
        }
    }
    TWI_Stop();
    _delay_ms(5);
}

/*****************************************************************
   MASTER ← SLAVE READ
******************************************************************/
char I2C_Receive(void){
    char rx = 0;
    if(TWI_Start()==START_ACK){
        if(TWI_Write_Address((SLAVE_ADDR<<1)|1)==MASTER_SEND_SLA_R_RECEIVE_ACK){
            rx = TWI_Read_Data_NACK();
        }
    }
    Printf("Master received: %d\r\n", rx);
    TWI_Stop();
    return rx;
}

/*****************************************************************
   I2C SLAVE INIT
******************************************************************/
void TWI_Slave_Init(uint8_t addr){
    TWAR = addr<<1;
    TWCR = (1<<TWINT)|(1<<TWEA)|(1<<TWEN)|(1<<TWIE);
}

/*****************************************************************
   UART INIT
******************************************************************/
void UARTSetting(){
    UBRR0H = (unsigned char)(BAUD_RATE>>8);
    UBRR0L = (unsigned char)BAUD_RATE;
    UCSR0A = 0x00;
    UCSR0B = (1<<TXEN0)|(1<<RXEN0);
    UCSR0C = (1<<UCSZ01)|(1<<UCSZ00);
}

/*****************************************************************
   SLAVE ISR  (핵심)
******************************************************************/
ISR(TWI_vect){
    uint8_t status = TWSR & 0xF8;

    switch(status){

        case SLAVE_RECEIVE_SLA_W_SEND_ACK:
            TWCR = (1<<TWINT)|(1<<TWEA)|(1<<TWEN)|(1<<TWIE);
            break;

        case SLAVE_RECEIVE_DATA_SEND_ACK:
            temporary_data = PRESENT_STATE;
            PRESENT_STATE = TWDR;
            TWCR = (1<<TWINT)|(1<<TWEA)|(1<<TWEN)|(1<<TWIE);
            break;

        case SLAVE_RECEIVE_STOP_OR_RESTART:
            TWCR = (1<<TWINT)|(1<<TWEA)|(1<<TWEN)|(1<<TWIE);
            break;

        case SLAVE_RECEIVE_SLA_R_SEND_ACK:
            TWDR = temporary_data;
            TWCR = (1<<TWINT)|(1<<TWEA)|(1<<TWEN)|(1<<TWIE);
            break;

        case SLAVE_SEND_DATA_RECEIVE_ACK:
            TWCR = (1<<TWINT)|(1<<TWEN)|(1<<TWIE);
            break;

        case SLAVE_SEND_DATA_RECEIVE_NACK:
        case SLAVE_SEND_LAST_DATA_RECEIVE_ACK:
            TWCR = (1<<TWINT)|(1<<TWEA)|(1<<TWEN)|(1<<TWIE);
            break;

        default:
            TWCR = (1<<TWINT)|(1<<TWEA)|(1<<TWEN)|(1<<TWIE);
            break;
    }
}

/*****************************************************************
   MAIN LOOP
******************************************************************/
int main(void){
    char uart_recv, rx;

    UARTSetting();
    TWI_Master_Init();
    TWI_Slave_Init(SLAVE_ADDR);
    sei();

    Printf("Combined MASTER + SLAVE Start!\r\n");

    while(1){
        uart_recv = rx_char();

        if(uart_recv>='0' && uart_recv<='3'){
            I2C_Transmit(uart_recv-'0');
            rx = I2C_Receive();

            Printf("Before: %d\r\n", rx);
            Printf("After : %c\r\n", uart_recv);
        }
        else Printf("Input 0~3\r\n");
    }
}
