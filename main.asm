#include "p16F877A.inc"
 __CONFIG _FOSC_XT & _WDTE_OFF & _PWRTE_OFF & _BOREN_ON & _LVP_OFF & _CPD_OFF & _WRT_OFF & _CP_OFF

var     UDATA
d1 res .1
d2 res .2
led_count res .1
animation_count_H res .1
animation_count_L res .1
disp res .1

res_vect    CODE    0x0000
    goto main

isr       CODE    0x004
    bcf PIR1,0 ; Clear the flag
    bcf T1CON,0 ; Turn the timer off.
    call reset_tmr
    bsf T1CON,0 ; Turn the timer on.

    decfsz led_count
    goto isr_s1
    call led_isr
    movlw .3
    movwf led_count

isr_s1
    decfsz animation_count_L
    goto isr_s2
    decfsz animation_count_H
    goto isr_s2
    call animation_isr
    movlw .4
    movwf animation_count_H

isr_s2
    retfie

led_isr
    movfw disp
    movwf PORTC
    clrf PORTD
    return

animation_isr
    movlw 0xff
    xorwf disp
    return

MAIN_PROG CODE

reset_tmr
    ; Set TIMER1's register to -500 decimal (will overflow in 500us)
    movlw 0xfe
    movwf TMR1H
    movlw 0x0C
    movwf TMR1L
    return

main
    movlw .1
    movwf led_count
    movwf animation_count_L
    movwf animation_count_H

    banksel TRISD
    clrf TRISD
    clrf TRISC
    banksel PORTD
    clrf PORTD
    movlw 0xff
    movwf PORTC

    bcf T1CON,TMR1ON ; Turn Timer 1 off.
    bcf T1CON,T1CKPS1 ; Set prescaler for divide
    bcf T1CON,T1CKPS0 ; by 1.
    bcf T1CON,T1OSCEN ; Disable the RC oscillator.
    bcf T1CON,TMR1CS ; Use the Fosc/4 source.
    clrf TMR1L ; Start timer at 0000h
    clrf TMR1H ;
    bsf T1CON,TMR1ON ; Start the timer

    banksel PIE1
    movlw 0x01
    movwf PIE1
    movlw 0xC0
    movwf INTCON
    banksel PORTD

die
    goto die
    END

