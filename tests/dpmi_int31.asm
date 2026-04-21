; dpmi_int31.asm — calls INT 31h AX=0400h (get DPMI version) without
; having first probed via INT 2Fh.  With the stage-1 denial stub
; installed, the return is CF=1 / AX=8001h ("unsupported DPMI function")
; and the program continues.  Without the stub, the call dispatches
; through an un-installed IVT entry and the CPU crashes or runs off into
; unmapped memory.
;
; Prints "int31=denied" on CF=1/AX=8001h, "int31=handled" on anything
; else (which would surprise us at stage 1).
;
; Assemble:  nasm -f bin dpmi_int31.asm -o DPMI_INT31.COM

    org 100h

    mov ax, 0400h            ; DPMI get-version
    int 31h
    jnc  not_denied
    cmp ax, 8001h
    jne not_denied

    mov ah, 9
    mov dx, denied_msg
    int 21h
    mov ax, 4C00h
    int 21h

not_denied:
    mov ah, 9
    mov dx, handled_msg
    int 21h
    mov ax, 4C01h
    int 21h

denied_msg  db 'int31=denied', 13, 10, '$'
handled_msg db 'int31=handled', 13, 10, '$'
