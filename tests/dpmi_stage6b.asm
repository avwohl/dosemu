; dpmi_stage6b.asm — verifies INT 31h AX=0503 (resize memory block).
;
; Flow (16-bit PM):
;   1. AX=0501 alloc 256 bytes.  Remember handle + base.
;   2. AX=0503 shrink to 64 bytes -> must succeed, handle + base stable
;      (mcb_resize shrinks in place by splitting off the tail).
;   3. AX=0503 grow back to 128 bytes -> may succeed if the freshly-
;      split tail MCB is still free and adjacent (which it is -- we
;      just freed it).  Verify CF=0.
;   4. AX=0502 free.
;
; Assemble:  nasm -f bin dpmi_stage6b.asm -o DPMI_STAGE6B.COM

    org 100h

BITS 16
    mov ax, 1687h
    int 2Fh
    test ax, ax
    jnz  not_present

    mov [entry_off], di
    mov [entry_seg], es

    cli
    xor ax, ax
    call far [entry_off]
    test ax, ax
    jnz  switch_failed

    ; Alloc 256 bytes
    mov ax, 0501h
    mov bx, 0
    mov cx, 256
    int 31h
    jc  fail_alloc
    mov [h_si], si
    mov [h_di], di
    mov [base_hi], bx
    mov [base_lo], cx

    ; Shrink to 64 bytes
    mov ax, 0503h
    mov si, [h_si]
    mov di, [h_di]
    mov bx, 0
    mov cx, 64
    int 31h
    jc  fail_shrink
    ; Base should be unchanged (in-place shrink).
    cmp bx, [base_hi]
    jne fail_shrink
    cmp cx, [base_lo]
    jne fail_shrink

    ; Grow back to 128 bytes -- adjacent tail MCB is free.
    mov ax, 0503h
    mov si, [h_si]
    mov di, [h_di]
    mov bx, 0
    mov cx, 128
    int 31h
    jc  fail_grow

    ; Free
    mov ax, 0502h
    mov si, [h_si]
    mov di, [h_di]
    int 31h
    jc  fail_free

    mov dx, ok_msg
    mov ah, 9
    int 21h
    mov ax, 4C00h
    int 21h

fail_alloc:
    mov dx, fail_alloc_msg
    jmp print_and_exit_1
fail_shrink:
    mov dx, fail_shrink_msg
    jmp print_and_exit_1
fail_grow:
    mov dx, fail_grow_msg
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

entry_off        dw 0
entry_seg        dw 0
h_si             dw 0
h_di             dw 0
base_hi          dw 0
base_lo          dw 0

ok_msg           db 'dpmi-stage6b=ok', 13, 10, '$'
fail_alloc_msg   db 'dpmi-stage6b=fail-alloc', 13, 10, '$'
fail_shrink_msg  db 'dpmi-stage6b=fail-shrink', 13, 10, '$'
fail_grow_msg    db 'dpmi-stage6b=fail-grow', 13, 10, '$'
fail_free_msg    db 'dpmi-stage6b=fail-free', 13, 10, '$'
absent_msg       db 'dpmi-stage6b=absent', 13, 10, '$'
failed_msg       db 'dpmi-stage6b=switch-failed', 13, 10, '$'
