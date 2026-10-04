#!/bin/sh
# Builds ST's STiROT_Appli without STM32CubeIDE and produces the STiRoT inputs:
# appli_enc_sign.hex (signed and encrypted code image) and STiRoT_Config.obk.
# The CubeH5 tree is only read; XML configs and keys are copied into the output directory.
#
# usage: build_stirot_appli.sh <STM32CubeH5> [out_dir] [version]
set -eu

cube=${1:?usage: $0 <STM32CubeH5> [out_dir] [version]}
out=${2:-$(dirname "$0")/../build/stirot_appli}
version=${3:-1.0.0}

clt=${STM32CUBECLT:-/opt/st/stm32cubeclt_1.22.0}
gcc_bin=$clt/GNU-tools-for-STM32/bin
tpc=$clt/STM32CubeProgrammer/bin/STM32TrustedPackageCreator_CLI

board=$cube/Projects/STM32H573I-DK
app=$board/Applications/ROT/STiROT_Appli
prov=$board/ROT_Provisioning/STiROT
hal=$cube/Drivers/STM32H5xx_HAL_Driver

mkdir -p "$out/obj"
out=$(cd "$out" && pwd)

cpu="-mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard"
cflags="$cpu -mcmse -std=gnu11 -Os -g3 -ffunction-sections -fdata-sections -Wall
  -DSTM32H573xx -DUSE_HAL_DRIVER -DUSE_STM32H573I_DK
  -I$app/Inc -I$hal/Inc -I$cube/Drivers/CMSIS/Device/ST/STM32H5xx/Include -I$cube/Drivers/CMSIS/Include"

# Source list from STM32CubeIDE/.project of STiROT_Appli. Src/startup_stm32h573xx.s is the IAR
# startup; the CubeIDE build uses its own GCC startup, syscalls and sysmem under Example/.
srcs="
  $app/STM32CubeIDE/Example/User/syscalls.c $app/STM32CubeIDE/Example/User/sysmem.c
  $app/Src/main.c $app/Src/loader.c $app/Src/com.c $app/Src/stm32h5xx_it.c
  $app/Src/system_stm32h5xx.c $app/Src/low_level_obkeys.c $app/Src/low_level_security.c
  $app/Src/test_versions.c
  $hal/Src/stm32h5xx_hal.c $hal/Src/stm32h5xx_hal_cortex.c $hal/Src/stm32h5xx_hal_cryp.c
  $hal/Src/stm32h5xx_hal_dma.c $hal/Src/stm32h5xx_hal_dma_ex.c $hal/Src/stm32h5xx_hal_exti.c
  $hal/Src/stm32h5xx_hal_flash.c $hal/Src/stm32h5xx_hal_flash_ex.c $hal/Src/stm32h5xx_hal_gpio.c
  $hal/Src/stm32h5xx_hal_gtzc.c $hal/Src/stm32h5xx_hal_pwr.c $hal/Src/stm32h5xx_hal_pwr_ex.c
  $hal/Src/stm32h5xx_hal_rcc.c $hal/Src/stm32h5xx_hal_rcc_ex.c $hal/Src/stm32h5xx_hal_uart.c
  $hal/Src/stm32h5xx_hal_uart_ex.c $hal/Src/stm32h5xx_hal_rng.c"

objs=""
for src in $srcs; do
  obj=$out/obj/$(basename "$src" .c).o
  # shellcheck disable=SC2086
  "$gcc_bin/arm-none-eabi-gcc" $cflags -c "$src" -o "$obj"
  objs="$objs $obj"
done
"$gcc_bin/arm-none-eabi-gcc" $cpu -x assembler-with-cpp -c "$app/STM32CubeIDE/Example/Startup/startup_stm32h573zitxq.s" -o "$out/obj/startup.o"

# shellcheck disable=SC2086
"$gcc_bin/arm-none-eabi-gcc" $cpu --specs=nano.specs -Wl,--gc-sections \
  -T"$app/STM32CubeIDE/STM32H573IIKXQ_FLASH.ld" -Wl,-Map="$out/appli.map" \
  $objs "$out/obj/startup.o" -o "$out/appli.elf"
"$gcc_bin/arm-none-eabi-objcopy" -O binary "$out/appli.elf" "$out/appli.bin"
"$gcc_bin/arm-none-eabi-size" "$out/appli.elf"

# TPC resolves the XML's relative paths (keys, LinkedXML) from the XML location, so keep the
# reference Config/Image/Keys layout side by side.
rm -rf "$out/stirot"
mkdir -p "$out/stirot/Binary"
cp -r "$prov/Config" "$prov/Image" "$prov/Keys" "$out/stirot/"

code_xml=$out/stirot/Image/STiRoT_Code_Image.xml
sed -i \
  -e "s#<Value>[^<]*appli\.bin</Value>#<Value>$out/appli.bin</Value>#" \
  -e "s#<Value>[^<]*appli_enc_sign\.hex</Value>#<Value>$out/appli_enc_sign.hex</Value>#" \
  "$code_xml"
# The Version param is the only <Value>1.0.0</Value> in the file; check that before rewriting it.
[ "$(grep -c '<Value>1.0.0</Value>' "$code_xml")" -eq 1 ] || { echo "version field not unique in $code_xml" >&2; exit 1; }
sed -i "s#<Value>1.0.0</Value>#<Value>$version</Value>#" "$code_xml"

"$tpc" -obk "$out/stirot/Config/STiRoT_Config.xml"
"$tpc" -pb "$code_xml"

sha256sum "$out/appli.bin" "$out/appli_enc_sign.hex" "$out/stirot/Binary/STiRoT_Config.obk"
