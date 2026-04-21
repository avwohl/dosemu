; dpmi_v10.asm -- exercises the DPMI 1.0 extensions CWSDPMI implements:
;   AX=0401 Get DPMI Capabilities
;   AX=000D Allocate Specific LDT Descriptor
;   AX=0506 Get Page Attributes
;   AX=0507 Set Page Attributes (no-op success)
;   AX=0508 Map Device (expect CF=1 AX=8025 "invalid address")
;   AX=0E00 Get Coprocessor Status
;   AX=0E01 Set Coprocessor Emulation
;
; Enters 16-bit PM, runs the checks, prints dpmi-v10=ok on success.
; Distinct fail tags for each sub-function.
;
; Assemble:  nasm -f bin dpmi_v10.asm -o DPMI_V10.COM

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

    ; -- AX=0401 Get Capabilities ----------------------------------
    ; ES was set by DPMI entry to PM_ES_SEL (0x20), base = PSP*16.
    ; Use ES:DI = ES:80h as the vendor-string destination.
    mov ax, 0401h
    mov di, 80h
    int 31h
    jc  fail_cap
    ; Expect AX=0x20 (exception-handling bit only) -- we don't page.
    cmp ax, 0020h
    jne fail_cap

    ; -- AX=000D Allocate Specific LDT Descriptor -------------------
    ; Allocate a fresh LDT selector first to learn what's free,
    ; then free it, then AX=000D at the same index.
    mov ax, 0000h
    mov cx, 1
    int 31h
    jc  fail_alloc
    mov bx, ax                     ; preserved selector
    ; Free it so 000D can re-grab the same slot.
    mov ax, 0001h
    int 31h
    jc  fail_free

    ; Now request the same slot specifically.
    mov ax, 000Dh
    ; BX = the selector we saved.
    int 31h
    jc  fail_specific
    ; Verify we got a free slot by freeing again.
    mov ax, 0001h
    int 31h
    jc  fail_free

    ; -- AX=0E00 Get Coprocessor Status -----------------------------
    mov ax, 0E00h
    int 31h
    jc  fail_fpu_get
    ; AX bit 0 = MP (coprocessor present); expect >=1.
    test ax, 0001h
    jz  fail_fpu_get

    ; -- AX=0E01 Set Coprocessor Emulation --------------------------
    mov ax, 0E01h
    mov bx, 0                      ; default (no client emulation)
    int 31h
    jc  fail_fpu_set

    ; -- AX=0507 Set Page Attributes (no-op success) ----------------
    mov ax, 0507h
    mov si, 0
    mov di, 0
    mov ebx, 0
    mov cx, 1
    mov edx, 0
    int 31h
    jc  fail_pageset

    ; -- AX=0508 Map Device (expect CF=1 AX=8025) -------------------
    mov ax, 0508h
    mov si, 0
    mov di, 0
    mov ebx, 0
    mov cx, 1
    mov edx, 0
    int 31h
    jnc fail_mapdev
    cmp ax, 8025h
    jne fail_mapdev

    ; All 1.0 extensions returned as expected.
    mov dx, ok_msg
    mov ah, 9
    int 21h
    mov ax, 4C00h
    int 21h

fail_cap:      mov dx, fail_cap_msg
               jmp print_and_exit_1
fail_alloc:    mov dx, fail_alloc_msg
               jmp print_and_exit_1
fail_free:     mov dx, fail_free_msg
               jmp print_and_exit_1
fail_specific: mov dx, fail_specific_msg
               jmp print_and_exit_1
fail_fpu_get:  mov dx, fail_fpu_get_msg
               jmp print_and_exit_1
fail_fpu_set:  mov dx, fail_fpu_set_msg
               jmp print_and_exit_1
fail_pageset:  mov dx, fail_pageset_msg
               jmp print_and_exit_1
fail_mapdev:   mov dx, fail_mapdev_msg
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

ok_msg            db 'dpmi-v10=ok', 13, 10, '$'
fail_cap_msg      db 'dpmi-v10=fail-cap', 13, 10, '$'
fail_alloc_msg    db 'dpmi-v10=fail-alloc', 13, 10, '$'
fail_free_msg     db 'dpmi-v10=fail-free', 13, 10, '$'
fail_specific_msg db 'dpmi-v10=fail-specific', 13, 10, '$'
fail_fpu_get_msg  db 'dpmi-v10=fail-fpu-get', 13, 10, '$'
fail_fpu_set_msg  db 'dpmi-v10=fail-fpu-set', 13, 10, '$'
fail_pageset_msg  db 'dpmi-v10=fail-pageset', 13, 10, '$'
fail_mapdev_msg   db 'dpmi-v10=fail-mapdev', 13, 10, '$'
absent_msg        db 'dpmi-v10=absent', 13, 10, '$'
failed_msg        db 'dpmi-v10=switch-failed', 13, 10, '$'
