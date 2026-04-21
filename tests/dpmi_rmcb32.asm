; dpmi_rmcb32.asm — 32-bit variant of DPMI_RMCB.COM.  Same end-to-end
; round-trip, but the PM callback's CS descriptor has D=1 (32-bit).
; Exercises do_rm_callback's 8-byte RETF frame path.
;
; Setup:
;   1. 16-bit PM client allocates an LDT descriptor (AX=0000).
;   2. Uses AX=0007 to set its base to the client's code base and
;      AX=0009 to set access rights = 0x9A (code, readable) with
;      D=1 (32-bit).
;   3. Registers a PM callback whose CS is that new selector, procedure
;      offset is `pm_cb32` in the .COM body.
;   4. Invokes it via AX=0301 / RM trampoline.
;   5. Callback (running as 32-bit code) writes 0xDEAD to struct.EAX
;      and RETFs.
;   6. Fixture verifies.
;
; Assemble:  nasm -f bin dpmi_rmcb32.asm -o DPMI_RMCB32.COM

    org 100h

BITS 16
    mov ax, 1687h
    int 2Fh
    test ax, ax
    jnz  not_present
    mov [entry_off], di
    mov [entry_seg], es

    mov ax, ds
    mov [saved_ds_rm], ax

    cli
    xor ax, ax                      ; 16-bit PM
    call far [entry_off]
    test ax, ax
    jnz  switch_failed

    ; Alloc 1 LDT descriptor
    mov ax, 0000h
    mov cx, 1
    int 31h
    jc  fail_alloc
    mov [sel32], ax

    ; Point it at our code (base = client CS base).  In 16-bit PM our
    ; CS base == saved_ds_rm * 16.  Compute CX:DX linear base.
    mov ax, [saved_ds_rm]
    mov dx, ax
    shl dx, 4
    mov cx, ax
    shr cx, 12
    mov ax, 0007h
    mov bx, [sel32]
    int 31h
    jc  fail

    ; AX=0009 set access rights: CL = 0x9A (code, r, NC, DPL=0, P),
    ; CH = 0x40 (flags nibble high = G=0 D=1 AVL=0 => 0x40).
    mov ax, 0009h
    mov bx, [sel32]
    mov cl, 9Ah
    mov ch, 40h
    int 31h
    jc  fail

    ; Zero the struct
    mov cx, 50
    mov di, rmcs
    xor ax, ax
    rep stosb

    ; AX=0303 register callback.  DS:SI = our 32-bit alias : pm_cb32,
    ; ES:DI = struct.  Stash the return values in BP:BX before
    ; restoring DS -- the result registers survive a segment-register
    ; load but fixture-data writes need DS back to a writable data
    ; selector.
    mov ax, ds
    mov es, ax
    mov bp, ax                      ; save original DS
    mov di, rmcs                    ; ES:DI = struct address
    mov ax, [sel32]
    mov ds, ax
    mov si, pm_cb32
    mov ax, 0303h
    int 31h
    jc  fail_0303_nosave
    ; Stash return values before restoring DS
    push cx
    push dx
    mov ax, bp
    mov ds, ax                      ; DS back to PM_DS_SEL
    pop dx
    pop cx
    mov [rm_cb_off], dx
    mov [rm_cb_seg], cx

    ; Populate struct CS:IP pointing at rm_proc in our RM segment.
    mov ax, [saved_ds_rm]
    mov [rmcs + 0x24], ax           ; DS
    mov [rmcs + 0x22], ax           ; ES
    mov [rmcs + 0x2C], ax           ; CS
    mov word [rmcs + 0x2A], rm_proc

    ; AX=0301 invoke rm_proc
    mov ax, ds
    mov es, ax
    mov ax, 0301h
    mov bh, 0
    xor cx, cx
    mov di, rmcs
    int 31h
    jc  fail_0301

    ; Verify struct.EAX = 0xDEAD
    mov ax, [rmcs + 0x1C]
    cmp ax, 0DEADh
    jne fail_verify

    ; Free callback + LDT slot
    mov cx, [rm_cb_seg]
    mov dx, [rm_cb_off]
    mov ax, 0304h
    int 31h

    mov ax, 0001h
    mov bx, [sel32]
    int 31h

    mov dx, ok_msg
    mov ah, 9
    int 21h
    mov ax, 4C00h
    int 21h

; ---- 32-bit PM callback.  Entered with cpu.code.big=1 so instructions
; are decoded as 32-bit.  ES:EDI points at struct (ES = struct_sel =
; our DS alias).  Writes struct.EAX = 0xDEAD, RETFs.  The RETF is
; 32-bit because we're in a 32-bit CS -- pops 8 bytes, matching what
; do_rm_callback pushed when cb_big was true.
BITS 32
pm_cb32:
    mov dword [es:edi + 0x1C], 0DEADh
    retf
BITS 16

; ---- RM trampoline
rm_proc:
    call far [rm_cb_off]
    retf

fail_alloc:
    mov dx, fail_alloc_msg
    jmp p1
fail_0303_nosave:
    ; DS may still be pointing at sel32 (code-readable).  Restore to
    ; a writable data selector before printing.
    mov ax, bp
    mov ds, ax
fail_0303:
    mov dx, fail_0303_msg
    jmp p1
fail_0301:
    mov dx, fail_0301_msg
    jmp p1
fail_verify:
    mov dx, fail_verify_msg
    jmp p1
fail:
    mov dx, fail_msg
    jmp p1

p1:
    mov ah, 9
    int 21h
    mov ax, 4C01h
    int 21h

not_present:
    mov dx, absent_msg
    mov ah, 9
    int 21h
    mov ax, 4C02h
    int 21h

switch_failed:
    mov dx, failed_msg
    mov ah, 9
    int 21h
    mov ax, 4C01h
    int 21h

entry_off        dw 0
entry_seg        dw 0
saved_ds_rm      dw 0
sel32            dw 0
rm_cb_off        dw 0
rm_cb_seg        dw 0

rmcs             times 50 db 0

ok_msg           db 'dpmi-rmcb32=ok', 13, 10, '$'
fail_alloc_msg   db 'dpmi-rmcb32=fail-alloc', 13, 10, '$'
fail_0303_msg    db 'dpmi-rmcb32=fail-0303', 13, 10, '$'
fail_0301_msg    db 'dpmi-rmcb32=fail-0301', 13, 10, '$'
fail_verify_msg  db 'dpmi-rmcb32=fail-verify', 13, 10, '$'
fail_msg         db 'dpmi-rmcb32=fail', 13, 10, '$'
absent_msg       db 'dpmi-rmcb32=absent', 13, 10, '$'
failed_msg       db 'dpmi-rmcb32=switch-failed', 13, 10, '$'
