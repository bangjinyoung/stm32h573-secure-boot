#!/bin/bash
# Builds ST's STiROT_OEMuROT boot path without STM32CubeIDE: OEMiROT_Boot as OEMuRoT
# (OEMUROT_ENABLE) and OEMiROT_Appli as one full-secure image, then the obk files and the
# signed images the provisioning steps write.
#
# ST's postbuild scripts rewrite XMLs, linker scripts, headers and provisioning scripts inside
# the CubeH5 tree from the preprocessed boot layout. They run unchanged on a copy of the
# project tree; Drivers, Middlewares and Utilities are symlinked and only read.
#
# usage: build_stirot_oemurot.sh <STM32CubeH5> [out_dir] [oemurot_version]
set -euo pipefail

cube=$(cd "${1:?usage: $0 <STM32CubeH5> [out_dir] [oemurot_version]}" && pwd)
out=${2:-$(dirname "$0")/../build/stirot_oemurot}
oemurot_version=${3:-2.0.0}

clt=${STM32CUBECLT:-/opt/st/stm32cubeclt_1.22.0}
gcc_bin=$clt/GNU-tools-for-STM32/bin
prog_bin=$clt/STM32CubeProgrammer/bin

mkdir -p "$out"
out=$(cd "$out" && pwd)
work=$out/cube
board=$work/Projects/STM32H573I-DK
rot=$board/Applications/ROT
prov=$board/ROT_Provisioning
boot=$rot/OEMiROT_Boot
appli=$rot/OEMiROT_Appli

rm -rf "$work"
mkdir -p "$rot"
for d in Drivers Middlewares Utilities; do ln -s "$cube/$d" "$work/$d"; done
cp -r "$cube/Projects/STM32H573I-DK/ROT_Provisioning" "$board/"
cp -r "$cube/Projects/STM32H573I-DK/Applications/ROT/OEMiROT_Boot" \
      "$cube/Projects/STM32H573I-DK/Applications/ROT/OEMiROT_Appli" "$rot/"
find "$work/Projects" -name '*.sh' -exec sed -i 's/\r$//' {} +
chmod -R u+w "$work/Projects"

# AppliCfg needs click. WSL here has no python3-venv or pip, so the venv comes from uv.
if [ ! -x "$out/venv/bin/python3" ]; then
  rm -rf "$out/venv"
  uv venv -q "$out/venv"
  uv pip install -q --python "$out/venv/bin/python3" \
    -r "$cube/Utilities/PC_Software/ROT_AppliConfig/requirements.txt"
fi
export PATH=$out/venv/bin:$gcc_bin:$prog_bin:$PATH
applicfg=$cube/Utilities/PC_Software/ROT_AppliConfig/AppliCfg.py

# Rewrite exactly one occurrence of $2 in file $1 to $3, or fail.
replace_once() {
  python3 - "$@" <<'PY'
import sys
path, old, new = sys.argv[1:4]
text = open(path, newline='').read()
if text.count(old) != 1:
    sys.exit(f"expected one '{old}' in {path}, found {text.count(old)}")
open(path, 'w', newline='').write(text.replace(old, new))
PY
}

replace_once "$prov/env.sh" 'oemirot_appli_path_project=Applications/ROT/OEMiROT_Appli_TrustZone' \
  'oemirot_appli_path_project=Applications/ROT/OEMiROT_Appli'

layout=$boot/Inc/flash_layout.h
python3 "$applicfg" setdefine -a uncomment -n OEMUROT_ENABLE -v 1 "$layout"
replace_once "$layout" '#define MCUBOOT_APP_IMAGE_NUMBER 2' '#define MCUBOOT_APP_IMAGE_NUMBER 1'
replace_once "$layout" '#define FLASH_NS_PARTITION_SIZE         (0xA0000)' \
  '#define FLASH_NS_PARTITION_SIZE         (0x0)'

sto=$prov/STiROT_OEMuROT
replace_once "$sto/Config/STiRoT_Config.xml" '<Value>.\..\Binary\STiRoT_Config.obk</Value>' \
  '<Value>./../Binary/STiRoT_Config.obk</Value>'
for xml in "$sto/Images/STiRoT_Code_Image.xml" "$sto/Images/STiRoT_Code_Init_Image.xml"; do
  replace_once "$xml" '<Value>1.0.0</Value>' "<Value>$oemurot_version</Value>"
done

