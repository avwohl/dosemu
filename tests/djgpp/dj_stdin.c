/* dj_stdin.c -- smoke test: stdin → stdout pass-through.
 *
 * Reads stdin until EOF and writes it back to stdout, appending the
 * marker line.  The test harness drives this by piping a known
 * string in.
 */
#include <stdio.h>

int main(void) {
    char buf[256];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), stdin)) > 0) {
        fwrite(buf, 1, n, stdout);
    }
    printf("dj-stdin=ok\n");
    return 0;
}
