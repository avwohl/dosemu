; envdump.asm — reads PSP:[2Ch] to find the env segment, then writes the
; first NUL-terminated variable to stdout followed by a newline.  Exits 1
; if PSP:[2Ch] is zero (env segment missing).
;
; Assemble:  nasm -f bin envdump.asm -o ENVDUMP.COM

    org 100h

    mov ax, [2Ch]             ; env segment from PSP
    test ax, ax
    jz  no_env

    mov es, ax
    xor si, si
write_char:
    mov al, es:[si]
    cmp al, 0
    je  done
    mov dl, al
    mov ah, 02h
    int 21h
    inc si
    jmp write_char

done:
    mov dl, 10
    mov ah, 02h
    int 21h
    mov ax, 4C00h
    int 21h

no_env:
    mov dx, no_env_msg
    mov ah, 09h
    int 21h
    mov ax, 4C01h
    int 21h

no_env_msg db 'no-env', 13, 10, '$'
