/* MRE (MediaTek) platform glue for the Tfilon port. */
#include "vmsys.h"
#include "vmio.h"
#include "vmgraph.h"
#include "vmchset.h"
#include "vmstdlib.h"
#include "ui.h"

static VMINT layer_hdl[1] = {-1};
static int sw, sh, inited;

void *plat_malloc(size_t n) { return vm_malloc((int)n); }
void  plat_free(void *p) { if (p) vm_free(p); }

void plat_time(int *y, int *mo, int *d, int *hh, int *mm) {
    vm_time_t t;
    vm_get_time(&t);
    *y = t.year; *mo = t.mon; *d = t.day; *hh = t.hour; *mm = t.min;
}

/* settings file: <app path without extension>.cfg */
static int cfg_path(VMINT16 *out) {
    int n, last = -1, i;
    if (vm_get_exec_filename(out) < 0) return 0;
    for (n = 0; out[n] && n < 250; n++) if (out[n] == '.') last = n;
    if (last < 0) last = n;
    i = last;
    out[i++] = '.'; out[i++] = 'c'; out[i++] = 'f'; out[i++] = 'g'; out[i] = 0;
    return 1;
}
int plat_cfg_read(uint8_t *buf, int cap) {
    VMINT16 path[260]; VMFILE h; VMUINT nread = 0;
    if (!cfg_path(path)) return -1;
    h = vm_file_open(path, MODE_READ, 1);
    if (h < 0) return -1;
    if (vm_file_read(h, buf, (VMUINT)cap, &nread) < 0) nread = 0;
    vm_file_close(h);
    return (int)nread;
}
void plat_cfg_write(const uint8_t *buf, int len) {
    VMINT16 path[260]; VMFILE h; VMUINT w = 0;
    if (!cfg_path(path)) return;
    h = vm_file_open(path, MODE_CREATE_ALWAYS_WRITE, 1);
    if (h < 0) return;
    vm_file_write(h, (void *)buf, (VMUINT)len, &w);
    vm_file_close(h);
}

static void do_paint(void) {
    VMUINT8 *buf; FB fb;
    if (!inited) { sw = vm_graphic_get_screen_width(); sh = vm_graphic_get_screen_height(); ui_init(sw, sh); inited = 1; }
    if (layer_hdl[0] < 0) layer_hdl[0] = vm_graphic_create_layer(0, 0, sw, sh, -1);
    if (layer_hdl[0] < 0) return;
    buf = vm_graphic_get_layer_buffer(layer_hdl[0]);
    if (!buf) return;
    fb.px = (uint16_t *)buf; fb.w = sw; fb.h = sh;
    ui_paint(&fb);
    vm_graphic_flush_layer(layer_hdl, 1);
}

static void drop_layer(void) {
    if (layer_hdl[0] >= 0) { vm_graphic_delete_layer(layer_hdl[0]); layer_hdl[0] = -1; }
}

static void handle_sysevt(VMINT message, VMINT param) {
    switch (message) {
    case VM_MSG_PAINT: do_paint(); break;
    case VM_MSG_HIDE: drop_layer(); break;
    case VM_MSG_QUIT: ui_shutdown(); drop_layer(); break;
    }
}

static void handle_keyevt(VMINT event, VMINT keycode) {
    int k = K_NONE;
    if (event != VM_KEY_EVENT_DOWN && event != VM_KEY_EVENT_REPEAT) return;
    switch (keycode) {
    case VM_KEY_UP: case VM_KEY_NUM2: k = K_UP; break;
    case VM_KEY_DOWN: case VM_KEY_NUM8: k = K_DOWN; break;
    case VM_KEY_LEFT: case VM_KEY_NUM4: k = K_LEFT; break;
    case VM_KEY_RIGHT: case VM_KEY_NUM6: k = K_RIGHT; break;
    case VM_KEY_OK: case VM_KEY_NUM5: k = K_OK; break;
    case VM_KEY_LEFT_SOFTKEY: k = K_SL; break;
    case VM_KEY_RIGHT_SOFTKEY: k = K_SR; break;
    case VM_KEY_BACK: case VM_KEY_CLEAR: k = K_BACK; break;
    case VM_KEY_POUND: k = K_POUND; break;
    case VM_KEY_STAR: k = K_STAR; break;
    }
    if (k == K_NONE) return;
    ui_key(k, event == VM_KEY_EVENT_REPEAT);
    if (ui_exit_requested()) { vm_exit_app(); return; }
    do_paint();
}

void vm_main(void) {
    layer_hdl[0] = -1;
    vm_reg_sysevt_callback(handle_sysevt);
    vm_reg_keyboard_callback(handle_keyevt);
}
