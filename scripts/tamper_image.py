#!/usr/bin/env python3
"""Flip one byte of a signed image and report what changed."""

import hashlib
import sys


def main():
    if len(sys.argv) != 4:
        sys.exit("usage: tamper_image.py <in.bin> <out.bin> <offset>")

    in_path, out_path, offset = sys.argv[1], sys.argv[2], int(sys.argv[3], 0)
    data = bytearray(open(in_path, "rb").read())
    if offset >= len(data):
        sys.exit(f"offset 0x{offset:x} is past the end of {in_path} (0x{len(data):x} bytes)")

    original = data[offset]
    data[offset] ^= 0x01
    open(out_path, "wb").write(data)

    print(f"input:  {in_path}")
    print(f"output: {out_path}")
    print(f"offset: 0x{offset:x}  0x{original:02x} -> 0x{data[offset]:02x}")
    print(f"sha256 before: {hashlib.sha256(open(in_path, 'rb').read()).hexdigest()}")
    print(f"sha256 after:  {hashlib.sha256(data).hexdigest()}")


if __name__ == "__main__":
    main()
