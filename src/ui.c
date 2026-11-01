/* Tfilon UI: menus, prayer reader, settings. Screens mirror the original MIDlet's structure. */
#include <string.h>
#include <limits.h>
#include "ui.h"

#include "ui_strs_gen.inc"

typedef struct { uint8_t t[80]; int n; int id; } Item;
enum { SC_MAIN, SC_NUSACH, SC_PRAYERS, SC_READER, SC_BOOKMARKS, SC_SETTINGS, SC_CITIES, SC_INFO, SC_ABOUT };

static int scr, sw, sh, quit_flag;
static Item items[130]; static int nitems, sel, scroll_pos;
static uint8_t title[80]; static int title_n;
static Doc doc; static int doc_ok, top_line, fullscr, reader_back;
static DayInfo dinfo; static int abs_today, after_sunset;
static uint16_t C_BG, C_FG, C_BAR, C_BARTXT, C_SEL;

/* ---------------- helpers ---------------- */
static int ui_d(void) { int r = g_txt_ratio_override > 0 ? g_txt_ratio_override : g_set.ratio; return r < 1 ? 1 : r; }
static int is_reader_screen(void);
static void set_colors(void) {
    if (g_set.night) { C_BG = RGB565(0, 0, 0); C_FG = RGB565(255, 255, 255); C_BAR = RGB565(40, 40, 100); C_BARTXT = RGB565(255, 255, 255); C_SEL = RGB565(70, 70, 150); }
    else             { C_BG = RGB565(255, 255, 255); C_FG = RGB565(0, 0, 0); C_BAR = RGB565(30, 60, 130); C_BARTXT = RGB565(255, 255, 255); C_SEL = RGB565(185, 210, 255); }
}
static int bar_h(void) { return (g_set.font == 2 ? 15 : 19) + 6; }   /* chrome is always drawn at scale 1 */
static void set_title(const char *u) { title_n = uni_to_internal(u, title, sizeof title); }
static void item_clear(void) { nitems = 0; sel = 0; scroll_pos = 0; }
static void item_add(const char *u, int id) {
    Item *it;
    if (nitems >= 130) return;
    it = &items[nitems++]; it->n = uni_to_internal(u, it->t, sizeof it->t); it->id = id;
}
static void item_add_raw(const uint8_t *t, int n, int id) {
    Item *it;
    if (nitems >= 130) return;
    it = &items[nitems++];
    if (n > (int)sizeof it->t - 1) n = (int)sizeof it->t - 1;
    memcpy(it->t, t, n); it->t[n] = 0; it->n = n; it->id = id;
}
static void ltr_text(FB *fb, const char *u, int xl, int y, uint16_t col) {      /* draw a UTF-8 label starting at left edge xl */
    uint8_t b[80]; int n = uni_to_internal(u, b, sizeof b), w = txt_width(b, n);
    txt_draw(fb, b, n, xl + w, y, col);
}
static const uint8_t PAL[4][3] = {{255, 0, 0}, {0, 0, 255}, {0, 150, 0}, {190, 50, 215}};
static void apply_colors(void) {
    int k;
    if (g_set.spec_i < 0 || g_set.spec_i > 3) g_set.spec_i = 0;
    if (g_set.kot_i < 0 || g_set.kot_i > 3) g_set.kot_i = 1;
    for (k = 0; k < 3; k++) { g_set.spec[k] = PAL[g_set.spec_i][k]; g_set.kot[k] = PAL[g_set.kot_i][k]; }
}
static void cfg_save(void) {
    uint8_t b[16];
    b[0] = 'T'; b[1] = 2; b[2] = (uint8_t)g_set.nusach; b[3] = (uint8_t)g_set.purim; b[4] = (uint8_t)g_set.font; b[5] = (uint8_t)g_set.ratio;
    b[6] = (uint8_t)g_set.night; b[7] = (uint8_t)g_set.summer; b[8] = (uint8_t)(g_set.city_idx & 255); b[9] = (uint8_t)(g_set.city_idx >> 8);
    b[10] = (uint8_t)g_set.censor; b[11] = (uint8_t)g_set.vaanenu; b[12] = (uint8_t)g_set.doctor; b[13] = (uint8_t)g_set.full;
    b[14] = (uint8_t)g_set.spec_i; b[15] = (uint8_t)g_set.kot_i;
    plat_cfg_write(b, 16);
}
static void cfg_load(void) {
    uint8_t b[16]; int n = plat_cfg_read(b, sizeof b);
    if (n >= 16 && b[0] == 'T' && b[1] == 2) {
        if (b[2] <= 3) g_set.nusach = b[2];
        if (b[3] >= 1 && b[3] <= 3) g_set.purim = b[3];
        if (b[4] == 1 || b[4] == 2) g_set.font = b[4];
        if (b[5] >= 1 && b[5] <= 3) g_set.ratio = b[5];
        g_set.night = b[6] ? 1 : 0; g_set.summer = b[7] ? 1 : 0;
        if ((b[8] | (b[9] << 8)) < g_ncities) g_set.city_idx = b[8] | (b[9] << 8);
        g_set.censor = b[10] ? 1 : 0; g_set.vaanenu = b[11] ? 1 : 0; g_set.doctor = b[12] ? 1 : 0; g_set.full = b[13] ? 1 : 0;
        g_set.spec_i = b[14]; g_set.kot_i = b[15];
    }
    apply_colors();
}
static const char *nusach_code(void) { return g_set.nusach == 1 ? "a" : (g_set.nusach == 2 ? "s" : "em"); }
static const char *nusach_name(int n) { return n == 1 ? "אשכנז" : (n == 2 ? "ספרד" : "עדות המזרח"); }

