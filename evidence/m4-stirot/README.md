# M4 STiRoT standalone

## Entry gate E13.1: TrustZone on and back off

Date: 2026-10-04
Board before: PRODUCT_STATE 0xED (Open), TZEN 0xC3, BOOT_UBE 0xB4, bl2 + signed bare app 1.0.0 from M3

| Step | Command | Result | File |
|---|---|---|---|
| A | `-ob displ` | TZEN 0xC3, SECBOOT_LOCK reads 0x0 ("Unknown Value"), SECWM1 empty | `2026-10-04-m4-gate-a-ob-before.log` |
| B | `-c port=SWD speed=fast ap=1 mode=Hotplug -ob TZEN=0xB4` | "Option Bytes successfully programmed". TZEN 0xB4, SECBOOTADD 0x0C000000, SECWM1 0x08000000–0x080FE000 (all of bank 1 secure), SECBOOT_LOCK 0xC3 | `2026-10-04-m4-gate-b-tzen-on.log`, `2026-10-04-m4-gate-b-ob-after.log` |
| B, console | VCP after the OB reload | bl2 verified and started the app as before (`image ok version=1.0.0+0`, jump to 0x08018400, heartbeat). VTOR 0x08018400 | `2026-10-04-m4-gate-b-vcp.log` |
| C1 | `-c port=SWD speed=fast ap=1 mode=Hotplug -hardRst -ob TZEN=0xC3`, run by the user from the prompt. The first try failed on a typo (`-hardrrst`, "Unknown command"); the retry printed "Option Bytes successfully programmed" at 17:59:45 (2.753 s) | TZEN 0xC3, SECBOOTADD 0x0, SECBOOT_LOCK reads 0x0 again. SECWM1 keeps 0x0–0x7F, which has no effect with TrustZone off | `2026-10-04-m4-gate-c1-tzen-off-{1,2,3}.png` (screenshots of the user's console), `2026-10-04-m4-gate-c1-ob-after.log` |
| C1, flash | `-r32 0x08000000 0x10` | bl2 vector table still present (`200A0000 08000E8D …`). Nothing was erased | same session, see below |
| C1, console | VCP after the OB reload | bl2 started at 17:59:45 and booted app 1.0.0 | `2026-10-04-m4-gate-c1-vcp.log` |
| C2 | password DA regression | not needed | |

## Result

In product state Open, TZEN went 0xC3 → 0xB4 → 0xC3 with option-byte writes alone, and user flash was kept. The DA regression path was not exercised, so it is still unverified (E13.2 covers it before M10).

The images built for TrustZone-disabled operation also booted while TZEN was 0xB4. Why they ran with bank 1 marked secure is not examined yet.

## Main work: STiRoT → ST STiROT_Appli

Date: 2026-10-04
Configuration: ST reference `ROT_Provisioning/STiROT` + `Applications/ROT/STiROT_Appli` (full secure, 1 image), ST default keys, PRODUCT_STATE Open. Reference: STM32CubeH5 e5c73ea1.

The reference scripts were not run. The same steps were run as separate CLI commands. Two reference steps were left out on purpose:

- `BOOT_UBE=0xC3` at the end of `ob_flash_programming.sh`, followed by `BOOT_UBE=0xB4` at the start of `obkey_provisioning.sh OPEN`. Nothing between them depends on 0xC3, so UBE stayed 0xB4 through OBK provisioning and was set to 0xC3 once at the end.
- `SECBOOT_LOCK=0xB4`. The Open path sets it back to 0xC3 anyway. `DA_Config.obk` was not provisioned either (DA is M10 scope).

| Step | Command | Result | File |
|---|---|---|---|
| Build | `scripts/build_stirot_appli.sh <CubeH5>` (GCC from CubeCLT 1.22.0, TPC v2.23.0) | `appli.bin` linked at 0x0C000400 / RAM 0x30000000; `appli_enc_sign.hex` (encrypted, signed, v1.0.0); `STiRoT_Config.obk` byte-identical to the shipped one | `2026-10-04-m4-e41-stirot-config.txt` |
| OB before | `-ob displ` | Open, TZEN 0xC3, BOOT_UBE 0xB4 | `2026-10-04-m4-e43-ob-before.log` |
| 1 | `-ob TZEN=0xB4` | programmed | `2026-10-04-m4-s1-tzen-on.log` |
| 2 | UR, remove protections, `SECBOOT_LOCK=0xC3`, `-e all` | programmed, mass erase done | `2026-10-04-m4-s2-remove-protect-erase.log` |
| 3 | `-ob SRAM2_RST=0 SRAM2_ECC=0` | programmed | `2026-10-04-m4-s3-sram2.log` |
| 4 | `-ob SECWM1_STRT=0x0 SECWM1_END=0xF`, `-ob SECWM2_STRT=0x7F SECWM2_END=0x0` | programmed | `2026-10-04-m4-s4-secwm.log`, `2026-10-04-m4-s5-ob-after.log` |
| 5 | `-d appli_enc_sign.hex`, `-r 0x081E0000 0x20000` | readback identical to the image (sha256 14db881b…) | `2026-10-04-m4-s5-download.log` |
| 6 | SW1 set to 1 by the user; `-hardRst`, VTOR and core registers | VTOR 0x0BF97000, PC 0x0BF9CD36: system bootloader, so BOOT0 is high | `2026-10-04-m4-s6-boot0-check.{log,png}` |
| 7 | `-hardRst`, `-sdp STiRoT_Config.obk`, `-hardRst`, `-sdp STiRoT_Data.obk` | "OBKey Provisioned successfully" twice. The second step's first connect failed ("Cannot connect to access port 1") and the CLI reconnected | `2026-10-04-m4-s7-s8-obk-ube{,-full}.log`, `.png` |
| 8 | `-hardRst`, `-ob BOOT_UBE=0xC3` | programmed. No boot followed: execution area still erased, core still in the bootloader | same, `2026-10-04-m4-s8-post-check.log` |
| 9 | `-hardRst` (SWD reset, no power cycle) | 21:27:13 VCP: ST banner, `User App #A`, `Appli Code version: 1.0.0`, main menu | `2026-10-04-m4-s9-hardrst.log`, `2026-10-04-m4-e44-vcp.log`, `2026-10-04-m4-e44-stirot-boot-banner.png` |
| After | VTOR, core registers, execution area readback, download area | VTOR 0x0C000400; PC 0x0C003D20 in `UART_WaitOnFlagUntilTimeout` (menu waiting for input); 0x0C000000 holds header v1.0.0 and the payload is byte-identical to plaintext `appli.bin`; download area 0x081E0000 erased | `2026-10-04-m4-e44-post-boot-state.log` |
| OB after | `-ob displ` | Open, TZEN 0xB4, BOOT_UBE 0xC3, SECBOOTADD 0x0C000000, SECBOOT_LOCK 0xC3, SECWM1 0x0–0xF, SECWM2 none, SRAM2_RST 0, SRAM2_ECC 0 | `2026-10-04-m4-e43-ob-after.log` |

The `rc=` lines in the step 7–8 log and PNG are the exit status of a sed filter, not of the CLI. Success there is read from the CLI messages.

The first VCP line at 21:27:13.196, `[mcuboot INF] Image…`, is not STiRoT output. bl2 was cut off mid-line by the 21:20:37 reset, `vcp_logger.py` kept that partial line, and the banner's first newline completed it.

## Result

With product state Open, STiRoT took the encrypted, signed image from the download area, installed it decrypted into the secure execution area at 0x0C000000, erased the download area, and started the application, which reported version 1.0.0. That shows the STiRoT boot path works. It does not yet show that STiRoT rejects a bad image; that is M5.

Not observed: a boot from power-on. Boot 9 came from an SWD system reset.

Second SWD reset at 21:30:48, with the download area already empty: the same banner and version 1.0.0 at 21:30:49.3, VTOR 0x0C000400 (`2026-10-04-m4-e44-second-reset.log`). This shows the installed image boots again on reset. It does not by itself show that STiRoT checked it on this boot; with BOOT_UBE 0xC3 STiRoT is the selected boot entry, and M5's tamper test is what shows the check.

The ST-LINK serial number is masked as `<masked>` in every log under `evidence/`.
