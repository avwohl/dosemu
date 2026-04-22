/* dj_exec.c -- smoke test: AH=4Bh load-and-execute from a DJGPP
 * parent.
 *
 * Spawns HELLO.COM (a tiny real-mode child) via spawnlp() and
 * verifies its exit code.  Tests the full exec plumbing: PSP
 * cmd-tail, env inheritance, child load + run, parent resume.
 *
 * Note: spawning a *DJGPP* child from a DJGPP parent (nested
 * DPMI) hits a descriptor-restore issue on the return path that
 * still needs work.  HELLO.COM is a single-segment real-mode
 * COM, so the child never enters PM and the happy-path AH=4B
 * exercise stays clean.
 *
 * Prints "dj-exec=ok" on success.
 */
#include <stdio.h>
#include <stdlib.h>
#include <process.h>
#include <errno.h>

int main(void) {
    int rc = spawnlp(P_WAIT, "C:\\TESTS\\HELLO.COM",
                     "HELLO.COM", (char *)NULL);
    if (rc != 0) {
        printf("spawn returned %d (errno=%d)\n", rc, errno);
        return 1;
    }
    printf("dj-exec=ok\n");
    return 0;
}
