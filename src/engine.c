/* Tfilon engine: resource unpacking, Hebrew bitmap-font text layout/rendering, prayer-script interpreter.
   A C port of the logic found in the original Tfilon J2ME MIDlet (classes g, j). */
#include <string.h>
#include "engine.h"
#include "data.h"

Settings g_set = { 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, {255, 0, 0}, {0, 0, 255}, 0, 0, 1 };

/* ============================ inflate (raw deflate) ============================ */
#define MAXBITS 15
typedef struct { const uint8_t *in; unsigned inlen, incnt; int bitbuf, bitcnt; uint8_t *out; unsigned outlen, outcnt; int err; } IState;
typedef struct { short count[MAXBITS + 1]; short symbol[288]; } Huff;

static int ibits(IState *s, int need) {
    long val = s->bitbuf;
    while (s->bitcnt < need) {
        if (s->incnt >= s->inlen) { s->err = 1; return 0; }
        val |= (long)s->in[s->incnt++] << s->bitcnt;
        s->bitcnt += 8;
    }
    s->bitbuf = (int)(val >> need);
    s->bitcnt -= need;
    return (int)(val & ((1L << need) - 1));
}
static int idecode(IState *s, const Huff *h) {
    int len, code = 0, first = 0, index = 0, count;
    for (len = 1; len <= MAXBITS; len++) {
        code |= ibits(s, 1);
        if (s->err) return -1;
        count = h->count[len];
        if (code - count < first) return h->symbol[index + (code - first)];
        index += count; first += count; first <<= 1; code <<= 1;
    }
    return -10;
}
static int iconstruct(Huff *h, const short *length, int n) {
    int symbol, len, left; short offs[MAXBITS + 1];
    for (len = 0; len <= MAXBITS; len++) h->count[len] = 0;
    for (symbol = 0; symbol < n; symbol++) h->count[length[symbol]]++;
    if (h->count[0] == n) return 0;
    left = 1;
    for (len = 1; len <= MAXBITS; len++) { left <<= 1; left -= h->count[len]; if (left < 0) return left; }
    offs[1] = 0;
    for (len = 1; len < MAXBITS; len++) offs[len + 1] = offs[len] + h->count[len];
    for (symbol = 0; symbol < n; symbol++) if (length[symbol] != 0) h->symbol[offs[length[symbol]]++] = symbol;
    return left;
}
static int icodes(IState *s, const Huff *lc, const Huff *dc) {
    static const short lens[29] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
    static const short lext[29] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
    static const short dists[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
    static const short dext[30] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
    int symbol, len; unsigned dist;
    do {
        symbol = idecode(s, lc);
        if (symbol < 0) return symbol;
        if (symbol < 256) {
            if (s->outcnt == s->outlen) return 1;
            s->out[s->outcnt++] = (uint8_t)symbol;
        } else if (symbol > 256) {
            symbol -= 257;
            if (symbol >= 29) return -10;
            len = lens[symbol] + ibits(s, lext[symbol]);
            symbol = idecode(s, dc);
            if (symbol < 0) return symbol;
            dist = dists[symbol] + ibits(s, dext[symbol]);
            if (dist > s->outcnt) return -11;
            if (s->outcnt + len > s->outlen) return 1;
            while (len--) { s->out[s->outcnt] = s->out[s->outcnt - dist]; s->outcnt++; }
        }
        if (s->err) return -1;
    } while (symbol != 256);
    return 0;
}
static int istored(IState *s) {
    unsigned len;
    s->bitbuf = 0; s->bitcnt = 0;
    if (s->incnt + 4 > s->inlen) return 2;
    len = s->in[s->incnt++]; len |= (unsigned)s->in[s->incnt++] << 8;
    if (s->in[s->incnt++] != (~len & 0xff) || s->in[s->incnt++] != ((~len >> 8) & 0xff)) return -2;
    if (s->incnt + len > s->inlen) return 2;
    if (s->outcnt + len > s->outlen) return 1;
    while (len--) s->out[s->outcnt++] = s->in[s->incnt++];
    return 0;
}
static int ifixed(IState *s) {
    Huff lc, dc; short lengths[288]; int sym;
    for (sym = 0; sym < 144; sym++) lengths[sym] = 8;
    for (; sym < 256; sym++) lengths[sym] = 9;
    for (; sym < 280; sym++) lengths[sym] = 7;
    for (; sym < 288; sym++) lengths[sym] = 8;
    iconstruct(&lc, lengths, 288);
    for (sym = 0; sym < 30; sym++) lengths[sym] = 5;
    iconstruct(&dc, lengths, 30);
    return icodes(s, &lc, &dc);
}
static int idynamic(IState *s) {
    static const short order[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
    Huff lc, dc; short lengths[320]; int nlen, ndist, ncode, index, err, symbol, len;
    nlen = ibits(s, 5) + 257; ndist = ibits(s, 5) + 1; ncode = ibits(s, 4) + 4;
    if (nlen > 286 || ndist > 30) return -3;
    for (index = 0; index < ncode; index++) lengths[order[index]] = (short)ibits(s, 3);
    for (; index < 19; index++) lengths[order[index]] = 0;
    err = iconstruct(&lc, lengths, 19);
    if (err != 0) return -4;
    index = 0;
    while (index < nlen + ndist) {
        symbol = idecode(s, &lc);
        if (symbol < 0) return symbol;
        if (symbol < 16) lengths[index++] = (short)symbol;
        else {
            len = 0;
            if (symbol == 16) { if (index == 0) return -5; len = lengths[index - 1]; symbol = 3 + ibits(s, 2); }
            else if (symbol == 17) symbol = 3 + ibits(s, 3);
            else symbol = 11 + ibits(s, 7);
            if (index + symbol > nlen + ndist) return -6;
            while (symbol--) lengths[index++] = (short)len;
        }
    }
    if (lengths[256] == 0) return -9;
    err = iconstruct(&lc, lengths, nlen);
    if (err && (err < 0 || nlen != lc.count[0] + lc.count[1])) return -7;
    err = iconstruct(&dc, lengths + nlen, ndist);
    if (err && (err < 0 || ndist != dc.count[0] + dc.count[1])) return -8;
    return icodes(s, &lc, &dc);
}
static int inflate_raw(const uint8_t *src, unsigned srclen, uint8_t *dst, unsigned dstlen) {
    IState s; int last, type, err = 0;
    s.in = src; s.inlen = srclen; s.incnt = 0; s.bitbuf = 0; s.bitcnt = 0;
    s.out = dst; s.outlen = dstlen; s.outcnt = 0; s.err = 0;
    do {
        last = ibits(&s, 1); type = ibits(&s, 2);
        if (s.err) return -1;
        err = type == 0 ? istored(&s) : type == 1 ? ifixed(&s) : type == 2 ? idynamic(&s) : -1;
        if (err != 0) break;
    } while (!last);
    return err ? -1 : (int)s.outcnt;
}

uint8_t *res_load(const char *name, int *len) {
    int lo = 0, hi = g_nres - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2, c = strcmp(name, g_res[mid].name);
        if (c == 0) {
            uint8_t *buf = (uint8_t *)plat_malloc(g_res[mid].rlen + 1);
            int n;
            if (!buf) return NULL;
            n = inflate_raw(g_blob + g_res[mid].off, g_res[mid].clen, buf, g_res[mid].rlen);
            if (n < 0) { plat_free(buf); return NULL; }
            buf[n] = 0; if (len) *len = n;
            return buf;
        }
        if (c < 0) hi = mid - 1; else lo = mid + 1;
    }
    return NULL;
}

/* ============================ unicode -> internal encoding ============================ */
static uint8_t u2b[0x60]; static int u2b_ready;
int uni_to_internal(const char *s, uint8_t *out, int cap) {
    int n = 0;
    if (!u2b_ready) {
        int i; memset(u2b, 0, sizeof u2b);
        for (i = 0; i < 55; i++) if (g_ab_uni[i] >= 0x5B0 && g_ab_uni[i] < 0x5B0 + 0x60) u2b[g_ab_uni[i] - 0x5B0] = g_ab_byte[i];
        u2b_ready = 1;
    }
    while (*s && n < cap - 1) {
        unsigned cp; unsigned char c = (unsigned char)*s++;
        if (c < 0x80) cp = c;
        else if ((c & 0xE0) == 0xC0 && *s) { cp = ((c & 0x1F) << 6) | ((unsigned char)*s++ & 0x3F); }
        else if ((c & 0xF0) == 0xE0 && s[0] && s[1]) { cp = ((c & 0x0F) << 12) | (((unsigned char)s[0] & 0x3F) << 6) | ((unsigned char)s[1] & 0x3F); s += 2; }
        else continue;
        if (cp < 0x80) out[n++] = (uint8_t)cp;
        else if (cp >= 0x5B0 && cp < 0x5B0 + 0x60 && u2b[cp - 0x5B0]) out[n++] = u2b[cp - 0x5B0];
    }
    out[n] = 0;
    return n;
}

/* ============================ glyph metrics / drawing ============================ */
static const uint8_t *G_w, *G_h, *G_bits; static const uint16_t *G_off;
static int D = 1, FONT = 1, CELL_H = 19;
enum { ALEF, BET, GIMEL, DALET, HE, VAV, ZAYIN, HET, TET, YOD, KAF, LAMED, MEM, NUN, SAMEKH, AYIN, PE, TSADI, QOF, RESH, SHIN, TAV, KAF_F, MEM_F, NUN_F, PE_F, TSADI_F,
       GL_DASH = 35, GL_NIK0 = 37, GL_HOLAM = 46, GL_QUBUTS = 47, GL_DAGESH = 48, GL_SHIN_DOT = 49, GL_SIN_DOT = 50, GL_K = 51, GL_TILDE = 54 };

int g_txt_ratio_override = 0;   /* >0: menus/chrome draw at this scale regardless of the text-size setting */
void txt_setup(void) {
    int r = g_txt_ratio_override > 0 ? g_txt_ratio_override : g_set.ratio;
    FONT = (g_set.font == 2) ? 2 : 1;
    D = r < 1 ? 1 : (r > 4 ? 4 : r);
    if (FONT == 1) { G_w = g_gl1_w; G_h = g_gl1_h; G_bits = g_gl1_bits; G_off = g_gl1_off; }
    else           { G_w = g_gl2_w; G_h = g_gl2_h; G_bits = g_gl2_bits; G_off = g_gl2_off; }
    CELL_H = G_h[0] * D;
}
int txt_cell_h(void) { return CELL_H; }
static int glw(int gi) { return G_w[gi] * D; }
static int is_nikud(int gi) { return gi >= GL_NIK0 && gi <= GL_K; }

static int ltr_char(uint8_t b) { return (b >= '0' && b <= '9') || (b >= 'A' && b <= 'Z') || (b >= 'a' && b <= 'z'); }
static int ltr_inner(uint8_t b) { return b == '.' || b == ':' || b == '/' || b == '@' || b == '_' || b == '-'; }
static int run_end(const uint8_t *s, int i, int len) {
    int j = i;
    while (j < len && (ltr_char(s[j]) || ltr_inner(s[j]))) j++;
    while (j > i + 1 && ltr_inner(s[j - 1])) j--;
    return j;
}

int txt_width(const uint8_t *s, int len) {
    int n = 2, i;
    for (i = 0; i < len; i++) {
        uint8_t b = s[i]; int gi;
        if (b == ' ') { n += 4 * D; continue; }
        if (b == 13) continue;
        gi = g_glyph_of_byte[b];
        if (gi == 255 && ltr_char(b)) { int j = run_end(s, i, len); n += (j - i) * 8 * D; i = j - 1; continue; }
        if (gi == 255) continue;
        if (gi == GL_TILDE) { n += (g_set.censor ? glw(GL_DASH) : 0) + (3 - FONT) * D; continue; }
        if (is_nikud(gi)) continue;
        n += glw(gi) + (3 - FONT) * D;
    }
    return n;
}

static void fill(FB *fb, int x, int y, int w, int h, uint16_t c) {
    int xx, yy;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > fb->w) w = fb->w - x;
    if (y + h > fb->h) h = fb->h - y;
    if (w <= 0 || h <= 0) return;
    for (yy = 0; yy < h; yy++) { uint16_t *p = fb->px + (y + yy) * fb->w + x; for (xx = 0; xx < w; xx++) p[xx] = c; }
}
void txt_fill(FB *fb, int x, int y, int w, int h, uint16_t c) { fill(fb, x, y, w, h, c); }

