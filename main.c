/*
 * File:   main.c
 * Author: Karshan
 *
 * Created on October 11, 2013, 10:48 PM
 */


#include <xc.h>

// CONFIG
#pragma config FOSC = XT        // Oscillator Selection bits (XT oscillator)
#pragma config WDTE = OFF       // Watchdog Timer Enable bit (WDT disabled)
#pragma config PWRTE = OFF      // Power-up Timer Enable bit (PWRT disabled)
#pragma config BOREN = ON       // Brown-out Reset Enable bit (BOR enabled)
#pragma config LVP = OFF        // Low-Voltage (Single-Supply) In-Circuit Serial Programming Enable bit (RB3 is digital I/O, HV on MCLR must be used for programming)
#pragma config CPD = OFF        // Data EEPROM Memory Code Protection bit (Data EEPROM code protection off)
#pragma config WRT = OFF        // Flash Program Memory Write Enable bits (Write protection off; all program memory may be written to by EECON control)
#pragma config CP = OFF         // Flash Program Memory Code Protection bit (Code protection off)

unsigned char state[8] = {
    0x00,
    0x24,
    0x5a,
    0x24,
    0x24,
    0x5a,
    0x24,
    0x00
};

unsigned char ledc = 0;
unsigned char led_row = 0;
long int animc = 2000;
bit anim_flag;

void led_isr();
void anim_isr();

void interrupt isr() {
    PIR1 &= 0xfe;
    T1CON &= 0xfe;

    TMR1L = 0x0c;
    TMR1H = 0xfe;

    T1CON |= 0x01;

    if (ledc <= 0) {
        led_isr();
        ledc = 3;
    }

    if (animc <= 0) {
        anim_flag = 1;
        animc = 500;
    }
    ledc--;
    animc--;
}

void led_isr() {
    PORTD = 0xff;
    PORTC = state[led_row];
    PORTD = ~(1 << led_row);
    led_row = (led_row + 1) & 0x07;
}

unsigned char bitcount(unsigned char a) {
    unsigned char out = 0;
    for (int i = 0; i < 8; i++) {
        out += a & 0x01;
        a = a >> 1;
    }
    return out;
}

unsigned char get_neighbors(int i, int j) {
    unsigned char a = 0, b = 0, c = 0, mask;
    a = state[i] & (~(1 << j));
    if (i != 0) b = state[i-1];
    if (i != 7) c = state[i+1];

    if (j == 0) {
        mask = 0x03;
    } else if (j == 7) {
        mask = 0xc0;
    } else {
        mask = (1 << j) | (1 << (j + 1)) | (1 << (j - 1));
    }
    return bitcount(a & mask) + bitcount(b & mask) + bitcount(c & mask);
}

void anim_isr() {
    unsigned char out[8];
    for (int i = 0; i < 8; i++) {
        out[i] = state[i];
    }

    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            unsigned char neighbors = get_neighbors(i, j);
            if (state[i] & (1 << j)) {
                if (neighbors < 2 || neighbors > 3) {
                    out[i] = out[i] & ~(1 << j);
                }
            } else {
                if (neighbors == 3) {
                    out[i] = out[i] | (1 << j);
                }
            }
        }
    }

    for (int i = 0; i < 8; i++) {
        state[i] = out[i];
    }
}

void main(void) {
    TRISC = TRISD = 0;

    TMR1 = 0;
    T1CON = 0x01;

    PIE1 = 0x01;
    INTCON = 0xc0;

    while(1) {
        if (anim_flag == 1) {
            anim_isr();
            anim_flag = 0;
        }
    }
}
