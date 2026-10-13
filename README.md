# stm32h573-secure-boot

A secure boot proof of concept on the STM32H573I-DK. ST's immutable root of trust (STiRoT) verifies a custom MCUboot bootloader. That bootloader verifies a Secure and a Non-secure application, with TrustZone isolation and anti-rollback through a security counter that BL2 keeps in the HDPL2 OBK area. Every step was run on the board and recorded under `evidence/`.

```text
Reset (BOOT_UBE 0xC3)
  │
  ▼
STiRoT                  HDPL1  ST system flash, immutable
  │ decrypt + verify (ST default key)
  ▼
Custom MCUboot BL2      HDPL2  0x0C000400, upstream MCUboot v2.4.0 + mbedTLS v3.6.6
  │ ECDSA P-256 + SHA-256 (project key), security counter in HDPL2 OBK
  ▼
Secure app              HDPL3  0x0C018000, SAU / GTZC setup, one NSC veneer
  │ blxns
  ▼
Non-secure app                 0x08020000, USART1 console
```

The board stays in the Open product state for the whole project, and STiRoT runs with ST's published default keys. This is a PoC, not a production configuration (see [Limitations](#limitations)).

## What was verified on the board

The work was done in milestones. Each trust component was checked on its own before the components were chained, so a failure had only one cause to look for.

| | Milestone | Result |
|---|---|---|
| M0 | Board baseline and recovery path | Option bytes, device ID, and DA state read; flash backed up |
| M1 | Bare application, CMake + GCC, no IDE | Runs from 0x08000000, VCP log and LED heartbeat |
| M2 | Upstream MCUboot port | `Reset → BL2 → app` with TrustZone off |
| M3 | MCUboot signature and tamper tests | Hash tamper (primary and secondary slot) and a different signing key are rejected; a valid update installs |
| M4 | STiRoT alone | TZEN on, STiRoT selected, ST reference app boots |
| M5 | STiRoT tamper test | A tampered download image is not installed |
| M6 | STiRoT → ST OEMuRoT → app | ST's reference two-stage chain boots (baseline) |
| M7 | Tamper tests on the ST chain | OEMuRoT rejects a tampered app; STiRoT rejects a tampered OEMuRoT |
| M8 | STiRoT → custom MCUboot → custom app | Custom BL2 packaged as the STiRoT HDPL2 image; the HDPL2 → HDPL3 jump goes through RSS; the M7 tamper cases give the same results |
| M9 | Update and anti-rollback | A version-only check is downgraded by a direct primary slot write; the security counter in HDPL2 OBK rejects the same write and fails closed |
| M10 | TrustZone Secure / Non-secure | Two-image boot; tampering either image stops the boot; Non-secure access to Secure SRAM, flash, or a function body raises a SecureFault, while the NSC veneer works |

Cross-cutting records:

| | Topic | Where |
|---|---|---|
| X2 | Every option byte snapshot from M0 to M10, and what changed when | `evidence/x2-security-config/` |
| X3 | Which key signs which image, and which verifier trusts it | `evidence/x3-signing-keys/` |
| X4 | Boot timing with the DWT cycle counter | `evidence/x4-boot-timing/` |
| X5 | Binary size and region use | `evidence/x5-binary-size/` |
| X7 | Architecture, drawn with only the arrows that have board evidence | `evidence/x7-architecture/` |

Boot timing at 250 MHz (X4): one ECDSA P-256 verification takes about 38.4 M cycles (153 ms) per image, about 97 % of each image check. SHA-256 runs at about 83 cycles/byte. The whole `boot_go()` for two images takes 327.7 ms. Five boots gave identical counts, to the cycle.

## Memory map

Final chain (M10):

