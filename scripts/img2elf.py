#!/usr/bin/env python3
"""ESP32 app image -> ELF32 (Xtensa LE) with one PT_LOAD + section per segment."""
import struct, sys

def region(a):
    if 0x400d0000 <= a < 0x40400000: return ".flash.text", 5   # R+X (IROM)
    if 0x400c2000 <= a < 0x400d0000: return ".flash.rodata_x", 5
    if 0x3f400000 <= a < 0x3f800000: return ".flash.rodata", 4  # DROM
    if 0x40080000 <= a < 0x400a0000: return ".iram0.text", 5
    if 0x3ffae000 <= a < 0x40000000: return ".dram0.data", 6
    if 0x50000000 <= a < 0x50002000: return ".rtc.data", 6
    if 0x400c0000 <= a < 0x400c2000: return ".rtc.text", 5
    return ".seg", 6

def main(src, dst):
    d = open(src, "rb").read()
    magic, nseg = d[0], d[1]
    entry = struct.unpack_from("<I", d, 4)[0]
    assert magic == 0xE9, "not an ESP image"
    off, segs = 24, []
    for _ in range(nseg):
        addr, size = struct.unpack_from("<II", d, off)
        segs.append((addr, d[off+8:off+8+size])); off += 8 + size
    names = [region(a) for a, _ in segs]
    shstr = b"\0" + b"".join(n.encode()+b"\0" for n, _ in names) + b".shstrtab\0"
    # file layout: ehdr(52) | phdrs | seg data | shstrtab | shdrs
    ph_off = 52; ph_sz = 32*len(segs)
    pos = ph_off + ph_sz
    offs = []
    for _, data in segs:
        pos = (pos + 3) & ~3; offs.append(pos); pos += len(data)
    str_off = pos; pos += len(shstr); pos = (pos + 3) & ~3
    sh_off = pos
    out = bytearray(sh_off + 40*(len(segs)+2))
    ident = b"\x7fELF" + bytes([1,1,1,0]) + b"\0"*8
    out[0:52] = ident + struct.pack("<HHIIIIIHHHHHH", 2, 94, 1, entry, ph_off, sh_off, 0,
                                    52, 32, len(segs), 40, len(segs)+2, len(segs)+1)
    nm_off = 1; nm = []
    for n, _ in names: nm.append(nm_off); nm_off += len(n)+1
    shstr_name = nm_off
    for i, ((addr, data), (n, fl)) in enumerate(zip(segs, names)):
        out[ph_off+32*i: ph_off+32*i+32] = struct.pack("<8I", 1, offs[i], addr, addr, len(data), len(data), fl, 4)
        out[offs[i]:offs[i]+len(data)] = data
        shf = 2 | (4 if fl & 1 else 0) | (1 if fl & 2 else 0)
        out[sh_off+40*(i+1): sh_off+40*(i+2)] = struct.pack("<10I", nm[i], 1, shf, addr, offs[i], len(data), 0, 0, 4, 0)
    out[str_off:str_off+len(shstr)] = shstr
    out[sh_off+40*(len(segs)+1): sh_off+40*(len(segs)+2)] = struct.pack("<10I", shstr_name, 3, 0, 0, str_off, len(shstr), 0, 0, 1, 0)
    open(dst, "wb").write(out)
    for (a, dta), (n, fl) in zip(segs, names): print(f"{n:16s} {a:#010x} {len(dta):#8x}")
    print(f"entry {entry:#x}")

main(*sys.argv[1:3])
