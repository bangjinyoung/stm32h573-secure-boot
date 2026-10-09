# X5: binary size and region use

Raw output: `2026-10-09-x5-size.txt` (`arm-none-eabi-size -A`, `.bin` sizes, linker memory regions, timing builds, rough split by component).

Build: 957db98 rebuilt 2026-10-09 with the M10 options (Release, `BL2_ROLLBACK=hw`, STM32CubeCLT 1.22.0, GCC 14.3.1). Its `bl2.bin` is byte-identical to the BL2-c on the board (`.workspace/tmp/m10/bl2c.bin`, e530643) once the 7-byte BUILD_ID string is swapped; the code added after e530643 is behind options that are off. `tz_s.bin` (28,704 B) and `tz_ns.bin` (5,740 B) match the signed M10 images: 0x400 header + bin + 162 B TLV = 29,890 B and 6,926 B. A second build from an empty directory at 36fa3eb (F-07 fix) gave the same result for all three: `bl2.bin` equals `bl2c.bin`, `tz_s.bin` and `tz_ns.bin` equal the bodies of `s_v3.bin` and `ns_v1.bin`, except the BUILD_ID string.

## M10 images (on the board)

| Image | Flash region (linker) | Flash used (text + data) | Use | Static RAM (data + bss + heap/stack) | RAM region | Signed image / slot |
|---|---|---|---|---|---|---|
| BL2-c (HDPL2) | 0x0C000400, 0x17000 (94,208 B) | 49,112 B | 52.1 % | 11,232 B | 0x30000000, 0xA0000 | STiRoT execution area 0x18000 |
| Secure app | 0x0C018400, 0x7000 (28,672 B) + NSC 0x0C01F400, 0x400 | 18,012 B + 32 B veneer | 62.8 % | 2,160 B | 0x30000000, 0x40000 (SRAM1) | 29,890 B of 0x8000 (91.2 %) |
| Non-secure app | 0x08020400, 0xF800 (63,488 B) | 5,736 B | 9.0 % | 1,992 B | 0x20050000, 0x50000 (SRAM3) | 6,926 B of 0x10000 (10.6 %) |

- The Secure image is 29,890 B although its code is about 18 KB: the NSC veneer sits at the fixed slot end 0x0C01F400, so the `.bin` is padded up to it. BL2 hashes everything up to the TLV area (29,732 B: header and padded body), which is why image 0 takes 2.47 M cycles of SHA-256 against 0.57 M for the NS image (X4).
- BL2 split by input section (approximate, `.text/.rodata/.data` before string merging, sum 52,742 B): mbedTLS 17,710 B, HAL/CMSIS and board 11,981 B, MCUboot bootutil 9,257 B, project code 8,188 B, libc/libgcc 5,606 B. The largest single symbol is `HAL_RCCEx_GetPeriphCLKFreq` (6,272 B).

## BL2 timing builds (X4, 48d4e12, not left on the board)

| Build | Flash used | vs BL2-c |
|---|---|---|
| `BL2_TIMING=ON`, HSI | 47,404 B | -1,708 B (log level INFO drops the DEBUG strings, the timing code is added) |
| `BL2_TIMING=ON`, `BL2_SYSCLK=pll250` | 49,588 B | +476 B (adds the PLL setup and HAL RCC code on top) |

## Limits

- Only the M10 images were measured. Earlier milestone images (M1 bare app, M2/M8 BL2, ST OEMuRoT in M6) were not rebuilt here; M1 and M2 sizes are in their build summaries.
- No comparison build with other optimisation or feature settings was made, so no saving is claimed.
