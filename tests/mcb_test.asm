; mcb_test.asm — proves the MCB allocator reuses memory after a free.
;
; Sequence:  alloc 0x100 paragraphs -> save seg; free; alloc 0x100 again.
; With a working free + forward-coalesce, the second allocation returns
; the same segment as the first.  A bump allocator would return a
; different one.
;
; Prints:
;   alloc-free-alloc-same   on success
;   alloc-free-alloc-diff   if the allocator didn't reuse the block
;   alloc-fail              if any AH=48h/49h returned CF=1
;
; Assemble:  nasm -f bin mcb_test.asm -o MCB_TEST.COM

    org 100h

    ; First allocation.
    mov ah, 48h
    mov bx, 100h
    int 21h
    jc  fail
    push ax                  ; save segment

    ; Free it.
    mov es, ax
    mov ah, 49h
    int 21h
    jc  fail

    ; Second allocation, same size.
    mov ah, 48h
    mov bx, 100h
    int 21h
    jc  fail

    pop bx                   ; first segment
    cmp ax, bx
    jne different

    mov ah, 9
    mov dx, same_msg
    int 21h
    mov ax, 4C00h
    int 21h

different:
    mov ah, 9
    mov dx, diff_msg
    int 21h
    mov ax, 4C01h
    int 21h

fail:
    mov ah, 9
    mov dx, fail_msg
    int 21h
    mov ax, 4C02h
    int 21h

same_msg db 'alloc-free-alloc-same', 13, 10, '$'
diff_msg db 'alloc-free-alloc-diff', 13, 10, '$'
fail_msg db 'alloc-fail', 13, 10, '$'
