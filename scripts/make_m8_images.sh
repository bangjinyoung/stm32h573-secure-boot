#!/bin/bash
# Builds the images the M8 board steps write, from one build of BL2 and bare_slot, into
# .workspace/tmp/m8 (not in git), and prints the record the evidence file keeps.
#
#   bl2_init.bin, bl2_enc.bin   STiRoT execution and download images (package_bl2_stirot.sh)
#   app_signed.bin              bare_slot_signed.bin, frozen for the whole run
#   ct-app-hash.bin             app body bit 0 flipped at 0x1400 (CT-APP-HASH)
#   ct-app-sig.bin              same flip, SHA256 TLV recomputed, M2 keyhash kept, signed by a key
#                               BL2 does not trust (CT-APP-SIG)
#   ct-bl-exec.bin              BL2 execution image bit 0 flipped at 0x1400 (CT-BL-EXEC)
#
# The flip offset 0x1400 is the one M3, M5 and M7 used: inside the body, after the 0x400 header.
#
# usage: make_m8_images.sh <cmake build dir> <untrusted signer pem> [bl2 version]
set -euo pipefail

build=$(realpath "${1:?usage: $0 <cmake build dir> <untrusted signer pem> [bl2 version]}")
foreign=$(realpath "${2:?usage: $0 <cmake build dir> <untrusted signer pem> [bl2 version]}")
version=${3:-2.0.0}

repo=$(cd "$(dirname "$0")/.." && pwd)
out=$repo/.workspace/tmp/m8
trusted=$repo/keys/mcuboot-ecdsa-p256.pub.pem
stirot_pub=$repo/build/stirot_oemurot/cube/Projects/STM32H573I-DK/ROT_Provisioning/STiROT_OEMuROT/Keys/STiRoT_Authentication_pub.pem
export PYTHONDONTWRITEBYTECODE=1

mkdir -p "$out"
"$repo/scripts/package_bl2_stirot.sh" "$build/boot/mcuboot/bl2.bin" "$out" "$version" | sed "s|$out/||"
cp "$build/app/bare/bare_slot_signed.bin" "$out/app_signed.bin"

python3 "$repo/scripts/tamper_image.py" "$out/app_signed.bin" "$out/ct-app-hash.bin" 0x1400
python3 "$repo/scripts/mcuboot_image.py" forge "$out/app_signed.bin" "$out/ct-app-sig.bin" 0x1400 "$foreign"
python3 "$repo/scripts/tamper_image.py" "$out/bl2_init.bin" "$out/ct-bl-exec.bin" 0x1400

for f in app_signed ct-app-hash ct-app-sig; do
  python3 "$repo/scripts/mcuboot_image.py" check "$out/$f.bin" "$trusted" "$foreign"
done
for f in bl2_init ct-bl-exec; do
  python3 "$repo/scripts/mcuboot_image.py" check "$out/$f.bin" "$stirot_pub"
done
(cd "$out" && sha256sum bl2_init.bin bl2_enc.bin app_signed.bin ct-app-hash.bin ct-app-sig.bin ct-bl-exec.bin)
