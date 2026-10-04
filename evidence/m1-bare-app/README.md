# M1 bare application

Date: 2026-10-04
Board: STM32H573I-DK, PRODUCT_STATE Open, TZEN 0xC3, BOOT_UBE 0xB4 (unchanged from M0)
Toolchain: STM32CubeCLT 1.22.0 (arm-none-eabi-gcc 14.3.1, CMake 4.3.1, Ninja)
Firmware: `app/bare`, Debug preset, reset clock (HSI 64 MHz divided by 2, so 32 MHz; the app prints `core_clock_hz=32000000`), no PLL
Key: none

## Commands

```sh
export PATH=/opt/st/stm32cubeclt_1.22.0/GNU-tools-for-STM32/bin:/opt/st/stm32cubeclt_1.22.0/CMake/bin:/opt/st/stm32cubeclt_1.22.0/Ninja/bin:$PATH
cmake --preset debug && cmake --build --preset debug
STM32_Programmer_CLI -c port=SWD -e all -d build/debug/app/bare/bare.bin 0x08000000 -v -hardRst
STM32_Programmer_CLI -c port=SWD mode=Hotplug -r32 0xE000ED08 4
STM32_Programmer_CLI -c port=SWD mode=Hotplug -halt -coreReg -run
```

The mass erase removed the ST factory demo. Its full readback is kept from M0.

## Result

| Item | Value | Source |
|---|---|---|
| Build | succeeded, 0 warnings from `app/` and `boards/` (9 from vendor HAL unused parameters and newlib nosys stubs) | `.workspace/logs/m1-build-debug.log` |
| Size | text 18500, data 104, bss 2056 | `2026-10-04-m1-build-summary.txt` |
| `.isr_vector` | 0x08000000, 0x24C bytes | same |
| Stack | `_estack` 0x200A0000, `_sstack` 0x2009FC00 (MSPLIM) | same |
| Reset vector in `bare.bin` | SP 0x200A0000, Reset_Handler 0x0800155D | `xxd` of the image, same file |
| Flash | mass erase, download, verify succeeded | `2026-10-04-m1-flash.log` |
| VTOR at runtime | 0x08000000 | `2026-10-04-m1-debugger-state.log` |
| Core at halt | PC 0x08000914 (`HAL_GetTick`), LR in `HAL_Delay`, MSP 0x2009FFC0 | same, resolved with `addr2line` |

The halted PC sits in the heartbeat loop's delay. MSP is inside the 1 KB stack window.

## Observation

2026-10-04, reported by the user from picocom on `/dev/ttyACM0` (115200 8N1): heartbeat lines arrive on the VCP and LED1 blinks.
The picocom session had no log file, so the exact text is not stored. Capture it with `picocom --logfile` the next time the app runs.
