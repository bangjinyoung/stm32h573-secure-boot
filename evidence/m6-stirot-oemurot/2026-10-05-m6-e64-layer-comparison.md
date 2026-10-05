# M6 E6.4: M2 direct port vs ST OEMuRoT, layer comparison

Date: 2026-10-05. Read-only code comparison on the host (agent-drafted, main-session spot-checked: MCUBOOT_USE_HASH_REF flash_layout.h:54, hash ref branch image_validate.c:517-529, counter update loader.c:2195-2204, hash ref store boot_hal.c:256-266, MCUBOOT_DOUBLE_SIGN_VERIF set in the .cproject defines).

Path shorthand (all under `/home/bjy/workspaces/stm32h573-secure-boot`):
- `M2` = `boot/mcuboot/`
- `CUBE` = `build/stirot_oemurot/cube`
- `BOOT` = `CUBE/Projects/STM32H573I-DK/Applications/ROT/OEMiROT_Boot`
- `STMCU` = `CUBE/Middlewares/Third_Party/mcuboot`
- `UPMCU` = `modules/mcuboot`

## Comparison table

| Layer | M2 direct port | ST OEMuRoT | Difference |
|---|---|---|---|
| Generic MCUboot | Upstream v2.4.0 from `modules/mcuboot` (`evidence/m2-mcuboot-port/README.md:6`). bootutil file list at `M2/CMakeLists.txt:35-49` (has `bootutil_loader.c`, `image_ecdsa.c`, `swap_scratch.c`). Upstream tree also has `swap_offset.c`, `swap_move.c`, `ed25519_psa.c`, `encrypted_psa.c` (`ls UPMCU/boot/bootutil/src`). Config: EC256, mbedTLS, overwrite-only, validate primary, 1 image (`M2/config/mcuboot_config/mcuboot_config.h:6-13`). No FIH profile, no HW rollback, no encryption, no hash ref in the config. | ST fork "mcuboot 1.7.2 modified by STMicroelectronics", tag v1.7.2.40 (`STMCU/README.md:3,8`; `STMCU/st_readme.txt:184`). Old-style `loader.c`, `image_ec256.c`, `swap_scratch.c`, `swap_move.c`, no `swap_offset` (`ls STMCU/boot/bootutil/src`; `grep SWAP_USING_OFFSET STMCU/boot` = none). Config: FIH profile HIGH (`BOOT/Inc/mcuboot_config/mcuboot_config.h:32`), EC256 sign + encrypt (`:51-52`), validate primary (`:54`), HW rollback protection (`:57`), encrypted images (`:58`), bootstrap (`:60`), HW accel via HAL + PKA (`:66-72`). Layout flags in `BOOT/Inc/flash_layout.h`: `OEMUROT_ENABLE` (`:29`), `MCUBOOT_OVERWRITE_ONLY` (`:31`), `MCUBOOT_USE_HASH_REF` (`:54`). Build adds `MCUBOOT_DOUBLE_SIGN_VERIF`, `FLOW_CONTROL`, `OEMIROT_DEV_MODE` (`scripts/build_stirot_oemurot.sh:124-126`). | Version gap v2.4.0 vs 1.7.2.x. ST-only: hash ref bypass (`STMCU/boot/bootutil/src/image_validate.c:517-559`), double sign verification, FIH flow control, OBK-backed NV counters, ST image magic support in imgtool. Swap-using-offset exists only upstream. Both are overwrite-only. |
| ST-specific HAL | `M2/port/low_level_flash.c` (657 lines), `low_level_device.c` (44), `flash_map_backend.c` (330), `mcuboot_logging.c`. Flash driver files are carried from H563, derived from ST OEMiROT_Boot (`evidence/m2-mcuboot-port/README.md:41-47`). No MPU, SAU, GTZC, RNG, SAES code (`grep` in `M2/port`, `M2/main.c` = none). `main.c` uses shared board console (`M2/main.c:5`). Handoff: `boot_handoff_jump()` validates vectors, quiesces NVIC/SysTick, sets VTOR/MSP, calls reset handler (`M2/port/boot_handoff.c:59-108`). | `BOOT/Src/low_level_flash.c` (640), `low_level_device.c` (62), `low_level_security.c` (1923), `low_level_obkeys.c`, `low_level_rng.c`, `low_level_com.c`, `boot_hal.c` (696), `mpu_armv8m_drv.c`, `sha256_alt.c`. Handoff: `boot_platform_quit()` (`BOOT/Src/boot_hal.c:224-307`) stores hash refs, applies run-time protections twice (`:285,293`), clears BL2 RAM, then jumps through the RSS `JumpHDPLvl3` service (`:254,300`; `boot_jump_to_RSS` `:166-195`). | `diff -w` of `low_level_flash.c` / `low_level_device.c` vs ST: 19 / 20 changed lines (measured, this session). The split is everything around them: security config (low_level_security.c), OBK, RNG, MPU/SAU/GTZC, RSS jump. M2 uses a plain VTOR/MSP jump. |
| OEMuRoT config | None. TZEN=0, STiRoT absent (checklist M2 Entry). Layout: bl2 `0x08000000` size `0x18000`, slots `0xA0000` each (`M2/config/flash_layout.h:9-22`). | `OEMUROT_ENABLE` (`BOOT/Inc/flash_layout.h:29`). WRP and HDP checks and enables are compiled out when `OEMUROT_ENABLE` is set (`BOOT/Inc/boot_hal_cfg.h:43-46`; our macro dump shows `RE_BL2_HDP_START=0x1 > END=0x0`, `layout-macros.txt` lines `RE_BL2_HDP_*`). Still on: ICache (`:40`), SRAM2 erase-at-reset and ECC (`:47-48`), BOOT_LOCK 0xB4 (`:61`), internal tamper (`:59`), Flash priv-only (`:68`), MPU (`:69`), fast wake-up (`:72`). Our build adds DEV_MODE: product state min OPEN (`:50-54`), error handler stops (`:63-65`), logging on (`BOOT/Inc/mcuboot_config/mcuboot_config.h:78`). Our changes to the layout: 1 image, NS size 0 (`cube-changes.diff` flash_layout.h hunks). | M2 is open and unprotected. OEMuRoT requires TZEN=1, BOOT_UBE=0xC3 (ST iRoT), SECBOOT_LOCK, SRAM2 settings and secure flash watermark (`BOOT/Src/low_level_security.c:690-880`). See section D. |
| HDPL / OBK | None. No OBK access, no HDPL service call. Anti-rollback absent (M8) (`M2/config/mcuboot_config/mcuboot_config.h` has no `HW_ROLLBACK_PROT`/`DOWNGRADE_PREVENTION`). | OBK HDPL2 config + data structs (`BOOT/Inc/low_level_obkeys.h:75-103`): HDPL3 auth pubkeys and encryption privkey in config; data holds `Image{0..3}CurVersion/PrevVersion` and `Image{0..3}SHA256`. Offsets `:37-40`. Read is plain flash read of OBK area with SHA256 integrity check (`BOOT/Src/low_level_obkeys.c:298-322`, `:463-486`). Image decryption key unwrapped with SAES + DHUK (`low_level_obkeys.c:161-241`, `OBK_InitDHUK` `:766`). Counters: `plat_*_nv_counter` map to OBK (`BOOT/Src/bl2_nv_services.c:27-79`, `low_level_obkeys.c:651-740`). Jump to app at HDPL3 via RSS (`boot_hal.c:254,300`). Checks `OBK_VerifyHdpl2Config` (`boot_hal.c:591`). | Anti-rollback state and hash refs live in OBK HDPL2 data, not in image slots. Key and counter placement cannot be reproduced without HDPL/OBK hardware services (inferred). |

