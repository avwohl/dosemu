; adlib.asm — program the AdLib/OPL2 to key on a note, print a marker, then
; exit WITHOUT keying off.  The note stays sounding, so DOSIZ_AUDIO_DUMP renders
; a non-silent tone — a regression gate for the guest-OUT -> OPL audio wiring.
    org 100h

; OPL reg write: index -> port 0x388, value -> port 0x389 (DX-addressed, since
; the ports are above 0xFF and OUT imm8 only reaches ports 0-255).
%macro OPL 2
    mov dx, 0x388
    mov al, %1
    out dx, al
    inc dx              ; 0x389
    mov al, %2
    out dx, al
%endmacro

    ; Classic AdLib "play a note" patch on channel 0 (modulator slot 0,
    ; carrier slot 3), then key-on with block/F-number.
    OPL 0x20, 0x01      ; modulator: multiple = 1
    OPL 0x40, 0x10      ; modulator: level ~ -40 dB
    OPL 0x60, 0xF0      ; modulator: fast attack, long decay
    OPL 0x80, 0x77      ; modulator: medium sustain/release
    OPL 0xA0, 0x98      ; channel 0: F-number low byte
    OPL 0x23, 0x01      ; carrier: multiple = 1
    OPL 0x43, 0x00      ; carrier: max volume
    OPL 0x63, 0xF0      ; carrier: fast attack, long decay
    OPL 0x83, 0x77      ; carrier: medium sustain/release
    OPL 0xB0, 0x31      ; channel 0: key-on + block 6 + F-number high

    mov ah, 0x09
    mov dx, msg
    int 0x21

    mov ax, 0x4C00      ; exit; note left keyed on for the audio dump
    int 0x21

msg db 'adlib-note-on', 0x0D, 0x0A, '$'
