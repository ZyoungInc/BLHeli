;**** **** **** **** ****
;
; BLHeli program for controlling brushless motors in helicopters and multirotors
;
; Copyright 2011, 2012 Steffen Skaug
; This program is distributed under the terms of the GNU General Public License
;
; This file is part of BLHeli.
;
; BLHeli is free software: you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation, either version 3 of the License, or
; (at your option) any later version.
;
; BLHeli is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with BLHeli.  If not, see <http://www.gnu.org/licenses/>.
;
;**** **** **** **** ****
;
; Mini BLS 8A: EFM8BB10F8G QFN20, derived from layout T.
; Internal A = PCB C, internal B = PCB B, internal C = PCB A.
; Six gate-control pins are active high. P1.5 is not a drive pin.
; This target uses low-side PWM only; Damped Light is disabled.
;
;**** **** **** **** ****



;*********************
; Device SiLabs EFM8BB1x/2x
;*********************
IF MCU_48MHZ == 0
$include (SI_EFM8BB1_Defs.inc)
ELSE
$include (SI_EFM8BB2_Defs.inc)
ENDIF


;**** **** **** **** ****
; Uses internal calibrated oscillator set to 24/48Mhz
;**** **** **** **** ****

;**** **** **** **** ****
; Constant definitions
;**** **** **** **** ****
IF MCU_48MHZ == 0
CSEG AT 1A40h
Eep_ESC_Layout:		DB	"#MB8_L_01#      "	; Mini BLS 8A non-damped layout
CSEG AT 1A50h
Eep_ESC_MCU:			DB	"#BLHELI$EFM8B10#"
ELSE
CSEG AT 1A40h
Eep_ESC_Layout:		DB	"#MB8_H_UNSUP#   "	; Unsupported: this target is BB10/L only
CSEG AT 1A50h
Eep_ESC_MCU:			DB	"#BLHELI$EFM8B21#"
ENDIF

TEMP_LIMIT			EQU 	49	; Temperature measurement ADC value for which main motor power is limited at 80degC (low byte, assuming high byte is 1)
TEMP_LIMIT_STEP		EQU	9	; Temperature measurement ADC value increment for another 10degC

;**** **** **** **** ****
; Bootloader definitions
;**** **** **** **** ****
RTX_PORT		  		EQU 	P0		; Receive/Transmit port
RTX_MDOUT				EQU	P0MDOUT	; Set to 1 for PUSHPULL
RTX_MDIN				EQU	P0MDIN	; Set to 1 for DIGITAL
RTX_SKIP				EQU	P0SKIP	; Set to 1 for SKIP
RTX_PIN				EQU 	5		; P0.5 signal input and bootloader data

SIGNATURE_001			EQU	0E8h		; Device signature
IF MCU_48MHZ == 0
SIGNATURE_002			EQU	0B1h
ELSE
SIGNATURE_002			EQU	0B2h
ENDIF


;*********************
; PORT 0 definitions *
;*********************
Rcp_In		EQU	5	;i
;			EQU	6	;i
Mux_A		EQU	0	;i, internal A = PCB C
;			EQU	4	;i
Mux_B		EQU	3	;i
Comp_Com		EQU	1	;i, virtual neutral
Mux_C		EQU	2	;i, internal C = PCB A

P0_DIGITAL	EQU	(1 SHL Rcp_In)	; Other P0 pins remain analog/high-Z
P0_INIT		EQU	0FFh
P0_PUSHPULL	EQU	0
P0_SKIP		EQU	0FFh

Get_Rcp_Capture_Values MACRO
	anl	TCON, #0EFh				; Disable timer0
	mov	Temp1, TL0				; Get timer0 values
	mov	Temp2, TH0
IF MCU_48MHZ == 1
	mov	Temp3, Timer0_X
	jnb	TCON_TF0, ($+4)			; Check if interrupt is pending
	inc	Temp3					; If it is pending, then timer has already wrapped
ENDIF
	mov	TL0, #0					; Reset timer0
	mov	TH0, #0
IF MCU_48MHZ == 1
	mov	Timer0_X, #0
ENDIF
	orl	TCON, #10h				; Enable timer0 again
IF MCU_48MHZ == 1
	mov	A, Clock_Set_At_48MHz
	jnz	Get_Rcp_End
	clr	C
	mov	A, Temp1
	rlc	A
	mov	Temp1, A
	mov	A, Temp2
	rlc	A
	mov	Temp2, A
	mov	A, Temp3
	rlc	A
	mov	Temp3, A
