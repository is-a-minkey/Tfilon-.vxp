/* host UI driver: replay a key script and dump screens as PPM. Keys: u d l r o (ok) s (left soft) S (right soft) # b(back) . (render) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/ui.h"
static int T_Y = 2026, T_MO = 10, T_D = 30, T_H = 14, T_M = 0;
void *plat_malloc(size_t n) { return malloc(n); }
void  plat_free(void *p) { free(p); }
void plat_time(int *y, int *mo, int *d, int *hh, int *mm) { *y = T_Y; *mo = T_MO; *d = T_D; *hh = T_H; *mm = T_M; }
static uint8_t cfg[16]; static int cfg_n = -1;
int  plat_cfg_read(uint8_t *buf, int cap) { if (cfg_n < 0) return -1; memcpy(buf, cfg, cfg_n < cap ? cfg_n : cap); return cfg_n; }
void plat_cfg_write(const uint8_t *buf, int len) { memcpy(cfg, buf, len); cfg_n = len; }
int main(int argc, char **argv) {
    const char *script = argv[1]; int W = 240, H = 320, shot = 0;
    if (argc > 5) { T_Y = atoi(argv[2]); T_MO = atoi(argv[3]); T_D = atoi(argv[4]); T_H = atoi(argv[5]); T_M = argc > 6 ? atoi(argv[6]) : 0; }
    uint16_t *px = malloc(W * H * 2); FB fb = { px, W, H };
    ui_init(W, H);
    for (const char *p = script; *p; p++) {
        int k = 0;
        switch (*p) { case 'u': k = K_UP; break; case 'd': k = K_DOWN; break; case 'l': k = K_LEFT; break; case 'r': k = K_RIGHT; break;
            case 'o': k = K_OK; break; case 's': k = K_SL; break; case 'S': k = K_SR; break; case '#': k = K_POUND; break; case 'b': k = K_BACK; break; }
        if (k) ui_key(k, 0);
        if (*p == '.') {
            ui_paint(&fb);
            char fn[64]; sprintf(fn, "/tmp/tfilon/test/ui_%d.ppm", shot++);
            FILE *f = fopen(fn, "wb"); fprintf(f, "P6\n%d %d\n255\n", W, H);
            for (int i = 0; i < W * H; i++) { uint16_t v = px[i]; fputc(((v >> 11) & 31) * 255 / 31, f); fputc(((v >> 5) & 63) * 255 / 63, f); fputc((v & 31) * 255 / 31, f); }
            fclose(f);
        }
        if (ui_exit_requested()) { fprintf(stderr, "exit requested\n"); break; }
    }
    ui_shutdown();
    return 0;
}
