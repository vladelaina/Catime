#!/usr/bin/env python3
import argparse
import os
import re
from pathlib import Path


MSVC_LINE_RE = re.compile(
    r"^(?P<file>[A-Za-z]:[\\/].*?)\((?P<line>\d+)(?:,(?P<col>\d+))?\): "
    r"(?P<severity>warning|error) (?P<code>[A-Za-z]+\d+): (?P<message>.*?)(?: \[.*\])?$"
)

ERROR_PATTERNS = (
    " error ",
    ": error ",
    "fatal error",
    "error msb",
    "lnk",
    "cannot open file",
    "undefined",
)

WARNING_PATTERNS = (
    ": warning ",
    " warning c",
)


def escape(value: str) -> str:
    return value.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")


def normalize_path(file_path: str, workspace: Path | None) -> str:
    normalized = file_path.replace("\\", "/")
    if workspace is not None:
        workspace_str = str(workspace).replace("\\", "/").rstrip("/")
        if normalized.lower().startswith(workspace_str.lower() + "/"):
            normalized = normalized[len(workspace_str) + 1 :]
    return normalized


def should_ignore(normalized_path: str, raw_line: str) -> bool:
    if "libs/miniaudio/" in normalized_path or "libs\\miniaudio\\" in raw_line:
        return True
    return False


def line_matches(kind: str, raw_line: str, parsed_severity: str | None) -> bool:
    lowered = raw_line.lower()
    if kind == "warnings":
        if parsed_severity == "warning":
            return True
        return any(pattern in lowered for pattern in WARNING_PATTERNS)
    if parsed_severity == "error":
        return True
    return any(pattern in lowered for pattern in ERROR_PATTERNS)


def emit_annotation(level: str, file_path: str, line: str, col: str | None, message: str) -> None:
    if file_path:
        location = f" file={file_path},line={line}"
        if col:
            location += f",col={col}"
        print(f"::{level}{location}::{escape(message)}")
    else:
        print(f"::{level}::{escape(message)}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log_path")
    parser.add_argument("kind", choices=("errors", "warnings"))
    parser.add_argument("--max", type=int, default=20)
    args = parser.parse_args()

    log_path = Path(args.log_path)
    if not log_path.exists():
        print("0")
        return 0

    workspace_env = os.environ.get("GITHUB_WORKSPACE")
    workspace = Path(workspace_env) if workspace_env else None
    lines = log_path.read_text(encoding="utf-8", errors="replace").splitlines()

    emitted = 0
    seen: set[str] = set()

    for raw_line in lines:
        raw_line = raw_line.strip()
        if not raw_line:
            continue

        match = MSVC_LINE_RE.match(raw_line)
        normalized_path = ""
        parsed_severity = None
        message = raw_line
        line = "1"
        col = None

        if match:
            normalized_path = normalize_path(match.group("file"), workspace)
            parsed_severity = match.group("severity").lower()
            line = match.group("line")
            col = match.group("col")
            message = f"{match.group('severity')} {match.group('code')}: {match.group('message').strip()}"

        if should_ignore(normalized_path, raw_line):
            continue
        if not line_matches(args.kind, raw_line, parsed_severity):
            continue

        dedupe_key = f"{normalized_path}|{line}|{col or ''}|{message}"
        if dedupe_key in seen:
            continue
        seen.add(dedupe_key)

        level = "notice" if args.kind == "warnings" else "error"
        if normalized_path:
            location = f"{normalized_path}:{line}"
            if col:
                location += f":{col}"
            print(f"COPYABLE {level.upper()} {location} {message}")
        else:
            print(f"COPYABLE {level.upper()} {message}")
        emit_annotation(level, normalized_path, line, col, message)

        emitted += 1
        if emitted >= args.max:
            break

    print(emitted)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
