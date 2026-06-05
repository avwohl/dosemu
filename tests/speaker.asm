; speaker.asm — sound the PC speaker via PIT channel 2 + port 0x61, print a
; marker, then exit WITHOUT silencing it.  The tone stays on, so
; DOSIZ_AUDIO_DUMP renders a non-silent square wave — regression gate for the
; PIT ch2 (0x42/0x43) + speaker gate (0x61) audio wiring.
    org 100h

    mov al, 0xB6        ; PIT ctrl: ch2, access lo+hi, mode 3 (square wave)
    out 0x43, al
    mov ax, 2712        ; reload = 1193182 / 2712 ~= 440 Hz
    out 0x42, al        ; low byte
    mov al, ah
    out 0x42, al        ; high byte

    in  al, 0x61        ; enable gate (bit0) + speaker data (bit1)
    or  al, 3
    out 0x61, al

    mov ah, 0x09
    mov dx, msg
    int 0x21

    mov ax, 0x4C00      ; exit; speaker left on for the audio dump
    int 0x21

msg db 'speaker-on', 0x0D, 0x0A, '$'