static void refresh_day(void) {
    int y, mo, d, hh, mm, ss = INT_MIN;
    plat_time(&y, &mo, &d, &hh, &mm);
    abs_today = cal_greg_to_abs(y, mo - 1, d);
    eng_sun_times(abs_today, &g_cities[g_set.city_idx], g_set.summer, NULL, &ss);
    after_sunset = 0;
    if (ss != INT_MIN) { int h6 = ss / 3600, m7 = (ss % 3600) / 60; after_sunset = (hh > h6 || (hh == h6 && mm >= m7)); }
    eng_make_dayinfo(&dinfo, abs_today + after_sunset);
}

/* ---------------- screens ---------------- */
static void close_doc(void) { if (doc_ok) { doc_free(&doc); doc_ok = 0; } }
static void go_main(void);
static void go_prayers(void);

static void open_doc_res(const char *res, int back) {
    close_doc();
    refresh_day();
    g_txt_ratio_override = 0; txt_setup();
    doc_ok = doc_build(&doc, res, sw - 4, &dinfo);
    top_line = 0; fullscr = g_set.full; reader_back = back; scr = SC_READER;
}
static void open_doc_text(const char *utf8, int back, int sc) {
    close_doc();
    refresh_day();
    g_txt_ratio_override = 0; txt_setup();
    doc_ok = doc_build_text(&doc, utf8, sw - 4, &dinfo);
    top_line = 0; fullscr = 0; reader_back = back; scr = sc;
}
static void go_main(void) {
    char tmp[64];
    scr = SC_MAIN; set_title("תפריט ראשי"); item_clear();
    if (g_set.nusach) {
        tmp[0] = 0; strcat(tmp, "נוסח "); strcat(tmp, nusach_name(g_set.nusach));
        item_add(tmp, 1); item_add("לבחירת נוסח אחר", 2);
    } else item_add("לבחירת נוסח", 2);
    item_add("תפילה לתקופת הקורונה", 3); item_add("תפילה לעת צרה", 4); item_add("הגדרות", 5);
    item_add("זמני היום", 6); item_add("עזרה", 7); item_add("אודות", 8);
}
static void go_nusach(void) {
    scr = SC_NUSACH; set_title("בחירת נוסח"); item_clear();
    item_add("אשכנז", 1); item_add("ספרד", 2); item_add("עדות המזרח", 3);
    if (g_set.nusach) sel = g_set.nusach - 1;
}
static void go_prayers(void) {
    static const char *const lab[13] = {"שחרית", "מנחה", "ערבית", "קריאת שמע שעל המיטה", "ברכת המזון", "מעין שלוש", "תיקון חצות", "שבע ברכות", "סדר ברית מילה", "בורא נפשות", "אשר יצר", "תפילת הדרך", "קידוש לבנה"};
    int i; char tmp[64]; uint64_t both;
    refresh_day();
    scr = SC_PRAYERS; set_title(nusach_name(g_set.nusach)); item_clear();
    for (i = 0; i < 13; i++) {
        if (i == 0) { tmp[0] = 0; strcat(tmp, lab[0]); if ((dinfo.flags & 0x2000000000ULL) == 0x2000000000ULL) strcat(tmp, " ומוסף"); item_add(tmp, 1); }
        else item_add(lab[i], i + 1);
    }
    if ((dinfo.flags & 0x80000ULL) == 0x80000ULL) item_add("ספירת העומר", 14);
    else {
        both = cal_flags(abs_today, dinfo.diaspora, g_set.purim) | cal_flags(abs_today + 1, dinfo.diaspora, g_set.purim);
        if ((both & 8ULL) == 8ULL) item_add("הדלקת נרות חנוכה", 14);
    }
}
static void open_prayer(int id) {
    static const char *const sfx[14] = {0, "_shacharit", "_mincha", "_arvit", "_mita", "_mazon", "_shalosh", 0, "_shevaBrachot", "_brit", "_nefashot", "_yatzar", "_derech", "_levana"};
    char nm[48]; nm[0] = 0;
    if (id == 7) strcpy(nm, "tikun_chatzot");
    else if (id == 14) { strcat(nm, nusach_code()); strcat(nm, (dinfo.flags & 0x80000ULL) == 0x80000ULL ? "_omer" : "_nerot"); }
    else { strcat(nm, nusach_code()); strcat(nm, sfx[id - 1 < 14 ? id : 1]); }
    open_doc_res(nm, SC_PRAYERS);
}
static void go_cities(void) {
    int i;
    scr = SC_CITIES; set_title("בחירת עיר"); item_clear();
    for (i = 0; i < g_ncities; i++) item_add_raw((const uint8_t *)g_cities[i].name, (int)strlen(g_cities[i].name), i);
    sel = g_set.city_idx; scroll_pos = 0;
}
enum { SR_NUSACH, SR_PURIM, SR_CENSOR, SR_VAANENU, SR_DOCTOR, SR_CITY, SR_SUMMER, SR_FONT, SR_RATIO, SR_NIGHT, SR_FULL, SR_SPEC, SR_KOT, SR_COUNT };
static const char *const set_label[SR_COUNT] = {"נוסח ברירת מחדל", "פורים", "צנזור שמות ה'", "ועננו בורא עולם", "יה\"ר לחולים", "מיקום", "שעון קיץ", "גופן (פונט)", "גודל כתב", "מצב תצוגה", "מצב מסך ברירת מחדל", "צבע טקסט מיוחד", "צבע הנחיות וכותרות"};
static const char *set_value(int row) {
    static const char *const col[4] = {"אדום", "כחול", "ירוק", "סגול"};
    switch (row) {
        case SR_NUSACH: return g_set.nusach == 0 ? "ללא" : nusach_name(g_set.nusach);
        case SR_PURIM: return g_set.purim == 1 ? "רק בי\"ד" : (g_set.purim == 2 ? "רק בט\"ו" : "שניהם");
        case SR_CENSOR: return g_set.censor ? "צנזר" : "אל תצנזר";
        case SR_VAANENU: return g_set.vaanenu ? "הצג" : "אל תציג";
        case SR_DOCTOR: return g_set.doctor ? "הצג" : "אל תציג";
        case SR_SUMMER: return g_set.summer ? "כן" : "לא";
        case SR_FONT: return g_set.font == 2 ? "Tahoma" : "System";
        case SR_RATIO: return g_set.ratio == 1 ? "קטן" : (g_set.ratio == 2 ? "בינוני" : "גדול");
        case SR_NIGHT: return g_set.night ? "מצב לילה" : "רגיל";
        case SR_FULL: return g_set.full ? "מסך מלא" : "רגיל";
        case SR_SPEC: return col[g_set.spec_i & 3];
        case SR_KOT: return col[g_set.kot_i & 3];
    }
    return "";
}
static void go_settings(void) { scr = SC_SETTINGS; set_title("הגדרות"); item_clear(); nitems = SR_COUNT; }
static void go_bookmarks(void) {
    int i, n;
    if (!doc_ok || doc.nbms == 0) return;
    scr = SC_BOOKMARKS; set_title("תפריט"); item_clear();
    for (i = 0; i < doc.nbms; i++) { const uint8_t *t = doc.arena + doc.bms[i].title_off; n = (int)strlen((const char *)t); item_add_raw(t, n, doc.bms[i].line); }
}

