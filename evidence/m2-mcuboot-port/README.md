# M2 MCUboot direct port

Date: 2026-10-04
Board: STM32H573I-DK, PRODUCT_STATE Open, TZEN 0xC3, BOOT_UBE 0xB4 (unchanged from M0)
Boot chain: Reset → MCUboot (bl2) → bare application in the primary slot
MCUboot: upstream v2.4.0, mbedtls v3.6.6, imgtool 2.4.0 from the same submodule
Key: this project's own ECDSA P-256 key. Public key `keys/mcuboot-ecdsa-p256.pub.pem` (DER sha256 `1a1db9cf…3fea`). The private key is not in the repository.

## Layout

| Area | Address | Size | Source |
|---|---|---|---|
| bl2 | 0x08000000 | 0x18000 (96 KB) | `boot/mcuboot/config/flash_layout.h` |
| Primary slot | 0x08018000 | 0xA0000 | same |
| App vector table | 0x08018400 | | slot + 0x400 image header |
| Secondary slot | 0x080B8000 | 0xA0000 | same |

The slots end at 0x158000, before the STiRoT download areas (0x1E6000) planned for M6. `flash_layout.h` checks this with `#error`.

## Commands

```sh
cmake --preset debug && cmake --build --preset debug
scripts/imgtool sign --key <private key> --header-size 0x400 --pad-header --align 16 --slot-size 0xA0000 --version 1.0.0 bare_slot.bin bare_slot_signed.bin
STM32_Programmer_CLI -c port=SWD -d bl2.bin 0x08000000 -v -d bare_slot_signed.bin 0x08018000 -v -hardRst
STM32_Programmer_CLI -c port=SWD mode=Hotplug -r32 0xE000ED08 4
STM32_Programmer_CLI -c port=SWD mode=Hotplug -r32 <uwTick> 4      # twice, 2 s apart
STM32_Programmer_CLI -c port=SWD mode=Hotplug -halt -coreReg -run
```

The build signs `bare_slot` in a post-build step when the private key is present.

## Result

| Item | Value | Source |
|---|---|---|
| Build | succeeded. 0 warnings from project code | `2026-10-04-m2-build-summary.txt` |
| bl2 size | text 48060, data 184, bss 11896 (8 KB stack) | same |
| Signed image | `imgtool verify`: "Image was correctly validated". TLVs SHA256, KEYHASH, ECDSASIG | same |
| Flash | two downloads verified, hard reset | `2026-10-04-m2-flash.log` |
| VTOR after boot | 0x08018400 | `2026-10-04-m2-debugger-state.log` |
| App SysTick | `uwTick` 0x6D5 → 0x1064 across a 2 s wait | same |
| Core at halt | PC 0x08018E46 in `HAL_Delay` of `bare_slot`, MSP 0x2009FFC0 | same, `addr2line` on `bare_slot.elf` |

## Port record (E2.3)

Taken from the H563 PoC (`h563_oemirot_boot`, which itself derives from ST OEMiROT_Boot):

| File | Change |
|---|---|
| `port/low_level_flash.c`, `port/low_level_device.c`, `port/flash_map_backend.c` | none. Built with vendor warning options |
| `port/mcuboot_logging.c`, `config/mcuboot_config/mcuboot_logging.h`, `mcuboot_assert.h`, `config/mbedtls_boot_config.h` | none |
| `port/include/` (`Driver_Flash.h`, `Driver_Common.h`, `low_level_flash.h`, `sysflash.h`, `flash_map_backend.h`, `os/os_malloc.h`) | none |

Written for H573:

| File | Why |
|---|---|
| `config/flash_layout.h` | overwrite-only slots, no swap-using-offset, guard against the M6 STiRoT areas |
| `config/mcuboot_config/mcuboot_config.h` | overwrite-only, no downgrade prevention yet (M8), no watchdog (M8) |
| `port/boot_handoff.c` | direct jump. H563 handed off through NSSLIB at HDPL3 |
| `main.c` | uses the shared `boards/stm32h573i_dk` console instead of CubeMX init |
| `scripts/gen_bootutil_keys.py` | key table generated from the public key PEM at build time. H563 pasted the key into `keys.c` |
| `cmake/stm32h573.cmake` | linker script, stack size, and `VECT_TAB_OFFSET` per image |

The other files under H563's `port/stm32h5` (security, OBK, RNG, MPU, loader) were never compiled there and are not carried over.

## Failures on the way

1. UsageFault STKOF in bl2. `.workspace/logs/2026-10-04-m2-stkof-debugger-state.log`: CFSR 0x00100000, MSP at `_sstack` 0x2009FC00, core in the default handler. The CMSIS template stack is 1 KB. bl2 now links with 8 KB, as the H563 port did.
2. VTOR reset by the application. `.workspace/logs/2026-10-04-m2-vtor-reset-debugger-state.log`: the app ran (PC in `HAL_Delay`) but VTOR read 0x08000000. The CMSIS `SystemInit` writes `FLASH_BASE | VECT_TAB_OFFSET` unconditionally and the offset defaulted to 0, so the app's interrupts would vector through bl2's table. `system_stm32h5xx.c` is now built per image with `VECT_TAB_OFFSET` derived from its link address. Whether the app actually stalled before the fix was not measured.

## Not captured yet

- VCP log of bl2 (`[bl2] mcuboot build=…`, `[bl2] image ok version=1.0.0+0`, `[bl2] jump to 0x08018400 …`) followed by the app's boot lines.
