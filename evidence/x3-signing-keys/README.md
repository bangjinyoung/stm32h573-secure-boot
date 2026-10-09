# X3: signing and key management

Which key signs which image, with which tool, and which key the verifier trusts. Fingerprints are SHA-256 of the public key in DER (SubjectPublicKeyInfo), computed 2026-10-09 with `openssl pkey -pubin -outform DER | sha256sum` (for the ST key files that are private PEMs, `openssl pkey -pubout -outform DER`). No private key is in this repository.

## Keys

| Key | Owner | Algorithm | Public key fingerprint | Where it is trusted | Used in |
|---|---|---|---|---|---|
| `keys/mcuboot-ecdsa-p256.pub.pem` | this project; private key only in `.workspace/keys/` on the build PC | ECDSA P-256 | `1a1db9cfb3fce15c45662346d8f74bc90e8214eafedf0d7bd1c7d3cce24b3fea` | compiled into the custom MCUboot BL2 (`bootutil_keys[]`, `scripts/gen_bootutil_keys.py`) | M2, M3, M8-M10 app images (bare, S, NS) |
| `STiRoT_Authentication.pem` | ST default key from STM32CubeH5 v1.7.0 (`ROT_Provisioning/STiROT_OEMuROT/Keys`, same bytes as the package) | ECDSA P-256 | `9e3c2ffacd4d6706708b06c8412a90e11573160a9d95137368d494510f403728` | STiRoT, through its OBK configuration (M4, re-provisioned M6) | M4-M5 STiRoT app, M6-M7 OEMuRoT, M8-M10 custom BL2 as the HDPL2 image |
| `STiRoT_Encryption.pem` | ST default (same package) | ECIES P-256 (AES-CTR-128 payload key) | `8241815b16a1c3ff09aa87979f58eb376447f1b7f5eccfe8ab44247c66baf590` (`_pub.pem`) | STiRoT decrypts the download image with the private key in OBK | the `_enc` download images (BL2, OEMuRoT) |
| `OEMuRoT_Authentication_S.pem` | ST default (same package) | ECDSA P-256 | `05b081d953fa2da76219365256a21a457b52001e5ca8296d907b13519c9540d3` | OEMuRoT (M6-M7 only) | M6-M7 full-secure app |
| `OEMuRoT_Authentication_NS.pem` | ST default (same package) | ECDSA P-256 | `3f6c9b372408cc68a8c2cd4e289135766df5782de44d24dc4bdf7e691c2a11fd` | OEMuRoT NS image (not used: the M6 app is full-secure) | none |

## Tools and commands

| Image | Tool | Command (from the scripts) |
|---|---|---|
| MCUboot app images (bare, S, NS) | `scripts/imgtool` (MCUboot v2.4.0 imgtool wrapper) | `imgtool sign --key <private key> --header-size 0x400 --pad-header --align 16 [--overwrite-only] --slot-size <slot> --version <v> [--security-counter <n>]` (`scripts/make_m9_images.sh`, `make_m10_images.sh`, M2 README) |
| Custom BL2 as the STiRoT HDPL2 image | STM32TrustedPackageCreator_CLI (STM32CubeCLT 1.22.0) | `-pb STiRoT_Code_Init_Image.xml` (execution image) and `-pb STiRoT_Code_Image.xml` (encrypted download image) via `scripts/package_bl2_stirot.sh` |
| OEMuRoT and its app (M6-M7) | ST's build and provisioning scripts | `scripts/build_stirot_oemurot.sh`, `scripts/stirot_oemurot_provision.sh` |
| Host check | `scripts/mcuboot_image.py check` | recomputes the hash TLV and checks the signature against a given public key (`m8-custom-chain/2026-10-05-m8-h1-stirot-sig.txt`, `m10-trustzone/2026-10-07-m10-h-host.txt`) |

## Board evidence that the keys are enforced

- Project key: an image signed with another P-256 key is rejected by BL2 (M3 case 4, `m3-mcuboot-tamper/README.md`). Hash tamper of S and NS images is rejected (M10 T1, T3).
- STiRoT key: STiRoT installs and runs the BL2 packaged with the ST default key (E8.1); a tampered download image is not installed (M5 E5.2).

## Limits

- STiRoT and OEMuRoT run with ST's published default keys. Anyone with the CubeH5 package can sign an image STiRoT accepts while the board is Open. Moving to own STiRoT keys was not done in this PoC (the board stays Open, D-31).
- The project private key exists only on one PC (`.workspace/keys/`); it has no backup procedure or HSM.