static void asc_blit(FB *fb, uint8_t ch, int xl, int y, uint16_t col) {
    int r, c;
    if (ch < 32 || ch > 126) return;
    for (r = 0; r < 16; r++) { uint8_t bits = g_asc[ch - 32][r]; for (c = 0; c < 8; c++) if (bits & (0x80 >> c)) fill(fb, xl + c * D, y + r * D, D, D, col); }
}

static void blit_glyph(FB *fb, int gi, int xr, int y, uint16_t col) {
    int w = G_w[gi], h = G_h[gi], nb = (w + 7) / 8, gx, gy;
    const uint8_t *bits = G_bits + G_off[gi];
    int x0 = xr - w * D;
    for (gy = 0; gy < h; gy++)
        for (gx = 0; gx < w; gx++)
            if (bits[gy * nb + (gx >> 3)] & (0x80 >> (gx & 7))) fill(fb, x0 + gx * D, y + gy * D, D, D, col);
}

/* x/y offsets of a combining mark relative to the previous base letter (port of the switch tables in the original g.a) */
static int nik_dx(int nik, int prev) {
    int wp = prev >= 0 ? glw(prev) : 0, wn = glw(nik);
    if (nik == GL_K || (nik >= GL_NIK0 && nik <= GL_NIK0 + 8) || nik == GL_QUBUTS) return -(wp / 2 + wn / 2);
    if (nik == GL_DAGESH) {
        if (FONT == 1) switch (prev) {
            case SHIN: return -8 * D; case TAV: return -7 * D;
            case ALEF: case HE: case TET: case MEM: case SAMEKH: case PE: case QOF: return -5 * D;
            case BET: case KAF_F: case KAF: case RESH: return -4 * D;
            case DALET: case LAMED: return -3 * D;
            case GIMEL: case NUN: case TSADI: return -2 * D;
            case VAV: case ZAYIN: case YOD: return -D;
            default: return 0;
        } else switch (prev) {
            case SAMEKH: case PE: case SHIN: case TAV: return -6 * D;
            case TET: case MEM: return -5 * D;
            case ALEF: case BET: case HE: case KAF_F: case KAF: case QOF: case RESH: return -4 * D;
            case LAMED: return -3 * D;
            case GIMEL: case DALET: case NUN: return -2 * D;
            case VAV: case ZAYIN: case YOD: case TSADI: return -D;
            default: return 0;
        }
    }
    if (nik == GL_SIN_DOT) return prev == SHIN ? (FONT == 1 ? -2 * D : -D) : 0;
    if (nik == GL_SHIN_DOT) return -10 * D;
    if (nik == GL_HOLAM) return prev == VAV ? (FONT == 1 ? -3 * D : -2 * D) : 0;
    return -(wp / 2 - wn / 2);
}
static int nik_dy(int nik, int prev) {
    int c = CELL_H;
    if (nik == GL_K || (nik >= GL_NIK0 && nik <= GL_NIK0 + 8) || nik == GL_QUBUTS) return c - 3 * D;
    if (nik == GL_DAGESH) {
        if (FONT == 1) switch (prev) {
            case GIMEL: case YOD: case PE: return c - 13 * D;
            case LAMED: case QOF: return c - 12 * D;
            case BET: case DALET: case HE: case VAV: case ZAYIN: case KAF_F: case KAF: case NUN: case SAMEKH: case RESH: case TAV: return c - 11 * D;
            case TET: case MEM: case TSADI: case SHIN: return c - 10 * D;
            case ALEF: return c - 7 * D;
            default: return 0;
        } else switch (prev) {
            case YOD: return c - 11 * D;
            case BET: case GIMEL: case DALET: case HE: case VAV: case ZAYIN: case TET: case KAF_F: case KAF: case LAMED: case MEM: case NUN: case SAMEKH: case QOF: case RESH: case TAV: return c - 10 * D;
            case PE: case TSADI: case SHIN: return c - 9 * D;
            case ALEF: return c - 7 * D;
            default: return 0;
        }
    }
    if (nik == GL_SHIN_DOT || nik == GL_SIN_DOT) return FONT == 1 ? -2 * D : -D;
    if (nik == GL_HOLAM) return prev == LAMED ? (FONT == 1 ? -4 * D : -2 * D) : (FONT == 1 ? -2 * D : -D);
    return c - 1000;
}

