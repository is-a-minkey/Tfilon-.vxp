#ifndef CAL_H
#define CAL_H
#include <stdint.h>

typedef struct { int d, m, y; } CalDate;         /* day (1-based), month (0-based), year */

int  hy_leap(int y);
int  cal_greg_to_abs(int y, int m, int d);           /* Gregorian (m 0-based) -> absolute day */
void cal_abs_to_greg(int abs, CalDate *g);           /* absolute day -> Gregorian date */
int  cal_hebrew_to_abs(int d, int m, int y, int clamp, int *day_out);
void cal_abs_to_hebrew(int abs, CalDate *h);
int  cal_hebrew_month_len(int y, int m);
/* holiday / day-type flags for the day with absolute number `abs` (port of g.long_a) */
uint64_t cal_flags(int abs, int diaspora, int purim);
/* helpers ported from g.int_a/int_b/int_c (omer day, sukkot day, chanuka day) */
int  cal_omer_day(int abs);          /* 1..49 (|256 for 105-based variant) or -1 */
int  cal_sukkot_day(int abs);        /* 1..7 or 0 */
int  cal_chanuka_day(int abs);       /* day index or -1 */
#endif