| Area | Address | Attribute | Content |
|---|---|---|---|
| BL2 area (installed and verified by STiRoT) | 0x0C000000–0x0C017FFF | Secure (SECWM1) | BL2 (STiRoT header at 0x0C000000, code at 0x0C000400) |
| Image 0 primary slot | 0x0C018000–0x0C01FFFF | Secure (SECWM1) | Secure app; NSC veneer at 0x0C01F400 |
| Image 1 primary slot | 0x08020000–0x0802FFFF | Non-secure | Non-secure app |
| Image 0 secondary slot | 0x08030000–0x08037FFF | Non-secure | Update slot for the Secure app |
| Image 1 secondary slot | 0x08038000–0x08047FFF | Non-secure | Update slot for the Non-secure app |
| STiRoT download area | 0x081E8000–0x081FFFFF | | Encrypted BL2 for STiRoT to install |
| SRAM (Secure alias) | 0x30000000 | Secure | BL2, then Secure app RAM (SRAM1) |
| SRAM3 | 0x20050000–0x2009FFFF | Non-secure | Non-secure app RAM |
| HDPL2 OBK | 0x0FFD0B10 | | Rollback record (security counters) |

The slot layout is in `boot/mcuboot/config/flash_layout.h`. The build fails if a slot crosses the Secure watermark or overlaps the STiRoT download area.

## Repository layout

```text
app/bare/        M1/M8 single-image app (bare_slot, linked for the MCUboot primary slot)
app/tz_s/        M10 Secure app: SAU/GTZC partition, NSC entry, starts the Non-secure image
app/tz_ns/       M10 Non-secure app: console and Secure access probes
boards/          STM32H573I-DK support (clock, VCP, LED); the only place the apps call the HAL
boot/mcuboot/    BL2: MCUboot configuration and the STM32H573 port
  port/          flash driver, RSS handoff, HDPL2 OBK rollback record, MPU/security setup, timing
cmake/           toolchain file, linker script generation, build ID
keys/            the project's MCUboot public key (no private keys in this repo)
modules/         submodules: CMSIS, STM32H5 HAL, MCUboot v2.4.0, mbedTLS v3.6.6
platform/        HAL configuration
scripts/         image signing, STiRoT packaging, tamper images, board steps, VCP logging
evidence/        per-milestone logs, captures, and result summaries
```

ST-specific code (RSS HDPL transition, OBK access, flash security attribution) lives in `boot/mcuboot/port/`. The MCUboot submodule is used unmodified.

## Build

Requirements:

