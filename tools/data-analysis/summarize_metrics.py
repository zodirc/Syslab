#!/usr/bin/env python3
"""Collect SYSLAB_METRIC lines from experiment stdout."""

import collections
import sys


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: summarize_metrics.py STDOUT", file=sys.stderr)
        return 1
    values = collections.OrderedDict()
    with open(sys.argv[1], "r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            if not line.startswith("SYSLAB_METRIC "):
                continue
            payload = line[len("SYSLAB_METRIC "):].strip()
            name, separator, raw = payload.partition("=")
            if separator != "=" or not name:
                print("malformed metric: " + line.rstrip(), file=sys.stderr)
                return 1
            values.setdefault(name, []).append(int(raw))
    for name, samples in values.items():
        ordered = sorted(samples)
        middle = ordered[(len(ordered) - 1) // 2]
        print("{0} n={1} min={2} median={3} max={4}".format(
            name, len(ordered), ordered[0], middle, ordered[-1]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
