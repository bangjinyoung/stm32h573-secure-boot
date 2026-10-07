#!/bin/bash
# Runs one M7, M8, M9 or M10 step on the board and appends the CLI output to
# <log dir>/<date>-<milestone>-<step>.log with the ST-LINK serial number masked.
# Board state on entry: Open, TZEN 0xB4, BOOT_UBE 0xC3, SECBOOT_LOCK 0xB4 (left locked, D-09).
# M7 runs the ST chain (OEMuRoT); M8 the custom one (M2 BL2 and bare_slot, images from
# make_m8_images.sh); M9 the update and rollback steps (make_m9_images.sh); M10 the Secure and
# Non-secure images (make_m10_images.sh). The HDPL2 bootloader is called OEMuRoT below; in M8-M10
# it is BL2.
#
# Hotplug programs SECWM-secure flash only while the halted core is Secure (M6). The w* steps
# halt whatever runs now (the full-secure app, or OEMuRoT stopped in Error_Handler) and refuse
# when DSCSR.CDS is 0. They never reset: a reset with SW1=0 would run OEMuRoT from SECBOOTADD
# without STiRoT, at the wrong HDPL, with the HDPL2 OBKs provisioned. SW1 stays 1.
#
# W1 showed that a halted running chain is not enough: with the app running, the flash loader
# erased sectors 12-15, programmed 0x8b0 bytes and faulted (CFSR 0x00018201). -u connects under
# reset with SW1=0, so the core halts at the SECBOOTADD reset vector before OEMuRoT configures
# anything, which is the state the M6 flash step programmed in. Set SW1 back to 1 before boot.
#
# usage: tamper_step.sh [-n] [-u] <m7|m8|m9|m10> <log dir> <step>
#   -n       print the commands only
#   -u       w* steps connect under reset (mode=UR, SW1=0) instead of Hotplug
#   observe  read-only: VTOR, DSCSR, CFSR, DEMCR, 4 DWT_PCSR samples, VTOR_NS, SFSR, SFAR, HFSR
#   ob       read-only: option bytes
#   boot     clear DEMCR vector catch, NRST via -hardRst, wait, observe
#   a*, b*, c*, t*, w*, r*  write one image (tables below), read it back into .workspace/tmp/<milestone>
#               and compare
set -euo pipefail

dry=0
ur=0
if [ "${1:-}" = -n ]; then dry=1; shift; fi
if [ "${1:-}" = -u ]; then ur=1; shift; fi
ms=${1:?usage: $0 [-n] [-u] <m7|m8|m9|m10> <log dir> <step>}
logdir=${2:?usage: $0 [-n] [-u] <m7|m8|m9|m10> <log dir> <step>}
step=${3:?usage: $0 [-n] [-u] <m7|m8|m9|m10> <log dir> <step>}

repo=$(cd "$(dirname "$0")/.." && pwd)
b=$repo/build/stirot_oemurot
m7=$repo/.workspace/tmp/m7
m8=$repo/.workspace/tmp/m8
t=$repo/.workspace/tmp/$ms
clt=${STM32CUBECLT:-/opt/st/stm32cubeclt_1.22.0}
cli=$clt/STM32CubeProgrammer/bin/STM32_Programmer_CLI
hp=(-c port=SWD speed=fast ap=1 mode=Hotplug)
log=$logdir/$(date +%F)-$ms-$step.log

