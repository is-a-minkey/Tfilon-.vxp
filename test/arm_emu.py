#!/usr/bin/env python3
"""Minimal MRE-style ARM emulator: loads a .vxp (ELF), applies dynamic relocations, serves the vm_* API calls the
Tfilon port uses, replays keys and saves every flushed frame as PNG.
usage: arm_emu.py app.vxp "keyscript" [year mon day hour min] [heap_kb]"""
import struct, sys, os
from unicorn import *
from unicorn.arm_const import *
from PIL import Image

vxp = open(sys.argv[1], 'rb').read()
script = sys.argv[2] if len(sys.argv) > 2 else ''
T = [int(x) for x in sys.argv[3:8]] if len(sys.argv) > 7 else [2026, 10, 30, 14, 0]
HEAP_KB = int(sys.argv[8]) if len(sys.argv) > 8 else 1024
OUT = os.environ.get('EMU_OUT', '/tmp/tfilon/test/emu')
os.makedirs(OUT, exist_ok=True)

BASE, HEAP, STACK, STUBS, RET = 0x00100000, 0x04000000, 0x08000000, 0x7F000000, 0x7F100000
STACK_SIZE = int(os.environ.get('EMU_STACK', 0x8000))      # 32 KB stack: stricter than most phones

def u16(o): return struct.unpack_from('<H', vxp, o)[0]
def u32(o): return struct.unpack_from('<I', vxp, o)[0]

uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
try: uc.ctl_set_cpu_model(UC_CPU_ARM_926)
except Exception as e: print("cpu model:", e)

# ---- load ELF ----
e_entry, e_phoff, e_phentsize, e_phnum = u32(24), u32(28), u16(42), u16(44)
img_size = 0; loads = []; dyn = None
for i in range(e_phnum):
    o = e_phoff + i * e_phentsize
    p_type, p_off, p_va, p_pa, p_fsz, p_msz, p_fl, p_al = struct.unpack_from('<8I', vxp, o)
    if p_type == 1: loads.append((p_off, p_va, p_fsz, p_msz)); img_size = max(img_size, p_va + p_msz)
    if p_type == 2: dyn = (p_off, p_va, p_fsz)
uc.mem_map(BASE, (img_size + 0xFFF) & ~0xFFF, UC_PROT_ALL)
for (off, va, fsz, msz) in loads: uc.mem_write(BASE + va, vxp[off:off + fsz])
d = {}
o = dyn[0]
while True:
    tag, val = struct.unpack_from('<II', vxp, o); o += 8
    if tag == 0: break
    d[tag] = val
