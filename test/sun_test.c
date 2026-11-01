#include <stdio.h>
#include <limits.h>
#include "../src/engine.h"
#include "../src/data.h"
void *plat_malloc(size_t n) { return 0; } void plat_free(void *p) {}
int main(void) {
    for (int idx = 0; idx < g_ncities; idx++) for (int summer = 0; summer < 2; summer++)
        for (int mon = 0; mon < 12; mon++) { int days[2] = {3, 21}; for (int k = 0; k < 2; k++) {
            int abs = cal_greg_to_abs(2026, mon, days[k]), sr, ss;
            eng_sun_times(abs, &g_cities[idx], summer, &sr, &ss);
            printf("%d %d %d %d %d\n", idx, summer, abs, sr, ss); } }
    return 0;
}
