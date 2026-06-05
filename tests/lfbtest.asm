; lfbtest.asm — a DPMI protected-mode client writes the VESA linear
; framebuffer through a mapped selector.
;
; Flow:
;   1. (real mode) set VESA mode 0x101 (640x480x8) with the LFB bit, which
;      activates the SVGA VRAM + the LFB aperture at physical 0xE0000000.
;   2. enter 16-bit protected mode via the DPMI host.
;   3. DPMI AX=0800 maps physical 0xE0000000 -> a linear address (identity,
;      since the host has no paging).
;   4. allocate an LDT selector, point its base at that linear address, give
;      it a 4 MB limit.
;   5. write a pattern through the selector at offsets 0, 0x100 and 0x30000
;      (the last well past the 64 KB bank window), then read it all back.
;
; A correct round-trip proves the PM client's stores reach svga_vram through
; the aperture (and the renderer would display them). Prints "lfb-ok".
;
; Assemble:  nasm -f bin lfbtest.asm -o LFBTEST.COM

    org 100h
BITS 16

    ; --- (1) set VESA mode 0x101 with the linear framebuffer ---
    mov ax, 4F02h
    mov bx, 4101h           ; mode 0x101 | 0x4000 (linear framebuffer)
    int 10h
    cmp ax, 004Fh
    jne fail_mode

    ; --- (2) get the DPMI mode-switch entry and switch to 16-bit PM ---
    mov ax, 1687h
    int 2Fh
    test ax, ax
    jnz  fail_dpmi
    mov [entry_off], di
    mov [entry_seg], es
    cli
    xor ax, ax              ; 16-bit PM client
    call far [entry_off]
    test ax, ax
    jnz  fail_dpmi

    ; --- (3) map the LFB physical address into linear space ---
    mov ax, 0800h
    mov bx, 0E000h          ; physical base high  -> 0xE0000000
    xor cx, cx              ; physical base low
    mov si, 0040h           ; size high (0x00400000 = 4 MB)
    xor di, di              ; size low
    int 31h
    jc  fail_map
    mov [lin_hi], bx        ; linear == physical (no paging)
    mov [lin_lo], cx

    ; --- (4) allocate an LDT selector aimed at the LFB ---
    mov ax, 0000h
    mov cx, 1
    int 31h
    jc  fail_sel
    mov [lfb_sel], ax

    mov bx, ax              ; AX=0007: set base = linear
    mov ax, 0007h
    mov cx, [lin_hi]
    mov dx, [lin_lo]
    int 31h
    jc  fail_sel

    mov bx, [lfb_sel]       ; AX=0008: set limit = 4 MB - 1
    mov ax, 0008h
    mov cx, 003Fh
    mov dx, 0FFFFh
    int 31h
    jc  fail_sel

    ; --- a second selector aimed at the 0xA0000 bank-window aperture, which
    ;     views the SAME svga_vram[window_off + ..] -- used to prove the LFB
    ;     write reached video memory rather than scratch RAM. ---
    mov ax, 0000h
    mov cx, 1
    int 31h
    jc  fail_sel
    mov [win_sel], ax
    mov bx, ax              ; base = 0x000A0000
    mov ax, 0007h
    xor cx, cx
    mov cx, 000Ah
    mov dx, 0000h
    int 31h
    jc  fail_sel
    mov bx, [win_sel]       ; limit = 0xFFFF
    mov ax, 0008h
    xor cx, cx
    mov dx, 0FFFFh
    int 31h
    jc  fail_sel

    ; --- (5) write a pattern through the LFB selector, read it back, and
    ;     cross-check it through the 0xA0000 window view of the same VRAM ---
    mov es, [lfb_sel]
    xor edi, edi
    mov byte [es:edi], 0AAh        ; LFB offset 0 (pixel 0,0)
    mov edi, 100h
    mov dword [es:edi], 12345678h
    mov edi, 30000h               ; 192 KB in (past the 64 KB window)
    mov byte [es:edi], 55h

    xor edi, edi                  ; round-trip through the LFB selector
    cmp byte [es:edi], 0AAh
    jne fail_rb
    mov edi, 100h
    mov eax, [es:edi]
    cmp eax, 12345678h
    jne fail_rb
    mov edi, 30000h
    cmp byte [es:edi], 55h
    jne fail_rb

    ; decisive: the 0xA0000 window must see the LFB write at svga_vram[0]
    mov fs, [win_sel]
    xor edi, edi
    cmp byte [fs:edi], 0AAh
    jne fail_vram

    mov dx, ok_msg
    jmp done
fail_mode:
    mov dx, mode_msg
    jmp done
fail_dpmi:
    mov dx, dpmi_msg
    jmp done
fail_map:
    mov dx, map_msg
    jmp done
fail_sel:
    mov dx, sel_msg
    jmp done
fail_rb:
    mov dx, rb_msg
    jmp done
fail_vram:
    mov dx, vram_msg
done:
    mov ah, 9
    int 21h
    mov ax, 4C00h
    int 21h

entry_off dw 0
entry_seg dw 0
lin_hi    dw 0
lin_lo    dw 0
lfb_sel   dw 0
win_sel   dw 0
ok_msg    db 'lfb-ok', 0Dh, 0Ah, '$'
mode_msg  db 'lfb-mode-fail', 0Dh, 0Ah, '$'
dpmi_msg  db 'lfb-dpmi-fail', 0Dh, 0Ah, '$'
map_msg   db 'lfb-map-fail', 0Dh, 0Ah, '$'
sel_msg   db 'lfb-sel-fail', 0Dh, 0Ah, '$'
rb_msg    db 'lfb-rb-fail', 0Dh, 0Ah, '$'
vram_msg  db 'lfb-vram-fail', 0Dh, 0Ah, '$'
