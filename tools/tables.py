#!/usr/bin/env python3
"""Write src/tables.h: the curves the engine needs, worked out here because the chip has no floats.

python3 tools/tables.py
"""

import math
from pathlib import Path

FS = 44100
OUT = Path(__file__).resolve().parents[1] / "src" / "tables.h"


def table(name, ctype, values, per_line=8):
  rows = [", ".join(str(v) for v in values[i : i + per_line]) for i in range(0, len(values), per_line)]
  return f"static const {ctype} {name}[{len(values)}] = {{\n  " + ",\n  ".join(rows) + ",\n};\n"


def main():
  semi = [round(2 ** (i / 12) * 65536) for i in range(12)]
  # Felucca's envelope times, 1 ms .. 10 s: a linear step per 32-frame block (Q24), an exponential one (Q16)
  ms = [10000 ** (v / 127) for v in range(128)]
  env_lin = [max(1, int((1 << 24) * 32 / (m * FS / 1000))) for m in ms]
  env_exp = [max(1, min(65535, int(65536 * (1 - math.exp(-32 / (m / 1000 / 4.6 * FS)))))) for m in ms]
  OUT.write_text(
    "/* made by tools/tables.py: do not edit */\n#ifndef TABLES_H\n#define TABLES_H\n#include <stdint.h>\n\n"
    "/* 2^(n/12), Q16 */\n"
    + table("SEMI", "uint32_t", semi, 12)
    + "\n/* Felucca's envelope (src/synths.c, VOICE): attack step a block, Q24; decay / release coefficient, Q16 */\n"
    + table("ENV_LIN", "uint32_t", env_lin)
    + table("ENV_EXP", "uint16_t", env_exp)
    + "\n#endif\n"
  )


if __name__ == "__main__":
  main()
