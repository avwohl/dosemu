; graf13.asm — set mode 13h and draw an XOR texture, then exit.
    org 100h
    mov ax, 0x0013
    int 0x10
    push 0xA000
    pop es
    xor di, di
    xor bx, bx          ; y
.row:
    xor cx, cx          ; x
.col:
    mov al, bl
    xor al, cl          ; color = (x ^ y) & 0xFF
    mov [es:di], al
    inc di
    inc cx
    cmp cx, 320
    jb .col
    inc bx
    cmp bx, 200
    jb .row
    mov ax, 0x4C00
    int 0x21
