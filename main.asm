#include "p16F877A.inc"
 __CONFIG _FOSC_XT & _WDTE_OFF & _PWRTE_OFF & _BOREN_ON & _LVP_OFF & _CPD_OFF & _WRT_OFF & _CP_OFF

var     UDATA
t0 res .1
t1 res .1
t2 res .1
led_count res .1
led_row res .1
animation_count_H res .1
animation_count_L res .1
state equ 0x70

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

MAIN_PROG CODE
led_isr
    movlw 0xff
    movwf PORTD ; turn off all rows

    movlw state
    addwf led_row, w
    movwf FSR
    movfw INDF ; W = *(state + led_row)
    movwf PORTC

    ; PORTD = ~(1 << led_row)
    movfw led_row
    movwf t0
    incf t0, f
    movlw .1
    movwf t1
led_isr_LOOP1
    decfsz t0, f
    goto led_isr_L1
    goto led_isr_L2
led_isr_L1
    rlf t1, f
    goto led_isr_LOOP1
led_isr_L2
    comf t1, f
    movfw t1
    movwf PORTD

    ; led_row = (led_row + 1) % 8
    incf led_row, f
    movlw 0x07
    andwf led_row, f
    return

animation_isr
    movlw .8
    movwf t0
    movwf t1
anim_LOOP
    movlw state
    addwf t0, w
    movwf FSR
    decf FSR, f
    movfw INDF  ; W = *(state + t0)

    movwf t2
    comf t2, f

    movlw state
    addwf t0, w
    movwf FSR
    decf FSR, f
    movfw t2
    movwf INDF  ; *(state + t0) = t2

    decfsz t0
    goto anim_LOOP
    return

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
    clrf led_row

    banksel TRISD
    clrf TRISD
    clrf TRISC
    banksel PORTD
    movlw 0xfe
    movwf PORTD

    movlw 0x00
    movwf 0x70
    movlw 0x38
    movwf 0x71
    movlw 0x00
    movwf 0x72
    movlw 0x00
    movwf 0x73
    movlw 0x00
    movwf 0x74
    movlw 0x00
    movwf 0x75
    movlw 0x00
    movwf 0x76
    movlw 0x00
    movwf 0x77

    
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