/* ---- daily info ("זמני היום"): date, sunrise/sunset and the status tags shown by the original ---- */
static void cat_i(char *b, int v, int w) { char t[12]; int n = 0, i; if (v < 0) v = 0; do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v && n < 10); while (n < w) t[n++] = '0'; i = (int)strlen(b); while (n) b[i++] = t[--n]; b[i] = 0; }
static void cat_hm(char *b, int secs) { if (secs == INT_MIN) { strcat(b, "--:--"); return; } cat_i(b, secs / 3600, 2); strcat(b, ":"); cat_i(b, (secs % 3600) / 60, 2); }
static void cat_date(char *b, int abs) {
    CalDate h; cal_abs_to_hebrew(abs, &h);
    strcat(b, S_DAYS[h.d - 1 < 30 ? h.d - 1 : 29]); strcat(b, " ב");
    if (h.m == 12) strcat(b, "אדר ב'"); else if (hy_leap(h.y) && h.m == 11) strcat(b, "אדר א'"); else strcat(b, S_MONTHS[h.m]);
}
static void go_info(void) {
    static const char *const wd[7] = {"יום ראשון", "יום שני", "יום שלישי", "יום רביעי", "יום חמישי", "יום שישי", "שבת"};
    char *b = (char *)plat_malloc(2600); uint64_t l2; CalDate today; int sr = INT_MIN, ss = INT_MIN, n10, n12;
    if (!b) return;
    refresh_day();
    l2 = dinfo.flags;
    cal_abs_to_hebrew(abs_today, &today); n10 = today.d; n12 = today.m;
    eng_sun_times(abs_today, &g_cities[g_set.city_idx], g_set.summer, &sr, &ss);
    b[0] = 0;
    strcat(b, wd[abs_today % 7]); strcat(b, "\r\n");
    if (after_sunset) strcat(b, "אור ל");
    cat_date(b, abs_today + after_sunset); strcat(b, "\r\n\r\n");
    strcat(b, "זריחה: "); cat_hm(b, sr); strcat(b, "\r\n");
    strcat(b, "שקיעה: "); cat_hm(b, ss); strcat(b, "\r\n\r\n");
    if ((l2 & 0x80000ULL) == 0x80000ULL) { cat_i(b, dinfo.omer_day & 255, 1); strcat(b, " ימים לעומר\r\n"); }
    if ((l2 & 0x20000000000ULL) == 0x20000000000ULL) strcat(b, "שלושת השבועות (בין המצרים)\r\n");
    if ((l2 & 0x200ULL) == 0x200ULL) strcat(b, "עשרת ימי תשובה\r\n");
    if ((l2 & 1ULL) || (l2 & 0x80ULL) || (l2 & 0x100ULL)) {
        if (l2 & 1ULL) strcat(b, "ראש חודש\r\n");
        else if (l2 & 0x80ULL) { strcat(b, "סוכות\r\n"); if ((l2 & 0x40000ULL) == 0x40000ULL) strcat(b, "הושענה רבה\r\n"); }
        else strcat(b, "פסח\r\n");
        strcat(b, "יעלה ויבא\r\n");
    }
    if ((l2 & 8ULL) || (l2 & 0x10ULL)) {
        if (l2 & 8ULL) { strcat(b, "חנוכה נר "); cat_i(b, dinfo.chanuka_day, 1); strcat(b, "\r\n"); if (dinfo.chanuka_day == 8) strcat(b, "זאת חנוכת\r\n"); }
        else strcat(b, "פורים\r\n");
        strcat(b, "על הניסים\r\n");
    }
    if (n12 == 0 && ((n10 == 27 && ((l2 & 0x8000000ULL) == 0x8000000ULL || (l2 & 0x20000000ULL) == 0x20000000ULL)) || (n10 == 28 && (l2 & 0x4000000ULL) == 0x4000000ULL) || (n10 == 26 && (l2 & 0x20000000ULL) == 0x20000000ULL)))
        strcat(b, "יום הזכרון לשואה ולגבורה\r\n");
    if ((l2 & 0x800000000ULL) == 0x800000000ULL) strcat(b, "יום הזכרון לחללי מערכות ישראל ולנפגעי פעולות האיבה\r\n");
    if ((l2 & 0x200000000ULL) == 0x200000000ULL) strcat(b, "יום העצמאות\r\n");
    if ((l2 & 0x400000000ULL) == 0x400000000ULL) strcat(b, "יום שחרור ירושלים\r\n");
    if ((l2 & 0x20000ULL) == 0x20000ULL) { if ((l2 & 0x10000ULL) == 0x10000ULL) strcat(b, "תשעה באב\r\n"); else if (!after_sunset) strcat(b, "תענית\r\n"); }
    if ((l2 & 0x200000ULL) == 0) strcat(b, "א\"א תחנון\r\n");
    if (!after_sunset && (l2 & 0x200000ULL) == 0x200000ULL && (l2 & 0x400000ULL) == 0) strcat(b, "א\"א תחנון במנחה\r\n");
    if ((l2 & 0x2000ULL) == 0x2000ULL) strcat(b, "משיב הרוח ומוריד הגשם\r\n");
    if ((l2 & 0x1000ULL) == 0x1000ULL) strcat(b, "מוריד הטל\r\n");
    if ((l2 & 0x8000ULL) == 0x8000ULL) strcat(b, "ותן טל ומטר\r\n");
    if ((l2 & 0x4000ULL) == 0x4000ULL) strcat(b, "ותן ברכה\r\n");
    strcat(b, "\r\n"); strcat(b, g_cities[g_set.city_idx].name[0] ? "" : "");
    open_doc_text(b, SC_MAIN, SC_INFO);
    plat_free(b);
}
static void go_about(void) { open_doc_text(S_ABOUT, SC_MAIN, SC_ABOUT); }

