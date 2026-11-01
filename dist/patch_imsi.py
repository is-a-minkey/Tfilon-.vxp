#!/usr/bin/env python3
"""
patch_imsi.py - authorise a .vxp for YOUR SIM card (runs completely offline).

Nokia Series 30+ phones (225, 220, 215, 5310 ...) only start a homebrew .vxp that carries the IMSI of the SIM that is
inside the phone. The IMSI is the 15-digit subscriber number of the SIM. The file stores it as the character "9"
followed by the 15 digits (16 bytes); this script adds the "9" for you - just give it the plain 15 digits.

usage:
    python3 patch_imsi.py Tfilon.vxp 425XXXXXXXXXXXX            -> writes Tfilon_signed.vxp
    python3 patch_imsi.py Tfilon.vxp 425XXXXXXXXXXXX out.vxp    -> custom output name
    python3 patch_imsi.py Tfilon.vxp --show                     -> print what is currently stored
    python3 patch_imsi.py Tfilon.vxp 425XXXXXXXXXXXX --ram 768  -> also change the requested RAM (KB)
"""
import struct, sys

def tags_start(data):
    pos = struct.unpack_from('<I', data, len(data) - 12)[0]
    if pos <= 0 or pos >= len(data) - 12 or data[:4] != b'\x7fELF':
        raise SystemExit("This does not look like a valid MRE .vxp file.")
    return pos

def find_tag(data, wanted):
    pos = tags_start(data)
    while True:
        tid, tlen = struct.unpack_from('<II', data, pos)
        if tid == 0:
            return None
        if tid == wanted:
            return pos, tlen
        pos += 8 + tlen

def get_imsi_raw(data):
    t = find_tag(data, 0x12)
    return data[t[0] + 8: t[0] + 8 + t[1]].rstrip(b'\0').decode('ascii', 'replace') if t else None

def get_ram(data):
    t = find_tag(data, 0x0F)
    return struct.unpack_from('<I', data, t[0] + 8)[0] if t else None

def set_imsi(data, imsi15):
    pos, tlen = find_tag(data, 0x12)
    body = b'9' + imsi15.encode('ascii')                      # exactly 16 bytes, no terminator
    return data[:pos] + struct.pack('<II', 0x12, len(body)) + body + data[pos + 8 + tlen:]

def set_ram(data, kb):
    pos, tlen = find_tag(data, 0x0F)
    return data[:pos + 8] + struct.pack('<I', kb) + data[pos + 12:]

def normalise_imsi(text):
    if not text.isdigit():
        raise SystemExit("The IMSI must contain digits only. Got: %r" % text)
    if len(text) == 16 and text[0] == '9':                      # user already added the leading 9
        text = text[1:]
    if len(text) != 15:
        raise SystemExit("The IMSI must be 15 digits (you gave %d). Check it and try again." % len(text))
    return text

def main():
    a = sys.argv[1:]
    if not a or a[0] in ('-h', '--help'):
        print(__doc__); return
    src = a[0]
    data = open(src, 'rb').read()
    if len(a) > 1 and a[1] == '--show':
        print("IMSI tag stored in file :", get_imsi_raw(data)); print("Requested RAM (KB)      :", get_ram(data)); return
    if len(a) < 2:
        print(__doc__); return
    imsi = normalise_imsi(a[1])
    ram = None
    rest = a[2:]
    if '--ram' in rest:
        i = rest.index('--ram'); ram = int(rest[i + 1]); del rest[i:i + 2]
    out = rest[0] if rest else src.rsplit('.', 1)[0] + '_signed.vxp'
    data = set_imsi(data, imsi)
    if ram: data = set_ram(data, ram)
    open(out, 'wb').write(data)
    chk = open(out, 'rb').read()
    print("Wrote %s  (IMSI tag %s, RAM %s KB)" % (out, get_imsi_raw(chk), get_ram(chk)))

if __name__ == '__main__':
    main()
