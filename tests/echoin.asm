; echoin.asm — reads stdin with AH=01h (auto-echo) until CR, then exits.
; Verifies: piped stdin reaches the DOS program, AH=01 echoes the byte to
; stdout, and LF from the host is reported as CR back to the guest.
;
; Assemble:  nasm -f bin echoin.asm -o ECHOIN.COM

    org 100h
loop_:
    mov ah, 01h
    int 21h
    cmp al, 0            ; 0 means host EOF -- bail.
    je  done
    cmp al, 13           ; CR terminates.
    jne loop_
done:
    mov ax, 4C00h
    int 21h
