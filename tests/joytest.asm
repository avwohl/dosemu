; joytest.asm — exercise the analog game port (0x201) and INT 15h AH=84h.
;
; With the default emulated stick (present, centred, no buttons): the game
; port's button nibble reads 0xF0 (all released), and INT 15h AH=84h returns
; centred axis counts with no error. Prints "joy-ok" when all three checks pass.
    org 100h

    ; --- game port: trigger the monostables, then read button nibble ---
    mov dx, 0x201
    out dx, al              ; fire axis one-shots (value ignored)
    in  al, dx
    and al, 0xF0            ; isolate the button bits
    cmp al, 0xF0            ; all four released?
    jne .fail

    ; --- INT 15h AH=84h DX=0: digital button read ---
    mov ax, 0x8400
    xor dx, dx
    int 0x15
    jc  .fail
    and al, 0xF0
    cmp al, 0xF0
    jne .fail

    ; --- INT 15h AH=84h DX=1: resistive axis read (centred => nonzero) ---
    mov ax, 0x8400
    mov dx, 1
    int 0x15
    jc  .fail
    or  ax, ax              ; axis A-x count must be nonzero
    jz  .fail

    mov dx, msg_ok
    jmp .print
.fail:
    mov dx, msg_no
.print:
    mov ah, 0x09
    int 0x21
    mov ax, 0x4C00
    int 0x21

msg_ok db 'joy-ok', 0x0D, 0x0A, '$'
msg_no db 'joy-no', 0x0D, 0x0A, '$'
