#!/usr/bin/env python3
"""Print every -ob displ snapshot in the given logs, one row per snapshot, with the fields that ever changed."""

import os
import re
import sys

FIELD = re.compile(r"^\s+([A-Z][A-Z0-9_]+)\s*:\s*(0x[0-9A-Fa-f]+)")
STEP = re.compile(r"^# (\d{4}-\d\d-\d\d \d\d:\d\d)")


def snapshots(path):
    """Yields (step time, fields) per snapshot; a snapshot starts at its PRODUCT_STATE line."""
    when, start, cur = None, None, None
    for line in open(path, errors="replace"):
        m = STEP.match(line)
        if m:
            when = m.group(1)
        m = FIELD.match(line)
        if not m:
            continue
        name, value = m.group(1), "0x" + m.group(2)[2:].upper()
        if name == "PRODUCT_STATE":
            if cur:
                yield start, cur
            start, cur = when, {}
        if cur is not None and name not in cur:
            cur[name] = value
    if cur:
        yield start, cur


def main():
    rows = [(os.path.basename(p), when, f) for p in sys.argv[1:] for when, f in snapshots(p)]
    names = []
    for _, _, f in rows:
        names += [n for n in f if n not in names]
    changed = [n for n in names if len({f.get(n) for _, _, f in rows}) > 1]
    print("fields:", len(names), "snapshots:", len(rows))
    print("unchanged:", " ".join(f"{n}={rows[0][2].get(n)}" for n in names if n not in changed))
    print("| log | step time | " + " | ".join(changed) + " |")
    print("|---|---|" + "---|" * len(changed))
    for log, when, f in rows:
        print(f"| {log} | {when or '-'} | " + " | ".join(f.get(n, "-") for n in changed) + " |")


if __name__ == "__main__":
    main()