# step: image, write address (0x08 alias as in M6), readback address, test id
declare -A img
case $ms in
m7) img=(
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
) ;;
# b: install the custom chain, w: the M7 CT ids that M8.5 replays, r: back to the M7 end state
m8) img=(
  [b1]="$t/bl2_enc.bin 0x081E8000 0x081E8000 install-bl2"
  [b2]="$t/app_signed.bin 0x08018000 0x0C018000 install-app"
  [w1]="$t/ct-app-hash.bin 0x08018000 0x0C018000 CT-APP-HASH"
  [w2]="$t/app_signed.bin 0x08018000 0x0C018000 restore-app"
  [w3]="$t/ct-app-sig.bin 0x08018000 0x0C018000 CT-APP-SIG"
  [w4]="$t/app_signed.bin 0x08018000 0x0C018000 restore-app"
  [w7]="$t/ct-bl-exec.bin 0x08000000 0x0C000000 CT-BL-EXEC"
  [w8]="$t/bl2_init.bin 0x08000000 0x0C000000 restore-bl"
  [w9]="$t/bl2_enc.bin 0x081E8000 0x081E8000 CT-BL-DL-CTRL"
  [r1]="$m7/rot_enc_sign.bin 0x081E8000 0x081E8000 recover-oemurot"
  [r2]="$b/rot_tz_s_app_init_sign.bin 0x08018000 0x0C018000 recover-st-app"
) ;;
# a: BL2-a (version check), b: BL2-b (HDPL2 OBK counter), r: back to the M8 BL2
m9) img=(
  [a1]="$t/bl2a_enc.bin 0x081E8000 0x081E8000 install-bl2a"
  [a2]="$t/app_v1.bin 0x08018000 0x0C018000 primary-v1"
  [a3]="$t/app_v2_upd.bin 0x08020000 0x08020000 E9.1-update-v2"
  [a4]="$t/app_v1_upd.bin 0x08020000 0x08020000 E9.2-secondary-v1"
  [a5]="$t/app_v1.bin 0x08018000 0x0C018000 E9.2L-primary-v1"
  [b1]="$t/app_v2_upd.bin 0x08020000 0x08020000 secondary-v2"
  [b2]="$t/bl2b_enc.bin 0x081E8000 0x081E8000 install-bl2b"
  [b4]="$t/app_v1_upd.bin 0x08020000 0x08020000 E9.3-secondary-v1"
  [b5]="$t/app_v1.bin 0x08018000 0x0C018000 E9.3-primary-v1"
  [b6]="$t/app_v2.bin 0x08018000 0x0C018000 restore-v2"
  [r1]="$m8/bl2_enc.bin 0x081E8000 0x081E8000 recover-m8-bl2"
) ;;
# c: install BL2-c and both images, t: E10.2 tamper and restore, r: back to the M9 end state (R0)
m10) img=(
  [c1]="$t/ns_v1.bin 0x08020000 0x08020000 install-ns"
  [c2]="$t/s_v3.bin 0x08018000 0x0C018000 install-s"
  [c3]="$t/bl2c_enc.bin 0x081E8000 0x081E8000 install-bl2c"
  [t1]="$t/s_hash.bin 0x08018000 0x0C018000 E10.2-S-HASH"
  [t2]="$t/s_v3.bin 0x08018000 0x0C018000 restore-s"
  [t3]="$t/ns_hash.bin 0x08020000 0x08020000 E10.2-NS-HASH"
  [t4]="$t/ns_v1.bin 0x08020000 0x08020000 restore-ns"
  [r1]="$repo/.workspace/tmp/m9/bl2b_enc.bin 0x081E8000 0x081E8000 recover-m9-bl2b"
  [r2]="$repo/.workspace/tmp/m9/app_v2.bin 0x08018000 0x0C018000 recover-m9-app"
) ;;
*) echo "unknown milestone: $ms" >&2; exit 2 ;;
esac

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
    -r32 0xE000101C 4 -r32 0xE000101C 4 -r32 0xE000101C 4 -r32 0xE000101C 4 \
    -r32 0xE002ED08 4 -r32 0xE000EDE4 4 -r32 0xE000EDE8 4 -r32 0xE000ED2C 4
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
[abctwr][0-9]*)
  [ -n "${img[$step]:-}" ] || { echo "unknown step: $step" >&2; exit 2; }
  read -r file addr rb id <<< "${img[$step]}"
  [ $ur -eq 1 ] && hp=(-c port=SWD speed=fast ap=1 mode=UR)
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
  run "${hp[@]}" -r "$rb" "$(printf 0x%x "$size")" "$t/$ms-$step-readback.bin"
  if [ $dry -eq 0 ]; then
    if cmp "$file" "$t/$ms-$step-readback.bin"; then
      echo "cmp $id: identical" | tee -a "$log"
    else
      echo "cmp $id: DIFFERENT" | tee -a "$log"; exit 1
    fi
  fi ;;
*)
  echo "unknown step: $step" >&2; exit 2 ;;
esac
