#!/usr/bin/env python3
"""Generate the MCUboot bootutil_keys[] table from a public key PEM."""

import subprocess
import sys


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: gen_bootutil_keys.py <public-key.pem> <out.c>")

    pem_path, out_path = sys.argv[1], sys.argv[2]
    der = subprocess.run(
        ["openssl", "pkey", "-pubin", "-in", pem_path, "-outform", "DER"],
        check=True,
        capture_output=True,
    ).stdout

    rows = []
    for i in range(0, len(der), 12):
        rows.append("    " + ", ".join(f"0x{b:02x}" for b in der[i : i + 12]) + ",")

    with open(out_path, "w") as out:
        out.write(f"/* Generated from {pem_path.split('/')[-1]} by gen_bootutil_keys.py. Do not edit. */\n")
        out.write("#include \"bootutil/sign_key.h\"\n\n")
        out.write("static const unsigned char pub_key_der[] = {\n")
        out.write("\n".join(rows) + "\n};\n")
        out.write("static const unsigned int pub_key_der_len = sizeof(pub_key_der);\n\n")
        out.write("const struct bootutil_key bootutil_keys[] = {\n")
        out.write("    {.key = pub_key_der, .len = &pub_key_der_len},\n};\n\n")
        out.write("const int bootutil_key_cnt = 1;\n")


if __name__ == "__main__":
    main()
