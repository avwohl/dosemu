; ems_map.asm -- exercises the EMS page-mapping path (not just the probe).
;
; Verifies the copy-flush mapping is coherent:
;   1. AH=43 allocate a 4-page handle.
;   2. Map logical page 0 -> physical slot 0; write 0xAAAA into the frame.
;   3. Map logical page 1 -> slot 0 (flushes page 0 back, loads page 1);
;      write 0xBBBB.
;   4. Map logical page 0 -> slot 0 again; the 0xAAAA must still be there
;      (proves page 0 was flushed to its pool page and restored).
;   5. Map logical page 1 -> slot 0; the 0xBBBB must still be there.
;   6. AH=4C handle page count must be 4.
;   7. AH=45 deallocate.
; Prints ems-map=ok on success, or the failing step.
;
; Assemble:  nasm -f bin ems_map.asm -o EMS_MAP.COM

    org 100h

    mov ah, 43h
    mov bx, 4
    int 67h
    or  ah, ah
    jnz fail_alloc
    mov [handle], dx

    mov ax, 0E000h          ; page-frame segment
    mov es, ax

    ; map logical page 0 -> physical slot 0, stamp 0xAAAA
    mov ah, 44h
    xor al, al
    xor bx, bx
    mov dx, [handle]
    int 67h
    or  ah, ah
    jnz fail_map
    mov word [es:0], 0AAAAh

    ; map logical page 1 -> slot 0, stamp 0xBBBB
    mov ah, 44h
    xor al, al
    mov bx, 1
    mov dx, [handle]
    int 67h
    or  ah, ah
    jnz fail_map
    mov word [es:0], 0BBBBh

    ; map logical page 0 back -> slot 0; 0xAAAA must have survived
    mov ah, 44h
    xor al, al
    xor bx, bx
    mov dx, [handle]
    int 67h
    or  ah, ah
    jnz fail_map
    cmp word [es:0], 0AAAAh
    jne fail_data

    ; map logical page 1 -> slot 0; 0xBBBB must have survived
    mov ah, 44h
    xor al, al
    mov bx, 1
    mov dx, [handle]
    int 67h
    cmp word [es:0], 0BBBBh
    jne fail_data

    ; handle page count must be 4
    mov ah, 4Ch
    mov dx, [handle]
    int 67h
    or  ah, ah
    jnz fail_count
    cmp bx, 4
    jne fail_count

    mov ah, 45h
    mov dx, [handle]
    int 67h
    or  ah, ah
    jnz fail_dealloc

    mov dx, s_ok
    mov ah, 9
    int 21h
    mov ax, 4C00h
    int 21h

fail_alloc:   mov dx, m_alloc
              jmp short fc
fail_map:     mov dx, m_map
              jmp short fc
fail_data:    mov dx, m_data
              jmp short fc
fail_count:   mov dx, m_count
              jmp short fc
fail_dealloc: mov dx, m_dealloc
fc:
    mov ah, 9
    int 21h
    mov ax, 4C01h
    int 21h

handle    dw 0
s_ok      db 'ems-map=ok',13,10,'$'
m_alloc   db 'ems-map=fail-alloc',13,10,'$'
m_map     db 'ems-map=fail-map',13,10,'$'
m_data    db 'ems-map=fail-data',13,10,'$'
m_count   db 'ems-map=fail-count',13,10,'$'
m_dealloc db 'ems-map=fail-dealloc',13,10,'$'
