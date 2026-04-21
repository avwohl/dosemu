; dpmi_stage4.asm — verifies the stage-4 INT 31h subset:
;
;   AX=0400  Get DPMI version     -> expect CF=0, CL=3 (386)
;   AX=0006  Get segment base     -> expect CF=0, base matches what we
;                                    asked for with AX=0007 immediately
;                                    before.
;   AX=0007  Set segment base     -> expect CF=0.
;
; All calls happen from 16-bit PM (after the DPMI switch) to exercise
; the PM IDT gate for INT 31h.  A regression in any step prints a
; distinct message so CI can tell which call broke.
;
; Assemble:  nasm -f bin dpmi_stage4.asm -o DPMI_STAGE4.COM

    org 100h

BITS 16
    mov ax, 1687h
    int 2Fh
    test ax, ax
    jnz  not_present

    mov [entry_off], di
    mov [entry_seg], es

    cli
    xor ax, ax                     ; 16-bit PM
    call far [entry_off]

    test ax, ax
    jnz  switch_failed

    ; -- AX=0400 Get DPMI version -----------------------------------
    mov ax, 0400h
    int 31h
    jc  fail_ver
    cmp cl, 3                      ; CPU type: 386
    jne fail_ver

    ; -- AX=0007 Set segment base of our ES selector (0x20) to 0x123450
    mov ax, 0007h
    mov bx, 0020h                  ; PM_ES_SEL
    mov cx, 0012h                  ; base high
    mov dx, 3450h                  ; base low
    int 31h
    jc  fail_set

    ; -- AX=0006 Get segment base back, verify it round-trips --------
    mov ax, 0006h
    mov bx, 0020h
    int 31h
    jc  fail_get
    cmp cx, 0012h
    jne fail_get
    cmp dx, 3450h
    jne fail_get

    ; -- AX=0006 with a bad selector -> expect CF=1 / AX=8022 --------
    mov ax, 0006h
    mov bx, 0FFF8h                 ; way past GDT_LIMIT
    int 31h
    jnc fail_badsel
    cmp ax, 8022h
    jne fail_badsel

    ; All checks passed.
    mov dx, ok_msg
    mov ah, 9
    int 21h
    mov ax, 4C00h
    int 21h

fail_ver:
    mov dx, fail_ver_msg
    jmp print_and_exit_1
fail_set:
    mov dx, fail_set_msg
    jmp print_and_exit_1
fail_get:
    mov dx, fail_get_msg
    jmp print_and_exit_1
fail_badsel:
    mov dx, fail_badsel_msg
    jmp print_and_exit_1

print_and_exit_1:
    mov ah, 9
    int 21h
    mov ax, 4C01h
    int 21h

not_present:
    mov dx, absent_msg
    mov ah, 9
    int 21h
    mov ax, 4C02h
    int 21h

switch_failed:
    mov dx, failed_msg
    mov ah, 9
    int 21h
    mov ax, 4C01h
    int 21h

entry_off        dw 0
entry_seg        dw 0

ok_msg           db 'dpmi-stage4=ok', 13, 10, '$'
fail_ver_msg     db 'dpmi-stage4=fail-ver', 13, 10, '$'
fail_set_msg     db 'dpmi-stage4=fail-set', 13, 10, '$'
fail_get_msg     db 'dpmi-stage4=fail-get', 13, 10, '$'
fail_badsel_msg  db 'dpmi-stage4=fail-badsel', 13, 10, '$'
absent_msg       db 'dpmi-stage4=absent', 13, 10, '$'
failed_msg       db 'dpmi-stage4=switch-failed', 13, 10, '$'
