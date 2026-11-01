#ifndef ENGINE_H
#define ENGINE_H
#include <stdint.h>
#include <stddef.h>
#include "cal.h"
#include "data.h"

typedef struct { uint16_t *px; int w, h; } FB;
#define RGB565(r,g,b) ((uint16_t)((((r)&0xF8)<<8)|(((g)&0xFC)<<3)|(((b)&0xF8)>>3)))

typedef struct {
    int nusach;               /* 0 = not chosen, 1 = a (Ashkenaz), 2 = s (Sepharad), 3 = em (Edot HaMizrach) */
    int purim;                /* 1 = YD, 2 = TU, 3 = both */
    int censor;
    int font;                 /* 1 or 2 */
    int ratio;                /* integer glyph scale */
    int night;
    int city_idx;             /* index into g_cities */
    int summer;
    int vaanenu, doctor;
    uint8_t spec[3], kot[3];
    int full, spec_i, kot_i;  /* default full-screen reader, colour choices (0 red,1 blue,2 green,3 purple) */
} Settings;
extern Settings g_set;

/* platform hooks */
void *plat_malloc(size_t n);
void  plat_free(void *p);

/* day information used by the prayer-text interpreter */
typedef struct {
    uint64_t flags;           /* flags of "today" or, after nightfall, "tomorrow" */
    int diaspora, sukkot_day, chanuka_day, omer_day, weekday;
} DayInfo;
void eng_make_dayinfo(DayInfo *di, int abs_effective);

/* text */
int  uni_to_internal(const char *utf8, uint8_t *out, int cap);
extern int g_txt_ratio_override;
void txt_setup(void);
int  txt_cell_h(void);
int  txt_width(const uint8_t *s, int len);
void txt_fill(FB *fb, int x, int y, int w, int h, uint16_t c);
void txt_draw(FB *fb, const uint8_t *s, int len, int xr, int y, uint16_t color);

/* document */
typedef struct { int line; uint8_t r, g, b; } Mark;
typedef struct { int line; int title_off; } Bookmark;
typedef struct {
    uint8_t *arena; int arena_len, arena_cap;
    int *off; int nlines, maxlines;
    Mark marks[256]; int nmarks;
    Bookmark bms[160]; int nbms;
} Doc;
int  doc_build(Doc *d, const char *resname, int wrap_w, const DayInfo *di);
int  doc_build_text(Doc *d, const char *utf8, int wrap_w, const DayInfo *di);
void doc_free(Doc *d);
const uint8_t *doc_line(const Doc *d, int i, int *len);
int  doc_color(const Doc *d, int line, uint8_t *r, uint8_t *g, uint8_t *b);

/* raw resource access (decompressed copy; caller frees with plat_free) */
uint8_t *res_load(const char *name, int *len);
void eng_sun_times(int abs, const City *c, int summer, int *sunrise, int *sunset);
#endif
