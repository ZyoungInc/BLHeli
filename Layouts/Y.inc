; Custom BB10/L QFN20 ESC layout with BB1 software CMPINV, derived from layout X.
; Only for EFM8BB10F8G. Keep the original T layout untouched.
;
; Internal A = PCB C, internal B = PCB B, internal C = PCB A.
; PWM outputs are low-side N-MOS; COM outputs drive high-side P-MOS
; through NPN inverters. Both MCU GPIO control paths are active high.
; P1.5 and P0.4/P0.6/P0.7 have unknown peripheral connections.

PWM_ACTIVE_HIGH EQU 1
COM_ACTIVE_HIGH EQU 1
COMPARATOR_PORT EQU 0
COMPARATOR_INVERT EQU 1

IF DEADTIME == 0
    PCA0CPM_POWER EQU PCA0CPM0
    PCA0CPL_POWER EQU PCA0CPL0
    PCA0CPH_POWER EQU PCA0CPH0
    PCA0CPM_DAMP EQU PCA0CPM1
    PCA0CPL_DAMP EQU PCA0CPL1
    PCA0CPH_DAMP EQU PCA0CPH1
ELSE
    PCA0CPM_POWER EQU PCA0CPM1
    PCA0CPL_POWER EQU PCA0CPL1
    PCA0CPH_POWER EQU PCA0CPH1
    PCA0CPM_DAMP EQU PCA0CPM0
    PCA0CPL_DAMP EQU PCA0CPL0
    PCA0CPH_DAMP EQU PCA0CPH0
ENDIF

; Port 0: CMP0 negative input is the floating phase, positive input P0.1.
RTX_PIN EQU 5                         ; P0.5, DShot input
A_Mux EQU 0                           ; PCB C BEMF, P0.0
B_Mux EQU 3                           ; PCB B BEMF, P0.3
C_Mux EQU 2                           ; PCB A BEMF, P0.2
V_Mux EQU 1                           ; Virtual neutral, P0.1
P0_DIGITAL EQU (1 SHL RTX_PIN)        ; All other P0 pins analog/high-Z
P0_INIT EQU 0FFh                      ; Input latch high, no push-pull
P0_PUSHPULL EQU 0
P0_SKIP EQU 0FFh                      ; No crossbar output on Port 0

; Port 1: all six driven signals are active high and initially low.
A_Pwm EQU 3                           ; PCB C low side, P1.3
A_Com EQU 2                           ; PCB C high side, P1.2
B_Pwm EQU 4                           ; PCB B low side, P1.4
B_Com EQU 1                           ; PCB B high side, P1.1
C_Pwm EQU 6                           ; PCB A low side, P1.6
C_Com EQU 0                           ; PCB A high side, P1.0
P1_DIGITAL EQU (1 SHL A_Pwm) + (1 SHL B_Pwm) + (1 SHL C_Pwm) + (1 SHL A_Com) + (1 SHL B_Com) + (1 SHL C_Com)
P1_INIT EQU 0A0h                      ; Driven bits low; P1.5/1.7 input latches high
P1_PUSHPULL EQU P1_DIGITAL
P1_SKIP EQU 0FFh                      ; Base macros unskip only selected PWM pair

; No bonded Port 2 power-control outputs on this QFN20 target.
P2_DIGITAL EQU 0
P2_PUSHPULL EQU 0
P2_SKIP EQU 0FFh

$include (Base.inc)
