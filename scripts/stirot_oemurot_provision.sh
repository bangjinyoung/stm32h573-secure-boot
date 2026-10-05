#!/bin/bash
# Runs one step of the STiROT_OEMuROT provisioning on the board, in the order of ST's
# ob_flash_programming.sh and obkey_provisioning.sh (OPEN), and appends the CLI output to
# <log dir>/<date>-m6-s2-<step>.log with the ST-LINK serial number masked.
# DA_Config.obk is left out (decision D-05). SECBOOT_LOCK=0xB4 is the lock step (decision D-06):
# OEMuRoT refuses to boot without it, and it can be unlocked again in Open.
#
# SW1 (BOOT0) as in ST provisioning.sh: 1 for ube-b4, 0 for flash and readback (Hotplug programs
# SECWM-secure flash only while the halted core is Secure), 1 again for obk and the boot.
#
# usage: stirot_oemurot_provision.sh [-n] <build dir> <log dir> <step>
#   -n     print the commands only
#   steps  ube-b4 erase secbootadd secwm flash readback obk ube-c3 lock
set -euo pipefail

dry=0
if [ "${1:-}" = -n ]; then dry=1; shift; fi
build=${1:?usage: $0 [-n] <build dir> <log dir> <step>}
logdir=${2:?usage: $0 [-n] <build dir> <log dir> <step>}
step=${3:?usage: $0 [-n] <build dir> <log dir> <step>}

clt=${STM32CUBECLT:-/opt/st/stm32cubeclt_1.22.0}
cli=$clt/STM32CubeProgrammer/bin/STM32_Programmer_CLI
export PATH=$clt/GNU-tools-for-STM32/bin:$PATH
hp=(-c port=SWD speed=fast ap=1 mode=Hotplug)
log=$logdir/$(date +%F)-m6-s2-$step.log

run() {
  echo "\$ STM32_Programmer_CLI $*" | tee -a "$log"
  [ $dry -eq 1 ] && return 0
  local rc=0
  "$cli" "$@" > "$log.part" 2>&1 || rc=$?
  sed -e 's/\x1b\[[0-9;]*m//g' -e 's/^\(ST-LINK SN *: \).*/\1<masked>/' -e 's/\r$//' "$log.part" >> "$log"
  rm -f "$log.part"
  echo "rc=$rc" | tee -a "$log"
  grep -E 'successfully|Error|error|OBKey|Mass erase|File download complete|verified' "$log" | tail -n 8 || true
  return $rc
}

[ $dry -eq 1 ] && log=/dev/null || echo "# $(date '+%F %T.%3N %z') step $step" >> "$log"

case $step in
ube-b4)
  # STiRoT is the boot entry while BOOT_UBE is 0xC3, and UR connects fail then (M5).
  run "${hp[@]}" -ob BOOT_UBE=0xB4
  run "${hp[@]}" -hardRst
  run "${hp[@]}" -r32 0xE000ED08 4 ;;
erase)
  # UR connects also fail in the System Bootloader (M6 S2 09:42), so Hotplug as in ube-b4.
  run "${hp[@]}" -ob SECBOOT_LOCK=0xC3
  run "${hp[@]}" -ob SECWM1_STRT=1 SECWM1_END=0 WRPSGn1=0xffffffff WRPSGn2=0xffffffff \
    SECWM2_STRT=1 SECWM2_END=0 HDP1_STRT=1 HDP1_END=0 HDP2_STRT=1 HDP2_END=0 SWAP_BANK=0 \
    SRAM2_RST=0 SRAM2_ECC=0 BOOT_UBE=0xB4 -e all ;;
secbootadd)
  run "${hp[@]}" -ob SECBOOTADD=0xC0004 ;;
secwm)
  run "${hp[@]}" -ob SECWM1_STRT=0x0 SECWM1_END=0xF
  run "${hp[@]}" -ob SECWM2_STRT=0x7F SECWM2_END=0x0 ;;
flash)
  run "${hp[@]}" -hardRst
  run "${hp[@]}" -r32 0xE000EE08 4
  if [ $dry -eq 0 ] && ! grep '^0xE000EE08 :' "$log" | tail -n 1 | grep -qE ': [0-9A-F]{3}[13579BDF]'; then
    echo "DSCSR.CDS is 0 (core Non-secure): set SW1 to 0" | tee -a "$log"; exit 1
  fi
  run "${hp[@]}" -d "$build/rot_tz_s_app_init_sign.hex" -v
  run "${hp[@]}" -d "$build/rot_init_sign.hex" -v ;;
readback)
  run "${hp[@]}" -r 0x0C000000 0x18000 "$build/readback-oemurot.bin"
  run "${hp[@]}" -r 0x0C018000 0x8000 "$build/readback-app.bin"
  if [ $dry -eq 0 ]; then
    for p in rot_init_sign:oemurot rot_tz_s_app_init_sign:app; do
      arm-none-eabi-objcopy -I ihex -O binary --gap-fill 0xFF "$build/${p%%:*}.hex" "$build/${p%%:*}.bin"
      cmp "$build/${p%%:*}.bin" "$build/readback-${p##*:}.bin" && echo "cmp ${p%%:*}: identical" | tee -a "$log"
    done
  fi ;;
obk)
  for f in STiRoT_Config STiRoT_Data OEMuRoT_Config OEMuRoT_Data; do
    run "${hp[@]}" -hardRst
    run "${hp[@]}" -sdp "$build/$f.obk"
  done ;;
ube-c3)
  # A Hotplug OB launch does not reset the MCU; the next -hardRst starts STiRoT.
  run "${hp[@]}" -hardRst
  run "${hp[@]}" -ob BOOT_UBE=0xC3
  run "${hp[@]}" -r32 0xE000EDFC 4 ;;
lock)
  run "${hp[@]}" -ob SECBOOT_LOCK=0xB4
  run "${hp[@]}" -ob displ ;;
*)
  echo "unknown step: $step" >&2; exit 2 ;;
esac
