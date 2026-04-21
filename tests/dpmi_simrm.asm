; dpmi_simrm.asm — INT 31h AX=0300 (Simulate Real Mode Interrupt).
;
; Populates a RealModeCallStructure asking for INT 1Ah AH=00 (read
; BIOS tick count; returns count in CX:DX).  Calls AX=0300 from 16-bit
; PM; on return the struct's ECX/EDX fields should reflect the RM
; INT's output.  Then prints "dpmi-simrm=ok" and exits.
;
; Validates the full mode-switch machinery: save PM state, swap IDTR
; to the RM IVT, clear CR0.PE, run the RM INT via CALLBACK_RunRealInt,
; restore CR0.PE + PM IDTR, reload segments (including a manual CS
; descriptor-cache refresh), write results back.  Without the IDTR
; swap the RM INT reads our PM 8-byte gate descriptors as 4-byte
; seg:off pairs and jumps into the weeds; without the CS refresh the
; shim's next fetch hits a stale RM-interpreted base.
;
; Assemble:  nasm -f bin dpmi_simrm.asm -o DPMI_SIMRM.COM

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

    ; Zero the 50-byte struct.
    mov cx, 50
    mov di, rmcs
    xor ax, ax
    rep stosb

    ; Point ES at our data segment (matches DS post-switch: both use
    ; PM selector 0x10 whose base equals the original real-mode DS*16).
    mov ax, ds
    mov es, ax

    ; INT 31h AX=0300: simulate INT 1Ah with struct's EAX = 0
    ; (AH=00 = get tick count).
    mov ax, 0300h
    mov bl, 1Ah
    mov bh, 0
    xor cx, cx
    mov di, rmcs
    int 31h
    jc  fail_simrm

    ; Verify: struct ECX and EDX should now hold the tick count.
    ; Since they're zero-initialized, any non-zero write confirms
    ; AX=0300 propagated results back through the mode-switch
    ; round-trip.  A broken round-trip either faults during the
    ; switch-back (emulator abort) or leaves the struct untouched
    ; (both ECX and EDX stay 0).
    mov ax, [rmcs + 0x18]           ; ECX low word
    or  ax, [rmcs + 0x1A]           ; ECX high word
    or  ax, [rmcs + 0x14]           ; EDX low word
    or  ax, [rmcs + 0x16]           ; EDX high word
    jz  fail_no_update

    mov dx, ok_msg
    mov ah, 9
    int 21h
    mov ax, 4C00h
    int 21h

fail_simrm:
    mov dx, fail_simrm_msg
    jmp print_and_exit_1
fail_no_update:
    mov dx, fail_no_update_msg
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

entry_off            dw 0
entry_seg            dw 0

rmcs                 times 50 db 0

ok_msg               db 'dpmi-simrm=ok', 13, 10, '$'
fail_simrm_msg       db 'dpmi-simrm=fail-simrm', 13, 10, '$'
fail_no_update_msg   db 'dpmi-simrm=fail-no-update', 13, 10, '$'
absent_msg           db 'dpmi-simrm=absent', 13, 10, '$'
failed_msg           db 'dpmi-simrm=switch-failed', 13, 10, '$'
