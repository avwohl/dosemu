; fill rows 0..24 with letters A..Y, then INT 10h AH=06 scroll up 3 lines.
    org 100h
    push 0xB800
    pop es
    xor si, si
.row:
    mov ax, si
    mov bx, 160
    mul bx
    mov di, ax              ; di = row*160
    mov ax, si
    add al, 'A'             ; al = 'A'+row
    mov ah, 0x0F            ; white attr
    mov cx, 80
    rep stosw
    inc si
    cmp si, 25
    jb .row
    mov ax, 0x0603          ; AH=06 (scroll up) AL=3
    mov bh, 0x07
    xor cx, cx              ; (0,0)
    mov dx, 0x184F          ; (24,79)
    int 0x10
    mov ax, 0x4C00
    int 0x21
