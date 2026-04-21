; size.asm — reads a filename from the PSP command tail, opens it,
; seeks to EOF via AH=42h to determine its size, prints "size=N\n" in
; decimal, closes, exits.  Exercises AH=3Dh open, AH=42h seek, AH=3Eh
; close, plus the decimal-print helper path.  File size assumed < 64K.
;
; Assemble:  nasm -f bin size.asm -o SIZE.COM

    org 100h

    ; Find start of filename in command tail (PSP:81h onward).
    mov si, 81h
.skip_space:
    mov al, [si]
    cmp al, ' '
    jne .found_name
    inc si
    jmp .skip_space
.found_name:
    mov bx, si
.find_end:
    mov al, [si]
    cmp al, 0
    je  .got_end
    cmp al, 0Dh
    je  .got_end
    cmp al, ' '
    je  .got_end
    inc si
    jmp .find_end
.got_end:
    mov byte [si], 0
    cmp bx, si
    je  bail
    mov dx, bx

    ; Open read-only.
    mov ah, 3Dh
    xor al, al
    int 21h
    jc  bail
    mov bx, ax

    ; Seek to end to get size.  DX:AX = position after; for files < 64K
    ; DX is zero and AX is the size.
    mov ah, 42h
    mov al, 2
    xor cx, cx
    xor dx, dx
    int 21h
    jc  close_bail

    push ax                 ; save size

    ; Print "size="
    mov ah, 9
    mov dx, prefix
    int 21h

    pop ax
    call print_decimal_ax

    ; Print newline.
    mov dl, 10
    mov ah, 2
    int 21h

    mov ah, 3Eh
    int 21h
    mov ax, 4C00h
    int 21h

close_bail:
    mov ah, 3Eh
    int 21h
bail:
    mov ax, 4C01h
    int 21h

; Print AX as decimal.  Pushes each digit on the stack, then pops and prints.
print_decimal_ax:
    push bx
    push cx
    push dx
    xor  cx, cx
    mov  bx, 10
    test ax, ax
    jnz  .divide
    mov  dl, '0'
    mov  ah, 2
    int  21h
    jmp  .done
.divide:
    xor  dx, dx
    div  bx
    push dx
    inc  cx
    test ax, ax
    jnz  .divide
.print_digit:
    pop  dx
    add  dl, '0'
    mov  ah, 2
    int  21h
    loop .print_digit
.done:
    pop  dx
    pop  cx
    pop  bx
    ret

prefix db 'size=', '$'