/* draw one line right-to-left; xr = canvas width (the original starts at width-2) */
void txt_draw(FB *fb, const uint8_t *s, int len, int xr, int y, uint16_t color) {
    int x = xr - 2, i, prev = -1;
    for (i = 0; i < len; i++) {
        uint8_t b = s[i]; int gi;
        if (b == ' ') { x -= 4 * D; continue; }
        if (b == 13 || b == 10) continue;
        gi = g_glyph_of_byte[b];
        if (gi == 255 && ltr_char(b)) {
            int j = run_end(s, i, len), k, w = (j - i) * 8 * D, xl = x - w;
            for (k = i; k < j; k++) { asc_blit(fb, s[k], xl, y + (CELL_H - 16 * D) / 2, color); xl += 8 * D; }
            x -= w; i = j - 1; continue;
        }
        if (gi == GL_TILDE) { if (!g_set.censor) continue; gi = GL_DASH; }
        if (gi == 255) continue;
        if (!is_nikud(gi)) {
            blit_glyph(fb, gi, x, y, color);
            x -= glw(gi) + (3 - FONT) * D;
            prev = gi;
        } else {
            int nx = nik_dx(gi, prev), ny = nik_dy(gi, prev);
            blit_glyph(fb, gi, x - nx + (3 - FONT) * D, y + ny, color);
        }
    }
}