Get_Rcp_End:
ENDIF
ENDM
Decode_Dshot_2Msb MACRO
	movx	A, @DPTR
	mov	Temp6, A
	clr	C
	subb	A, Temp5					; Subtract previous timestamp
	clr	C
	subb	A, Temp1
	jc	t1_int_msb_fail			; Check that bit is longer than minimum

	subb	A, Temp1					; Check if bit is zero or one
	mov	A, Temp4					; Shift bit into data byte
	rlc	A
	mov	Temp4, A
	inc	DPL						; Next bit
	movx	A, @DPTR
	mov	Temp5, A
	clr	C
	subb	A, Temp6
	clr	C
	subb	A, Temp1
	jc	t1_int_msb_fail

	subb	A, Temp1
	mov	A, Temp4
	rlc	A
	mov	Temp4, A
	inc	DPL
ENDM
Decode_Dshot_2Lsb MACRO
	movx	A, @DPTR
	mov	Temp6, A
	clr	C
	subb	A, Temp5					; Subtract previous timestamp
	clr	C
	subb	A, Temp1
	jc	t1_int_lsb_fail			; Check that bit is longer than minimum

	subb	A, Temp1					; Check if bit is zero or one
	mov	A, Temp3					; Shift bit into data byte
	rlc	A
	mov	Temp3, A
	inc	DPL						; Next bit
	movx	A, @DPTR
	mov	Temp5, A
	clr	C
	subb	A, Temp6
	clr	C
	subb	A, Temp1
	jc	t1_int_lsb_fail

	subb	A, Temp1
	mov	A, Temp3
	rlc	A
	mov	Temp3, A
	inc	DPL
ENDM
Initialize_PCA MACRO
	mov	PCA0CN0, #40h				; PCA enabled
	mov	PCA0MD, #08h				; PCA clock is SYSCLK
	mov	PCA0PWM, #81h				; 9-bit PWM, nominal 23.9 kHz
	mov	PCA0CENT, #03h				; Center aligned
ENDM
Set_Pwm_Polarity MACRO
	mov	PCA0POL, #00h				; CEX0 power PWM noninverted
ENDM
Enable_Power_Pwm_Module MACRO
	mov	PCA0CPM0, #42h				; CEX0 only
ENDM
Enable_Damp_Pwm_Module MACRO
	mov	PCA0CPM1, #00h				; CEX1 always disabled
ENDM
Set_Power_Pwm_Regs MACRO
	clr	C
	mov	A, Power_Pwm_Reg_H
	rrc	A
	mov	Temp1, A
	mov	A, Power_Pwm_Reg_L
	rrc	A
	mov	PCA0CPL0, A
	mov	PCA0CPH0, Temp1
ENDM
Clear_COVF_Interrupt MACRO
	anl	PCA0PWM, #0DFh
ENDM
Clear_CCF_Interrupt MACRO	; CCF interrupt is only used for FETON_DELAY == 0
	anl	PCA0CN0, #0FEh
ENDM
Enable_COVF_Interrupt MACRO
	orl	PCA0PWM, #40h
ENDM
Enable_CCF_Interrupt MACRO
	orl	PCA0CPM0,#01h
ENDM
Disable_COVF_Interrupt MACRO
	anl	PCA0PWM, #0BFh
ENDM
Disable_CCF_Interrupt MACRO
	anl	PCA0CPM0,#0FEh
ENDM


;*********************
; PORT 1 definitions *
;*********************
;			EQU	7	;i
;			EQU	6	;i
CpwmFET		EQU	6	;o, PCB A low side
BpwmFET		EQU	4	;o
ApwmFET		EQU	3	;o
AcomFET		EQU	2	;o
BcomFET		EQU	1	;o
CcomFET		EQU	0	;o

P1_DIGITAL	EQU	(1 SHL ApwmFET)+(1 SHL BpwmFET)+(1 SHL CpwmFET)+(1 SHL AcomFET)+(1 SHL BcomFET)+(1 SHL CcomFET)
P1_INIT		EQU 	0A0h		; Six controls low; P1.5/P1.7 input latches high
P1_PUSHPULL	EQU	(1 SHL ApwmFET)+(1 SHL BpwmFET)+(1 SHL CpwmFET)+(1 SHL AcomFET)+(1 SHL BcomFET)+(1 SHL CcomFET)
P1_SKIP		EQU	0FFh		; Skip all until one PWM/COM pair is selected

ApwmFET_on MACRO
	setb	P1.ApwmFET
ENDM
ApwmFET_off MACRO
	clr	P1.ApwmFET
ENDM
BpwmFET_on MACRO
	setb	P1.BpwmFET
ENDM
BpwmFET_off MACRO
	clr	P1.BpwmFET
ENDM
CpwmFET_on MACRO
	setb	P1.CpwmFET
ENDM
CpwmFET_off MACRO
	clr	P1.CpwmFET
ENDM
All_pwmFETs_Off MACRO
	clr	P1.ApwmFET
	clr	P1.BpwmFET
	clr	P1.CpwmFET
ENDM

AcomFET_on MACRO
	setb	P1.AcomFET
ENDM
AcomFET_off MACRO
	clr	P1.AcomFET
