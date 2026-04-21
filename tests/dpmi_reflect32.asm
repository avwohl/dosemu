; dpmi_reflect32.asm — 32-bit PM variant of dpmi_reflect.asm.
;
; Issues INT 10h AH=0F (get video mode) from 32-bit PM.  Without a
; 32-bit IDT gate + CB_IRETD-style shim, the plain 16-bit IRET at the
; end of the RM BIOS stub can't unwind the 12-byte frame a 32-bit gate
; pushed and the emulator aborts with "IRET:Illegal descriptor type"
; (CS pops as zero = null descriptor).  The new per-vector shim in
; PM_SHIM_SEG ends in `66 CF` (IRETD) which correctly handles the
; 32-bit frame.
;
; Oracle: INT 10h AH=0F in real mode.  PM result must match.
;
; Assemble:  nasm -f bin dpmi_reflect32.asm -o DPMI_REFLECT32.COM

    org 100h

BITS 16
    mov ah, 0Fh
    int 10h
    mov [oracle_al], al
    mov [oracle_bh], bh

    mov ax, 1687h
    int 2Fh
    test ax, ax
    jnz  not_present

    mov [entry_off], di
    mov [entry_seg], es

    cli
    xor eax, eax
    mov ax, 1                      ; AX=1 -> 32-bit PM
    call far [entry_off]
    test ax, ax
    jnz  switch_failed

BITS 32
    ; Still-16-bit-encoded safe instructions are fine; we need to exercise
    ; the 32-bit IDT gate path.  INT 10h via the gate.
    mov ah, 0Fh
    int 10h
    mov bl, [oracle_al]
    cmp al, bl
    jne fail_mode
    mov al, [oracle_bh]
    cmp bh, al
    jne fail_page

    mov ah, 9
    mov edx, ok_msg
    int 21h
    mov eax, 4C00h
    int 21h

fail_mode:
    mov edx, fail_mode_msg
    jmp print_and_exit_1_pm32
fail_page:
    mov edx, fail_page_msg
    jmp print_and_exit_1_pm32

print_and_exit_1_pm32:
    mov ah, 9
    int 21h
    mov eax, 4C01h
    int 21h

BITS 16
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
oracle_al      db 0
oracle_bh      db 0

ok_msg         db 'dpmi-reflect32=ok', 13, 10, '$'
fail_mode_msg  db 'dpmi-reflect32=fail-mode', 13, 10, '$'
fail_page_msg  db 'dpmi-reflect32=fail-page', 13, 10, '$'
absent_msg     db 'dpmi-reflect32=absent', 13, 10, '$'
failed_msg     db 'dpmi-reflect32=switch-failed', 13, 10, '$'