## A. Boot-log lines: security counter and hash ref

Log strings (all in ST fork, `STMCU/boot/bootutil/src`):
- `"verify counter  %d %x %x"` (image index, image counter, stored counter): `image_validate.c:621`. Stored value comes from `boot_nv_security_counter_get` (`:617`).
- `"verify sig key id %d"`: `image_validate.c:567`.
- `"checking public key"`: `image_ec256.c:174`. `"verifying signature hlen"`: `image_ec256.c:192`.
- `"signature OK/KO"`: `image_validate.c:569-570`.
- `"hash ref OK"`: `image_validate.c:529`.

Security counter:
- Image counter: imgtool "auto" is `(major<<24)+(minor<<16)+revision` (`STMCU/scripts/imgtool/image.py:505-509`). Version 1.0.0 gives `0x1000000`, which matches the log value. Counter is a protected TLV `IMAGE_TLV_SEC_CNT` (`image_validate.c:596-640`).
- Stored counter: OBK HDPL2 data `Image0CurVersion` (counter id `PLAT_NV_COUNTER_3` + image id, `STMCU/bl2/src/security_cnt.c` defines `BOOT_NV_COUNTER_0 = PLAT_NV_COUNTER_3`; `BOOT/Src/low_level_obkeys.c:659-668`). Backend is `bl2_nv_services.c:67-79`, so "NV counter impl" = OBK flash, not an RTC/backup register.
- Check: image counter lower than stored is rejected (`image_validate.c:625-631`). Equal passes.
- Update: `boot_update_security_counter()` (`STMCU/boot/bootutil/src/loader.c:772-796`) calls `plat_set_nv_counter`. It only raises (`bl2_nv_services.c:45-58`: never decrement; writes `Prev=Cur; Cur=new` in `low_level_obkeys.c:710-731`). Called after install (`loader.c:1059,1181`) and, for overwrite-only builds, after primary-slot validation (`loader.c:2200-2208`, guarded by `MCUBOOT_HW_ROLLBACK_PROT && MCUBOOT_OVERWRITE_ONLY`).
- Explains the log: 1st boot stored 0 (blank OBK data), after the 1st boot the update at `loader.c:2200` writes `0x1000000`, so the later boot prints `1000000 1000000`.