/* ============================ word wrap (port of g.a(String,int)/d()/b()) ============================ */
typedef struct { const uint8_t *c; int f, g, h, i; } Wrap;
static int wrap_b(const Wrap *w, int n) {
    int sp = w->f, nl = w->f, k;
    for (k = n; k < w->f; k++) if (w->c[k] == ' ') { sp = k; break; }
    for (k = n; k < w->f; k++) if (w->c[k] == '\n') { nl = k; break; }
    return sp < nl ? sp : nl;
}
static int wrap_d(Wrap *w) {
    int n2 = wrap_b(w, w->h), n3 = -1, n4 = txt_width(w->c + w->h, n2 - w->h);
    while (n2 < w->f && n4 <= w->g) {
        if (w->c[n2] == ' ') n3 = n2;
        else if (w->c[n2] == '\n') { n3 = n2; break; }
        if (++n2 >= w->f) continue;
        n2 = wrap_b(w, n2);
        n4 = txt_width(w->c + w->h, n2 - w->h);
    }
    {
        int nh;
        if (n2 == w->f && n4 <= w->g) nh = n2;
        else if (n3 == w->h) nh = w->h + 1;
        else if (n3 < w->h) nh = n2;
        else nh = n3;
        w->h = nh;
    }
    return w->h;
}

