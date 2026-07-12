#!/usr/bin/env python3
"""Require upstream-repository guards on every publishing-capable Actions job."""

from __future__ import annotations

import re
import sys
from pathlib import Path


UPSTREAM_GUARD = "github.repository == 'vladelaina/Catime'"
WORKFLOW_DIRECTORY = Path(__file__).resolve().parents[2] / "workflows"

# Secret access catches current and future publishing integrations by default.
# The explicit markers also cover publishers that only use GITHUB_TOKEN or
# create Store artifacts without a third-party secret in the same job.
SENSITIVE_MARKERS = (
    "${{ secrets.",
    "signpath/github-action-submit-signing-request",
    "softprops/action-gh-release",
    "winget-releaser",
    "choco push",
    "build-store-package.ps1",
    "./.github/workflows/signpath-sign.yml",
    "./.github/workflows/chocolatey.yml",
)


def extract_jobs(text: str) -> list[tuple[str, str]]:
    lines = text.splitlines()
    jobs_start = next(
        (index for index, line in enumerate(lines) if re.fullmatch(r"jobs:\s*", line)),
        None,
    )
    if jobs_start is None:
        return []

    headings: list[tuple[int, str]] = []
    for index in range(jobs_start + 1, len(lines)):
        line = lines[index]
        if line and not line.startswith((" ", "\t", "#")):
            break
        match = re.fullmatch(r"  ([A-Za-z0-9_-]+):\s*", line)
        if match:
            headings.append((index, match.group(1)))

    jobs: list[tuple[str, str]] = []
    for position, (start, name) in enumerate(headings):
        end = headings[position + 1][0] if position + 1 < len(headings) else len(lines)
        jobs.append((name, "\n".join(lines[start:end])))
    return jobs


def main() -> int:
    failures: list[str] = []
    checked_jobs = 0

    workflow_paths = sorted(WORKFLOW_DIRECTORY.glob("*.yml"))
    workflow_paths.extend(sorted(WORKFLOW_DIRECTORY.glob("*.yaml")))

    for path in workflow_paths:
        text = path.read_text(encoding="utf-8")
        for job_name, job_text in extract_jobs(text):
            matched = [marker for marker in SENSITIVE_MARKERS if marker in job_text]
            if not matched:
                continue

            checked_jobs += 1
            if UPSTREAM_GUARD not in job_text:
                marker_list = ", ".join(matched)
                failures.append(
                    f"{path.relative_to(WORKFLOW_DIRECTORY.parent.parent)}: "
                    f"job '{job_name}' lacks upstream guard; matched {marker_list}"
                )

    if failures:
        print("Publishing guard policy failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print(
            f"Required job condition: {UPSTREAM_GUARD}",
            file=sys.stderr,
        )
        return 1

    print(f"Publishing guard policy passed for {checked_jobs} sensitive jobs.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
