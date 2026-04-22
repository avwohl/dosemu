/* dj_signal.c -- smoke test: SIGFPE handler + longjmp recovery.
 *
 * DJGPP maps x86 #DE (divide error) to POSIX SIGFPE.  We install a
 * handler, trigger a divide-by-zero, and verify the handler ran by
 * longjmp'ing back out of it to a known label.  Exercises our PM
 * exception dispatch + CWSDPMI-style frame construction + the
 * user-handler-LRETs-through-user_exception_return path.
 *
 * Prints "caught" from inside the handler, then "dj-signal=ok" on
 * the normal path after longjmp recovery.
 */
#include <stdio.h>
#include <signal.h>
#include <setjmp.h>
#include <stdlib.h>

static sigjmp_buf g_env;

static void handler(int sig) {
    printf("caught sig=%d\n", sig);
    siglongjmp(g_env, 1);
}

int main(void) {
    signal(SIGFPE, handler);
    if (sigsetjmp(g_env, 1) == 0) {
        volatile int a = 7, b = 0;
        volatile int c = a / b;  /* #DE -> SIGFPE */
        printf("UNEXPECTED: div returned %d\n", c);
        return 1;
    }
    /* Second time through: siglongjmp landed here. */
    printf("dj-signal=ok\n");
    return 0;
}
