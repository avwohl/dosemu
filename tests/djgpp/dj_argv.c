/* dj_argv.c -- smoke test: argc/argv parsing from DOS cmd tail.
 *
 * Prints each argv as "[i]=<s>" lines, then "argc=<N>", then
 * "dj-argv=ok".  The test harness validates both the count and a
 * couple of specific values.
 */
#include <stdio.h>

int main(int argc, char **argv) {
    for (int i = 0; i < argc && i < 16; i++) {
        printf("[%d]=%s\n", i, argv[i]);
    }
    printf("argc=%d\n", argc);
    printf("dj-argv=ok\n");
    return 0;
}
