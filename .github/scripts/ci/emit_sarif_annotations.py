#!/usr/bin/env python3
import json
import sys
from pathlib import Path


def escape(value: str) -> str:
    return value.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")


def extract_message(result: dict) -> str:
    message = result.get("message") or {}
    text = message.get("text") or message.get("markdown") or ""
    return " ".join(str(text).split())


def extract_location(result: dict) -> tuple[str, str]:
    for location in result.get("locations") or []:
        physical = location.get("physicalLocation") or {}
        artifact = physical.get("artifactLocation") or {}
        uri = (artifact.get("uri") or "").replace("\\", "/")
        region = physical.get("region") or {}
        line = str(region.get("startLine") or "1")
        if uri:
            return uri, line
    return "", "1"


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: emit_sarif_annotations.py <sarif-file> <check-name>", file=sys.stderr)
        return 2

    sarif_path = Path(sys.argv[1])
    check_name = sys.argv[2]

    if not sarif_path.exists():
        print("0")
        return 0

    try:
        data = json.loads(sarif_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        print(f"::error::{escape(check_name + ' SARIF report could not be parsed')}")
        print("1")
        return 0

    results = []
    for run in data.get("runs") or []:
        results.extend(run.get("results") or [])

    for result in results[:50]:
        level = (result.get("level") or "warning").lower()
        annotation_level = "error" if level == "error" else "warning"
        message = extract_message(result) or f"{check_name} finding"
        file_path, line = extract_location(result)
        full_message = f"{check_name}: {message}"
        if file_path:
            print(f"::{annotation_level} file={file_path},line={line}::{escape(full_message)}")
        else:
            print(f"::{annotation_level}::{escape(full_message)}")

    print(len(results))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
