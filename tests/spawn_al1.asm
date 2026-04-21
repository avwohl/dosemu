; spawn_al1.asm — INT 21h AH=4B AL=1 "Load without execute".
;
; Load CHILD.COM into a child memory region; verify the parameter
; block comes back with non-zero SS:SP and CS:IP.  We don't actually
; JMP to the child -- AL=1's primary use is for debuggers that want
; to set breakpoints before running, which we don't support.  The
; test just validates that the load succeeded and the handoff info
; is written correctly.
;
; Assemble:  nasm -f bin spawn_al1.asm -o SPAWN_AL1.COM

    org 100h

    mov ax, cs
    mov word [pblock + 0], 0            ; env_seg = inherit
    mov word [pblock + 2], cmdtail      ; cmdtail offset
    mov word [pblock + 4], ax           ; cmdtail seg
    mov word [pblock + 6], fcb1
    mov word [pblock + 8], ax
    mov word [pblock + 10], fcb2
    mov word [pblock + 12], ax
    ; +0E..+15 output fields; zero so a non-modification shows up.
    mov word [pblock + 14], 0
    mov word [pblock + 16], 0
    mov word [pblock + 18], 0
    mov word [pblock + 20], 0

    mov es, ax
    mov bx, pblock
    mov dx, childname
    mov ax, 4B01h                       ; AL=1 load without execute
    int 21h
    jc  fail_load

    ; Verify CS:IP and SS:SP were filled.
    mov ax, [pblock + 14]
    or  ax, [pblock + 16]
    or  ax, [pblock + 18]
    or  ax, [pblock + 20]
    jz  fail_empty

    mov dx, ok_msg
    mov ah, 9
    int 21h
    mov ax, 4C00h
    int 21h

fail_load:
    mov dx, fail_load_msg
    jmp p1
fail_empty:
    mov dx, fail_empty_msg
    jmp p1

p1:
    mov ah, 9
    int 21h
    mov ax, 4C01h
    int 21h

childname      db 'CHILD.COM', 0
cmdtail        db 0, 13
fcb1           times 16 db 0
fcb2           times 16 db 0
pblock         times 22 db 0           ; 14 input + 8 output bytes

ok_msg         db 'spawn-al1=ok', 13, 10, '$'
fail_load_msg  db 'spawn-al1=fail-load', 13, 10, '$'
fail_empty_msg db 'spawn-al1=fail-empty', 13, 10, '$'
