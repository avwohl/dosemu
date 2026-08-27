# The original brief

This is the founding prompt for dosiz, in the owner's own words. It lived at
the bottom of `todo.txt` under a `--- original brief ---` marker from the start
of the project until that file was rewritten as a list of open items. It is
kept here because it is the record of what the project was asked to be, not
because any of it is outstanding work.

Read it as of 2026-04-21, the date of `d34c72c` "Initial scaffolding for
dosemu". Two of its assumptions did not survive. "Here use dosbox for the 80386
emulator" held for the first 251 commits and was replaced by the emu88 core in
`4014c3a`, commit 252 of the 286 on main today. And the project was called
`dosemu` until `90169f2` renamed it at commit 224. What became of the rest is
in `CHANGELOG.md`; what is still open is in `todo.txt`.

    create public repo avwohl/dosiz
    create an msdos emulator
    for a template of how it should work see ../cpmemu
     that provides a z80 emulator and translates CP/M system
     calls to linux.  Here use dosbox for the 80386 emulator
     and provide fake msdos calls that translate to linux.
    Most emulators use native disks (dos, cp/m) when doing
     development this involves a lot moving files in and out
     of native disks.  By translating system calls this is avoided
    Note the .cfg section of cpmemu for specifying options and
     end of line translations
    This emulator should provide DPMI emulation as well as msdos.
    For a refrence of what msdos does I suggest https://freedos.org/
     It is free open source. Feel free to use code , however be sure to
     credit it and keep the copyright and provide a copy of its source code if you do.
    write as much as possible in c++
    for command line dos tools like compilers there should be a way to run this
     emulator as a command line linux tool. For example if a dos c compiler is run
     it need not create a window.
    It will probably be nessary to povide video card emulation like ../qxDOS

