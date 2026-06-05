; vesa.asm — VBE 640x480x8 (mode 0x101), fill via bank switching, then exit.
    org 100h
    mov ax, 0x4F02
    mov bx, 0x101
    int 0x10
    cmp ax, 0x004F
    jne fail
    xor si, si              ; bank
.bank:
    mov ax, 0x4F05
    xor bx, bx
    mov dx, si
    int 0x10
    push 0xA000
    pop es
    xor di, di
    mov cx, 0x8000          ; 64KB
    mov ax, si
    mov ah, al              ; word = bank:bank
    rep stosw
    inc si
    cmp si, 5
    jb .bank
    mov ax, 0x4C00
    int 0x21
fail:
    mov ax, 0x4C01
    int 0x21
