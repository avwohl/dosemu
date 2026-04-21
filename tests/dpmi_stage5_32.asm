; dpmi_stage5_32.asm — exercises DPMI stage 5' (32-bit PM path).
;
; Identical in spirit to dpmi_stage5.asm, but switches to 32-bit PM
; (AX=1 on the DPMI entry) and executes 32-bit-encoded instructions
; after the mode switch.  If the D flag on the code descriptor or the
; 32-bit IDT gate (type 0x8E, 32-bit IRET frame) regresses, the CPU
; mis-decodes the bytes after `call far`, the INT 21h gate faults, or
; the IRETD pops the wrong-size frame — any of which fails the test.
;
; Test path:
;   1. Probe DPMI (INT 2Fh AX=1687h).
;   2. CLI (no IDT handler for IRQs).
;   3. Clear EAX so the post-call `test ax, ax` still passes when the
;      instruction bytes are re-decoded as `test eax, eax` in 32-bit PM.
;   4. AX=1, far-call the entry — on return we're either in 32-bit PM
;      (EAX low 16 = 0) or back in real mode (AX != 0).
;   5. In 32-bit PM: prove D=1 is in effect with a 32-bit MOV EAX, imm32
;      round-trip, then INT 21h AH=09 to reach the host handler via the
;      32-bit IDT gate, then INT 21h AH=4C AL=0 to exit cleanly.
;
; The success string includes '32' so CI can distinguish this run from
; the 16-bit stage 5 fixture.
;
; Assemble:  nasm -f bin dpmi_stage5_32.asm -o DPMI_STAGE5_32.COM

    org 100h

BITS 16
    mov ax, 1687h
    int 2Fh
    test ax, ax
    jnz  not_present

    mov [entry_off], di
    mov [entry_seg], es

    cli
    xor eax, eax                   ; zero full EAX so post-call test is clean
    mov ax, 1                      ; AX=1 -> 32-bit PM
    call far [entry_off]

    ; After `call far` we are either still in 16-bit RM (switch failed:
    ; AX != 0) or in 32-bit PM (success: AX = 0, upper EAX = 0 from the
    ; xor above).  The `test ax, ax ; jnz` bytes (85 C0 75 xx) decode
    ; identically in both modes except that 85 C0 becomes `test eax, eax`
    ; under D=1 — still a valid zero/non-zero test with upper EAX = 0.
    test ax, ax
    jnz  switch_failed

    ; At this point we are definitely in 32-bit PM.  Switch NASM to
    ; emit 32-bit encodings so the CPU (D=1) decodes them correctly.
BITS 32
    ; Prove D=1: 32-bit immediate round-trip through EAX.
    mov eax, 0DEADBEEFh
    cmp eax, 0DEADBEEFh
    jne panic_pm32                 ; D=0 would have fetched the wrong bytes
                                   ; and never reached a matching compare.

    ; INT 21h AH=09: DS:(E)DX -> host handler.  DS selector has base =
    ; PSP*16, so edx = pm_msg offset reaches the real-mode data area.
    mov ah, 9
    mov edx, pm_msg
    int 21h

    mov eax, 4C00h
    int 21h

panic_pm32:
    ; Unreachable unless the 32-bit decode path is broken.  Spin so the
    ; CI timeout surfaces the regression rather than silently exiting 0.
    jmp panic_pm32

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

entry_off   dw 0
entry_seg   dw 0

pm_msg      db 'dpmi-stage5-32=hello-from-pm32', 13, 10, '$'
absent_msg  db 'dpmi-stage5-32=absent', 13, 10, '$'
failed_msg  db 'dpmi-stage5-32=switch-failed', 13, 10, '$'
