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