/* ============================ document ============================ */
typedef struct { uint8_t *buf; int len, pos, owned; } Frame;
typedef struct { Doc *d; Frame st[32]; int sp; int wrap_w; const DayInfo *di; int budget; } Ctx;

static void emit(Doc *d, const uint8_t *s, int n) {
    if (d->nlines >= d->maxlines || d->arena_len + n + 1 > d->arena_cap) return;
    memcpy(d->arena + d->arena_len, s, n);
    d->arena[d->arena_len + n] = 0;
    d->off[d->nlines++] = d->arena_len;
    d->arena_len += n + 1;
}
static int push_frame(Ctx *c, uint8_t *buf, int len, int owned) {
    if (c->sp >= 32) { if (owned) plat_free(buf); return 0; }
    c->st[c->sp].buf = buf; c->st[c->sp].len = len; c->st[c->sp].pos = 0; c->st[c->sp].owned = owned; c->sp++;
    return 1;
}
static int push_utf8(Ctx *c, const char *u) {
    int n = (int)strlen(u) + 4; uint8_t *b = (uint8_t *)plat_malloc(n);
    if (!b) return 0;
    n = uni_to_internal(u, b, n);
    return push_frame(c, b, n, 1);
}
static int push_internal_copy(Ctx *c, const uint8_t *s, int n) {
    uint8_t *b = (uint8_t *)plat_malloc(n + 1);
    if (!b) return 0;
    memcpy(b, s, n); b[n] = 0;
    return push_frame(c, b, n, 1);
}
static void push_resource(Ctx *c, const char *name, int nlen) {
    char nm[48]; uint8_t *b; int n;
    if (nlen <= 0 || nlen >= (int)sizeof nm) return;
    memcpy(nm, name, nlen); nm[nlen] = 0;
    if (!strncmp(nm, "specials", 8)) return;
    b = res_load(nm, &n);
    if (b) push_frame(c, b, n, 1);
    else push_utf8(c, "([\r\nשגיאה: בעיה בקריאה מקובץ.\r\n])\r\n");
}
static int starts(const uint8_t *t, int n, const char *p) { int k = (int)strlen(p); return n >= k && !memcmp(t, p, k); }

static int cond_true(const char *u, const DayInfo *di) {
    uint64_t l2 = di->flags; int diaspora = di->diaspora, sukkot_day = di->sukkot_day, chanuka_day = di->chanuka_day;
    int vaanenu = g_set.vaanenu, doctor = g_set.doctor;
    (void)vaanenu; (void)doctor; (void)sukkot_day; (void)chanuka_day; (void)diaspora;
#include "conds_gen.inc"
    return 0;
}

