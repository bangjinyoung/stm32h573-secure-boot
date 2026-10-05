#!/bin/bash
# Builds the images the M9 board steps write into .workspace/tmp/m9 (not in git), and prints the
# record the evidence file keeps.
#
#   bl2a_init.bin, bl2a_enc.bin   BL2-a (BL2_ROLLBACK=sw), STiRoT execution and download images
#   bl2b_init.bin, bl2b_enc.bin   BL2-b (BL2_ROLLBACK=hw), same
#   app_v1.bin, app_v2.bin        bare_slot 1.0.0 / SEC_CNT 1 and 2.0.0 / SEC_CNT 2, for a direct
#                                 primary write: no trailer, so BL2 sees no pending upgrade
#   app_v1_upd.bin, app_v2_upd.bin  the same images padded to the slot with the trailer magic,
#                                 for the secondary slot: overwrite-only only looks at a
#                                 secondary image that carries it
#
# Both apps are signed from one bare_slot.bin, so the version and the counter are the only
# difference. Both BL2s keep STiRoT image version 2.0.0 (D-12).
#
# usage: make_m9_images.sh <BL2-a build dir> <BL2-b build dir>
set -euo pipefail

build_a=$(realpath "${1:?usage: $0 <BL2-a build dir> <BL2-b build dir>}")
build_b=$(realpath "${2:?usage: $0 <BL2-a build dir> <BL2-b build dir>}")

repo=$(cd "$(dirname "$0")/.." && pwd)
out=$repo/.workspace/tmp/m9
key=$repo/.workspace/keys/mcuboot-ecdsa-p256.pem
trusted=$repo/keys/mcuboot-ecdsa-p256.pub.pem
stirot_pub=$repo/build/stirot_oemurot/cube/Projects/STM32H573I-DK/ROT_Provisioning/STiROT_OEMuROT/Keys/STiRoT_Authentication_pub.pem
export PYTHONDONTWRITEBYTECODE=1

grep -q '^BL2_ROLLBACK:STRING=sw$' "$build_a/CMakeCache.txt" || { echo "$build_a is not a BL2_ROLLBACK=sw build" >&2; exit 1; }
grep -q '^BL2_ROLLBACK:STRING=hw$' "$build_b/CMakeCache.txt" || { echo "$build_b is not a BL2_ROLLBACK=hw build" >&2; exit 1; }
# The app does not depend on BL2_ROLLBACK; two different binaries mean the trees are out of sync.
cmp "$build_a/app/bare/bare_slot.bin" "$build_b/app/bare/bare_slot.bin"

mkdir -p "$out"
cp "$build_a/boot/mcuboot/bl2.bin" "$out/bl2a.bin"
cp "$build_b/boot/mcuboot/bl2.bin" "$out/bl2b.bin"
for v in a b; do
  "$repo/scripts/package_bl2_stirot.sh" "$out/bl2$v.bin" "$out" 2.0.0 | sed "s|$out/||"
done

sign() {
  "$repo/scripts/imgtool" sign --key "$key" --header-size 0x400 --pad-header --align 16 --slot-size 0x8000 \
    --version "$1" --security-counter "$2" "${@:4}" "$build_b/app/bare/bare_slot.bin" "$out/$3"
}
sign 1.0.0 1 app_v1.bin
sign 2.0.0 2 app_v2.bin
sign 1.0.0 1 app_v1_upd.bin --pad
sign 2.0.0 2 app_v2_upd.bin --pad

for f in app_v1 app_v2 app_v1_upd app_v2_upd; do
  python3 "$repo/scripts/mcuboot_image.py" check "$out/$f.bin" "$trusted"
done
for f in bl2a_init bl2b_init; do
  python3 "$repo/scripts/mcuboot_image.py" check "$out/$f.bin" "$stirot_pub"
done
(cd "$out" && sha256sum bl2a_init.bin bl2a_enc.bin bl2b_init.bin bl2b_enc.bin app_v1.bin app_v2.bin app_v1_upd.bin app_v2_upd.bin)
