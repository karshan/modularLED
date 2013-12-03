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


#define STEPT 2000

unsigned char state[8] = {
#if 0
    0x80,
    0x40,
    0x20,
    0x10,
    0x10,
    0x20,
    0x40,
    0x80
#else
    0x01,
    0x02,
    0x04,
    0x08,
    0x08,
    0x04,
    0x02,
    0x01
#endif
};

unsigned char leds[8]; // for debugging

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
long int animc = 0, stepc = 0;
bit anim_flag = 0;
bit step_flag;

void led_isr();
void anim_isr();

unsigned char p[5]; // port cache

void update_ports() {
    PORTA = p[0];
    PORTB = p[1];
    PORTC = p[2];
    PORTD = p[3];
    PORTE = p[4];
}


void reset_comm() {
    for (int i = 0; i < 4; i++) {
        RX[i] = TX[i] = 1;
        comm[i].state = comm[i].rstate = comm[i].tstate = 0;
        nbrstate[i] = 0;
    }
}

void update_rx() {
    RX[0] = (PORTC & (1 << 7)) ? 1:0; // LEFT
    RX[1] = (PORTE & (1 << 1)) ? 1:0; // RIGHT
}

void update_tx() {
    if (TX[0]) {
        p[3] |= (1 << 4);
    } else {
        p[3] &= ~(1 << 4);
    }

    if (TX[1]) {
        p[4] |= (1 << 0);
    } else {
        p[4] &= ~(1 << 0);
    }
    update_ports();
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

    TMR1L = 0x2f;
    TMR1H = 0xf8;

    T1CON |= 0x01;

    if (--ledc <= 0) {
        led_isr();
        ledc = 3;
    }

    if (anim_flag == 0) {
        if (--animc <= 0) {
            anim_flag = 1;
        }
    }

    if (step_flag == 0) {
        if (--stepc <= 0) {
            step_flag = 1;
        }
    }

    update_rx();
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
    update_tx();
}

//cols: RD1 RB6 RB7 RC2 RA1 RC3 RD3 RC4
//rows: RC1 RD2 RA3 RD0 RB4 RA2 RB5 RA0

//cols: RD1 RB6 RB7 RC2 RA1 RC3 RD3 RC4
void set_cols(unsigned char cols) {
    if (cols & 0x80) {
        p[3] |= (1 << 1);
    } else {
        p[3] &= ~(1 << 1);
    }

    if (cols & 0x40) {
        p[1] |= (1 << 6);
    } else {
        p[1] &= ~(1 << 6);
    }

    if (cols & 0x20) {
        p[1] |= (1 << 7);
    } else {
        p[1] &= ~(1 << 7);
    }

    if (cols & 0x10) {
        p[2] |= (1 << 2);
    } else {
        p[2] &= ~(1 << 2);
    }

    if (cols & 8) {
        p[0] |= (1 << 1);
    } else {
        p[0] &= ~(1 << 1);
    }

    if (cols & 4) {
        p[2] |= (1 << 3);
    } else {
        p[2] &= ~(1 << 3);
    }

    if (cols & 2) {
        p[3] |= (1 << 3);
    } else {
        p[3] &= ~(1 << 3);
    }

    if (cols & 1) {
        p[2] |= (1 << 4);
    } else {
        p[2] &= ~(1 << 4);
    }
    update_ports();
}

//rows: RC1 RD2 RA3 RD0 RB4 RA2 RB5 RA0
void set_rows(unsigned char rows) {
    if (rows & 0x80) {
        p[2] |= (1 << 1);
    } else {
        p[2] &= ~(1 << 1);
    }

    if (rows & 0x40) {
        p[3] |= (1 << 2);
    } else {
        p[3] &= ~(1 << 2);
    }

    if (rows & 0x20) {
        p[0] |= (1 << 3);
    } else {
        p[0] &= ~(1 << 3);
    }

    if (rows & 0x10) {
        p[3] |= (1 << 0);
    } else {
        p[3] &= ~(1 << 0);
    }

    if (rows & 8) {
        p[1] |= (1 << 4);
    } else {
        p[1] &= ~(1 << 4);
    }

    if (rows & 4) {
        p[0] |= (1 << 2);
    } else {
        p[0] &= ~(1 << 2);
    }

    if (rows & 2) {
        p[1] |= (1 << 5);
    } else {
        p[1] &= ~(1 << 5);
    }

    if (rows & 1) {
        p[0] |= (1 << 0);
    } else {
        p[0] &= ~(1 << 0);
    }
    update_ports();
}

void led_isr() {
    set_rows(0xff);
    set_cols(state[led_row]);
    /*if (led_row == 0 || led_row == 1) {
        set_cols(leds[led_row]);
    }*/
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
#if 0
    leds[1] = comm[0].tbuf;
#else
    leds[1] = comm[1].tbuf;
#endif
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

    ANSELA = ANSELB = ANSELC = ANSELD = ANSELE = 0;

    TRISA = TRISB = TRISD = 0;
    TRISC = 0x80;
    TRISE = 0x02;

    TMR1 = 0;
    T1CON = 0x01;

    PIE1 = 0x01;
    INTCON = 0xc0;

    reset_comm();

    unsigned char mstate = 0;
    unsigned char tmp = 5;
    unsigned char ls, rs;
    
    // dont wait for button
#if 0
    mstate = 0;
#else
    __delay_ms(500);
    tx_all(0xf0);
    mstate = 1;
    animc = STEPT;
    stepc = STEPT * 4;
    step_flag = 0;
    anim_flag = 0;
#endif
    while(1) {
        // comm loop
        for (int i = 0; i < 2; i++) {
            if (comm[i].rstate == 3) { // received a byte on the comm
                leds[0] = comm[i].rbuf;

                if (comm[i].rbuf == 0xef && (stepc > STEPT*2)) {
                    stepc = STEPT * 4;
                    step_flag = 0;
                }

                if (mstate == 0) {
                    if (comm[i].rbuf == 0xf0) {
                        mstate = 1;
                        animc = STEPT - 160;
                        stepc = STEPT * 4 - 160;
                        step_flag = 0;
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
#if 0
            if((PORTD & 0x10) == 0) { // go button pressed
                tx_all(0xf0);
                mstate = 1;

                animc = STEPT;
                anim_flag = 0;
            }
#endif
        } else if (mstate == 1) {
            if (anim_flag) {
                animc = STEPT;
                anim_flag = 0;
                mstate = 2;
            }
        } else if (mstate == 2) {
            if (anim_flag) {
                ls = col_state(7);
                rs = col_state(0);
                tx(0, (ls & 0x0f) | 0x10);
                tx(1, (rs & 0x0f) | 0x10);

                animc = STEPT;
                anim_flag = 0;
                mstate = 3;
            }
        } else if (mstate == 3) {
            if (anim_flag) {
                tx(0, ((ls >> 4) & 0x0f) | 0x20);
                tx(1, ((rs >> 4) & 0x0f) | 0x20);

                animc = STEPT;
                anim_flag = 0;
                mstate = 4;
            }
        } else if (mstate == 4) {
            if (step_flag) {
                tx_all(0xef);
                stepc = STEPT * 4;
                step_flag = 0;

                anim_isr();
                animc = STEPT;
                anim_flag = 0;
                mstate = 1;
            }
        }
    }
}
