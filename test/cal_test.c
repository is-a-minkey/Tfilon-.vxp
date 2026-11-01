#include <stdio.h>
#include <stdlib.h>
#include "../src/cal.h"
int main(int argc, char **argv) {
    int y0 = atoi(argv[1]), y1 = atoi(argv[2]);
    int abs = cal_greg_to_abs(y0, 0, 1), end = cal_greg_to_abs(y1 + 1, 0, 1);
    for (; abs < end; abs++) {
        CalDate g, h;
        cal_abs_to_greg(abs, &g);
        cal_abs_to_hebrew(abs, &h);
        printf("%d %d %d %d %d %d %d %llx %llx %llx\n", g.y, g.m, g.d, abs, h.d, h.m, h.y,
               (unsigned long long)cal_flags(abs, 0, 1), (unsigned long long)cal_flags(abs, 1, 3), (unsigned long long)cal_flags(abs, 0, 2));
    }
    return 0;
}
