#!/usr/bin/env python3
import json
import sys
from pathlib import Path


def load_statuses(root: Path):
    statuses = []
    for status_file in sorted(root.rglob("status.json")):
        try:
            data = json.loads(status_file.read_text(encoding="utf-8"))
        except json.JSONDecodeError:
            continue
        data["_artifact_dir"] = str(status_file.parent.relative_to(root))
        statuses.append(data)
    return statuses


def normalize_status(value: str) -> str:
    value = (value or "").lower()
    if value in {"pass", "passed", "success", "succeeded"}:
        return "pass"
    if value in {"skip", "skipped"}:
        return "skipped"
    return "fail"


def render_table(statuses):
    lines = [
        "## Quality Report",
        "",
        "| Check | Status | Findings | Summary | Artifact |",
        "| --- | --- | ---: | --- | --- |",
    ]
    for item in statuses:
        status = normalize_status(item.get("status", "fail"))
        icon = {"pass": "PASS", "fail": "FAIL", "skipped": "SKIP"}[status]
        findings = item.get("findings", "")
        summary = item.get("summary", "")
        artifact = item.get("_artifact_dir", "-")
        lines.append(
            f"| {item.get('check', 'Unknown')} | {icon} | {findings} | {summary} | `{artifact}` |"
        )
    return lines


def render_findings(statuses):
    lines = ["", "### Key Findings", ""]
    emitted = False
    for item in statuses:
        details = item.get("details") or []
        if not details:
            continue
        emitted = True
        lines.append(f"**{item.get('check', 'Unknown')}**")
        lines.append("")
        for detail in details[:5]:
            lines.append(f"- {detail}")
        lines.append("")
    if not emitted:
        lines.append("- No additional findings were summarized.")
        lines.append("")
    return lines


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: render_summary.py <artifacts-dir>", file=sys.stderr)
        return 2

    root = Path(sys.argv[1])
    statuses = load_statuses(root)
    lines = render_table(statuses) + render_findings(statuses)
    print("\n".join(lines))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
