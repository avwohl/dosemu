#!/bin/bash
# Verify djgpp-libc-c1loadef-stack-smash.patch fixes the bug.
#
# Downloads djlsr205.zip, extracts c1loadef.c, runs the test harness
# both UN-patched (to confirm the overflow crashes with SIGBUS/SEGV)
# and patched (to confirm clean behavior).  Requires a working C
# compiler, patch, unzip, curl.
#
# Run from anywhere; uses /tmp/ for scratch.

set -eu
cd "$(dirname "$0")"
patch_file="$(pwd)/djgpp-libc-c1loadef-stack-smash.patch"

scratch=$(mktemp -d)
trap "rm -rf $scratch" EXIT
cd "$scratch"

# Fetch DJGPP libc source if not already cached.
if [[ ! -f /tmp/djlsr205.zip ]]; then
    echo "==> Downloading djlsr205.zip from delorie.com"
    curl -sSL -o /tmp/djlsr205.zip \
        'https://www.delorie.com/pub/djgpp/current/v2/djlsr205.zip'
fi
unzip -q /tmp/djlsr205.zip 'src/libc/crt0/c1loadef.c'

# Empty stubs for headers we don't care about in the test.
mkdir -p stubs/libc
: > stubs/libc/stubs.h
: > stubs/crt0.h
: > stubs/io.h

# Test harness.
cat > test_c1loadef.c <<'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#define _open open
#define _read read
#define _close close

/* Non-DJGPP putenv doesn't copy; shim so our free(buf) doesn't dangle. */
static int my_putenv(char *s) { return putenv(strdup(s)); }
#define putenv(s) my_putenv((s))

#include "src/libc/crt0/c1loadef.c"

int main(void)
{
    const char *env_text =
        "DJDIR=C:/DJGPP\n"
        "PATH=%DJDIR%/BIN\n"
        "C_INCLUDE_PATH=%/>;C_INCLUDE_PATH%%DJDIR%/include\n"
        "PAD1=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
              "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
              "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
        "PAD2=bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
              "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
              "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\n"
        ;

    char path[] = "/tmp/djgpp.env.XXXXXX";
    int fd = mkstemp(path);
    write(fd, env_text, strlen(env_text));
    close(fd);

    setenv("DJGPP", path, 1);

    /* Pre-populate C_INCLUDE_PATH with 40 KB of text.  The recursive
       reference `%/>;C_INCLUDE_PATH%%DJDIR%/include` then pulls 40 KB
       into a buffer originally sized to the 300-byte input line.  */
    size_t big = 40 * 1024;
    char *preload = malloc(big + 1);
    memset(preload, 'x', big);
    preload[big] = 0;
    setenv("C_INCLUDE_PATH", preload, 1);
    free(preload);

    __crt0_load_environment_file("cpp.exe");
    unlink(path);

    const char *c_inc = getenv("C_INCLUDE_PATH");
    if (!c_inc) { fprintf(stderr, "FAIL: C_INCLUDE_PATH null\n"); return 1; }
    if (strlen(c_inc) < big) {
        fprintf(stderr, "FAIL: C_INCLUDE_PATH too short (%zu)\n", strlen(c_inc));
        return 1;
    }
    printf("ok: C_INCLUDE_PATH is %zu bytes\n", strlen(c_inc));
    return 0;
}
EOF

echo
echo "==> Building + running UN-patched (expect crash / nonzero rc)"
cc -O0 -Istubs -o test_orig test_c1loadef.c
if ./test_orig; then
    echo "WARNING: unpatched version ran clean -- test may not be triggering overflow"
    rc_orig=0
else
    rc_orig=$?
    echo "    unpatched rc=$rc_orig (expected nonzero; crash)"
fi

echo
echo "==> Applying patch"
patch -p1 < "$patch_file"

echo
echo "==> Building + running patched (expect success, rc=0)"
cc -O0 -Istubs -o test_patched test_c1loadef.c
./test_patched
echo "    patched rc=$? (expected 0)"

echo
if [[ $rc_orig -ne 0 ]]; then
    echo "VERIFIED: patch fixes the overflow."
else
    echo "INCONCLUSIVE: both versions ran clean (environment-dependent)."
fi
