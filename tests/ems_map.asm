; ems_map.asm -- exercises the EMS page-mapping path (not just the probe).
;
; Verifies copy-flush mapping coherence, handle naming (AH=53), and the
; move-region function (AH=57):
;   1. AH=43 allocate a 4-page handle.
;   2. Map logical page 0 -> physical slot 0; write 0xAAAA into the frame.
;   3. Map logical page 1 -> slot 0 (flushes page 0 back, loads page 1);
;      write 0xBBBB.
;   4. Map logical page 0 -> slot 0 again; the 0xAAAA must still be there.
;   5. Map logical page 1 -> slot 0; the 0xBBBB must still be there.
;   6. AH=53 set then get the handle name; must round-trip.
;   7. AH=57 move 8 bytes conventional -> EMS handle logical page 2; map
;      page 2 and confirm the bytes landed.
;   8. AH=4C handle page count must be 4.
;   9. AH=45 deallocate.
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

    ; ---- AH=53 handle name round-trip (buffer at ES:DI for both) ----
    push es
    mov ax, ds
    mov es, ax                 ; ES = DS so ES:DI addresses our buffers
    mov ah, 53h
    mov al, 1                  ; set name
    mov dx, [handle]
    mov di, set_name
    int 67h
    or  ah, ah
    jnz fail_name_pop
    mov ah, 53h
    xor al, al                 ; get name
    mov dx, [handle]
    mov di, got_name
    int 67h
    or  ah, ah
    jnz fail_name_pop
    pop es
    ; compare set_name vs got_name (8 bytes)
    mov si, set_name
    mov di, got_name
    mov cx, 8
.cmpname:
    mov al, [si]
    cmp al, [di]
    jne fail_name
    inc si
    inc di
    loop .cmpname

    ; ---- AH=57 move 8 bytes: conventional srcbuf -> EMS handle page 2 ----
    mov ax, ds
    mov [mr_src_seg], ax           ; src_page_seg = DS (conventional)
    mov word [mr_src_off], srcbuf  ; src_offset
    mov ax, [handle]
    mov [mr_dst_handle], ax
    mov ah, 57h
    xor al, al                     ; move
    mov si, mr
    int 67h
    or  ah, ah
    jnz fail_move
    ; map EMS logical page 2 -> slot 0 and verify the 8 bytes landed
    mov ah, 44h
    xor al, al
    mov bx, 2
    mov dx, [handle]
    int 67h
    or  ah, ah
    jnz fail_move
    mov si, srcbuf
    xor di, di
    mov cx, 8
.cmp57:
    mov al, [si]
    cmp al, [es:di]
    jne fail_move
    inc si
    inc di
    loop .cmp57

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

fail_name_pop: pop es
fail_name:    mov dx, m_name
              jmp short fc
fail_alloc:   mov dx, m_alloc
              jmp short fc
fail_map:     mov dx, m_map
              jmp short fc
fail_data:    mov dx, m_data
              jmp short fc
fail_move:    mov dx, m_move
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
set_name  db 'EMSTEST0'
got_name  db '????????'
srcbuf    db 'MOVEDATA'
; MoveRegion: bytes(4) src_type(1) src_handle(2) src_off(2) src_seg(2)
;             dest_type(1) dest_handle(2) dest_off(2) dest_seg(2)
mr:
mr_bytes      dd 8
mr_src_type   db 0
mr_src_handle dw 0
mr_src_off    dw 0
mr_src_seg    dw 0
mr_dst_type   db 1
mr_dst_handle dw 0
mr_dst_off    dw 0
mr_dst_seg    dw 2        ; dest logical page 2

s_ok      db 'ems-map=ok',13,10,'$'
m_alloc   db 'ems-map=fail-alloc',13,10,'$'
m_map     db 'ems-map=fail-map',13,10,'$'
m_data    db 'ems-map=fail-data',13,10,'$'
m_name    db 'ems-map=fail-name',13,10,'$'
m_move    db 'ems-map=fail-move',13,10,'$'
m_count   db 'ems-map=fail-count',13,10,'$'
m_dealloc db 'ems-map=fail-dealloc',13,10,'$'
