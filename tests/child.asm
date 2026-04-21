; child.asm — a trivial .COM program that's meant to be EXEC'd by
; spawn.asm via INT 21h AH=4Bh.  Prints a distinctive line and exits
; with code 0x42 so the parent can verify the exit code round-trip.
;
; Assemble:  nasm -f bin child.asm -o CHILD.COM

    org 100h

    mov ah, 9
    mov dx, msg
    int 21h

    mov ax, 4C42h      ; AH=4C exit, AL=0x42
    int 21h

msg db 'child=ran', 13, 10, '$'
