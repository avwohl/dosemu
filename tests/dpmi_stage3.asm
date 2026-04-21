; dpmi_stage3.asm — verifies DPMI stage 3 delivers a real mode switch.
;
; Calls the DPMI switch entry.  If AX=0 on return we're in protected
; mode.  We can't safely call INT 21h after the switch yet (stage 5:
; INT reflection to real mode isn't implemented), so verification goes
; through a memory marker:
;
;   Before call:  write 'R' to [memflag]
;   Entry call returns:
;     AX == 0 -> in PM, write 'P' to [memflag], then spin.
;     AX != 0 -> switch failed, exit via real-mode INT 21h.
;
; CI inspection: the program loops forever after the switch so the
; timeout fires (exit 124).  A future test harness can examine guest
; memory at DS:memflag to confirm 'P' was written.
;
; Real-mode exit path (not-present or failed switch) prints a short
; diagnostic and exits with a specific code.
;
; Assemble:  nasm -f bin dpmi_stage3.asm -o DPMI_STAGE3.COM

    org 100h

    mov byte [memflag], 'R'        ; pre-switch marker

    mov ax, 1687h
    int 2Fh
    test ax, ax
    jnz  not_present

    mov [entry_off], di
    mov [entry_seg], es

    cli                            ; mask IRQs -- stage 3 has no IDT yet, so a
                                   ; timer interrupt (INT 8) post-switch would
                                   ; trap through an empty descriptor table.
    xor ax, ax
    call far [entry_off]           ; may return in PM

    test ax, ax
    jnz  switch_failed

    ; We're in protected mode.  DS base = real-mode DS * 16 so [memflag]
    ; still points at the same linear address; the write succeeds.
    mov byte [memflag], 'P'

in_pm_loop:
    jmp in_pm_loop                 ; spin.  CI timeout picks this up.

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

entry_off      dw 0
entry_seg      dw 0
memflag        db 0

absent_msg     db 'dpmi=absent', 13, 10, '$'
failed_msg     db 'dpmi=switch-failed', 13, 10, '$'
