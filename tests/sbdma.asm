; sbdma.asm — Sound Blaster 8-bit auto-init DMA playback + IRQ5 test.
;
; Programs the 8237 DMA controller (channel 1) and the SB DSP for auto-init
; 8-bit DAC output of a 16-sample square wave, installs an IRQ5 (INT 0Dh) ISR,
; and busy-waits for the block-end IRQ. Prints "sb-irq=ok" once the ISR has run.
; Auto-init keeps the buffer looping, so DOSIZ_AUDIO_DUMP also captures the
; DMA'd waveform (proving the SB fetched guest memory through mem_read).
;
; Exercises the whole chain: guest OUT -> EmuCpu::port_out -> hardware (DSP/DMA)
; -> render() consumes guest PCM -> raise_irq -> PIC -> run-loop delivery ->
; INT 0Dh ISR.
    org 100h
    cli

    ; --- install the IRQ5 handler at INT 0Dh (DS = CS for a .COM) ---
    mov ax, 0x250D          ; AH=25h set-vector, AL=0Dh
    mov dx, isr
    int 0x21

    ; --- reset the DSP: 1 then 0 to 0x226, then read 0xAA from 0x22A ---
    mov dx, 0x226
    mov al, 1
    out dx, al
    in  al, dx              ; brief delay
    in  al, dx
    xor al, al
    out dx, al
    mov cx, 2000
.rp:
    mov dx, 0x22E           ; DSP read-buffer status
    in  al, dx
    test al, 0x80
    jz  .rp_next
    mov dx, 0x22A           ; consume the 0xAA "ready" byte
    in  al, dx
    jmp .reset_done
.rp_next:
    loop .rp
.reset_done:

    ; --- physical address of the DMA buffer = (CS << 4) + offset ---
    xor eax, eax
    mov ax, cs
    shl eax, 4
    movzx ebx, word [bufptr]
    add eax, ebx
    mov [phys_lo], ax        ; A0-A15
    shr eax, 16
    mov [phys_pg], al        ; A16-A23 (8237 page register)

    ; --- program 8237 DMA channel 1: single mode, auto-init, read ---
    mov al, 0x05
    out 0x0A, al            ; mask channel 1
    mov al, 0xFF
    out 0x0C, al            ; clear byte-pointer flip-flop
    mov al, 0x59            ; single(0x40)+autoinit(0x10)+read(0x08)+ch1(0x01)
    out 0x0B, al
    mov ax, [phys_lo]
    out 0x02, al           ; address low
    mov al, ah
    out 0x02, al           ; address high
    mov al, [phys_pg]
    out 0x83, al           ; channel-1 page register
    mov ax, [blocklen]
    out 0x03, al           ; count low  (len-1)
    mov al, ah
    out 0x03, al           ; count high
    mov al, 0x01
    out 0x0A, al           ; unmask channel 1

    ; --- program the SB DSP (write port 0x22C; always ready) ---
    mov dx, 0x22C
    mov al, 0xD1
    out dx, al             ; speaker on
    mov al, 0x40
    out dx, al             ; set time constant...
    mov al, 0xA5
    out dx, al             ; ...~11 kHz
    mov al, 0x48
    out dx, al             ; set DMA block size...
    mov ax, [blocklen]
    out dx, al             ; ...len-1 low
    mov al, ah
    out dx, al             ; ...len-1 high
    mov al, 0x1C
    out dx, al             ; 8-bit auto-init DMA output -> start

    ; --- unmask IRQ5 at the master PIC, then wait for the block-end IRQ ---
    in  al, 0x21
    and al, 0xDF            ; clear bit 5 (IRQ5)
    out 0x21, al
    sti
    mov ecx, 20000000
.wait:
    cmp byte [irqcount], 0
    jne .got
    dec ecx
    jnz .wait
.got:
    cli
    cmp byte [irqcount], 0
    je  .fail
    mov dx, msg_ok
    jmp .print
.fail:
    mov dx, msg_no
.print:
    mov ah, 0x09
    int 0x21
    mov ax, 0x4C00          ; exit; SB left auto-init playing for the audio dump
    int 0x21

; --- IRQ5 service routine ---------------------------------------------------
isr:
    push ax
    push dx
    inc byte [cs:irqcount]
    mov dx, 0x22E           ; ack 8-bit DMA IRQ (read DSP status)
    in  al, dx
    mov al, 0x20            ; EOI to the master PIC
    out 0x20, al
    pop dx
    pop ax
    iret

; --- data -------------------------------------------------------------------
bufptr   dw buffer
phys_lo  dw 0
phys_pg  db 0
blocklen dw 15             ; 16-sample block (length-1)
irqcount db 0
msg_ok   db 'sb-irq=ok', 0x0D, 0x0A, '$'
msg_no   db 'sb-irq=no', 0x0D, 0x0A, '$'

align 2
buffer:
    times 8 db 0xFF
    times 8 db 0x00
