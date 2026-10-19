"""ui_texkeys.py <romfs.bin> <out.tsv> [<lasrecomp.log>]: the interface textures of the game files and their content keys.

Every SARC archive (.arc: Nintendo LYT layouts and their BNTX textures, language variants included) in a raw RomFS
image; for each texture the key the runtime logs with LARECOMP_TEXKEY_LOG=1 and cache/ui2x.pak is indexed by
(runtime/ui2x.c ui2x_key): CRC-32 and Adler-32 (zlib's) of level 0's block rows untiled. Output: key, width, height, BNTX format,
archive, texture name. With a log of a run with LARECOMP_TEXKEY_LOG=1: which uploaded textures have a match.
(The interface at 2x: runtime/ui2x.h.)
"""
import collections
import re
import struct
import sys
import zlib

# BNTX format type (high byte) -> bytes per block, block width, block height
FMT = {0x01: (1, 1, 1), 0x02: (1, 1, 1), 0x03: (2, 1, 1), 0x04: (2, 1, 1), 0x05: (2, 1, 1), 0x06: (2, 1, 1),
       0x07: (2, 1, 1), 0x08: (2, 1, 1), 0x09: (2, 1, 1), 0x0a: (2, 1, 1), 0x0b: (4, 1, 1), 0x0c: (4, 1, 1),
       0x0e: (4, 1, 1), 0x1a: (8, 4, 4), 0x1b: (16, 4, 4), 0x1c: (16, 4, 4), 0x1d: (8, 4, 4), 0x1e: (16, 4, 4),
       0x1f: (16, 4, 4), 0x20: (16, 4, 4), 0x2d: (16, 4, 4)}


def romfs_files(f):
    """(path, size, absolute offset) of every file of a raw RomFS image."""
    f.seek(0)
    _, _, _, dm_off, dm_size, _, _, fm_off, fm_size, data_off = struct.unpack('<10Q', f.read(0x50))
    f.seek(dm_off); dm = f.read(dm_size)
    f.seek(fm_off); fm = f.read(fm_size)
    stack = [(0, '')]
    while stack:
        o, path = stack.pop()
        _, sibling, child, fo, _, nlen = struct.unpack_from('<6I', dm, o)
        name = dm[o + 0x18:o + 0x18 + nlen].decode('utf-8', 'replace')
        here = f'{path}/{name}' if name else path
        while fo != 0xFFFFFFFF:
            _, sib, doff, dsize, _, fl = struct.unpack_from('<IIQQII', fm, fo)
            yield f'{here}/{fm[fo + 0x20:fo + 0x20 + fl].decode("utf-8", "replace")}'.lstrip('/'), dsize, data_off + doff
            fo = sib
        c = child
        while c != 0xFFFFFFFF:
            stack.append((c, here))
            c = struct.unpack_from('<6I', dm, c)[1]


def sarc_files(d):
    """(name, data) of each file of a SARC archive."""
    if d[:4] != b'SARC':
        return []
    hlen = struct.unpack_from('<H', d, 4)[0]
    data_off = struct.unpack_from('<I', d, 0x0C)[0]
    n = struct.unpack_from('<H', d, hlen + 6)[0]
    names = hlen + 12 + n * 16 + 8
    out = []
    for i in range(n):
        _, attr, s, e = struct.unpack_from('<IIII', d, hlen + 12 + i * 16)
        no = names + (attr & 0xFFFF) * 4
        name = d[no:d.index(b'\0', no)].decode('utf-8', 'replace') if attr & 0x01000000 else ''
        out.append((name, d[data_off + s:data_off + e]))
    return out


def div_up(a, b):
    return (a + b - 1) // b


# within a GOB (64 bytes x 8 rows): where each row's 16-byte columns are (runtime/nvn/tegra.c gob_offset)
GOB = [[((x >> 5) << 8) | ((y >> 1) << 6) | (((x >> 4) & 1) << 5) | ((y & 1) << 4) for x in range(0, 64, 16)] for y in range(8)]


