#!/usr/bin/env python3
"""Inspect an MCUboot-format signed image, or forge a re-hashed one with a foreign signature.

check <img.bin> <pub.pem>...      hash TLV valid?, which key the key-hash names, which key verifies
forge <in.bin> <out.bin> <offset> <signer.pem>
    flip bit 0 at <offset>, recompute the SHA256 TLV, keep the original key-hash TLV and
    sign with <signer.pem>. The result passes the hash check, names the trusted key, and fails
    signature verification against it.
"""

import hashlib
import struct
import sys
from dataclasses import dataclass

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, utils

TLV_PROT_MAGIC = 0x6908
TLV_INFO_MAGIC = 0x6907
TLV_KEYHASH = 0x01
TLV_SHA256 = 0x10
TLV_ECDSA256 = 0x22


@dataclass
class Image:
    raw: bytes
    magic: int
    hdr_size: int
    prot_size: int
    img_size: int
    version: str
    tlvs: list  # (type, offset of value, value)

    @property
    def signed_end(self):
        """Header + body + protected TLVs: the bytes the hash and the signature cover."""
        return self.hdr_size + self.img_size + self.prot_size

    def tlv(self, kind):
        found = [t for t in self.tlvs if t[0] == kind]
        if len(found) != 1:
            sys.exit(f"expected one TLV 0x{kind:02x}, found {len(found)}")
        return found[0]


def parse(raw):
    magic, _load, hdr_size, prot_size, img_size, _flags = struct.unpack_from("<IIHHII", raw, 0)
    major, minor, rev, build = struct.unpack_from("<BBHI", raw, 20)
    tlvs, off = [], hdr_size + img_size
    for want in ((TLV_PROT_MAGIC,) if prot_size else ()) + (TLV_INFO_MAGIC,):
        info_magic, total = struct.unpack_from("<HH", raw, off)
        if info_magic != want:
            sys.exit(f"TLV info magic 0x{info_magic:04x} at 0x{off:x}, expected 0x{want:04x}")
        end, off = off + total, off + 4
        while off < end:
            kind, length = struct.unpack_from("<HH", raw, off)
            tlvs.append((kind, off + 4, raw[off + 4:off + 4 + length]))
            off += 4 + length
    return Image(raw, magic, hdr_size, prot_size, img_size, f"{major}.{minor}.{rev}+{build}", tlvs)


def load_pub(path):
    pem = open(path, "rb").read()
    if b"PRIVATE" in pem:
        key = serialization.load_pem_private_key(pem, None)
        return key.public_key(), key
    return serialization.load_pem_public_key(pem), None


def key_hash(pub):
    der = pub.public_bytes(serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo)
    return hashlib.sha256(der).digest()


def canonical_der(sig):
    # ST's imgtool binary pads r and s to 33 bytes (72-byte DER); strict DER parsing rejects the
    # padding when the top bit is clear, so rebuild the signature from the two integers.
    if sig[0] != 0x30:
        return sig
    off, ints = 2, []
    for _ in range(2):
        if sig[off] != 0x02:
            return sig
        length = sig[off + 1]
        ints.append(int.from_bytes(sig[off + 2:off + 2 + length], "big"))
        off += 2 + length
    return utils.encode_dss_signature(*ints)


def verifies(pub, sig, data):
    try:
        pub.verify(canonical_der(sig), data, ec.ECDSA(hashes.SHA256()))
        return True
    except Exception:
        return False


def check(path, key_paths):
    img = parse(open(path, "rb").read())
    signed = img.raw[:img.signed_end]
    digest = hashlib.sha256(signed).digest()
    print(f"image:    {path}")
    print(f"magic:    0x{img.magic:08x}  version {img.version}  hdr 0x{img.hdr_size:x}  "
          f"img_size 0x{img.img_size:x}  prot_tlv 0x{img.prot_size:x}")
    print(f"tlvs:     {' '.join(f'0x{t[0]:02x}/{len(t[2])}' for t in img.tlvs)}")
    print(f"hash tlv: {'MATCH' if img.tlv(TLV_SHA256)[2] == digest else 'MISMATCH'}  "
          f"computed {digest.hex()[:16]}")
    keyhash, sig = img.tlv(TLV_KEYHASH)[2], img.tlv(TLV_ECDSA256)[2]
    for kp in key_paths:
        pub, _ = load_pub(kp)
        print(f"key {kp.rsplit('/', 1)[-1]}: keyhash {'MATCH' if key_hash(pub) == keyhash else 'no'}  "
              f"signature {'VALID' if verifies(pub, sig, signed) else 'INVALID'}")


def forge(src, dst, offset, signer_path):
    img = parse(open(src, "rb").read())
    if not img.hdr_size <= offset < img.hdr_size + img.img_size:
        sys.exit(f"offset 0x{offset:x} is outside the body 0x{img.hdr_size:x}-0x{img.hdr_size + img.img_size:x}")
    raw = bytearray(img.raw)
    raw[offset] ^= 0x01
    _, hash_off, _ = img.tlv(TLV_SHA256)
    raw[hash_off:hash_off + 32] = hashlib.sha256(bytes(raw[:img.signed_end])).digest()
    _, signer = load_pub(signer_path)
    _, sig_off, old_sig = img.tlv(TLV_ECDSA256)
    # DER length varies with r and s; resign until it matches so every offset and size stays put
    for _ in range(64):
        sig = signer.sign(bytes(raw[:img.signed_end]), ec.ECDSA(hashes.SHA256()))
        if len(sig) == len(old_sig):
            break
    else:
        sys.exit("no signature of the original length after 64 tries")
    raw[sig_off:sig_off + len(sig)] = sig
    open(dst, "wb").write(raw)
    changed = [i for i in range(len(raw)) if raw[i] != img.raw[i]]
    print(f"forged {dst}: flip at 0x{offset:x}, {len(changed)} bytes differ, "
          f"in 0x{changed[0]:x}-0x{changed[-1]:x}")
    print(f"sha256 before: {hashlib.sha256(img.raw).hexdigest()}")
    print(f"sha256 after:  {hashlib.sha256(raw).hexdigest()}")


def main():
    if len(sys.argv) >= 4 and sys.argv[1] == "check":
        check(sys.argv[2], sys.argv[3:])
    elif len(sys.argv) == 6 and sys.argv[1] == "forge":
        forge(sys.argv[2], sys.argv[3], int(sys.argv[4], 0), sys.argv[5])
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