/* ---------------- input ---------------- */
static void list_move(int d) {
    int vis = (sh - 2 * bar_h()) / (bar_h() * (scr == SC_SETTINGS ? 2 : 1));
    sel += d; if (sel < 0) sel = nitems - 1; if (sel >= nitems) sel = 0;
    if (vis < 1) vis = 1;
    if (sel < scroll_pos) scroll_pos = sel;
    if (sel >= scroll_pos + vis) scroll_pos = sel - vis + 1;
}
static int reader_rows(int H) {
    int e = g_set.font == 2 ? 2 : 1, d = ui_d(), c = txt_cell_h();
    int rows = (int)(((float)H - (float)((3 - e) * d)) / ((float)c + (float)((5 - e) * d)));
    return rows < 1 ? 1 : rows;
}
static void reader_scroll(int d) {
    int n = doc_ok ? doc.nlines : 0;
    top_line += d; if (top_line > n - 1) top_line = n - 1; if (top_line < 0) top_line = 0;
}
static void leave_reader(void) {
    close_doc();
    if (reader_back == SC_PRAYERS) go_prayers(); else go_main();
}

static int is_reader_screen(void) { return scr == SC_READER || scr == SC_INFO || scr == SC_ABOUT; }
void ui_key(int key, int repeat) {
    int id;
    g_txt_ratio_override = is_reader_screen() ? 0 : 1; txt_setup();
    if (scr == SC_READER || scr == SC_INFO || scr == SC_ABOUT) {
        int rows = reader_rows(sh - (fullscr ? 0 : bar_h())), step = repeat ? 3 : 1;
        switch (key) {
            case K_UP: reader_scroll(-step); break;
            case K_DOWN: reader_scroll(step); break;
            case K_LEFT: reader_scroll(rows); break;
            case K_RIGHT: reader_scroll(-rows); break;
            case K_POUND: fullscr = !fullscr; break;
            case K_SL: if (scr == SC_READER) go_bookmarks(); break;
            case K_SR: case K_BACK: leave_reader(); break;
        }
        return;
    }
    if (scr == SC_SETTINGS) {
        switch (key) {
            case K_UP: list_move(-1); break;
            case K_DOWN: list_move(1); break;
            case K_LEFT: case K_RIGHT: case K_OK: {
                int dir = key == K_LEFT ? -1 : 1;
                switch (sel) {
                    case SR_NUSACH: g_set.nusach = (g_set.nusach + dir + 4) % 4; break;
                    case SR_PURIM: g_set.purim += dir; if (g_set.purim < 1) g_set.purim = 3; if (g_set.purim > 3) g_set.purim = 1; break;
                    case SR_CENSOR: g_set.censor = !g_set.censor; break;
                    case SR_VAANENU: g_set.vaanenu = !g_set.vaanenu; break;
                    case SR_DOCTOR: g_set.doctor = !g_set.doctor; break;
                    case SR_CITY: if (key == K_OK) { cfg_save(); go_cities(); return; } break;
                    case SR_SUMMER: g_set.summer = !g_set.summer; break;
                    case SR_FONT: g_set.font = g_set.font == 1 ? 2 : 1; break;
                    case SR_RATIO: g_set.ratio += dir; if (g_set.ratio < 1) g_set.ratio = 3; if (g_set.ratio > 3) g_set.ratio = 1; break;
                    case SR_NIGHT: g_set.night = !g_set.night; break;
                    case SR_FULL: g_set.full = !g_set.full; break;
                    case SR_SPEC: g_set.spec_i = (g_set.spec_i + dir + 4) % 4; break;
                    case SR_KOT: g_set.kot_i = (g_set.kot_i + dir + 4) % 4; break;
                }
                apply_colors(); cfg_save();
            } break;
            case K_SR: case K_BACK: case K_SL: cfg_save(); go_main(); break;
        }
        return;
    }
    switch (key) {
        case K_UP: list_move(-1); break;
        case K_DOWN: list_move(1); break;
        case K_OK: case K_SL: case K_RIGHT:
            if (nitems == 0) break;
            id = items[sel].id;
            if (scr == SC_MAIN) {
                switch (id) {
                    case 1: go_prayers(); break;
                    case 2: go_nusach(); break;
                    case 3: refresh_day(); open_doc_res("korona", SC_MAIN); break;
                    case 4: refresh_day(); open_doc_res("tzahal", SC_MAIN); break;
                    case 5: go_settings(); break;
                    case 6: go_info(); break;
                    case 7: refresh_day(); open_doc_res("help", SC_MAIN); break;
                    case 8: go_about(); break;
                }
            } else if (scr == SC_NUSACH) { g_set.nusach = id; cfg_save(); go_prayers(); }
            else if (scr == SC_PRAYERS) open_prayer(id);
            else if (scr == SC_CITIES) { g_set.city_idx = id; cfg_save(); go_settings(); sel = SR_CITY; }
            else if (scr == SC_BOOKMARKS) { top_line = id; scr = SC_READER; }
            break;
        case K_SR: case K_BACK: case K_LEFT:
            if (scr == SC_MAIN) { if (key != K_LEFT) quit_flag = 1; }
            else if (scr == SC_NUSACH) go_main();
            else if (scr == SC_PRAYERS) go_main();
            else if (scr == SC_CITIES) { go_settings(); sel = SR_CITY; }
            else if (scr == SC_BOOKMARKS) scr = SC_READER;
            break;
    }
}

