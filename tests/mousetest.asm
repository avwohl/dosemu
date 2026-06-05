    org 100h
    mov ax, 0x13        ; mode 13h (320x200) so cursor pos maps nicely
    int 0x10
    xor ax, ax          ; mouse reset
    int 0x33
    cmp ax, 0xFFFF
    jne done
    mov ax, 4           ; set position
    mov cx, 320         ; virtual x (range 0..639 -> middle)
    mov dx, 100         ; virtual y (range 0..199 -> middle)
    int 0x33
    mov ax, 1           ; show cursor
    int 0x33
done:
    mov ax, 0x4C00
    int 0x21
