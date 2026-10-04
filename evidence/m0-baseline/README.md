# M0 baseline (read-only)

Date: 2026-10-04
Board: STM32H573I-DK (CLI reports `Board : STM32H573I-DK`)
Probe: ST-LINK V3, FW V3J17M10, attached to WSL over usbipd
Tool: STM32CubeProgrammer 2.23.0 (STM32CubeCLT 1.22.0)
Key: none used

No write, erase, OB change, or regression was done. All SWD access used hot-plug (`mode=Hotplug`), except DA discovery, which asserts a target reset by design.

## Commands

```sh
CN="-c port=SWD speed=fast ap=1 mode=Hotplug"
STM32_Programmer_CLI $CN                                   # 2026-10-04-m0-connect.log
STM32_Programmer_CLI $CN -ob displ                         # 2026-10-04-m0-ob-displ.log
STM32_Programmer_CLI -c port=SWD debugauth=2               # 2026-10-04-m0-da-discovery.log (SoC ID masked)
STM32_Programmer_CLI $CN -u 0x08000000 0x200000 <backup>   # kept outside the repo
```

## Result

| Item | Value | Source |
|---|---|---|
| Device | ID 0x484, Rev X, STM32H56x/573, Cortex-M33, 2 MB flash | connect log |
| Bootloader / SFSP | BL 0xE4, SFSP v2.5.0 | connect log |
| PRODUCT_STATE | 0xED (Open) | OB log |
| TZEN | 0xC3 (TrustZone disabled) | OB log |
| BOOT_UBE | 0xB4 (OEM-iRoT, user flash) | OB log |
| NSBOOTADD / NSBOOT_LOCK | 0x08000000 / 0xC3 (not locked) | OB log |
| SECBOOTADD / SECBOOT_LOCK | 0x0 / 0x0 (CLI prints "Unknown Value") | OB log |
| WRP | WRPSGn1, WRPSGn2 = 0xFFFFFFFF (no write protection) | OB log |
| HDP | HDP1, HDP2 start > end (no HDP area) | OB log |
| SECWM | start > end on both banks (no secure area; TZEN is off) | OB log |
| SWAP_BANK | 0 | OB log |
| DA lifecycle | `ST_LIFECYCLE_OPEN` | discovery log |
| DA cryptosystems | `ST Password` | discovery log |
| DA provisioning integrity | `0xF5F5F5F5` INVALID (no DA configuration provisioned) | discovery log |
| HDPL1/2/3 status | 0xFFFFFFFF | discovery log |
| Flash content | ST factory demo (TouchGFX, Wi-Fi, `FW Version: V1.1.0`), 242 of 256 8 KB sectors used, reset vector 0x0800A31D | backup readback |
| Flash backup sha256 | `7c1a6ea12bf8ec5319b5e442e16a66b99bf5bdfc0045228902188a6310661805` | `sha256sum` |

The backup image and the unmasked discovery log stay in the repo's `.workspace/` and are not published.

## Not captured yet

- VCP output of the demo after the discovery reset (picocom runs in the user's pane without a log file).
- Board label photo (MB1677 revision).
