# Changelog

## Version 1.1 (Build 21)

### 386 Protected Mode & DPMI
- Full 386 protected mode: GDT/LDT/IDT descriptor tables, ring 0–3 privilege transitions, call gates, TSS-based stack switching
- Virtual 8086 (V86) mode with IOPL-sensitive instruction trapping
- Paging: 2-level page tables, 4KB and 4MB (PSE) pages, U/S and R/W protection, accessed/dirty bits, page fault exceptions
- Exception handling with proper error codes; double/triple fault detection
- System instructions: LGDT, LIDT, LLDT, LTR, SGDT, SIDT, SLDT, STR, SMSW, LMSW, VERR, VERW, LAR, LSL, ARPL, CLTS, INVLPG, MOV CR0–CR4, MOV DR0–DR7, CPUID, RDTSC
- CWSDPMI loaded at boot for DPMI support (DOS extenders, 32-bit games)
- DPMITEST.COM diagnostic tool included on FreeDOS HDD

### CPU Selector
- Runtime CPU type selection: 8088, 286, or 386
- RAM scales by CPU: 1 MB (8088/186), 16 MB (286), 64 MB (386)

### Speed Modes
- Added 386SX 16 MHz, 386DX 33 MHz, 486DX2 66 MHz speed modes with cycle multipliers calibrated to 8088-based cycle table

### Test Suite Validation
- **8088**: [SingleStepTests/8088](https://github.com/SingleStepTests/8088) — 100% pass rate (557K tests). Includes SST-specific fixes for IMUL/IDIV carry/overflow, DAA/DAS OF, AAA/AAS carry.
- **286**: Mostly passes — known edge cases with IMUL/IDIV sign-extension corner cases
- **386**: [barotto/test386.asm](https://github.com/barotto/test386.asm) — 100% pass (POST 0xFF). Full protected mode, paging, exceptions, V86 mode, privilege levels.

### Bug Fixes
- Fixed BIOS trap IRET flag propagation: CF/ZF/SF/OF from BIOS handlers now correctly returned through the interrupt chain (fixes status flags under V86 reflection for DPMI servers)
- Segment cache updated on BIOS trap IRET via load_segment_real()

---

## Version 1.0.0 (Build 4)

Initial release.

### Emulator Core
- 8088/80186/386 CPU emulator (real mode) with 245 unit tests
- FreeDOS 1.4 boots successfully (386 CPU detection passes)
- CGA, MDA, and Hercules display adapters
- INT 33h mouse driver with touch-to-mouse mapping
- Floppy, hard disk, and CD-ROM ISO image support
- NE2000 (DP8390) Ethernet adapter emulation
- Speed control: Full speed, IBM PC 4.77 MHz, IBM AT 8 MHz, Turbo 25 MHz

### File Transfer
- R.COM and W.COM utilities for host-to-DOS and DOS-to-host file transfer
- INT E0h host file service interface

### App Features
- Disk image catalog with download from GitHub releases
- Download disk images from any URL
- Named configuration profiles
- Create blank floppy and hard disk images
- Boot drive selection (floppy, hard disk, CD-ROM)
- In-app help browser with remote content
- Keyboard toolbar with Ctrl, Esc, Tab, arrow keys
- Copy/paste support
- Mac Catalyst support