/* ---------------- painting ---------------- */
static void softkeys(FB *fb, const char *left, const char *right) {
    int bh = bar_h(), y = sh - bh, pad = 3, keep = g_txt_ratio_override;
    uint8_t b[40]; int n;
    g_txt_ratio_override = 1; txt_setup();
    txt_fill(fb, 0, y, sw, bh, C_BAR);
    if (right && right[0]) { n = uni_to_internal(right, b, sizeof b); txt_draw(fb, b, n, sw - 2, y + pad, C_BARTXT); }
    if (left && left[0]) ltr_text(fb, left, 4, y + pad, C_BARTXT);
    g_txt_ratio_override = keep; txt_setup();
}
static void title_bar(FB *fb) {
    int bh = bar_h(), pad = 3;
    txt_fill(fb, 0, 0, sw, bh, C_BAR);
    txt_draw(fb, title, title_n, sw - 2, pad, C_BARTXT);
}
static void paint_list(FB *fb) {
    int bh = bar_h(), vis = (sh - 2 * bh) / bh, i, y, pad = 3;
    title_bar(fb);
    for (i = 0; i < vis && scroll_pos + i < nitems; i++) {
        int k = scroll_pos + i; y = bh + i * bh;
        if (k == sel) txt_fill(fb, 0, y, sw, bh, C_SEL);
        txt_draw(fb, items[k].t, items[k].n, sw - 2, y + pad, C_FG);
    }
    softkeys(fb, "בחר", scr == SC_MAIN ? "יציאה" : "חזור");
}
static void paint_settings(FB *fb) {
    int bh = bar_h(), rh = 2 * bh, vis = (sh - 2 * bh) / rh, i, y, pad = 3;
    uint16_t vcol = g_set.night ? RGB565(150, 190, 255) : RGB565(20, 70, 170);
    title_bar(fb);
    if (vis < 1) vis = 1;
    if (sel < scroll_pos) scroll_pos = sel;
    if (sel >= scroll_pos + vis) scroll_pos = sel - vis + 1;
    for (i = 0; i < vis && scroll_pos + i < SR_COUNT; i++) {
        int k = scroll_pos + i; uint8_t b[96]; int n;
        y = bh + i * rh;
        if (k == sel) txt_fill(fb, 0, y, sw, rh, C_SEL);
        n = uni_to_internal(set_label[k], b, sizeof b); txt_draw(fb, b, n, sw - 2, y + pad, C_FG);
        if (k == SR_CITY) { const char *nm = g_cities[g_set.city_idx].name; txt_draw(fb, (const uint8_t *)nm, (int)strlen(nm), sw - 2, y + bh + pad, vcol); }
        else { n = uni_to_internal(set_value(k), b, sizeof b); txt_draw(fb, b, n, sw - 2, y + bh + pad, vcol); }
    }
    softkeys(fb, "", "חזור");
}
static void paint_reader(FB *fb) {
    int bh = fullscr ? 0 : bar_h(), H = sh - bh, rows, e = g_set.font == 2 ? 2 : 1, d = ui_d(), c = txt_cell_h(), i, y;
    rows = reader_rows(H);
    y = (3 - e) * d;
    if (!doc_ok) { ltr_text(fb, "out of memory", 4, 20, C_FG); }
    else for (i = top_line; i < top_line + rows && i < doc.nlines; i++) {
        int len; const uint8_t *l = doc_line(&doc, i, &len); uint8_t r = 0, g = 0, b = 0; uint16_t col = C_FG;
        if (doc_color(&doc, i, &r, &g, &b)) col = RGB565(r, g, b);
        txt_draw(fb, l, len, sw, y, col);
        y += c + (rows != 1 ? (H - rows * c - (3 - e) * d) / (rows - 1) : 0);
    }
    if (doc_ok && doc.nlines > 1) {                      /* slim position bar on the left edge */
        int th = H - 4, thumb = th / (doc.nlines > 60 ? 30 : 6); if (thumb < 6) thumb = 6;
        int ty = 2 + (int)((long)(th - thumb) * top_line / (doc.nlines - 1));
        txt_fill(fb, 0, 2, 2, th, g_set.night ? RGB565(50, 50, 50) : RGB565(220, 220, 220));
        txt_fill(fb, 0, ty, 2, thumb, g_set.night ? RGB565(160, 160, 255) : RGB565(60, 90, 200));
    }
    if (!fullscr) softkeys(fb, (scr == SC_READER && doc_ok && doc.nbms) ? "תפריט" : "", "חזור");
}
void ui_paint(FB *fb) {
    int i; uint16_t *p = fb->px;
    set_colors(); g_txt_ratio_override = is_reader_screen() ? 0 : 1; txt_setup();
    for (i = 0; i < fb->w * fb->h; i++) p[i] = C_BG;
    switch (scr) {
        case SC_READER: case SC_INFO: case SC_ABOUT: paint_reader(fb); break;
        case SC_SETTINGS: paint_settings(fb); break;
        default: paint_list(fb); break;
    }
}
void ui_init(int w, int h) { sw = w; sh = h; quit_flag = 0; doc_ok = 0; cfg_load(); g_txt_ratio_override = 1; txt_setup(); go_main(); }
int  ui_exit_requested(void) { return quit_flag; }
void ui_shutdown(void) { close_doc(); }