Hash ref:
- What: SHA-256 of the image computed by `bootutil_img_hash` (`image_validate.c:416`) on every boot, compared against a reference (`:526`). Match logs "hash ref OK" and `continue`s past the signature TLV (`:529-555`), so no ECDSA verify.
- Where stored: RAM array `ImageValidHashRef` (`BOOT/Src/boot_hal.c:77`), persisted in OBK HDPL2 data `Image0SHA256` (`boot_hal.c:333-368` store, `:374-400` load). Loaded early in boot (`boot_hal.c:602-607`).
- When written: after a successful full signature verification, `boot_hash_ref_set` queues the hash (`image_validate.c:581-590`; `boot_hal.c:409-430` sets `ImageValidHashUpdate`). The flash write happens in `boot_platform_quit()` just before the jump (`boot_hal.c:256-266`).
- When used: only when `ImageValidEnable == 1`, which `loader.c:2130` sets around the primary-slot validation and `:2139` clears. The secondary slot is always signature-verified (inferred from the flag scope).
- Signature skipped on later boots: yes, for the primary image when its hash equals the OBK reference. Matches the NRST log.
- Security argument ST uses: the code comment says "reduce boot time (signature check bypass)" (`BOOT/Inc/flash_layout.h:53`) and "if matching, the signature verification can be bypassed" (`image_validate.c:513-517`). Inferred argument: the reference was only written after a full signature check, the hash is recomputed over the whole image each boot, and the reference sits in OBK HDPL2 data that only HDPL2 code can write (OBK read function itself has only a SHA256 checksum, not a MAC: `low_level_obkeys.c:463-486`, so the real protection is the hardware OBK access level; hardware not verifiable from source). Fault-injection hardening: with `MCUBOOT_DOUBLE_SIGN_VERIF` the match is re-checked via `ImageValidStatus[]` before the jump (`image_validate.c:532-546`, `boot_hal.c:228-251`). In our build `MCUBOOT_DOUBLE_SIGN_VERIF` is defined (`scripts/build_stirot_oemurot.sh:125`).
- Consequence for M7: a tamper test that changes the primary image after the first boot is caught by the hash mismatch, then falls back to a full signature verify. Tampering the OBK reference needs HDPL2 access. (inferred)

## B. Image magic 0x73AB1024 vs 0x96F3B83D