def reloc(tbl, sz):
    n = 0
    for k in range(sz // 8):
        r_off, r_info = struct.unpack_from('<II', vxp, tbl + k * 8)
        typ, sym = r_info & 0xFF, r_info >> 8
        loc = BASE + r_off
        cur = struct.unpack('<I', bytes(uc.mem_read(loc, 4)))[0]
        if typ == 23:   val = (cur + BASE) & 0xFFFFFFFF                    # R_ARM_RELATIVE
        elif typ in (2, 21, 22):                                              # ABS32 / GLOB_DAT / JUMP_SLOT
            st_value = struct.unpack_from('<I', vxp, d[6] + sym * 16 + 4)[0]
            val = ((st_value + BASE if st_value else 0) + (cur if typ == 2 else 0)) & 0xFFFFFFFF
        else: raise SystemExit("unsupported relocation type %d" % typ)
        uc.mem_write(loc, struct.pack('<I', val)); n += 1
    return n
nrel = reloc(d[17], d[18]) + (reloc(d[23], d[2]) if 23 in d else 0)
print("loaded %d bytes at 0x%x, %d relocations applied, entry 0x%x" % (img_size, BASE, nrel, BASE + e_entry))

# ---- memory regions ----
uc.mem_map(HEAP, 8 * 1024 * 1024, UC_PROT_ALL)
uc.mem_map(STACK, STACK_SIZE, UC_PROT_ALL)
uc.mem_map(STUBS, 0x1000, UC_PROT_ALL); uc.mem_map(RET, 0x1000, UC_PROT_ALL)
uc.mem_write(STUBS, struct.pack('<I', 0xE12FFF1E) * 0x400)             # every stub = bx lr

heap_limit = HEAP_KB * 1024
blocks = []                                                            # sorted (addr, size)
peak = 0
def heap_used(): return sum(s for _, s in blocks)
def alloc(size, zero=False):
    global peak
    size = (max(size, 1) + 7) & ~7
    prev_end = HEAP
    for idx, (a, s) in enumerate(blocks + [(HEAP + heap_limit, 0)]):
        if a - prev_end >= size:
            blocks.insert(idx, (prev_end, size))
            if zero: uc.mem_write(prev_end, b'\0' * size)
            peak = max(peak, heap_used())
            return prev_end
        prev_end = a + s
    return 0
def free(p):
    for i, (a, s) in enumerate(blocks):
        if a == p: del blocks[i]; return

def rd_cstr(p):
    out = bytearray()
    while True:
        b = uc.mem_read(p, 1)[0]
        if b == 0: break
        out.append(b); p += 1
    return out.decode('latin1')
def rd_wstr(p):
    s = ''
    while True:
        c = struct.unpack('<H', bytes(uc.mem_read(p, 2)))[0]
        if c == 0: return s
        s += chr(c); p += 2

# ---- fake MRE API ----
state = {'sys_cb': 0, 'key_cb': 0, 'exit': False, 'layers': {}, 'next_layer': 0, 'frames': 0}
import pickle
CFG = os.environ.get('EMU_CFG')
files = pickle.load(open(CFG,'rb')) if CFG and os.path.exists(CFG) else {}
fhandles = {}
def reg(r): return uc.reg_read(r)
def ret(v): uc.reg_write(UC_ARM_REG_R0, v & 0xFFFFFFFF)
def arg(i):
    if i < 4: return uc.reg_read([UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3][i])
    return struct.unpack('<I', bytes(uc.mem_read(uc.reg_read(UC_ARM_REG_SP) + 4 * (i - 4), 4)))[0]

def api_vm_malloc(): ret(alloc(arg(0)))
def api_vm_calloc(): ret(alloc(arg(0), True))
def api_vm_realloc():
    p, n = arg(0), arg(1)
    q = alloc(n)
    if q and p:
        old = [s for a, s in blocks if a == p]
        if old: uc.mem_write(q, bytes(uc.mem_read(p, min(old[0], n))))
        free(p)
    ret(q)
def api_vm_free(): free(arg(0))
def api_vm_get_time():
    p = arg(0); uc.mem_write(p, struct.pack('<6i', T[0], T[1], T[2], T[3], T[4], 0)); ret(0)
def api_vm_graphic_get_screen_width(): ret(240)
def api_vm_graphic_get_screen_height(): ret(320)
def api_vm_graphic_get_character_height(): ret(16)
def api_vm_graphic_create_layer():
    x, y, w, h = arg(0), arg(1), arg(2), arg(3)
    buf = alloc(w * h * 2)
    if not buf: ret(0xFFFFFFFF); return
    hnd = state['next_layer']; state['next_layer'] += 1
    state['layers'][hnd] = (buf, w, h); ret(hnd)
def api_vm_graphic_get_layer_buffer(): ret(state['layers'][arg(0)][0])
def api_vm_graphic_delete_layer():
    l = state['layers'].pop(arg(0), None)
    if l: free(l[0])
    ret(0)
def api_vm_graphic_flush_layer():
    arr, n = arg(0), arg(1)
    hnd = struct.unpack('<i', bytes(uc.mem_read(arr, 4)))[0]
    buf, w, h = state['layers'][hnd]
    raw = bytes(uc.mem_read(buf, w * h * 2))
    img = Image.frombuffer('RGB', (w, h), b''.join(bytes(((v >> 11) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31)) for v in struct.unpack('<%dH' % (w * h), raw)))
    img.save('%s/frame_%03d.png' % (OUT, state['frames'])); state['frames'] += 1; ret(0)
def api_vm_reg_sysevt_callback(): state['sys_cb'] = arg(0)
def api_vm_reg_keyboard_callback(): state['key_cb'] = arg(0)
def api_vm_exit_app(): state['exit'] = True
def api_vm_get_exec_filename():
    s = 'E:\\Apps\\Tfilon.vxp'; uc.mem_write(arg(0), b''.join(struct.pack('<H', ord(c)) for c in s) + b'\0\0'); ret(0)
def api_vm_file_open():
    name, mode = rd_wstr(arg(0)), arg(1)
    if mode == 1 and name not in files: ret(0xFFFFFFFE); return          # VM_FILE_OPEN_ERROR
    if mode in (2, 4, 8) and (mode == 4 or name not in files): files[name] = bytearray()
    h = len(fhandles) + 1; fhandles[h] = [name, 0]; ret(h)
def api_vm_file_read():
    h, buf, n, pn = arg(0), arg(1), arg(2), arg(3)
    name, pos = fhandles[h]; data = bytes(files[name][pos:pos + n]); uc.mem_write(buf, data); fhandles[h][1] += len(data)
    if pn: uc.mem_write(pn, struct.pack('<I', len(data)))
    ret(0)
def api_vm_file_write():
    h, buf, n, pw = arg(0), arg(1), arg(2), arg(3)
    name, pos = fhandles[h]; files[name][pos:pos + n] = bytes(uc.mem_read(buf, n)); fhandles[h][1] += n
    if pw: uc.mem_write(pw, struct.pack('<I', n))
    ret(0)
def api_vm_file_close(): ret(0)

APIS = {k[4:]: v for k, v in list(globals().items()) if k.startswith('api_')}
stub_addr = {}; stub_name = {}
def stub_for(name):
    if name not in stub_addr:
        a = STUBS + 4 * (len(stub_addr) + 1); stub_addr[name] = a; stub_name[a] = name
    return stub_addr[name]
seen_missing = set()
def on_stub(uc_, address, size, ud):
    if address == STUBS:                       # vm_get_sym_entry(name)
        name = rd_cstr(reg(UC_ARM_REG_R0)); ret(stub_for(name)); return
    name = stub_name.get(address)
    if name is None: return
    fn = APIS.get(name)
    if fn: fn()
    else:
        if name not in seen_missing: seen_missing.add(name); print("  [unimplemented API called: %s]" % name)
        ret(0)
uc.hook_add(UC_HOOK_CODE, on_stub, begin=STUBS, end=STUBS + 0xFFF)

icount = [0]
def on_block(uc_, address, size, ud): icount[0] += size // 4 if size else 1
uc.hook_add(UC_HOOK_BLOCK, on_block)
def on_fault(uc_, access, address, size, value, ud):
    print("!! memory fault: access=%d addr=0x%x pc=0x%x lr=0x%x sp=0x%x" % (access, address, reg(UC_ARM_REG_PC), reg(UC_ARM_REG_LR), reg(UC_ARM_REG_SP)))
    return False
uc.hook_add(UC_HOOK_MEM_READ_UNMAPPED | UC_HOOK_MEM_WRITE_UNMAPPED | UC_HOOK_MEM_FETCH_UNMAPPED, on_fault)

def call(fn, *args, label=''):
    for i, a in enumerate(args[:4]): uc.reg_write([UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3][i], a & 0xFFFFFFFF)
    uc.reg_write(UC_ARM_REG_SP, STACK + STACK_SIZE - 16)
    uc.reg_write(UC_ARM_REG_LR, RET)
    start = icount[0]
    try:
        uc.emu_start(fn, RET, count=900_000_000)
    except UcError as e:
        print("!! emulation error in %s: %s  pc=0x%x" % (label, e, reg(UC_ARM_REG_PC))); raise SystemExit(1)
    return icount[0] - start

# ---- run: entry -> vm_main registers callbacks ----
n = call(BASE + e_entry, STUBS, 0, 0, label='entry')
print("entry/vm_main done (%d instructions). sysevt_cb=0x%x key_cb=0x%x" % (n, state['sys_cb'], state['key_cb']))
VM_MSG_PAINT, VM_MSG_CREATE, VM_MSG_QUIT = 1, 4, 5
call(state['sys_cb'], VM_MSG_CREATE, 0, label='create')
n = call(state['sys_cb'], VM_MSG_PAINT, 0, label='paint'); print("first paint: %d instructions, heap peak %d KB" % (n, peak // 1024))

KEYS = {'u': -1, 'd': -2, 'l': -3, 'r': -4, 'o': -5, 's': -6, 'S': -7, 'b': -9, '#': 35, '*': 42, '2': 50, '8': 56, '4': 52, '6': 54, '5': 53}
for ch in script:
    if state['exit']: print("app requested exit"); break
    if ch in KEYS:
        f0 = state['frames']
        n = call(state['key_cb'], 2, KEYS[ch], label='key ' + ch)
        print("key %r: %9d instr  heap %4d KB (peak %d KB)  frames %d  %s" % (ch, n, heap_used() // 1024, peak // 1024, state['frames'] - f0, '~%.2fs @260MHz' % (n / 150e6)))
call(state['sys_cb'], VM_MSG_QUIT, 0, label='quit')
print("final heap in use: %d bytes; frames saved: %d in %s" % (heap_used(), state['frames'], OUT))
print("files written by the app:", {k: len(v) for k, v in files.items()})
if CFG: pickle.dump(files, open(CFG, "wb"))