# Same obk steps as STiROT_OEMuROT/provisioning.sh step 1. STiRoT_Data.obk and
# OEMuRoT_Data.obk are shipped prebuilt and have no generator in the reference flow.
tpc=STM32TrustedPackageCreator_CLI
(cd "$sto" &&
  $tpc -obk Config/STiRoT_Config.xml &&
  $tpc -obk Config/OEMuRoT_Config_Keys.xml &&
  $tpc -pb ST/OEMuRoT_ST_Settings_1.xml &&
  $tpc -obk ST/OEMuRoT_ST_Settings_2.xml) > "$out/obk.log" 2>&1

cpu="-mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard"
common="$cpu -mcmse -std=gnu11 -Os -g3 -ffunction-sections -fdata-sections -Wall -fstack-usage"
hal=../../../../../../../Drivers/STM32H5xx_HAL_Driver
cmsis=../../../../../../../Drivers/CMSIS
mw=../../../../../../../Middlewares/Third_Party

# compile_link <project dir> <elf name> <linker script> <cflags> <sources...>
# Runs in <project>/STM32CubeIDE/Debug like the IDE, so the .cproject relative paths hold.
compile_link() {
  local dir=$1 name=$2 ld=$3 flags=$4 objs="" src obj
  shift 4
  for src in "$@"; do
    obj=obj/$(basename "${src%.*}").o
    if [ "${src##*.}" = s ]; then
      arm-none-eabi-gcc $cpu -g3 -x assembler-with-cpp -c "$src" -o "$obj"
    else
      # shellcheck disable=SC2086
      arm-none-eabi-gcc $flags -c "$src" -o "$obj"
    fi
    objs="$objs $obj"
  done
  # shellcheck disable=SC2086
  arm-none-eabi-gcc $cpu --specs=nano.specs -static -Wl,--gc-sections -T"$ld" \
    -Wl,-Map="$name.map" -Wl,--start-group -lc -lm -Wl,--end-group $objs -o "$name.elf"
  arm-none-eabi-objcopy -O binary "$name.elf" "../../Binary/$name.bin"
  arm-none-eabi-size "$name.elf"
}

# OEMiROT_Boot: sources, defines and includes from STM32CubeIDE/.project and .cproject (Debug).
mkdir -p "$boot/STM32CubeIDE/Debug/obj" "$boot/Binary"
cd "$boot/STM32CubeIDE/Debug"
pp="-E -P -xc -DSTM32H573xx -DBL2 -I../../../Linker -I../../Inc -I$cmsis/Include
  -I$cmsis/Device/ST/STM32H5xx/Include -I$hal/Inc"
# shellcheck disable=SC2086
arm-none-eabi-gcc $pp -o./output.ld ../stm32h5xx_bl2.ld
# shellcheck disable=SC2086
arm-none-eabi-gcc $pp -o../image_macros_preprocessed_bl2.c ../../Src/image_macros_to_preprocess_bl2.c

boot_flags="$common -DDEBUG -DSTM32H573xx -DUSE_HAL_DRIVER -DMBEDTLS_CONFIG_FILE=<mbedtls_config.h>
  -DBOOT_MAX_ALIGN=16 -DBL2 -DMCUBOOT_DOUBLE_SIGN_VERIF -DFLOW_CONTROL -DNDEBUG -DOEMIROT_DEV_MODE
  -DDEFAULT_UART_BAUDRATE=115200
  -I../../Inc -I$hal/Inc -I$cmsis/Include -I$cmsis/Device/ST/STM32H5xx/Include
  -I$mw/mbed-crypto/include -I$mw/mbed-crypto/include/mbedtls -I$mw/mbed-crypto/include/psa
  -I$mw/mcuboot/platform/include -I$mw/mcuboot -I$mw/mcuboot/platform/ext/common
  -I$mw/mcuboot/platform/ext/driver -I$mw/mcuboot/bl2/include -I$mw/mcuboot/bl2/ext/mcuboot
  -I$mw/mcuboot/bl2/ext/mcuboot/include -I$mw/mcuboot/boot/bootutil/include
  -I$mw/mcuboot/boot/bootutil/src -I$mw/mcuboot/ext/tinycrypt/lib/include -I$mw/mcuboot/ext/hal/Inc"
