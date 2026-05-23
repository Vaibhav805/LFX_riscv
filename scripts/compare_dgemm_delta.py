#!/usr/bin/env python3
"""Generate scalar-vs-linked DGEMM relative-error delta tables."""

from __future__ import annotations

import json
import sys
from pathlib import Path


def load_results(path: Path) -> dict:
    with path.open() as f:
        data = json.load(f)
    return {row["case"]: row for row in data["results"]}


def main() -> int:
    scalar_path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("docs/dgemm_scalar_results.json")
    rvv_path = Path(sys.argv[2]) if len(sys.argv) > 2 else Path("docs/dgemm_rvv_results.json")
    out_path = Path(sys.argv[3]) if len(sys.argv) > 3 else Path("docs/dgemm_delta_table.md")

    scalar = load_results(scalar_path)
    rvv = load_results(rvv_path)
    cases = [case for case in scalar if case in rvv]

    lines = [
        "# DGEMM Scalar vs Linked OpenBLAS Delta",
        "",
        "| Case | Scalar err | Linked err | Delta | Both |",
        "|---|---:|---:|---:|---|",
    ]
    for case in cases:
        s = scalar[case]["relative_error"]
        r = rvv[case]["relative_error"]
        both = "PASS" if scalar[case]["status"] == "PASS" and rvv[case]["status"] == "PASS" else "FAIL"
        lines.append(f"| {case} | {s:.4e} | {r:.4e} | {abs(s - r):.4e} | {both} |")

    max_delta = max((abs(scalar[c]["relative_error"] - rvv[c]["relative_error"]) for c in cases), default=0.0)
    lines.extend([
        "",
        f"Compared cases: {len(cases)}",
        f"Maximum absolute relative-error delta: {max_delta:.4e}",
        "",
        "Note: in the current workspace both linked archives report `RISCV64_GENERIC` and no matching RVV opcodes, so this table is a no-V baseline comparison until a true RVV archive is installed.",
    ])
    out_path.write_text("\n".join(lines) + "\n")
    print(f"wrote {out_path} ({len(cases)} cases, max delta {max_delta:.4e})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