#include "strs_gen.inc"
static void catn(char *r, const char *s) { strcat(r, s); }
#define O(k) S_OMER[k]
static void omer_text(int n, const char *nus, char *r) {
    int em = !strcmp(nus, "em"), s = !strcmp(nus, "s"), a = !strcmp(nus, "a");
    const char *str2 = (em || s) ? O(18) : (a ? O(20) : "");
    r[0] = 0;
    if (n < 1 || n > 49) return;
    if (n == 1) { catn(r, O(21)); catn(r, str2); }
    else if (n == 2) { catn(r, O(22)); catn(r, str2); }
    else if (n < 10) {
        catn(r, O(23)); catn(r, O(n)); catn(r, O(24));
        if (em) catn(r, str2);
        if (n >= 7) {
            if (em) catn(r, O(27));
            catn(r, O(28));
            if (n == 8) catn(r, O(29)); else if (n == 9) catn(r, O(30));
            if (a || s) catn(r, O(33));
        }
        if (a || s) catn(r, str2);
    } else if (n == 10) { if (em) catn(r, O(37)); else { catn(r, O(38)); catn(r, str2); } }
    else if (n == 11) { if (em) catn(r, O(40)); else { catn(r, O(41)); catn(r, str2); } }
    else if (n == 12) { if (em) catn(r, O(43)); else { catn(r, O(44)); catn(r, str2); } }
    else {
        if (n % 10 == 0) { catn(r, O(45)); catn(r, O(10 + n / 10) + 4); catn(r, O(46)); }   /* substring(2): drop vav + sheva (2 bytes each) */
        else { catn(r, O(47)); catn(r, O(n % 10)); catn(r, O(48)); catn(r, O(10 + n / 10)); catn(r, O(49)); }
        if (em) { catn(r, str2); catn(r, O(51)); }
        catn(r, O(52));
        if (n < 14) { catn(r, O(53)); catn(r, (n % 7 == 4 || n % 7 == 6) ? O(54) : O(55)); catn(r, O(n % 7)); catn(r, O(56)); }
        else if (n < 21) {
            catn(r, O(57));
            if (n == 15) catn(r, O(58)); else if (n == 16) catn(r, O(59));
            else if (n != 14) { catn(r, O(60)); catn(r, (n % 7 == 4 || n % 7 == 6) ? O(61) : (n % 7 == 5 ? O(62) : O(63))); catn(r, O(n % 7)); catn(r, O(64)); }
        } else {
            catn(r, O(n / 7)); catn(r, O(65));
            if (n % 7 == 1) catn(r, O(66)); else if (n % 7 == 2) catn(r, O(67));
            else if (n % 7 != 0) { catn(r, O(68)); catn(r, (n % 7 == 4 || n % 7 == 6) ? O(69) : (n % 7 == 5 ? O(70) : O(71))); catn(r, O(n % 7)); catn(r, O(72)); }
        }
        if (a || s) { catn(r, O(75)); catn(r, str2); }
    }
    catn(r, O(76));
}
#undef O

static const char *const HOSH[6][6] = {
    {"hoshanot1", "", "hoshanot2", "hoshanot2", "", "hoshanot2"}, {"", "hoshanot3", "", "hoshanot3", "hoshanot3", ""},
    {"hoshanot3", "", "hoshanot2", "", "hoshanot4", "hoshanot5"}, {"", "hoshanot5", "", "hoshanot5", "", "hoshanot5"},
    {"hoshanot6", "", "hoshanot6", "", "hoshanot6", ""}, {"hoshanaRabaHoshanotStart", "hoshanaRabaHoshanotStart", "", "hoshanaRabaHoshanotStart", "", "hoshanaRabaHoshanotStart"} };

static void add_mark(Doc *d, int line, uint8_t r, uint8_t g, uint8_t b) {
    if (d->nmarks < 256) { Mark *m = &d->marks[d->nmarks++]; m->line = line; m->r = r; m->g = g; m->b = b; }
}

