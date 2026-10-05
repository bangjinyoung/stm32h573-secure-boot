#!/bin/bash
# Runs one M7 step on the board and appends the CLI output to
# <log dir>/<date>-m7-<step>.log with the ST-LINK serial number masked.
# Board state on entry: Open, TZEN 0xB4, BOOT_UBE 0xC3, SECBOOT_LOCK 0xB4 (left locked, D-09).
#
# Hotplug programs SECWM-secure flash only while the halted core is Secure (M6). The w* steps
# halt whatever runs now (the full-secure app, or OEMuRoT stopped in Error_Handler) and refuse
# when DSCSR.CDS is 0. They never reset: a reset with SW1=0 would run OEMuRoT from SECBOOTADD
# without STiRoT, at the wrong HDPL, with the HDPL2 OBKs provisioned. SW1 stays 1.
#
# usage: m7_tamper_step.sh [-n] <log dir> <step>
#   -n       print the commands only
#   observe  read-only: VTOR, DSCSR, CFSR, DEMCR, 4 DWT_PCSR samples
#   ob       read-only: option bytes
#   boot     clear DEMCR vector catch, NRST via -hardRst, wait, observe
#   w1..w10  write one image (table below), read it back into .workspace/tmp/m7 and compare
set -euo pipefail

dry=0
if [ "${1:-}" = -n ]; then dry=1; shift; fi
logdir=${1:?usage: $0 [-n] <log dir> <step>}
step=${2:?usage: $0 [-n] <log dir> <step>}

repo=$(cd "$(dirname "$0")/.." && pwd)
b=$repo/build/stirot_oemurot
t=$repo/.workspace/tmp/m7
clt=${STM32CUBECLT:-/opt/st/stm32cubeclt_1.22.0}
cli=$clt/STM32CubeProgrammer/bin/STM32_Programmer_CLI
hp=(-c port=SWD speed=fast ap=1 mode=Hotplug)
log=$logdir/$(date +%F)-m7-$step.log

# step: image, write address (0x08 alias as in M6), readback address, test id
declare -A img=(
  [w1]="$t/b2-app-payload-flip.bin 0x08018000 0x0C018000 CT-APP-HASH"
  [w2]="$b/rot_tz_s_app_init_sign.bin 0x08018000 0x0C018000 restore-app"
  [w3]="$t/b3-app-foreign-sig.bin 0x08018000 0x0C018000 CT-APP-SIG"
  [w4]="$b/rot_tz_s_app_init_sign.bin 0x08018000 0x0C018000 restore-app"
  [w5]="$t/rot_tz_s_app_enc_sign.bin 0x08020000 0x08020000 CT-APP-DL-CTRL"
  [w6]="$t/b1-app-dl-flip.bin 0x08020000 0x08020000 CT-APP-DL"
  [w7]="$t/a2-oemurot-exec-flip.bin 0x08000000 0x0C000000 CT-BL-EXEC"
  [w8]="$b/rot_init_sign.bin 0x08000000 0x0C000000 restore-bl"
  [w9]="$t/rot_enc_sign.bin 0x081E8000 0x081E8000 CT-BL-DL-CTRL"
  [w10]="$t/a1-oemurot-dl-flip.bin 0x081E8000 0x081E8000 CT-BL-DL"
)

run() {
  echo "\$ STM32_Programmer_CLI $*" | tee -a "$log"
  [ $dry -eq 1 ] && return 0
  local rc=0
  "$cli" "$@" > "$log.part" 2>&1 || rc=$?
  sed -e 's/\x1b\[[0-9;]*m//g' -e 's/^\(ST-LINK SN *: \).*/\1<masked>/' -e 's/\r$//' "$log.part" >> "$log"
  rm -f "$log.part"
  echo "rc=$rc" | tee -a "$log"
  grep -E 'Error|error|File download complete|verified|^0x' "$log" | tail -n 8 || true
  return $rc
}

observe() {
  run "${hp[@]}" -r32 0xE000ED08 4 -r32 0xE000EE08 4 -r32 0xE000ED28 4 -r32 0xE000EDFC 4 \
    -r32 0xE000101C 4 -r32 0xE000101C 4 -r32 0xE000101C 4 -r32 0xE000101C 4
}

[ $dry -eq 1 ] && log=/dev/null || { mkdir -p "$logdir"; echo "# $(date '+%F %T.%3N %z') step $step" >> "$log"; }

case $step in
observe) observe ;;
ob) run "${hp[@]}" -ob displ ;;
boot)
  # OB programming can leave VC_CORERESET (bit 0) set (M6). Clear it, keep bit 20, and keep
  # TRCENA (bit 24): DWT_PCSR samples need it (baseline DEMCR 0x01110000).
  run "${hp[@]}" -w32 0xE000EDFC 0x01100000
  run "${hp[@]}" -hardRst
  [ $dry -eq 1 ] || sleep 3
  observe ;;
w[0-9]*)
  [ -n "${img[$step]:-}" ] || { echo "unknown step: $step" >&2; exit 2; }
  read -r file addr rb id <<< "${img[$step]}"
  size=$(stat -c %s "$file")
  echo "# $id $(basename "$file") sha256 $(sha256sum "$file" | cut -c1-16) -> $addr" | tee -a "$log"
  run "${hp[@]}" -r32 0xE000EE08 4
  # only the SECWM-secure targets (read back through 0x0C) need a Secure halted core
  if [ $dry -eq 0 ] && [ "${rb:0:4}" = 0x0C ] &&
     ! grep '^0xE000EE08 :' "$log" | tail -n 1 | grep -qE ': [0-9A-F]{3}[13579BDF]'; then
    echo "DSCSR.CDS is 0 (core Non-secure): stop, no reset" | tee -a "$log"; exit 1
  fi
  run "${hp[@]}" -d "$file" "$addr" -v || true
  # FLASH NSSR, SECSR: PGSERR and friends if the write was refused (D-09 evidence)
  run "${hp[@]}" -r32 0x40022020 4 -r32 0x50022024 4
  run "${hp[@]}" -r "$rb" "$(printf 0x%x "$size")" "$t/m7-$step-readback.bin"
  if [ $dry -eq 0 ]; then
    if cmp "$file" "$t/m7-$step-readback.bin"; then
      echo "cmp $id: identical" | tee -a "$log"
    else
      echo "cmp $id: DIFFERENT" | tee -a "$log"; exit 1
    fi
  fi ;;
*)
  echo "unknown step: $step" >&2; exit 2 ;;
esac