- STM32CubeCLT 1.22.0 (arm-none-eabi-gcc 14.3.1, CMake, Ninja, STM32CubeProgrammer, STM32TrustedPackageCreator). The scripts expect it in `/opt/st/stm32cubeclt_1.22.0`; set `STM32CUBECLT` to override.
- Python 3 and [uv](https://github.com/astral-sh/uv). `scripts/imgtool` runs imgtool from the MCUboot submodule through `uvx`, so signing matches the bootloader version.
- STM32CubeH5 v1.7.0, only to package BL2 as the STiRoT image and to build the M6/M7 ST reference chain.

```sh
git clone --recurse-submodules https://github.com/bangjinyoung/stm32h573-secure-boot.git
cd stm32h573-secure-boot

export PATH=/opt/st/stm32cubeclt_1.22.0/GNU-tools-for-STM32/bin:/opt/st/stm32cubeclt_1.22.0/CMake/bin:/opt/st/stm32cubeclt_1.22.0/Ninja/bin:$PATH
cmake --preset release -DBL2_ROLLBACK=hw
cmake --build --preset release
```

This builds `bl2`, `tz_s`, `tz_ns`, and `bare_slot` under `build/release/`. A successful build does not reproduce the boot chain. The board also needs TrustZone on and STiRoT provisioned (option bytes and STiRoT OBK, see `evidence/m4-stirot/` and `evidence/m6-stirot-oemurot/`), BL2 packaged and installed through the STiRoT download area, and `tz_s` and `tz_ns` signed with the private key that matches the key BL2 trusts (next section). BL2 options:

| Option | Values | Meaning |
|---|---|---|
| `BL2_ROLLBACK` | `hw` (default), `sw` | `hw`: security counter in the HDPL2 OBK record. `sw`: MCUboot version check only |
| `BL2_TIMING` | `OFF` (default), `ON` | Measure image checks with the DWT cycle counter and print them after `boot_go()` |
| `BL2_SYSCLK` | `hsi` (default), `pll250` | Run BL2 at the clock STiRoT leaves, or at 250 MHz |

BL2 trusts the key in `keys/mcuboot-ecdsa-p256.pub.pem`. The private key is not in this repo. The build signs `bare_slot` with `MCUBOOT_SIGNING_KEY` (default `.workspace/keys/mcuboot-ecdsa-p256.pem`, local and git-ignored) and builds it unsigned when that file is missing; `scripts/make_m9_images.sh` and `scripts/make_m10_images.sh` read the same path. To use your own key, generate a P-256 key pair, replace the public key file, and put the private key at that path or point `MCUBOOT_SIGNING_KEY` at it.

## Signing and packaging

Application images are signed with imgtool:

```sh
scripts/imgtool sign --key <private key> --header-size 0x400 --pad-header --align 16 \
  --overwrite-only --slot-size 0x8000 --version 3.0.0 --security-counter 2 \
  build/release/app/tz_s/tz_s.bin s_v3.bin
```

BL2 is packaged as the STiRoT HDPL2 image with ST's tooling. This gives an execution image and an encrypted download image:

```sh
scripts/build_stirot_oemurot.sh <STM32CubeH5>     # once: ST's STiROT_OEMuROT tree and keys
scripts/package_bl2_stirot.sh build/release/boot/mcuboot/bl2.bin out/ 2.0.0
```

`scripts/make_m10_images.sh <build dir>` does both for the M10 image set and adds the one-bit tampered copies used in the tests. `scripts/mcuboot_image.py check <image> <public key>` recomputes the hash TLV and checks the signature on the host.

## Board procedure

Provisioning STiRoT (TZEN, option bytes, OBK) changes the board's security state. Read `evidence/m4-stirot/` and `evidence/x2-security-config/` before you repeat it on your board. What is known about undoing it:

- In Open, TZEN went 0xC3 → 0xB4 → 0xC3 with option byte writes alone, and user flash was kept (M4 entry gate). That was before STiRoT, its OBKs, SECBOOT_LOCK, and the rollback record were provisioned. The round trip was not repeated on the final configuration, and what happens to the OBK contents was not checked. SECBOOT_LOCK=0xB4 freezes BOOT_UBE and SECBOOTADD and has to be set back to 0xC3 first, which RM0481 allows in Open.
- Debug authentication was never provisioned on this board, and no regression was run. Outside Open, RM0481 returns the device to Open only through a full regression authorized by debug authentication, which mass-erases user flash; Locked cannot be regressed. This project did not test that path.

- Secure flash can be written only while the core is Secure. The board steps connect under reset with SW1 (BOOT0) at 0, write, read back, and compare. Boots run with SW1 at 1.
- `scripts/tamper_step.sh [-n] [-u] <m7|m8|m9|m10> <log dir> <step>` runs one recorded step (write, boot, observe, option byte read) and logs the CLI output. `-n` prints the commands without running them.
- `scripts/vcp_logger.py <tty> <log>` timestamps the VCP console. `scripts/board_observe.sh` reads VTOR, DSCSR, fault registers, and PC samples over SWD.
- With SW1 at 0, BL2 sees that it runs at HDPL1 and stops (`hdpl=0x51, not started by stirot: stop`). This is expected during board writes.

## Limitations

- The board stays Open (PRODUCT_STATE 0xED). Product state and debug authentication are out of scope.
- STiRoT uses ST's default keys. While the board is Open, anyone with the CubeH5 package can sign an HDPL2 image that STiRoT accepts, and such an image could rewrite the rollback record. Lowering the counter this way was not tested.
- In Open, BOOT0 lets unverified code run at HDPL1 without STiRoT (seen in M8).
- Application images are signed, not encrypted. TrustZone isolates the running Secure app, not its update image: both secondary slots lie in Non-secure flash outside SECWM1, so a staged Secure image is not confidential to anything that can read that flash, such as a debugger in Open. The Secure app's SAU setup keeps the other slots out of the Non-secure app's regions; whether Non-secure code can reach them some other way was not tested. In M10 the images were written straight to the primary slots, so no update through a secondary slot was run on the two-image chain.
- MCUboot runs overwrite-only. Swap/revert and watchdog recovery are not implemented.
- The M10 tamper tests changed the payload hash only. Header, TLV, and other-key cases were not repeated on the two-image chain.
- The project private key has no backup procedure or HSM.

Each evidence summary lists its own limits and marks what was inferred rather than measured.

## License

MIT, see `LICENSE`. Submodules keep their own licenses.
