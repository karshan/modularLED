#include "p16F877A.inc"
 __CONFIG _FOSC_XT & _WDTE_OFF & _PWRTE_OFF & _BOREN_ON & _LVP_OFF & _CPD_OFF & _WRT_OFF & _CP_OFF

var     UDATA
led_count res .1
led_row res .1
led_tmp res .1
led_tmp2 res .1
animation_count_H res .1
animation_count_L res .1
anim_row res .1
anim_col res .1
anim_tmp res .1
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

    ; turn on next row
    movfw led_row
    movwf led_tmp
    incf led_tmp, f
    movlw .1
    movwf led_tmp2
led_isr_LOOP1
    decfsz led_tmp, f
    goto led_isr_L1
    goto led_isr_L2
led_isr_L1
    rlf led_tmp2, f
    goto led_isr_LOOP1
led_isr_L2
    comf led_tmp2, f
    movfw led_tmp2
    movwf PORTD

    ; led_row = (led_row + 1) % 8
    incf led_row, f
    movlw 0x07
    andwf led_row, f
    return

animation_isr
    movlw .8
    movwf anim_row
    movwf anim_col
anim_LOOP
    movlw state
    addwf anim_row, w
    movwf FSR
    decf FSR, f
    movfw INDF  ; W = *(state + anim_row)

    movwf anim_tmp
    comf anim_tmp, f

    movlw state
    addwf anim_row, w
    movwf FSR
    decf FSR, f
    movfw anim_tmp
    movwf INDF  ; *(state + anim_row) = anim_tmp

    decfsz anim_row
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

