; dpmitest.com - Detect DPMI server and switch to protected mode
; Assemble: nasm -f bin -o dpmitest.com dpmitest.asm
org 100h

start:
    ; Detect DPMI (INT 2Fh AX=1687h)
    mov ax, 1687h
    int 2Fh
    test ax, ax
    jnz .no_dpmi

    ; DPMI found! Save entry point (ES:DI) and host data size (SI)
    mov [entry_off], di
    mov [entry_seg], es
    mov [host_para], si

    ; Print detection message
    mov ah, 09h
    mov dx, msg_found
    int 21h

    ; Print version (DH=major, DL=minor from the INT 2Fh call)
    ; DX was clobbered by INT 21h, so we saved nothing. Skip version print.

    ; Allocate private data for DPMI host
    mov bx, [host_para]
    test bx, bx
    jz .skip_alloc
    mov ah, 48h
    int 21h
    jc .no_mem
    mov es, ax
.skip_alloc:

    ; Switch to protected mode (16-bit client)
    mov ax, 0           ; 0 = 16-bit client
    call far [entry_off]
    jc .pm_fail

    ; We're in protected mode! CS/DS/ES/SS are valid selectors now.
    mov ah, 09h
    mov dx, msg_pm
    int 21h

    ; Test INT 31h AX=0400h - Get DPMI version
    mov ax, 0400h
    int 31h
    jc .skip_ver

    ; AH=major, AL=minor version
    push ax
    mov ah, 09h
    mov dx, msg_ver
    int 21h
    pop ax

    ; Print major version digit
    push ax
    mov dl, ah
    add dl, '0'
    mov ah, 02h
    int 21h
    mov dl, '.'
    mov ah, 02h
    int 21h
    pop ax

    ; Print minor version digit
    mov dl, al
    add dl, '0'
    mov ah, 02h
    int 21h

    ; Newline
    mov ah, 09h
    mov dx, msg_crlf
    int 21h

.skip_ver:
    ; Test INT 31h AX=0500h - Get free memory info
    mov ax, 0500h
    push es
    push ds
    pop es          ; ES = DS (our data segment)
    mov edi, meminfo
    int 31h
    pop es
    jc .skip_mem

    ; Print largest free block
    mov ah, 09h
    mov dx, msg_free
    int 21h

    ; Print the value in decimal (first dword = largest free block in bytes)
    mov eax, [meminfo]
    shr eax, 10     ; Convert to KB
    call print_dec

    mov ah, 09h
    mov dx, msg_kb
    int 21h

.skip_mem:
    ; Exit from protected mode
    mov ax, 4C00h
    int 21h

.no_dpmi:
    mov ah, 09h
    mov dx, msg_nodpmi
    int 21h
    mov ax, 4C01h
    int 21h

.no_mem:
    mov ah, 09h
    mov dx, msg_nomem
    int 21h
    mov ax, 4C01h
    int 21h

.pm_fail:
    mov ah, 09h
    mov dx, msg_pmfail
    int 21h
    mov ax, 4C01h
    int 21h

; Print EAX as unsigned decimal
print_dec:
    push ebx
    push ecx
    push edx
    mov ecx, 0
    mov ebx, 10
.div_loop:
    xor edx, edx
    div ebx
    push edx
    inc ecx
    test eax, eax
    jnz .div_loop
.print_loop:
    pop edx
    add dl, '0'
    mov ah, 02h
    int 21h
    loop .print_loop
    pop edx
    pop ecx
    pop ebx
    ret

; Data
entry_off   dw 0
entry_seg   dw 0
host_para   dw 0

msg_found   db 'DPMI server detected.', 13, 10, '$'
msg_pm      db 'Protected mode switch OK!', 13, 10, '$'
msg_ver     db 'DPMI version: $'
msg_free    db 'Free DPMI memory: $'
msg_kb      db ' KB', 13, 10, '$'
msg_crlf    db 13, 10, '$'
msg_nodpmi  db 'No DPMI server found.', 13, 10, '$'
msg_nomem   db 'Memory allocation failed.', 13, 10, '$'
msg_pmfail  db 'Protected mode switch FAILED!', 13, 10, '$'

align 4
meminfo:    times 48 db 0   ; DPMI memory info structure (12 dwords)
