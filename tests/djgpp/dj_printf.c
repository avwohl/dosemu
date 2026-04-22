/* dj_printf.c -- smoke test: printf with varied format specifiers.
 *
 * Verifies stdio buffering, format engine (%d, %s, %.*f, %lX), and
 * exit-code propagation.  Prints a sequence of lines culminating in
 * the marker "dj-printf=ok".
 */
#include <stdio.h>

int main(void) {
    printf("int=%d str=%s\n", 42, "hello");
    printf("flt=%.3f hex=0x%lX\n", 3.14159, 0xDEADBEEFUL);
    printf("dj-printf=ok\n");
    return 7;
}
