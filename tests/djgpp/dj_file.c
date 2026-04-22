/* dj_file.c -- smoke test: fopen/fprintf/fclose/fopen/fgets round-trip.
 *
 * Exercises AH=3C (create via LFN fallback), AH=40 (write), AH=3E
 * (close), AH=3D (open for read), AH=3F (read), AH=42 (seek is not
 * used but fgets chains into 3F).  Creates djfile.tmp, writes 2
 * lines, reads them back, unlinks, then prints marker.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    const char *fname = "djfile.tmp";
    FILE *f = fopen(fname, "w");
    if (!f) { printf("FAIL: fopen w\n"); return 1; }
    fprintf(f, "first\nsecond\n");
    fclose(f);

    f = fopen(fname, "r");
    if (!f) { printf("FAIL: fopen r\n"); return 2; }
    char buf[64];
    if (!fgets(buf, sizeof(buf), f)) { printf("FAIL: fgets 1\n"); return 3; }
    if (strcmp(buf, "first\n") != 0)  { printf("FAIL: line1='%s'\n", buf); return 4; }
    if (!fgets(buf, sizeof(buf), f)) { printf("FAIL: fgets 2\n"); return 5; }
    if (strcmp(buf, "second\n") != 0) { printf("FAIL: line2='%s'\n", buf); return 6; }
    fclose(f);
    unlink(fname);
    printf("dj-file=ok\n");
    return 0;
}
