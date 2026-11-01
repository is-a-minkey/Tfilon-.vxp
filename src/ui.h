#ifndef UI_H
#define UI_H
#include "engine.h"
enum { K_NONE, K_UP, K_DOWN, K_LEFT, K_RIGHT, K_OK, K_SL, K_SR, K_STAR, K_POUND, K_BACK };
void ui_init(int w, int h);
void ui_key(int key, int repeat);
void ui_paint(FB *fb);
int  ui_exit_requested(void);
void ui_shutdown(void);
/* platform hooks */
void plat_time(int *y, int *mo, int *d, int *hh, int *mm);   /* local date/time, month 1..12 */
int  plat_cfg_read(uint8_t *buf, int cap);                    /* returns bytes read or -1 */
void plat_cfg_write(const uint8_t *buf, int len);
#endif
