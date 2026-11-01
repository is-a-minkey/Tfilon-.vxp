/* Sunrise/sunset: faithful port of the original app's classes x (astronomy) and t (time-zone/summer-time handling). */
#include <math.h>
#include <limits.h>
#include "engine.h"
#include "data.h"

#define PI_ 3.141592653589793
static double rad(double d) { return d / 180.0 * PI_; }
static double deg(double r) { return r * 180.0 / PI_; }
static double x_asin(double v) { if (v > 1.0 || v < -1.0) return NAN; return asin(v); }
static double x_acos(double v) { if (v > 1.0 || v < -1.0) return NAN; return 1.5707963267948966 - x_asin(v); }

static double xa(double d) { return (d - 2451545.0) / 36525.0; }
static double xb(double d) { return 280.46646 + d * (36000.76983 + d * 3.032E-4); }
static double xc(double d) { return 357.52911 + d * (35999.05029 - d * 1.537E-4); }
static double xd(double d) {
    double d3 = 21.448 - d * (46.815 + d * (5.9E-4 - d * 0.001813));
    double d4 = 23.0 + (26.0 + d3 / 60.0) / 60.0;
    double d5 = 125.04 - d * 1934.136;
    return d4 + 0.00256 * cos(rad(d5));
}
static double xe(double d) {                       /* solar declination (radians) */
    double d4 = xd(d), d6 = xb(d), d9 = rad(xc(d)), d10 = sin(d9), d12 = sin(d9 + d9), d14 = sin(d9 + d9 + d9);
    double d15 = d10 * (1.914602 - d * (0.004817 + d * 1.4E-5)) + d12 * (0.019993 - d * 1.01E-4) + d14 * 2.89E-4;
    double d16 = d6 + d15, d17 = 125.04 - d * 1934.136;
    double d18 = d16 - 0.00569 - 0.00478 * sin(rad(d17));
    return x_asin(sin(rad(d4)) * sin(rad(d18)));
}
static double xf(double d) {                       /* equation of time (seconds) */
    double d3 = xd(d), d4 = xb(d), d6 = 0.016708634 - d * (4.2037E-5 + d * 1.267E-7), d7 = xc(d);
    double d8 = tan(rad(d3) / 2.0); d8 *= d8;
    double d9 = sin(2.0 * rad(d4)), d10 = sin(rad(d7)), d11 = cos(2.0 * rad(d4)), d12 = sin(4.0 * rad(d4)), d13 = sin(2.0 * rad(d7));
    double d14 = d8 * d9 - d6 * 2.0 * d10 + d6 * 4.0 * d8 * d10 * d11 - d8 * 0.5 * d8 * d12 - d6 * 1.25 * d6 * d13;
    return deg(d14) * 240.0;
}
static double xha(double lat, double decl, double zen) {   /* hour angle in degrees */
    double d5 = NAN;
    if (!isnan(decl)) {
        double d6 = rad(lat);
        double d7 = cos(rad(zen)) / (cos(d6) * cos(decl)) - tan(d6) * tan(decl);
        d5 = x_acos(d7);
        if (!isnan(d5)) { d5 = deg(d5); if (zen < 0.0) d5 = -d5; }
    }
    return d5;
}
static double xg(double elev) { return deg(x_acos(6356.9 / (6356.9 + elev / 1000.0))) + 90.83333333333333; }

/* x.a(abs, lat_arcsec, lon_arcsec, elevation, out[2]) : UTC seconds of sunrise/sunset */
static void x_sun(int abs, int lat_as, int lon_as, int elev, int out[2]) {
    double jd = (double)abs + 347997.0, lat = (double)lat_as / 3600.0, c = (double)lon_as / -3600.0;
    double t = xa(jd + 0.5 + c / 360.0), eq = xf(t), decl = xe(t);
    double ha = xha(lat, decl, xg(elev)); int k;
    if (isnan(ha)) { out[0] = out[1] = INT_MIN; return; }
    for (k = 0; k < 2; k++) {
        double d4 = k == 0 ? c - ha : c + ha;
        double d5 = (d4 + 180.0) * 240.0 - eq;
        double d6 = xa(jd + d5 / 86400.0), d7 = xf(d6), d8 = xe(d6);
        double d9 = xha(lat, d8, xg(elev));
        if (isnan(d9)) { out[k] = INT_MIN; continue; }
        d4 = k == 0 ? c - d9 : c + d9;
        d5 = (d4 + 180.0) * 240.0 - d7;
        out[k] = (int)d5;
    }
}
static int dms_secs(const int *a, int limit_check_deg, int neg_only_if_lt180) {
    int v = a[0] * 3600 + a[1] * 60 + a[2];
    if (a[3] > 0 && (!neg_only_if_lt180 || a[0] < 180)) v = -v;
    (void)limit_check_deg;
    return v;
}
static int to_local(int utc, int tz_min, int summer) {     /* t.int_a(int) */
    if (utc == INT_MIN) return utc;
    utc += tz_min * 60;
    if (summer) utc += 3600;
    if (utc < 0) return 86400 - (-utc) % 86400;
    return utc % 86400;
}
/* local sunrise/sunset seconds-of-day for the given absolute day (INT_MIN when the sun does not rise/set) */
void eng_sun_times(int abs, const City *c, int summer, int *sunrise, int *sunset) {
    int out[2];
    x_sun(abs, dms_secs(c->lat, 90, 0), dms_secs(c->lon, 180, 1), c->elev, out);
    if (sunrise) *sunrise = to_local(out[0], c->tz_min, summer);
    if (sunset)  *sunset  = to_local(out[1], c->tz_min, summer);
}
