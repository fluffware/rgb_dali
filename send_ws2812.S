#include <avr/io.h>
	
	.global send_ws2812_s
	shift_table = 0x1928

	port = _SFR_IO_ADDR(PORTD)
	bit = 5

	;; X (r26:r27) GRB values
	;; r19:r18 length in bytes
send_ws2812_s:


	;;  0: H: 0.35us L: 0.8us
	;; 	  6	    14
	;;  1: H: 0.7us  L: 0.6us
	;; 	  11	    9

	push r16
	push r17
	;; Get number of bits
	add r18,r18
	adc r19,r19
	add r18,r18
	adc r19,r19
	add r18,r18
	adc r19,r19
	
reload:
	sbi port,bit		 ; 2	+6
	ld r16, x+		 ; 2	+8
next_bit:
	lsl r16			; 1	+10
	brcs one		; 1/2	+11
	cbi port,bit		 ; 2	+12
	nop			   ;   	+14
	nop
	nop
	rjmp  check_loop	   ; 2	+17

one:
	nop			;	+13
	nop
	nop
	nop
	cbi port,bit		 ; 2	+17
check_loop:

	subi r18, 1		; 1	+19
	sbci r19, 0		; 1	+0
	breq end		; 1/2	+1
	mov r17,r18		; 1	+2
	andi r17, 0x07		; 1	+3
	breq reload		; 1/2	+4
	nop			; 1	+5
	sbi port,bit		 ; 2	+6
	rjmp next_bit		; 2	+8
	
end:
	nop			; 1	+3
	nop
	nop
	sbi port,bit		; 1	+6
	nop
	nop
	nop
	nop
	nop
	cbi port,bit		 ; 2
	// wait 50us		 
	ldi r17,200
reset_wait:
	nop
	dec r17	
	brne reset_wait
	pop r17
	pop r16
	ret
	

