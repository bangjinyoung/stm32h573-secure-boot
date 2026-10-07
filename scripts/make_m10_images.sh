#!/bin/bash
# Builds the images the M10 board steps write into .workspace/tmp/m10 (not in git), and prints the
# record the evidence file keeps.
#
#   bl2c_init.bin, bl2c_enc.bin  BL2-c (two images, BL2_ROLLBACK=hw), STiRoT execution and download images
#   s_v3.bin                     tz_s 3.0.0 / SEC_CNT 2, image 0 primary (no trailer: a direct primary write)
#   ns_v1.bin                    tz_ns 1.0.0 / SEC_CNT 0, image 1 primary (no trailer)
#   s_hash.bin, ns_hash.bin      the same images with one body bit flipped (E10.2, BL2 hash step)
#
# The counters equal what the HDPL2 record already holds (counter0=2, counter1=0), so the first BL2-c
# boot writes no OBK (D-26). BL2-c keeps STiRoT image version 2.0.0 (D-12).
#
# usage: make_m10_images.sh <build dir>
set -euo pipefail

build=$(realpath "${1:?usage: $0 <build dir>}")

repo=$(cd "$(dirname "$0")/.." && pwd)
out=$repo/.workspace/tmp/m10
key=$repo/.workspace/keys/mcuboot-ecdsa-p256.pem
trusted=$repo/keys/mcuboot-ecdsa-p256.pub.pem
stirot_pub=$repo/build/stirot_oemurot/cube/Projects/STM32H573I-DK/ROT_Provisioning/STiROT_OEMuROT/Keys/STiRoT_Authentication_pub.pem
export PYTHONDONTWRITEBYTECODE=1

grep -q '^BL2_ROLLBACK:STRING=hw$' "$build/CMakeCache.txt" || { echo "$build is not a BL2_ROLLBACK=hw build" >&2; exit 1; }

mkdir -p "$out"
cp "$build/boot/mcuboot/bl2.bin" "$out/bl2c.bin"
"$repo/scripts/package_bl2_stirot.sh" "$out/bl2c.bin" "$out" 2.0.0 | sed "s|$out/||"

sign() {
  # BL2 is overwrite-only; without the flag imgtool sizes the trailer for swap and refuses a 29K image in 0x8000.
  "$repo/scripts/imgtool" sign --key "$key" --header-size 0x400 --pad-header --align 16 --overwrite-only \
    --slot-size "$1" --version "$2" --security-counter "$3" "$4" "$out/$5"
}
sign 0x8000 3.0.0 2 "$build/app/tz_s/tz_s.bin" s_v3.bin
sign 0x10000 1.0.0 0 "$build/app/tz_ns/tz_ns.bin" ns_v1.bin

# 0x1400 is inside both bodies (tz_s 0x7020 B, tz_ns 0x1674 B after the 0x400 header).
python3 "$repo/scripts/tamper_image.py" "$out/s_v3.bin" "$out/s_hash.bin" 0x1400
python3 "$repo/scripts/tamper_image.py" "$out/ns_v1.bin" "$out/ns_hash.bin" 0x1400

for f in s_v3 ns_v1 s_hash ns_hash; do
  python3 "$repo/scripts/mcuboot_image.py" check "$out/$f.bin" "$trusted"
done
python3 "$repo/scripts/mcuboot_image.py" check "$out/bl2c_init.bin" "$stirot_pub"

# The words BL2 (image 0) and the Secure app (image 1) check before they jump.
python3 - "$out/s_v3.bin" "$out/ns_v1.bin" <<'PY'
import struct, sys
for path in sys.argv[1:]:
    sp, reset = struct.unpack_from("<II", open(path, "rb").read(), 0x400)
    print(f"{path.rsplit('/', 1)[1]}: vector table sp=0x{sp:08x} reset=0x{reset:08x}")
PY
(cd "$out" && sha256sum bl2c_init.bin bl2c_enc.bin s_v3.bin ns_v1.bin s_hash.bin ns_hash.bin)
