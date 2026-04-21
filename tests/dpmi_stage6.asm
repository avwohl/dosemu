; dpmi_stage6.asm — verifies stage-6 minimal linear memory allocation.
;
; Flow (all in 16-bit PM):
;   1. INT 31h AX=0501 asks for 256 bytes.  On success, BX:CX holds the
;      linear address of the block and SI:DI holds an opaque handle.
;   2. INT 31h AX=0007 repoints the PM ES selector (0x20) at that linear
;      address.  `mov es, ax` reloads the descriptor cache so subsequent
;      es-relative writes hit the new block.
;   3. Write a 16-bit pattern, read it back, verify it round-trips.
;      Without stage 6 this would trap (the memory was never allocated)
;      or with a buggy alloc would corrupt adjacent MCBs and the pattern
;      could come back altered.
;   4. INT 31h AX=0502 frees the block via the saved handle.
;   5. Print dpmi-stage6=ok and exit.
;
; A regression in any step prints a distinct fail tag so CI can tell
; which call broke.
;
; Assemble:  nasm -f bin dpmi_stage6.asm -o DPMI_STAGE6.COM

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

    ; -- AX=0501 Allocate 256 bytes ---------------------------------
    mov ax, 0501h
    mov bx, 0
    mov cx, 256
    int 31h
    jc  fail_alloc

    ; Save outputs: BX:CX = linear addr, SI:DI = handle
    mov [base_hi], bx
    mov [base_lo], cx
    mov [handle_hi], si
    mov [handle_lo], di

    ; -- AX=0007 Point ES (selector 0x20) at the new block ----------
    mov ax, 0007h
    mov bx, 0020h
    mov cx, [base_hi]
    mov dx, [base_lo]
    int 31h
    jc  fail_setbase

    ; Reload ES so the descriptor-cache base refreshes.
    mov ax, 0020h
    mov es, ax

    ; -- Write + read back pattern ----------------------------------
    mov word [es:0],   0ABCDh
    mov word [es:254], 1234h        ; last addressable word in the 256-byte block
    mov ax, [es:0]
    cmp ax, 0ABCDh
    jne fail_readback
    mov ax, [es:254]
    cmp ax, 1234h
    jne fail_readback

    ; -- AX=0502 Free the block -------------------------------------
    mov ax, 0502h
    mov si, [handle_hi]
    mov di, [handle_lo]
    int 31h
    jc  fail_free

    ; All checks passed.
    mov dx, ok_msg
    mov ah, 9
    int 21h
    mov ax, 4C00h
    int 21h

fail_alloc:
    mov dx, fail_alloc_msg
    jmp print_and_exit_1
fail_setbase:
    mov dx, fail_setbase_msg
    jmp print_and_exit_1
fail_readback:
    mov dx, fail_readback_msg
    jmp print_and_exit_1
fail_free:
    mov dx, fail_free_msg
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

entry_off         dw 0
entry_seg         dw 0
base_hi           dw 0
base_lo           dw 0
handle_hi         dw 0
handle_lo         dw 0

ok_msg            db 'dpmi-stage6=ok', 13, 10, '$'
fail_alloc_msg    db 'dpmi-stage6=fail-alloc', 13, 10, '$'
fail_setbase_msg  db 'dpmi-stage6=fail-setbase', 13, 10, '$'
fail_readback_msg db 'dpmi-stage6=fail-readback', 13, 10, '$'
fail_free_msg     db 'dpmi-stage6=fail-free', 13, 10, '$'
absent_msg        db 'dpmi-stage6=absent', 13, 10, '$'
failed_msg        db 'dpmi-stage6=switch-failed', 13, 10, '$'