static void process_line(Ctx *c, const uint8_t *p, int n) {
    Doc *d = c->d; const DayInfo *di = c->di;
    const uint8_t *t = p; int tn = n;
    while (tn > 0 && t[0] <= ' ') { t++; tn--; }
    while (tn > 0 && t[tn - 1] <= ' ') tn--;

    if (starts(t, tn, "()")) {
        int sp = -1, k;
        for (k = 0; k < tn; k++) if (t[k] == ' ') { sp = k; break; }
        if (sp >= 0) {
            char cond[40]; int cl = tn - sp - 1, j;
            if (cl >= (int)sizeof cond) cl = (int)sizeof cond - 1;
            for (j = 0; j < cl; j++) { char ch = (char)t[sp + 1 + j]; cond[j] = (ch >= 'a' && ch <= 'z') ? (char)(ch - 32) : ch; }
            cond[cl] = 0;
            if (cond_true(cond, di)) push_resource(c, (const char *)t + 2, sp - 2);
        } else push_resource(c, (const char *)t + 2, tn - 2);
        return;
    }
    if (starts(t, tn, "-----")) {
        if (d->nbms < 160 && d->arena_len + tn - 5 + 1 <= d->arena_cap) {
            Bookmark *b = &d->bms[d->nbms++];
            b->line = d->nlines; b->title_off = d->arena_len;
            memcpy(d->arena + d->arena_len, t + 5, tn - 5); d->arena[d->arena_len + tn - 5] = 0; d->arena_len += tn - 5 + 1;
        }
        return;
    }
    if (starts(t, tn, "=====")) { push_utf8(c, "(:\r\nכאן קוראים בתורה\r\n:)"); return; }
    if (starts(t, tn, "::::::") || starts(t, tn, ":::::") || (starts(t, tn, "::::") )) {
        char buf[1200]; const char *nus = starts(t, tn, "::::::") ? "s" : (starts(t, tn, ":::::") ? "em" : "a");
        omer_text(di->omer_day, nus, buf); if (buf[0]) push_utf8(c, buf);
        return;
    }
    if (starts(t, tn, ":::")) {
        char buf[200]; int n3 = di->omer_day;
        if (n3 >= 1 && n3 <= 49) { n3--; buf[0] = 0; catn(buf, S_SEF[n3 % 7]); catn(buf, S_SEF[7]); catn(buf, S_SEF[n3 / 7]); push_utf8(c, buf); }
        return;
    }
    if (starts(t, tn, ":=:=")) return;                         /* doctor names (user setting, empty by default) */
    if (starts(t, tn, ":-:-")) {
        char buf[1500]; int n4 = di->sukkot_day - 2;
        if (n4 < 0 || n4 > 5) return;
        buf[0] = 0;
        if (di->diaspora && n4 >= 1) { catn(buf, S_SUK[19]); catn(buf, S_SUK[n4 - 1]); catn(buf, S_SUK[20]); catn(buf, S_SUK[6 + n4 - 1]); catn(buf, S_SUK[21]); catn(buf, S_SUK[12 + n4 - 1]); catn(buf, S_SUK[22]); }
        catn(buf, S_SUK[23]); catn(buf, S_SUK[n4]); catn(buf, S_SUK[24]); catn(buf, S_SUK[6 + n4]); catn(buf, S_SUK[25]); catn(buf, S_SUK[12 + n4]); catn(buf, S_SUK[26]);
        push_utf8(c, buf);
        return;
    }
    if (starts(t, tn, "=-=-")) {
        int n5 = di->sukkot_day - 2;
        if (n5 < 0 || n5 > 5) return;
        { char nm[48] = "()a_"; strcat(nm, HOSH[n5][di->weekday % 7]); push_internal_copy(c, (const uint8_t *)nm, (int)strlen(nm)); }
        return;
    }
    if (starts(t, tn, "==(==(")) {
        char nm[48]; nm[0] = 0; strcat(nm, "()em_hoshanot_"); { int s2 = di->sukkot_day; char dg[4]; dg[0] = (char)('0' + (s2 / 10) % 10); dg[1] = (char)('0' + s2 % 10); dg[2] = 0; strcat(nm, s2 >= 10 ? dg : dg + 1); }
        push_internal_copy(c, (const uint8_t *)nm, (int)strlen(nm));
        return;
    }
    if (starts(t, tn, "([")) { add_mark(d, d->nlines, g_set.spec[0], g_set.spec[1], g_set.spec[2]); return; }
    if (starts(t, tn, "])")) { add_mark(d, d->nlines - 1, g_set.spec[0], g_set.spec[1], g_set.spec[2]); return; }
    if (starts(t, tn, "(:")) { add_mark(d, d->nlines, g_set.kot[0], g_set.kot[1], g_set.kot[2]); return; }
    if (starts(t, tn, ":)")) { add_mark(d, d->nlines - 1, g_set.kot[0], g_set.kot[1], g_set.kot[2]); return; }
    if (starts(t, tn, "(---")) { add_mark(d, d->nlines, 3, 147, 163); return; }
    if (starts(t, tn, "---)")) { add_mark(d, d->nlines - 1, 3, 147, 163); return; }
    if (starts(t, tn, "(===")) { add_mark(d, d->nlines, 255, 128, 64); return; }
    if (starts(t, tn, "===)")) { add_mark(d, d->nlines - 1, 255, 128, 64); return; }
    if (starts(t, tn, "[][]")) return;                          /* random inspirational quote: not ported */

    /* plain text */
    if (n == 1 && p[0] == '\r') { emit(d, (const uint8_t *)" ", 1); return; }
    {
        Wrap w; int len = n;
        w.c = p; w.f = len; w.g = c->wrap_w; w.h = 0; w.i = 0;
        while (w.h < w.f - 1) {
            int n4 = wrap_d(&w), piece_s, piece_e;
            if (w.i >= len || n4 > len) break;
            piece_s = w.i; piece_e = n4; w.i = n4;
            if (len - 1 > w.i && (p[w.i] == '\n' || p[w.i] == ' ')) { ++w.h; ++w.i; }
            while (piece_s < piece_e && p[piece_s] <= ' ') piece_s++;
            while (piece_e > piece_s && p[piece_e - 1] <= ' ') piece_e--;
            emit(d, p + piece_s, piece_e - piece_s);
        }
    }
}

