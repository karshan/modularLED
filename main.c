/*
 * File:   main.c
 * Author: Karshan
 *
 * Created on October 11, 2013, 10:48 PM
 */


#include <xc.h>

#define _XTAL_FREQ 16000000

// CONFIG
#pragma config FOSC = HS        // Oscillator Selection bits
#pragma config WDTE = OFF       // Watchdog Timer Enable bit (WDT disabled)
#pragma config PWRTE = OFF      // Power-up Timer Enable bit (PWRT disabled)
#pragma config BOREN = ON       // Brown-out Reset Enable bit (BOR enabled)
#pragma config LVP = OFF        // Low-Voltage (Single-Supply) In-Circuit Serial Programming Enable bit (RB3 is digital I/O, HV on MCLR must be used for programming)
//#pragma config CPD = OFF        // Data EEPROM Memory Code Protection bit (Data EEPROM code protection off)
#pragma config WRT = OFF        // Flash Program Memory Write Enable bits (Write protection off; all program memory may be written to by EECON control)
#pragma config CP = OFF         // Flash Program Memory Code Protection bit (Code protection off)

unsigned char state[8] = {
    0x00,
    0x03,
    0x00,
    0xe0,
    0x00,
    0x07,
    0x01,
    0x02
};

//unsigned char leds[8]; // for debugging

unsigned char nbrstate[4];

struct {
    unsigned char state;
    unsigned char rstate;
    unsigned char tstate;
    unsigned char rbuf;
    unsigned char tbuf;
    int ri; // bit index into rbuf
    int ti; // bit index into tbuf
    int rc; // counter for when to sample
    int tc; // counter for when to transmit
}comm[4]; // L R U D
unsigned char RX[4] = {1, 1, 1, 1}; // buffer actual pins for code simplicity
unsigned char TX[4] = {1, 1, 1, 1};

unsigned char ledc = 0;
unsigned char led_row = 0;
long int animc = 0;
bit anim_flag = 0;

void led_isr();
void anim_isr();

void reset_comm() {
    for (int i = 0; i < 4; i++) {
        RX[i] = TX[i] = 1;
        comm[i].state = comm[i].rstate = comm[i].tstate = 0;
        nbrstate[i] = 0;
    }
}

unsigned char col_state(int col) {
    unsigned char out = 0;
    for (int i = 0; i < 8; i++) {
        out |= (((state[i] & (1 << col)) >> col) << i);
    }
    return out;
}

// left: TX  RX   right: TX RX
//       RC7 RC6         RC5 RC4
void interrupt isr() {
    PIR1 &= 0xfe;
    T1CON &= 0xfe;

    TMR1L = 0x0c;
    TMR1H = 0xfe;

    T1CON |= 0x01;

    if (--ledc <= 0) {
        led_isr();
        ledc = 2;
    }

    if (anim_flag == 0) {
        if (--animc <= 0) {
            anim_flag = 1;
        }
    }

    RX[0] = 1;//(PORTC & 0x40) ? 1:0;
    RX[1] = 1;//(PORTC & 0x10) ? 1:0;
    // TODO rx buffering (multiple bytes)
    for (int i = 0; i < 2; i++) {
        if (comm[i].rstate == 0) {
            if (RX[i] == 0) { // see a start bit
                comm[i].rstate = 1;
                comm[i].rc = 24;
                comm[i].ri = 0;
                comm[i].rbuf = 0;
            }
        } else if (comm[i].rstate == 1) {
            if (--comm[i].rc <= 0) {
                comm[i].rbuf |= RX[i] << comm[i].ri;
                comm[i].rc = 16;
                comm[i].ri++;
                if (comm[i].ri >= 8) {
                    comm[i].rstate = 2;
                }
            }
        } else if (comm[i].rstate == 2) {
            if (RX[i] == 1) { // wait for end bit
                comm[i].rstate = 3;
            }
        }

        if (comm[i].tstate == 1) {
            if (--comm[i].tc <= 0) {
                if (comm[i].ti == 0) {
                    TX[i] = 0; //start
                } else {
                    TX[i] = (comm[i].tbuf & (1 << (comm[i].ti - 1))) ? 1:0;
                }
                comm[i].ti++;
                comm[i].tc = 16;
                if (comm[i].ti >= 9) {
                    comm[i].tstate = 2;
                }
            }
        } else if (comm[i].tstate == 2) {
            if (--comm[i].tc <= 0) {
                TX[i] = 1; //end
                comm[i].tstate = 0;
            }
        }
    }
    /*if (TX[1]) {
        TRISC |= 0x20;
    } else {
        TRISC &= 0xdf;
        PORTC &= 0xdf;
    }
    if (TX[0]) {
        TRISC |= 0x80;
    } else {
        TRISC &= 0x7f;
        PORTC &= 0x7f;
    }*/
}

//cols: RA2 RA5 RE0 RD3 RE1 RA3 RA1 RA0
//rows: RE2 RC0 RD1 RC1 RC2 RD0 RC3 RD2

//cols: RD1 RB6 RB7 RC2 RA1 RC3 RD3 RC4
//rows: RC1 RD2 RA3 RD0 RB4 RA2 RB5 RA0
unsigned char rows = 0, cols = 0;