- `0x96F3B83D` is `IMAGE_MAGIC` in MCUboot (`STMCU/boot/bootutil/include/bootutil/image.h:42`), imgtool default `-ma` (`STMCU/scripts/imgtool/main.py:575-576`). The app image uses it.
- `0x73AB1024` is the STiRoT image magic: imgtool list `STiROT=[0x73ab1024]` (`STMCU/scripts/imgtool/image.py:98`), set by the TPC xml param "Image magic" `-ma 0x73ab1024` (`CUBE/Projects/STM32H573I-DK/ROT_Provisioning/STiROT_OEMuROT/Images/STiRoT_Code_Image.xml:27-31`, same in `STiRoT_Code_Init_Image.xml:34-38`). The OEMuRoT image is the "STiRoT code image", so it is signed in STiRoT format.
- imgtool changes behavior for this magic: forces SHORT_OVERWRITE (`image.py:478-479`), pads the image to full size (`image.py:1189-1192`), `--no-pad-tlv` option text (`main.py:577-578`).
- Why different: the image is parsed by STiRoT (ST ROM, source not available here), not by MCUboot. OEMuRoT's own MCUboot loader only accepts `IMAGE_MAGIC` (`loader.c:548`), so the two formats cannot be mixed. (Why ST chose a separate magic: inferred, no ST doc in the tree.)
- Evidence from build: `evidence/m6-stirot-oemurot/2026-10-05-m6-e61-build.txt` signed image table (magic column).

## C. MSP 0x30045778 vs `_estack` 0x30050000

- Vector table word 0 at `0x0C000400` = `0x30045778` (read from the ELF, `.text` at 0x0c000400).
- Startup puts `__INITIAL_SP` in slot 0 (`BOOT/Src/startup_stm32h5xx.c:247`). CMSIS maps `__INITIAL_SP` to `__StackTop` for GCC (`CUBE/Drivers/CMSIS/Core/Include/cmsis_gcc.h:169-170`; armclang: `Image$$ARM_LIB_STACK$$ZI$$Limit`, `cmsis_armclang.h:120`).
- Linker script: `.msp_stack` is placed after `.bss` with `BL2_MSP_STACK_SIZE = 0x2000` (`BOOT/STM32CubeIDE/stm32h5xx_bl2.ld:37,172-177`; `BOOT/Inc/region_defs.h:22`). `Image$$ARM_LIB_STACK$$ZI$$Limit`, `__StackTop` and `__stack` are its end (`ld:178,192-193`).
- ELF sections: `.msp_stack` 0x30043778 + 0x2000 = 0x30045778 = `__StackTop`; next `._user_heap_stack` 0x600 bytes at 0x30045778 (readelf, this session; `ld:181-190`).
- `_estack = ORIGIN(RAM)+LENGTH(RAM)` = end of SRAM2 `0x30040000 + 0x10000` (`ld:31`; `region_defs.h:29,40,122-123`). It is never used for the vector table, so it is a leftover generic symbol.
- Result: the BL2 stack is not at the top of RAM, it is a fixed 8 KB block after `.bss`, and `__set_MSPLIM(&__STACK_LIMIT)` guards it (`startup_stm32h5xx.c:581`). This also matches the M2 STKOF fix (8 KB stack, `evidence/m2-mcuboot-port/README.md` Failures 1).

## D. OB checks in `low_level_security.c` (`LL_SECU_CheckStaticProtections`, `BOOT/Src/low_level_security.c:690-880`)

All failures call `Error_Handler()`. Active in our build = `OEMUROT_ENABLE` set, `OEMIROT_DEV_MODE` set.

| # | Check | Line | Macro / expected | Active |
|---|---|---|---|---|
| 1 | TZEN == enabled | 709 | none (always) | yes |
| 2 | SWAP_BANK == 0 | 717 | none | yes |
| 3 | BOOT_UBE == ST iRoT 0xC3 under OEMUROT_ENABLE (0xB4 OEM iRoT otherwise) | 724-728 | `OB_UBE_ST_IROT` (`stm32h5xx_hal_flash_ex.h:465`) | yes, 0xC3 |
| 4 | Secure boot address == `BL2_BOOT_VTOR_ADDR` | 735-742 | `!OEMUROT_ENABLE` | no (compiled out) |
| 5 | Bank 1 secure watermark (SECWM) start 0, end from S partition | 745-757 | `S_IMAGE_PRIMARY_PARTITION_OFFSET + FLASH_S_PARTITION_SIZE` | yes |
| 6 | Bank 2 secure watermark / fully unsecure | 760-780 | same | yes |
| 7 | WRP on bank 1 covering BL2 | 784-803 | `OEMIROT_WRP_PROTECT_ENABLE` (`boot_hal_cfg.h:44`, only if `!OEMUROT_ENABLE`) | no |
| 8 | HDP start 0 / end = `FLASH_BL2_HDP_END` | 805-824 | `OEMIROT_HDP_PROTECT_ENABLE` (`boot_hal_cfg.h:45`) | no |
| 9 | SRAM2 erase at reset | 826-833 | `OEMIROT_SECURE_USER_SRAM2_ERASE_AT_RESET` (`boot_hal_cfg.h:47`) | yes |
| 10 | SRAM2 ECC | 835-842 | `OEMIROT_SECURE_USER_SRAM2_ECC` (`boot_hal_cfg.h:48`) | yes |
| 11 | BootLock == 0xB4 | 844-852 | `OEMIROT_OB_BOOT_LOCK = OB_BOOT_LOCK_ENABLE` (`boot_hal_cfg.h:61`; `stm32h5xx_hal_flash_ex.h:793` = 0xB4) | yes |
| 12 | Product state >= selected | 854-880 | `OEMIROT_OB_PRODUCT_STATE_VALUE` (`boot_hal_cfg.h:50-54`): OPEN with DEV_MODE, CLOSED without. Order list `low_level_security.c:353-360` (OPEN, PROVISIONING, IROT_PROVISIONED, TZ_CLOSED, CLOSED, LOCKED) | yes, minimum OPEN, so any state passes |

