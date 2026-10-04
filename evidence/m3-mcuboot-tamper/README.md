# M3 MCUboot signature and tamper tests

Date: 2026-10-04
Board: STM32H573I-DK, PRODUCT_STATE Open, TZEN 0xC3, BOOT_UBE 0xB4
Firmware: bl2 and `bare_slot` from commit `230c718` (M2), Debug build
Key: this project's ECDSA P-256 key (public DER sha256 `1a1db9cf…3fea`). Case 4 uses a throwaway key.

## Method

Each case writes images with `STM32_Programmer_CLI -d … -v -hardRst`, waits, and runs `scripts/board_observe.sh`.
The script reads VTOR, halts the core briefly for PC and MSP, resolves PC against `bl2.elf` or `bare_slot.elf` by VTOR, and reads the first 32 bytes of both slots.
VTOR 0x08018400 means bl2 handed off to the application. VTOR 0x08000000 means control stayed in bl2.

Tampering uses `scripts/tamper_image.py`, which XORs one byte with 0x01. The byte is at image offset 0x1400, inside the application code (payload offset 0x1000). The image header, magic, size, and version stay valid, so a rejection comes from the hash or signature check, not from header parsing.

Slot header words: magic, load_addr, hdr_size, img_size, flags, version (`0x00000001` is 1.0.0, `0x00000101` is 1.1.0), build_num.

## Results

| Case | Slot contents before reset | VTOR | PC | Slots after | Verdict | Files |
|---|---|---|---|---|---|---|
| 1 Valid | primary 1.0.0 signed | 0x08018400 | app `HAL_Delay` | primary 1.0.0, secondary empty | boots | `*-case1-*` |
| 2 Primary payload tamper | primary 1.0.0, byte 0x1400 0x42→0x43 | 0x08000000 | bl2 `HAL_Delay` (5 samples) | primary header unchanged, readback shows 0x43 at 0x08019400 | rejected | `*-case2-*` |
| 3b Secondary payload tamper | primary 1.0.0 valid, secondary 1.1.0 padded with byte 0x1400 changed | 0x08018400 | app | primary 1.0.0, secondary erased | update rejected, old image boots | `*-case3b-*` |
| 3a Valid update (control for 3b) | secondary 1.1.0 padded, valid | 0x08018400 | app | primary 1.1.0, secondary erased | update installed | `*-case3a-*` |
| 4 Unknown key | primary 1.0.0 signed with another P-256 key | 0x08000000 | bl2 `HAL_Delay` | primary header unchanged | rejected | `*-case4-*` |
| 5 Restore | primary 1.0.0 signed | 0x08018400 | app `HAL_GetTick` | primary 1.0.0 | boots | `*-case5-*` |

Image hashes and the changed offsets are in the `*-tamper.txt` and `*-image.txt` files. `imgtool verify` with the project key reports "No signature found for the given key" for the case 4 image.

## What this shows

- bl2 runs the application only when the primary slot image matches its SHA-256 TLV and carries an ECDSA P-256 signature from the embedded key (cases 1, 2, 4, 5).
- The same check runs before an update is installed. A valid update replaces the primary image, and an altered one is erased without touching the primary image (cases 3a, 3b).

## Limits

- No serial log was captured (picocom in the user's pane had no log file). The MCUboot messages that would name the failing check are not recorded yet, so the evidence does not distinguish a hash mismatch from a signature failure in case 2.
- The bl2 failure path is identified by VTOR and by PC samples inside bl2's delay loop, not by a bl2 status variable.
