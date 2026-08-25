#!/usr/bin/env python3
"""Block source patterns that can trigger avoidable security detections."""

from __future__ import annotations

import argparse
import math
import re
import subprocess
import sys
from collections import Counter
from pathlib import Path
from urllib.parse import parse_qsl, unquote, urlsplit


URL_PATTERN = re.compile(r"https?://[^\s<>\"']+", re.IGNORECASE)
POWERSHELL_POLICY_PATTERN = re.compile(
    r"(?i)(?:-|/)executionpolicy(?:\s+|\s*=\s*)(?:\"|')?" + "by" + "pass\b"
)
RISKY_BUILD_SWITCHES = {
    "-fno-" + "stack-protector": "disabling compiler stack protection is forbidden",
    "--strip-" + "all": "stripping all PE metadata is forbidden",
    "--no-insert-" + "timestamp": "suppressing the PE timestamp is forbidden",
}
OPAQUE_TOKEN_PATTERN = re.compile(r"^[A-Za-z0-9_+/%=-]+$")
SENSITIVE_QUERY_KEYS = {
    "access_token",
    "apikey",
    "api_key",
    "auth",
    "authkey",
    "busi_data",
    "credential",
    "data",
    "key",
    "secret",
    "sig",
    "signature",
    "token",
}
SKIPPED_PREFIXES = (".git/", "build/", "build-")


def shannon_entropy(value: str) -> float:
    if not value:
        return 0.0
    counts = Counter(value)
    length = len(value)
    return -sum((count / length) * math.log2(count / length) for count in counts.values())


def looks_generated(value: str) -> bool:
    return any(marker in value for marker in ("${{", "$env:", "<version>", "$version"))


def is_high_entropy_token(value: str, minimum_length: int = 48) -> bool:
    decoded = unquote(value).strip()
    if len(decoded) < minimum_length or looks_generated(decoded):
        return False
    if not OPAQUE_TOKEN_PATTERN.fullmatch(decoded):
        return False
    classes = sum(
        bool(re.search(pattern, decoded))
        for pattern in (r"[a-z]", r"[A-Z]", r"[0-9]", r"[_+/%=-]")
    )
    return classes >= 3 and shannon_entropy(decoded) >= 4.0


def suspicious_url_reason(url: str) -> str | None:
    cleaned = url.rstrip(".,;:!?)]}")
    try:
        parsed = urlsplit(cleaned)
    except ValueError:
        return None

    for key, value in parse_qsl(parsed.query, keep_blank_values=True):
        if key.lower() in SENSITIVE_QUERY_KEYS and is_high_entropy_token(
            value, minimum_length=24
        ):
            return f"high-entropy value in sensitive URL parameter '{key}'"
        if is_high_entropy_token(value):
            return f"high-entropy URL parameter '{key}'"

    for segment in parsed.path.split("/"):
        if is_high_entropy_token(segment):
            return "high-entropy URL path token"
    return None


def tracked_files(root: Path) -> list[Path]:
    result = subprocess.run(
        ["git", "ls-files", "-z"],
        cwd=root,
        check=True,
        capture_output=True,
    )
    return [root / path for path in result.stdout.decode("utf-8").split("\0") if path]


def is_scannable(path: Path, root: Path) -> bool:
    relative = path.relative_to(root).as_posix()
    if relative.startswith(SKIPPED_PREFIXES) or not path.is_file():
        return False
    try:
        sample = path.read_bytes()[:8192]
    except OSError:
        return False
    return b"\0" not in sample


def annotation_value(value: str) -> str:
    return (
        value.replace("%", "%25")
        .replace("\r", "%0D")
        .replace("\n", "%0A")
        .replace(":", "%3A")
        .replace(",", "%2C")
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path.cwd())
    args = parser.parse_args()
    root = args.root.resolve()

    findings: list[tuple[Path, int, str]] = []
    files = [path for path in tracked_files(root) if is_scannable(path, root)]
    for path in files:
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError as exc:
            print(f"Unable to read {path}: {exc}", file=sys.stderr)
            return 2

        for line_number, line in enumerate(text.splitlines(), start=1):
            if POWERSHELL_POLICY_PATTERN.search(line):
                findings.append(
                    (path, line_number, "PowerShell execution-policy bypass is forbidden")
                )
            for switch, reason in RISKY_BUILD_SWITCHES.items():
                if switch in line:
                    findings.append((path, line_number, reason))
            for match in URL_PATTERN.finditer(line):
                reason = suspicious_url_reason(match.group(0))
                if reason:
                    findings.append((path, line_number, reason))

    if findings:
        print("Source security policy failed:", file=sys.stderr)
        for path, line_number, reason in findings:
            relative = path.relative_to(root).as_posix()
            print(f"- {relative}:{line_number}: {reason}", file=sys.stderr)
            print(
                f"::error file={annotation_value(relative)},line={line_number},"
                f"title=Source security policy::{annotation_value(reason)}"
            )
        return 1

    print(f"Source security policy passed for {len(files)} tracked text files.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
