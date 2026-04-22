/* dj_write.c -- smoke test: direct write() to stdout.
 *
 * Verifies that DJGPP's lowest-level syscall path works: _write_int
 * → dosmemput → AH=40.  Produces the marker line "dj-write=ok".
 */
#include <unistd.h>
#include <string.h>

int main(void) {
    const char msg[] = "dj-write=ok\n";
    write(1, msg, sizeof(msg) - 1);
    return 0;
}