ENDM
BcomFET_on MACRO
	setb	P1.BcomFET
ENDM
BcomFET_off MACRO
	clr	P1.BcomFET
ENDM
CcomFET_on MACRO
	setb	P1.CcomFET
ENDM
CcomFET_off MACRO
	clr	P1.CcomFET
ENDM
All_comFETs_Off MACRO
	clr	P1.AcomFET
	clr	P1.BcomFET
	clr	P1.CcomFET
ENDM

Set_Pwm_A MACRO
	mov	P1SKIP, #0F7h				; CEX0 to low-side P1.3 only
ENDM
Set_Pwm_B MACRO
	mov	P1SKIP, #0EFh				; CEX0 to low-side P1.4 only
ENDM
Set_Pwm_C MACRO
	mov	P1SKIP, #0BFh				; CEX0 to low-side P1.6 only
ENDM
Set_Pwms_Off MACRO
	mov	P1SKIP, #0FFh
ENDM

Set_Comp_Phase_A MACRO
	mov	CMP0MX, #01h	; PCB C BEMF negative, P0.1 reference positive
ENDM
Set_Comp_Phase_B MACRO
	mov	CMP0MX, #31h	; PCB B BEMF negative, P0.1 reference positive
ENDM
Set_Comp_Phase_C MACRO
	mov	CMP0MX, #21h	; PCB A BEMF negative, P0.1 reference positive
ENDM
Read_Comp_Out MACRO
	mov	A, CMP0CN0	; Read comparator output
	cpl	A		; Low-side PWM / high-side COM: reverse zero-cross sense
ENDM


;*********************
; PORT 2 definitions *
;*********************
P2_PUSHPULL	EQU	0	; No bonded P2 power-control output on QFN20


;**********************
; MCU specific macros *
;**********************
Interrupt_Table_Definition MACRO
CSEG AT 0				; Code segment start
	jmp reset
CSEG AT 03h			; Int0 interrupt
	jmp	int0_int
IF MCU_48MHZ == 1
CSEG AT 0Bh			; Timer0 overflow interrupt
	jmp	t0_int
ENDIF
CSEG AT 13h			; Int1 interrupt
	jmp	int1_int
CSEG AT 1Bh			; Timer1 overflow interrupt
	jmp	t1_int
CSEG AT 2Bh			; Timer2 overflow interrupt
	jmp	t2_int
CSEG AT 5Bh			; Pca interrupt
	jmp	pca_int
CSEG AT 73h			; Timer3 overflow/compare interrupt
	jmp	t3_int
ENDM

Initialize_Xbar MACRO
	mov	XBR2, #40h				; Crossbar enabled
	mov	XBR1, #01h				; Route CEX0 only
ENDM

Initialize_Comparator MACRO
	mov	CMP0CN0, #80h	; Comparator enabled, no hysteresis
	mov	CMP0MD, #00h	; Comparator response time 100ns
ENDM
Initialize_Adc MACRO
	mov	REF0CN, #0Ch	; Set vdd (3.3V) as reference. Enable temp sensor and bias
IF MCU_48MHZ == 0
	mov	ADC0CF, #59h	; ADC clock 2MHz, PGA gain 1
ELSE
	mov	ADC0CF, #0B9h	; ADC clock 2MHz, PGA gain 1
ENDIF
	mov	ADC0MX, #10h	; Select temp sensor input
	mov	ADC0CN0, #80h	; ADC enabled
	mov	ADC0CN1, #01h	; Common mode buffer enabled
ENDM
Start_Adc MACRO
	mov	ADC0CN0, #90h	; ADC start
ENDM
Read_Adc_Result MACRO
	mov	Temp1, ADC0L
	mov	Temp2, ADC0H
ENDM
Stop_Adc MACRO
ENDM
Set_RPM_Out MACRO
ENDM
Clear_RPM_Out MACRO
ENDM
Set_MCU_Clk_24MHz MACRO
	mov	CLKSEL, #13h		; Set clock to 24MHz
	mov	SFRPAGE, #10h
	mov	PFE0CN, #00h		; Set flash timing for 24MHz
	mov	SFRPAGE, #00h
	mov	Clock_Set_At_48MHz, #0
ENDM
Set_MCU_Clk_48MHz MACRO
	mov	SFRPAGE, #10h
	mov	PFE0CN, #30h		; Set flash timing for 48MHz
	mov	SFRPAGE, #00h
	mov	CLKSEL, #03h		; Set clock to 48MHz
	mov	Clock_Set_At_48MHz, #1
ENDM
Set_LED_0 MACRO
ENDM
Clear_LED_0 MACRO
ENDM
Set_LED_1 MACRO
ENDM
Clear_LED_1 MACRO
ENDM
Set_LED_2 MACRO
ENDM
Clear_LED_2 MACRO
ENDM
Set_LED_3 MACRO
ENDM
Clear_LED_3 MACRO
ENDM
