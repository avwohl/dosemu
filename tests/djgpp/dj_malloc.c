/* dj_malloc.c -- smoke test: malloc/free round-trip.
 *
 * Allocates 64 blocks of varying sizes, writes a distinctive pattern
 * to each, verifies the pattern reads back intact, frees them, then
 * prints the marker.  Exercises DJGPP's sbrk path + internal free
 * list, as well as our DPMI AX=0501 alloc/free.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    enum { N = 64, MAX = 2048 };
    char *p[N];
    size_t sz[N];
    for (int i = 0; i < N; i++) {
        sz[i] = 128 + (size_t)i * 32;      /* 128..2144 */
        p[i] = malloc(sz[i]);
        if (!p[i]) { printf("FAIL: malloc %d\n", i); return 1; }
        memset(p[i], 'A' + (i % 26), sz[i]);
    }
    /* Verify each block's contents survived the allocator's bookkeeping. */
    for (int i = 0; i < N; i++) {
        char expected = (char)('A' + (i % 26));
        for (size_t j = 0; j < sz[i]; j++) {
            if (p[i][j] != expected) {
                printf("FAIL: block %d byte %zu = 0x%02x\n", i, j, p[i][j] & 0xff);
                return 2;
            }
        }
    }
    long total = 0;
    for (int i = 0; i < N; i++) total += (long)sz[i];
    printf("blocks=%d total=%ld\n", N, total);
    for (int i = 0; i < N; i++) free(p[i]);
    printf("dj-malloc=ok\n");
    return 0;
}
