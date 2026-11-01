/* Hebrew calendar + day flags. Faithful port of the original app's classes b, r, v and g.long_a. */
#include "cal.h"

int hy_leap(int y) { return (y * 7 + 1) % 19 < 7; }

static int hy_start(int n2) {                         /* b.int_a: absolute day of Rosh Hashana */
    int n3 = n2 - 1;
    int n4 = n3 / 19;
    int n5 = n3 % 19;
    n3 = n4 * 235 + n5 * 12 + (n5 * 7 + 1) / 19;
    n4 = n3 / 25920;
    n5 = n3 % 25920;
    int n6 = n5 * 13753 + 5604;
    n3 = n3 * 29 + n4 * 13753 + n6 / 25920 + 1;
    n6 %= 25920;
    n4 = n3;
    n5 = n4 % 7;
    if (n6 >= 19440 || (n5 == 2 && n6 >= 9924 && !hy_leap(n2)) || (n5 == 1 && n6 >= 16789 && hy_leap(n2 - 1))) {
        n4 = ++n3;
        n5 = n4 % 7;
    }
    if (n5 == 0 || n5 == 3 || n5 == 5) ++n3;
    return n3;
}

typedef struct { int year, start, nm, len[13]; } HYear;

static int load_year(HYear *h, int y) {
    int s, diff, leap, i, n2;
    if (y <= 0 || y > 5000000) return 0;
    s = hy_start(y);
    diff = hy_start(y + 1) - s;
    leap = diff > 356;
    h->nm = leap ? 13 : 12;
    for (i = 0; i < 12; i += 2) { h->len[i] = 30; h->len[i + 1] = 29; }
    if (leap) { h->len[11] = 30; h->len[12] = 29; n2 = diff - 384; }
    else      { n2 = diff - 354; }
    if (n2 < 0) h->len[8]--; else if (n2 > 0) h->len[7]++;
    h->start = s; h->year = y;
    return 1;
}

int cal_hebrew_month_len(int y, int m) {
    HYear h;
    if (!load_year(&h, y) || m < 0 || m >= h.nm) return 0;
    return h.len[m];
}

void cal_abs_to_hebrew(int abs, CalDate *out) {      /* b.a(r,int) */
    HYear h;
    int n4, n5, n3, n7;
    n4 = (abs - 1) / 6940;
    n5 = n4 * 19;
    n4 = n4 * 6940 + 1;
    while (n4 < abs) { n4 += hy_leap(n5) ? 384 : 354; ++n5; }
    while ((n4 = hy_start(n5)) < abs) ++n5;
    if (n4 > abs) --n5;
    load_year(&h, n5);
    n7 = 6;
    for (n4 = abs - h.start; n4 >= (n3 = h.len[n7]); n4 -= n3) {
        if (++n7 < h.nm) continue;
        n7 = 0;
    }
    out->d = n4 + 1; out->m = n7; out->y = n5;
}

int cal_hebrew_to_abs(int d, int m, int y, int clamp, int *day_out) {   /* b.a(r,q,boolean) */
    HYear h;
    int n3, n2;
    if (m < 0 || d <= 0 || !load_year(&h, y)) return -1;
    if (clamp) {
        if (m >= h.nm) m = h.nm - 1;
        if (d > h.len[m]) d = h.len[m];
    } else if (m >= h.nm || d > h.len[m]) return -1;
    if (day_out) *day_out = d;
    n3 = d - 1 + h.start;
    n2 = 6;
    if (m < 6) { while (n2 < h.nm) n3 += h.len[n2++]; n2 = 0; }
    while (n2 < m) n3 += h.len[n2++];
    return n3;
}

/* ---- Gregorian <-> absolute day (modern, post-1582 dates only) ---- */
static int g_leap(int Y) { return Y % 4 == 0 && !(Y % 100 == 0 && Y % 400 != 0); }
static const int g_ml[12] = {31,28,31,30,31,30,31,31,30,31,30,31};

