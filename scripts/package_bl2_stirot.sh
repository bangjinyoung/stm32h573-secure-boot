#!/bin/bash
# Packages a BL2 binary as STiRoT's HDPL2 code image with ST's own tooling: the STiRoT image XMLs
# and the STiRoT_Config they link (keys, magic, areas) from the build_stirot_oemurot.sh tree that
# provisioned the board. Only the input, the output and the version change.
#
# Writes <name>_init.{hex,bin} (execution image: clear body, confirmed, for the HDPL2 code area)
# and <name>_enc.{hex,bin} (download image: encrypted, for the STiRoT download slot), and prints
# the sha256 of each .bin.
#
# usage: package_bl2_stirot.sh <bl2.bin> <out_dir> [version] [stirot_oemurot_build_dir]
set -euo pipefail

in=$(realpath "${1:?usage: $0 <bl2.bin> <out_dir> [version] [stirot_oemurot_build_dir]}")
out=${2:?usage: $0 <bl2.bin> <out_dir> [version] [stirot_oemurot_build_dir]}
version=${3:-2.0.0}
st_build=${4:-$(dirname "$0")/../build/stirot_oemurot}

clt=${STM32CUBECLT:-/opt/st/stm32cubeclt_1.22.0}
tpc=$clt/STM32CubeProgrammer/bin/STM32TrustedPackageCreator_CLI
objcopy=$clt/GNU-tools-for-STM32/bin/arm-none-eabi-objcopy
images=$(realpath "$st_build")/cube/Projects/STM32H573I-DK/ROT_Provisioning/STiROT_OEMuROT/Images

mkdir -p "$out"
out=$(realpath "$out")
name=$(basename "$in" .bin)

# Copy $1 next to the original (the XML links STiRoT_Config by a relative path), with the input,
# output and version replaced; prints the copy's path.
xml_copy() {
  local src=$images/$1 dst=$images/m8_$1 hex=$2
  python3 - "$src" "$dst" "$in" "$hex" "$version" <<'PY'
import re, sys
src, dst, binary, hex_out, version = sys.argv[1:6]
text = open(src).read()
for name, value in (("Firmware binary input file", binary), ("Image output file", hex_out), ("Version", version)):
    pattern = r"(<Name>" + re.escape(name) + r"</Name>\s*<Value>)[^<]*(</Value>)"
    text, n = re.subn(pattern, lambda m: m.group(1) + value + m.group(2), text)
    if n != 1:
        sys.exit(f"expected one '{name}' value in {src}, found {n}")
open(dst, "w").write(text)
PY
  echo "$dst"
}

for kind in init enc; do
  case $kind in
    init) xml=STiRoT_Code_Init_Image.xml ;;
    enc) xml=STiRoT_Code_Image.xml ;;
  esac
  hex=$out/${name}_$kind.hex
  rm -f "$hex"
  "$tpc" -pb "$(xml_copy $xml "$hex")" >"$out/${name}_$kind.tpc.log"
  [ -s "$hex" ] || { echo "TPC wrote no $hex, see $out/${name}_$kind.tpc.log" >&2; exit 1; }
  "$objcopy" -I ihex -O binary --gap-fill 0xFF "$hex" "$out/${name}_$kind.bin"
  sha256sum "$out/${name}_$kind.bin"
done