boot_srcs=(../Example/User/*.c
  ../../Src/{bl2_nv_services,boot_hal,keys_map,low_level_com,low_level_device,low_level_flash}.c
  ../../Src/{low_level_obkeys,low_level_obkeys_device,low_level_rng,low_level_security}.c
  ../../Src/{mpu_armv8m_drv,sha256_alt,startup_stm32h5xx,stm32h5xx_hal_msp,tick,system_stm32h5xx}.c
  $hal/Src/stm32h5xx_hal{,_cortex,_cryp,_cryp_ex,_dma,_dma_ex,_exti,_flash,_flash_ex,_gpio,_gtzc}.c
  $hal/Src/stm32h5xx_hal{_hash,_icache,_pka,_pwr,_pwr_ex,_rcc,_rcc_ex,_rng,_rng_ex,_rtc,_rtc_ex}.c
  $hal/Src/stm32h5xx_hal{_uart,_uart_ex}.c
  $mw/mbed-crypto/library/{aes,asn1parse,bignum,bignum_core,cipher,constant_time,hmac_drbg}.c
  $mw/mbed-crypto/library/{ecdh,ecdsa,ecp,ecp_curves,entropy,hkdf,md,memory_buffer_alloc}.c
  $mw/mbed-crypto/library/{platform,platform_util,sha256}.c
  $mw/mcuboot/bl2/ext/mcuboot/{bl2_main,flash_map_extended,flash_map_legacy}.c
  $mw/mcuboot/bl2/src/{flash_map,security_cnt}.c
  $mw/mcuboot/boot/bootutil/src/{bootutil_misc,caps,encrypted,fault_injection_hardening}.c
  $mw/mcuboot/boot/bootutil/src/{fault_injection_hardening_delay_rng_mbedtls,image_ec,image_ec256}.c
  $mw/mcuboot/boot/bootutil/src/{image_validate,loader,swap_misc,swap_move,swap_scratch,tlv}.c
  $mw/mcuboot/platform/ext/common/uart_stdout.c)
compile_link "$boot" OEMiROT_Boot ./output.ld "$boot_flags" "${boot_srcs[@]}" > "$out/boot-build.log" 2>&1
bash ../postbuild.sh Debug > "$out/boot-postbuild.log" 2>&1 || { cat ../postbuild.log >&2; exit 1; }

# OEMiROT_Appli: the boot postbuild has already rewritten its linker script, flash layout,
# main.h and postbuild.sh for this layout.
mkdir -p "$appli/STM32CubeIDE/Debug/obj" "$appli/Binary"
cd "$appli/STM32CubeIDE/Debug"
appli_flags="$common -DSTM32H573xx -DUSE_HAL_DRIVER -DBL2 -DUSE_STM32H573I_DK
  -I../../Inc -I../../Secure_nsclib -I$cmsis/Include -I$cmsis/Device/ST/STM32H5xx/Include -I$hal/Inc"
appli_srcs=(../Example/User/*.c ../Example/Startup/startup_stm32h573zitxq.s
  ../../Src/{low_level_device,low_level_flash,main,com,loader,low_level_security,stm32h5xx_it}.c
  ../../Src/system_stm32h5xx.c
  $hal/Src/stm32h5xx_hal{,_cortex,_dma,_dma_ex,_exti,_flash,_flash_ex,_gpio,_gtzc,_pwr,_pwr_ex}.c
  $hal/Src/stm32h5xx_hal{_rcc,_rcc_ex,_uart,_uart_ex}.c)
compile_link "$appli" rot_tz_s_app ../STM32H573IIKXQ_FLASH.ld "$appli_flags" "${appli_srcs[@]}" \
  > "$out/appli-build.log" 2>&1
bash ../postbuild.sh secure Debug > "$out/appli-postbuild.log" 2>&1 || { cat ../postbuild.log >&2; exit 1; }

# What ST's scripts and this script changed in the copied tree, for the evidence record.
: > "$out/cube-changes.diff"
for d in Applications/ROT/OEMiROT_Boot Applications/ROT/OEMiROT_Appli ROT_Provisioning; do
  diff -ru --strip-trailing-cr -x Debug -x Binary -x '*.log' -x image_macros_preprocessed_bl2.c \
    "$cube/Projects/STM32H573I-DK/$d" "$board/$d" >> "$out/cube-changes.diff" || true
done

cp "$boot/Binary/OEMiROT_Boot.bin" "$boot/Binary/rot_init_sign.hex" "$boot/Binary/rot_enc_sign.hex" \
   "$boot/STM32CubeIDE/Debug/OEMiROT_Boot.elf" "$boot/STM32CubeIDE/Debug/OEMiROT_Boot.map" \
   "$boot/STM32CubeIDE/image_macros_preprocessed_bl2.c" \
   "$appli/Binary/rot_tz_s_app.bin" "$appli/Binary/rot_tz_s_app_init_sign.hex" \
   "$appli/Binary/rot_tz_s_app_enc_sign.hex" \
   "$appli/STM32CubeIDE/Debug/rot_tz_s_app.elf" "$appli/STM32CubeIDE/Debug/rot_tz_s_app.map" \
   "$sto/Binary/"*.obk "$out/"
cd "$out"
sha256sum ./*.bin ./*.hex ./*.obk
