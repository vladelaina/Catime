#!/usr/bin/env python3
import json
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: merge_sarif.py <input-dir> <output-file>", file=sys.stderr)
        return 2

    input_dir = Path(sys.argv[1])
    output_file = Path(sys.argv[2])
    sarif_files = sorted(input_dir.rglob("*.sarif"))

    merged_runs = []
    result_count = 0

    for sarif_file in sarif_files:
        try:
            data = json.loads(sarif_file.read_text(encoding="utf-8"))
        except json.JSONDecodeError:
            continue
        runs = data.get("runs", [])
        for run in runs:
            result_count += len(run.get("results", []))
        merged_runs.extend(runs)

    merged = {
        "$schema": "https://json.schemastore.org/sarif-2.1.0.json",
        "version": "2.1.0",
        "runs": merged_runs,
    }
    output_file.write_text(json.dumps(merged, indent=2), encoding="utf-8")
    print(result_count)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