static int g_days_before_year(int Y) { int n = Y - 1; return n * 365 + n / 4 + (n / 400 - n / 100 + 32); }

int cal_greg_to_abs(int y, int m, int d) {
    int Y = y + 4000, doy, i;
    if (m < 0) m = 0; if (m > 11) m = 11;
    { int ml = (m == 1 && g_leap(Y)) ? 29 : g_ml[m]; if (d > ml) d = ml; }
    doy = d - 1;
    for (i = 0; i < m; i++) doy += (i == 1 && g_leap(Y)) ? 29 : g_ml[i];
    return g_days_before_year(Y) + doy - 87573;
}

void cal_abs_to_greg(int abs, CalDate *g) {
    int n2 = abs + 87573, n5 = 0, n6 = n2, n4, n3, mi;
    n6 -= 32;
    n5 += n6 / 146097 * 400; n6 %= 146097;
    n5 += n6 / 36524 * 100;  n6 %= 36524;
    n5 += (n6 / 1461) << 2;  n6 %= 1461;
    n5 += n6 / 365 * 365;
    n4 = g_days_before_year(n5 + 1);
    while (n4 > n2) { --n5; n4 -= 365 + g_leap(n5); }
    /* Java: for (n4 = int_a(n5,bl); n4 > n2; n4 -= b(--n5,bl)) -- int_a(n)=(n-1)*365+... so year index n5 there is 'Y' */
    n4 = g_days_before_year(n5 + 0);
    (void)n4;
    {
        int Y = n5;
        /* normalise with a direct search to be robust */
        while (g_days_before_year(Y) > n2) --Y;
        while (g_days_before_year(Y + 1) <= n2) ++Y;
        n6 = n2 - g_days_before_year(Y);
        mi = 0;
        while (n6 >= ((mi == 1 && g_leap(Y)) ? 29 : g_ml[mi])) { n6 -= (mi == 1 && g_leap(Y)) ? 29 : g_ml[mi]; ++mi; }
        g->d = n6 + 1; g->m = mi; g->y = Y - 4000;
    }
}

uint64_t cal_flags(int abs, int diaspora, int purim) {
    CalDate h;
    int hd, hm, hy, leapY, kislev30, gy, gm, gd;
    CalDate g;
    cal_abs_to_hebrew(abs, &h);
    hd = h.d; hm = h.m; hy = h.y;
    leapY = hy_leap(hy);
    kislev30 = (cal_hebrew_month_len(hy, 8) == 30);
    cal_abs_to_greg(abs, &g);
    gd = g.d; gm = g.m; gy = g.y;
#include "flags_gen.inc"
}

int cal_omer_day(int abs) {                         /* g.int_a */
    CalDate h; int n5, n4, n3, n2, n6, i, len[13];
    cal_abs_to_hebrew(abs, &h);
    n5 = h.m;
    if (n5 <= 2) { n4 = 15; n3 = 49; n2 = 0; }
    else if (n5 >= 3 && n5 <= 4) { n4 = 105; n3 = 21; n2 = 256; }
    else return -1;
    n6 = h.d;
    for (i = 0; i < n5; i++) n6 += cal_hebrew_month_len(h.y, i);
    n6 -= n4;
    return (n6 <= 0 || n6 > n3) ? -1 : (n6 | n2);
}

int cal_sukkot_day(int abs) {                       /* g.int_b */
    CalDate h; cal_abs_to_hebrew(abs, &h);
    if (h.m == 6 && h.d >= 15 && h.d <= 21) return h.d - 14;
    return 0;
}

int cal_chanuka_day(int abs) {                      /* g.int_c */
    CalDate h; cal_abs_to_hebrew(abs, &h);
    if (h.m == 8 && h.d >= 24) return h.d - 24;
    if (h.m == 9) {
        if (cal_hebrew_month_len(h.y, 8) == 30) { if (h.d <= 2) return 6 + h.d; }
        else if (h.d <= 3) return 5 + h.d;
    }
    return -1;
}
