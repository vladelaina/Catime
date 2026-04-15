#!/usr/bin/env python3
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


def escape(value: str) -> str:
    return value.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: emit_cppcheck_annotations.py <cppcheck-xml>", file=sys.stderr)
        return 2

    xml_path = Path(sys.argv[1])
    if not xml_path.exists():
        print("0")
        return 0

    root = ET.parse(xml_path).getroot()
    errors = root.findall(".//error")

    for item in errors[:50]:
        severity = item.attrib.get("severity", "warning").lower()
        message = item.attrib.get("msg", "").strip() or item.attrib.get("id", "Cppcheck finding")
        location = item.find("location")
        level = "error" if severity in {"error"} else "warning"
        if location is not None:
            file_path = location.attrib.get("file", "")
            line = location.attrib.get("line", "1")
            print(f"::{level} file={file_path},line={line}::{escape(message)}")
        else:
            print(f"::{level}::{escape(message)}")

    print(len(errors))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