static int doc_run(Doc *d, uint8_t *top, int toplen, int wrap_w, const DayInfo *di) {
    Ctx *c;
    memset(d, 0, sizeof *d);
    {   /* allocate the line store; degrade gracefully on a tight heap */
        int cap = 190000;
        d->arena = NULL;
        while (cap >= 40000 && !(d->arena = (uint8_t *)plat_malloc(cap))) cap = cap * 2 / 3;
        d->arena_cap = d->arena ? cap : 0;
        d->maxlines = d->arena_cap / 30; if (d->maxlines > 5200) d->maxlines = 5200;
        d->off = d->arena ? (int *)plat_malloc(d->maxlines * sizeof(int)) : NULL;
    }
    c = (Ctx *)plat_malloc(sizeof(Ctx));
    if (!d->arena || !d->off || !c) { if (c) plat_free(c); if (top) plat_free(top); doc_free(d); return 0; }
    c->d = d; c->sp = 0; c->wrap_w = wrap_w; c->di = di; c->budget = 200000;
    if (top) push_frame(c, top, toplen, 1);
    else push_utf8(c, "([\r\n\xd7\xa9\xd7\x92\xd7\x99\xd7\x90\xd7\x94\r\n])\r\n");
    while (c->sp > 0 && c->budget-- > 0) {
        Frame *f = &c->st[c->sp - 1];
        int s0, e;
        if (f->pos >= f->len) { if (f->owned) plat_free(f->buf); c->sp--; continue; }
        s0 = f->pos; e = s0;
        while (e < f->len && f->buf[e] != '\n') e++;
        f->pos = (e < f->len) ? e + 1 : e;
        process_line(c, f->buf + s0, e - s0);
    }
    while (c->sp > 0) { Frame *f = &c->st[--c->sp]; if (f->owned) plat_free(f->buf); }
    plat_free(c);
    return 1;
}
int doc_build(Doc *d, const char *resname, int wrap_w, const DayInfo *di) {
    int n = 0; uint8_t *raw = res_load(resname, &n), *top = NULL;
    if (raw) {
        top = (uint8_t *)plat_malloc(n + 3);
        if (top) { memcpy(top, raw, n); top[n] = '\r'; top[n + 1] = '\n'; top[n + 2] = 0; }
        plat_free(raw);
    }
    return doc_run(d, top, top ? n + 2 : 0, wrap_w, di);
}
int doc_build_text(Doc *d, const char *utf8, int wrap_w, const DayInfo *di) {
    int cap = (int)strlen(utf8) + 4, n;
    uint8_t *b = (uint8_t *)plat_malloc(cap);
    if (!b) return 0;
    n = uni_to_internal(utf8, b, cap);
    return doc_run(d, b, n, wrap_w, di);
}
void doc_free(Doc *d) { if (d->arena) plat_free(d->arena); if (d->off) plat_free(d->off); d->arena = NULL; d->off = NULL; d->nlines = 0; }
const uint8_t *doc_line(const Doc *d, int i, int *len) {
    const uint8_t *p = d->arena + d->off[i];
    if (len) *len = (int)strlen((const char *)p);
    return p;
}
int doc_color(const Doc *d, int line, uint8_t *r, uint8_t *g, uint8_t *b) {
    int k, s, e; const Mark *m;
    for (k = 0; k < d->nmarks; k++) if (d->marks[k].line >= line) break;
    if (k >= d->nmarks) return 0;
    m = &d->marks[k];
    if (k & 1) { s = d->marks[k - 1].line; e = m->line; }
    else { if (k + 1 >= d->nmarks) return 0; s = m->line; e = d->marks[k + 1].line; }
    if (s <= line && line <= e) { *r = m->r; *g = m->g; *b = m->b; return 1; }
    return 0;
}

void eng_make_dayinfo(DayInfo *di, int abs_effective) {
    di->diaspora = !g_cities[g_set.city_idx].israel;
    di->flags = cal_flags(abs_effective, di->diaspora, g_set.purim);
    di->sukkot_day = cal_sukkot_day(abs_effective);
    di->chanuka_day = cal_chanuka_day(abs_effective);
    di->omer_day = cal_omer_day(abs_effective);
    di->weekday = abs_effective % 7;
}
