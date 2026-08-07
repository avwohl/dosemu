#include <stdio.h>
/* Dense switch -> jump table, like mp_binary_op's dispatch on op. */
int dispatch(int op) {
    switch (op) {
        case 0:  return 100;
        case 1:  return 101;
        case 2:  return 102;
        case 3:  return 103;
        case 4:  return 104;
        case 5:  return 105;
        case 6:  return 106;
        case 7:  return 107;
        case 8:  return 108;
        case 9:  return 109;
        case 10: return 110;
        case 11: return 111;
        case 12: return 112;
        case 13: return 113;
        case 14: return 114;
        case 15: return 115;
        default: return -1;
    }
}
int main(void) {
    int bad = 0;
    for (int i = 0; i < 16; i++) {
        int r = dispatch(i);
        if (r != 100 + i) { printf("MISMATCH op=%d got=%d\n", i, r); bad++; }
    }
    printf("jumptable bad=%d\n", bad);
    return 0;
}