void update_rowscols() {
    PORTA = (((cols & 8) ? 1:0) << 1) |
            (((rows & 4) ? 1:0) << 2) |
            (((rows & 0x20) ? 1:0) << 3) |
            (((rows & 1) ? 1:0) << 0);
    PORTC = (((cols & 0x10) ? 1:0) << 2) |
            (((cols & 4) ? 1:0) << 3) |
            (((cols & 1) ? 1:0) << 4) |
            (((rows & 0x80) ? 1:0) << 1);
    PORTB = (((cols & 0x40) ? 1:0) << 6) |
            (((cols & 0x20) ? 1:0) << 7) |
            (((rows & 2) ? 1:0) << 5) |
            (((rows & 8) ? 1:0) << 4);
    PORTD = (((cols & 0x80) ? 1:0) << 1) |
            (((cols & 2) ? 1:0) << 3) |
            (((rows & 0x40) ? 1:0) << 2) |
            (((rows & 0x10) ? 1:0) << 0);
}

void set_cols(unsigned char a) {
    cols = a;
    update_rowscols();
}

void set_rows(unsigned char a) {
    rows = a;
    update_rowscols();
}

void led_isr() {
    set_rows(0xff);
    set_cols(state[led_row]);
    set_rows(~(1 << led_row));
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
    unsigned char out;
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
    out = bitcount(a & mask) + bitcount(b & mask) + bitcount(c & mask);

    if (j == 0) {
        out += ((nbrstate[1] & (1 << i)) ? 1:0);
        if (i > 0) {
            out += ((nbrstate[1] & (1 << (i-1))) ? 1:0);
        }
        if (i < 7) {
            out += ((nbrstate[1] & (1 << (i+1))) ? 1:0);
        }
    }
    if (j == 7) {
        out += ((nbrstate[0] & (1 << i)) ? 1:0);
        if (i > 0) {
            out += ((nbrstate[0] & (1 << (i-1))) ? 1:0);
        }
        if (i < 7) {
            out += ((nbrstate[0] & (1 << (i+1))) ? 1:0);
        }
    }

    return out;
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

void tx(int c, unsigned char buf) {
    while(comm[c].tstate != 0) {} // wait for comm to finish txing last byte

    comm[c].tbuf = buf;
    comm[c].ti = 0;
    comm[c].tstate = 1;
}

void tx_all(unsigned char buf) {
    for (int i = 0; i < 2; i++) {
        tx(i, buf);
    }
}

void main(void) {
//cols: RD1 RB6 RB7 RC2 RA1 RC3 RD3 RC4
//rows: RC1 RD2 RA3 RD0 RB4 RA2 RB5 RA0
// left: TX  RX   right: TX RX
//       RC7 RC6         RC5 RC4
// switch: RD4

    /*    TRISA = TRISE = 0;
    TRISC = 0x50;
    TRISD = 0x10;*/

    OSCCON = 0x7a;

    TRISA = TRISB = TRISC = TRISD = 0;
 
    TMR1 = 0;
    T1CON = 0x01;

    PIE1 = 0x01;
    INTCON = 0xc0;

    reset_comm();

    unsigned char mstate = 0;
    unsigned char tmp = 5;
    unsigned char ls, rs;

    // dont wait for button
    mstate = 1;
    while(1) {
        // comm loop
        for (int i = 0; i < 2; i++) {
            if (comm[i].rstate == 3) { // received a byte on the comm 
                if (mstate == 0) {
                    if (comm[i].rbuf == 0xf0) {
                        mstate = 1;
                        animc = 500;
                        anim_flag = 0;
                    }
                }
                if ((comm[i].rbuf & 0xf0) == 0x10) {
                    nbrstate[i] = comm[i].rbuf & 0x0f;
                } else if ((comm[i].rbuf & 0xf0) == 0x20) {
                    nbrstate[i] |= (comm[i].rbuf << 4);
                }

                comm[i].rstate = 0; // acknowledge so interrupt driver can read more bytes
            }
        }

        if (mstate == 0) {
            if((PORTD & 0x10) == 0) { // go button pressed
                tx_all(0xf0);
                mstate = 1;

                animc = 500;
                anim_flag = 0;
            }
        } else if (mstate == 1) {
            if (anim_flag) {
                animc = 500;
                anim_flag = 0;
                mstate = 2;
            }
        } else if (mstate == 2) {
            if (anim_flag) {
                ls = col_state(7);
                rs = col_state(0);
                tx(0, (ls & 0x0f) | 0x10);
                tx(1, (rs & 0x0f) | 0x10);

                animc = 500;
                anim_flag = 0;
                mstate = 3;
            }
        } else if (mstate == 3) {
            if (anim_flag) {
                tx(0, ((ls >> 4) & 0x0f) | 0x20);
                tx(1, ((rs >> 4) & 0x0f) | 0x20);

                animc = 500;
                anim_flag = 0;
                mstate = 4;
            }
        } else if (mstate == 4) {
            if (anim_flag) {
                anim_isr();
                animc = 500;
                anim_flag = 0;
                mstate = 1;
            }
        }
    }
}
