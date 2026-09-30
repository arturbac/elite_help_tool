#!/usr/bin/env python3
"""The layer's newest sample of the screen as a PNG - for looking at what the game shows from outside it.

    sample_to_png.py [sample.bin] [out.png]

The sample is the file the layer keeps beside the socket, captures/sample.bin (see overlay::sample_header_t).
Its seq is odd while the layer writes, so a copy is taken again until it was the same even number before
and after. Nothing but the standard library.
"""
import os
import struct
import sys
import time
import zlib

HEADER = struct.Struct('<IIQIIQffffII16x')
MAGIC = 0x53544845


def read(path):
    for _ in range(50):
        with open(path, 'rb') as f:
            data = f.read()
        magic, version, seq, w, h, taken, left, top, rw, rh, sw, sh = HEADER.unpack_from(data)
        if magic != MAGIC:
            sys.exit(f'{path}: not a sample')
        if seq % 2 == 0:
            with open(path, 'rb') as f:
                again = HEADER.unpack_from(f.read(HEADER.size))[2]
            if again == seq:
                return dict(seq=seq, width=w, height=h, taken_ms=taken, region=(left, top, rw, rh),
                            surface=(sw, sh), rgba=data[64:64 + w * h * 4])
        time.sleep(0.01)
    sys.exit(f'{path}: the layer kept writing it')


def png(path, w, h, rgba):
    rows = b''.join(b'\0' + rgba[y * w * 4:(y + 1) * w * 4] for y in range(h))
    chunk = lambda kind, body: struct.pack('>I', len(body)) + kind + body + struct.pack('>I', zlib.crc32(kind + body))
    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0))
                + chunk(b'IDAT', zlib.compress(rows, 6)) + chunk(b'IEND', b''))


def main():
    spool = os.path.join(os.environ.get('HOME', '/tmp'), '.local/share/elite_help_tool/captures')
    source = sys.argv[1] if len(sys.argv) > 1 else os.path.join(spool, 'sample.bin')
    target = sys.argv[2] if len(sys.argv) > 2 else 'sample.png'
    s = read(source)
    png(target, s['width'], s['height'], s['rgba'])
    age = time.time() * 1000 - s['taken_ms']
    print(f"{target}: {s['width']}x{s['height']}, seq {s['seq']}, {age / 1000:.1f} s old, "
          f"region {s['region']} of {s['surface'][0]}x{s['surface'][1]}")


if __name__ == '__main__':
    main()
