/* host test: build a prayer document and render a few screens to PPM files */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/engine.h"
#include "../src/data.h"

void *plat_malloc(size_t n) { return malloc(n); }
void  plat_free(void *p) { free(p); }

int main(int argc, char **argv) {
    const char *res = argv[1];
    int y = atoi(argv[2]), m = atoi(argv[3]), d = atoi(argv[4]), after = atoi(argv[5]);
    int top = argc > 6 ? atoi(argv[6]) : 0, nscreens = argc > 7 ? atoi(argv[7]) : 1;
    int font = argc > 8 ? atoi(argv[8]) : 1, ratio = argc > 9 ? atoi(argv[9]) : 1, W = 240, H = 296, city = argc > 10 ? atoi(argv[10]) : 0;
    g_set.font = font; g_set.ratio = ratio; g_set.city_idx = city;
    txt_setup();
    DayInfo di; eng_make_dayinfo(&di, cal_greg_to_abs(y, m - 1, d) + after);
    Doc doc;
    doc_build(&doc, res, W - 4, &di);
    fprintf(stderr, "doc %s: %d lines, arena %d bytes, %d marks, %d bookmarks, flags=%llx\n", res, doc.nlines, doc.arena_len, doc.nmarks, doc.nbms, (unsigned long long)di.flags);
    int c = txt_cell_h(), e = g_set.font;
    int rows = (int)(((float)H - (float)((3 - e) * g_set.ratio)) / ((float)c + (float)((5 - e) * g_set.ratio)));
    uint16_t *px = malloc(W * H * 2);
    for (int sc = 0; sc < nscreens; sc++) {
        FB fb = { px, W, H };
        for (int i = 0; i < W * H; i++) px[i] = RGB565(255, 255, 255);
        int yy = (3 - e) * g_set.ratio;
        for (int i = top + sc * rows; i < top + sc * rows + rows && i < doc.nlines; i++) {
            int len; const uint8_t *l = doc_line(&doc, i, &len);
            uint8_t r = 0, g = 0, b = 0; doc_color(&doc, i, &r, &g, &b);
            txt_draw(&fb, l, len, W, yy, RGB565(r, g, b));
            yy += c + (rows != 1 ? (H - rows * c - (3 - e) * g_set.ratio) / (rows - 1) : 0);
        }
        char fn[64]; sprintf(fn, "/tmp/tfilon/test/out_%d.ppm", sc);
        FILE *f = fopen(fn, "wb"); fprintf(f, "P6\n%d %d\n255\n", W, H);
        for (int i = 0; i < W * H; i++) { uint16_t v = px[i]; fputc(((v >> 11) & 31) * 255 / 31, f); fputc(((v >> 5) & 63) * 255 / 63, f); fputc((v & 31) * 255 / 31, f); }
        fclose(f);
    }
    return 0;
}
