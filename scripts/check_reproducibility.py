#!/usr/bin/env python3
"""Run a validator repeatedly and hash relative-error vectors."""

from __future__ import annotations

import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path


def main() -> int:
    binary = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("tests/dgemm_rvv")
    runs = int(sys.argv[2]) if len(sys.argv) > 2 else 10
    output = Path(sys.argv[3]) if len(sys.argv) > 3 else Path("docs/reproducibility_hashes.json")

    records = []
    with tempfile.TemporaryDirectory() as td:
        for i in range(1, runs + 1):
            result_path = Path(td) / f"run_{i}.json"
            subprocess.run(
                ["qemu-riscv64-static", "-L", "/usr/riscv64-linux-gnu", str(binary), str(result_path)],
                check=True,
            )
            data = json.loads(result_path.read_text())
            vals = [row["relative_error"] for row in data["results"]]
            digest = hashlib.md5(str(vals).encode()).hexdigest()
            records.append({"run": i, "hash": digest})
            print(f"Run {i}: hash={digest}")

    unique = sorted({row["hash"] for row in records})
    output.write_text(json.dumps({
        "binary": str(binary),
        "runs": runs,
        "deterministic": len(unique) == 1,
        "unique_hashes": unique,
        "records": records,
    }, indent=2) + "\n")
    print(f"deterministic={len(unique) == 1}, wrote {output}")
    return 0 if len(unique) == 1 else 1


if __name__ == "__main__":
    raise SystemExit(main())
