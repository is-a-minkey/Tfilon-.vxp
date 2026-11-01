#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/engine.h"
#include "../src/data.h"
void *plat_malloc(size_t n) { return malloc(n); }
void  plat_free(void *p) { free(p); }
int main(void) {
    static const char *nus[] = {"a", "s", "em"};
    static const char *pr[] = {"shacharit","mincha","arvit","mita","mazon","shalosh","shevaBrachot","brit","nefashot","yatzar","derech","levana","omer","nerot"};
    static const char *top[] = {"korona","tzahal","help","tikun_chatzot"};
    int maxl = 0, maxa = 0, nb = 0; long builds = 0; static long unk[256];
    int diaspora_city = -1, israel_city = 0;
    for (int i = 0; i < g_ncities; i++) if (!g_cities[i].israel) { diaspora_city = i; break; }
    int start = cal_greg_to_abs(2026, 8, 1);
    char name[64];
    txt_setup();
    for (int day = 0; day < 400; day += 3) {
        for (int after = 0; after < 2; after++) for (int ci = 0; ci < 2; ci++) {
            g_set.city_idx = ci ? diaspora_city : israel_city;
            DayInfo di; eng_make_dayinfo(&di, start + day + after);
            for (int n = 0; n < 3; n++) {
                for (int p = 0; p < 14 + (n == 0 ? 4 : 0); p++) {
                    if (p < 14) snprintf(name, sizeof name, "%s_%s", nus[n], pr[p]); else strcpy(name, top[p - 14]);
                    Doc d; if (!doc_build(&d, name, 236, &di)) { printf("BUILD FAIL %s\n", name); continue; }
                    builds++;
                    if (d.nlines > maxl) maxl = d.nlines; if (d.arena_len > maxa) maxa = d.arena_len; if (d.nbms > nb) nb = d.nbms;
                    if (d.nlines >= d.maxlines || d.arena_len > d.arena_cap - 2000) printf("NEAR LIMIT %s lines=%d arena=%d\n", name, d.nlines, d.arena_len);
                    for (int i = 0; i < d.nlines; i++) { int len; const uint8_t *l = doc_line(&d, i, &len); for (int k = 0; k < len; k++) if (l[k] != ' ' && g_glyph_of_byte[l[k]] == 255) unk[l[k]]++; }
                    doc_free(&d);
                }
            }
        }
    }
    printf("builds=%ld max_lines=%d max_arena=%d max_bookmarks=%d\n", builds, maxl, maxa, nb);
    for (int b = 0; b < 256; b++) if (unk[b]) printf("unmapped byte 0x%02x (%c) x%ld\n", b, b >= 32 && b < 127 ? b : '?', unk[b]);
    return 0;
}