Notes:
- Product state values: OPEN 0xED, PROVISIONING 0x17, IROT_PROVISIONED 0x2E, TZ_CLOSED 0xC6, CLOSED 0x72, LOCKED 0x5C (`stm32h5xx_hal_flash_ex.h:410-415`).
- Without DEV_MODE a production build would need CLOSED or higher (inferred from the list order and `:868-880`).
- The checks run in `boot_hal.c:564` and again at `:587` (double check), after `LL_SECU_ApplyRunTimeProtections`.

## E. `OEMIROT_DEV_MODE` and `OEMIROT_ERROR_HANDLER_STOP_EXEC`

`OEMIROT_DEV_MODE` is defined by the build (`scripts/build_stirot_oemurot.sh:125`, and in ST's `.cproject` Debug config). Effects (all `#ifdef OEMIROT_DEV_MODE`):
- Product state minimum becomes OPEN instead of CLOSED (`BOOT/Inc/boot_hal_cfg.h:50-54`).
- Defines `OEMIROT_ERROR_HANDLER_STOP_EXEC` (`boot_hal_cfg.h:63-65`).
- Log level INFO instead of OFF in `low_level_security.c:27-31`, `low_level_flash.c:24-28`, and `MCUBOOT_HAVE_LOGGING` (`BOOT/Inc/mcuboot_config/mcuboot_config.h:78-80`). Without DEV_MODE there is no UART log at all, so the boot log in the M6 evidence depends on it.
- Tamper event handling: startup clears the event, deactivates tampers and records `TamperEventCleared` instead of `NVIC_SystemReset()`, and does not enable TAMP IRQ (`BOOT/Src/startup_stm32h5xx.c:500-507,530-562`). `active_tamper()` logs and calls `Error_Handler()` if a tamper was active at boot (`low_level_security.c:1769-1780`), and deactivates tamper when it is configured off (`:1835,1862`). With `OEMIROT_TAMPER_ENABLE = INTERNAL_TAMPER_ONLY` (`boot_hal_cfg.h:59`), internal tamper stays on.
- Assert prints a message (ARMCC only, `boot_hal.c:674-676`).

`OEMIROT_ERROR_HANDLER_STOP_EXEC` (`boot_hal.c:635-663`): `Error_Handler()` becomes `while(1);` (halt, so a failed check hangs with no reset). Without it, `Error_Handler()` calls `NVIC_SystemReset()` (reset loop). Placed in section `.BL2_Error_Code` either way.

Impact on tests (inferred):
- With DEV_MODE, every failed check (OB mismatch, bad double-verify, hash-ref store error) halts silently apart from the log line. M7 tamper tests will show a hung OEMuRoT, not a reset loop.
- A DEV_MODE build is not a security configuration: product state OPEN is accepted, and tamper reset is disabled. The final M6/M7 claim should say it runs the DEV_MODE build.

## Not determined

- The STiRoT side of magic `0x73AB1024` (ROM code, no source in tree).
- Why a 1st boot may print "verify counter" and a signature check once vs twice (secondary then primary validation): not measured against the logs.
- Hardware access control of OBK HDPL2 data (reference manual statement, not in these sources).
