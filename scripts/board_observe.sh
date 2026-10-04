#!/bin/sh
# Reads where the board ended up after a reset: VTOR, core registers resolved
# against bl2 or the slot app, and the image headers in both slots.
# Usage: board_observe.sh <build dir>
set -eu

build=${1:-build/debug}
bl2_elf=$build/boot/mcuboot/bl2.elf
app_elf=$build/app/bare/bare_slot.elf
cn="-c port=SWD mode=Hotplug"

cli() {
    STM32_Programmer_CLI $cn "$@" 2>&1 | sed 's/\x1b\[[0-9;]*m//g'
}

vtor=$(cli -r32 0xE000ED08 4 | awk '/^0xE000ED08 :/{print $3}')
regs=$(cli -halt -coreReg -run)
pc=$(echo "$regs" | awk '$1=="PC"{print $3}')
msp=$(echo "$regs" | awk '$1=="MSP"{print $3}')

case $((0x$vtor)) in
$((0x08000000))) owner=bl2 elf=$bl2_elf ;;
*) owner=app elf=$app_elf ;;
esac

echo "vtor:      0x$vtor ($owner)"
echo "pc:        $pc $(arm-none-eabi-addr2line -f -e "$elf" "$pc" | head -1)"
echo "msp:       $msp"
# Image header words: magic, load_addr, hdr_size|protect_tlv_size, img_size, flags, version (major.minor.revision), build_num.
header() {
    cli -r32 "$1" 0x20 | grep -E "^$1 :|^$2 :" | cut -d: -f2 | tr -s ' \n' ' '
}

echo "primary:   $(header 0x08018000 0x08018010)"
echo "secondary: $(header 0x080B8000 0x080B8010)"
