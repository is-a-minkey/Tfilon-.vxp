#ifndef DATA_H
#define DATA_H
typedef struct { int id; int lat[4]; int lon[4]; const char *name; int candle; int israel; int tz_min; int elev; } City;
typedef struct { const char *name; unsigned int off; unsigned int clen; unsigned int rlen; } ResEntry;
extern const unsigned short g_ab_uni[55];
extern const unsigned char g_ab_byte[55];
extern const unsigned char g_glyph_of_byte[256];
extern const unsigned char g_gl1_w[54], g_gl1_h[54], g_gl2_w[54], g_gl2_h[54];
extern const unsigned short g_gl1_off[54], g_gl2_off[54];
extern const unsigned char g_gl1_bits[], g_gl2_bits[];
extern const unsigned char g_asc[95][16];
extern const City g_cities[]; extern const int g_ncities;
extern const ResEntry g_res[]; extern const int g_nres;
extern const unsigned char g_blob[];
#endif