def deswizzle(src, wb, hb, bpb, bh_log2):
    """Block-linear -> block rows (tegra_deswizzle, one slice)."""
    row = wb * bpb
    block = 512 << bh_log2
    width_gobs = div_up(row, 64)
    out = bytearray(row * hb)
    for y in range(hb):
        gy = y >> 3
        base = (gy >> bh_log2) * width_gobs * block + (gy & ((1 << bh_log2) - 1)) * 512
        g = GOB[y & 7]
        lo = y * row
        for x in range(0, row, 16):
            n = min(16, row - x)
            o = base + (x >> 6) * block + g[(x & 63) >> 4]
            out[lo + x:lo + x + n] = src[o:o + n]
    return bytes(out)


def bntx_textures(b):
    """(name, width, height, BNTX format, key) of each texture of a BNTX file."""
    res = []
    for m in re.finditer(b'BRTI', b):
        o = m.start()
        try:
            fmt = struct.unpack_from('<I', b, o + 0x1C)[0]
            w, h = struct.unpack_from('<ii', b, o + 0x24)
            layout = struct.unpack_from('<i', b, o + 0x34)[0]
            name_off, _, ptrs = struct.unpack_from('<QQQ', b, o + 0x60)
            nl = struct.unpack_from('<H', b, name_off)[0]
            name = b[name_off + 2:name_off + 2 + nl].decode('utf-8', 'replace')
            data0 = struct.unpack_from('<Q', b, ptrs)[0]
        except (struct.error, ValueError):
            continue
        f = FMT.get(fmt >> 8)
        if not f or not (0 < w <= 16384 and 0 < h <= 16384):
            continue
        bpb, bw, bh = f
        wb, hb = div_up(w, bw), div_up(h, bh)
        bhl = layout & 7  # (BNTX: log2 GOBs per block, shrunk as tegra.c level_block_height does)
        while bhl > 0 and (8 << (bhl - 1)) >= hb:
            bhl -= 1
        lin = deswizzle(b[data0:], wb, hb, bpb, bhl)
        key = (zlib.crc32(lin) & 0xFFFFFFFF) << 32 | (zlib.adler32(lin) & 0xFFFFFFFF)
        res.append((name, w, h, fmt, f'{key:016x}'))
    return res


def main():
    rom = open(sys.argv[1], 'rb')
    rows = []
    for path, size, off in list(romfs_files(rom)):
        if not path.endswith('.arc'):
            continue
        rom.seek(off)
        for _, fd in sarc_files(rom.read(size)):
            if fd[:4] == b'BNTX':
                rows += [(key, w, h, fmt, path, name) for name, w, h, fmt, key in bntx_textures(fd)]
    with open(sys.argv[2], 'w', encoding='utf-8') as out:
        for key, w, h, fmt, path, name in rows:
            out.write(f'{key}\t{w}\t{h}\t{fmt:#x}\t{path}\t{name}\n')
    keys = {(k, w, h) for k, w, h, *_ in rows}
    print(f'{len(rows)} textures, {len(keys)} keys')
    if len(sys.argv) > 3:
        seen = {}
        for line in open(sys.argv[3], encoding='utf-8', errors='replace'):
            m = re.search(r'\[texkey\] ([0-9a-f]{16}) (\d+)x(\d+) nvn (0x[0-9a-f]+) bpb \d+ levels (\d+)', line)
            if m:
                seen[(m.group(1), int(m.group(2)), int(m.group(3)))] = int(m.group(5))
        hit = [k for k in seen if k in keys]
        kinds = collections.Counter('match' if k in keys else 'mipmapped (3D)' if lv > 1 else 'other' for k, lv in seen.items())
        print(f'uploaded: {len(seen)} textures - {dict(kinds)}')
        names = {(k, w, h): (p.split("/")[-1], n) for k, w, h, _, p, n in rows}
        for k in sorted(hit, key=lambda k: -k[1] * k[2])[:20]:
            print(f'  {k[0]} {k[1]}x{k[2]}  {names[k][0]}  {names[k][1]}')


if __name__ == '__main__':
    main()
