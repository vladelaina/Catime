#!/usr/bin/env python3
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


def escape(value: str) -> str:
    return value.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")


def should_ignore(file_path: str, message: str) -> bool:
    normalized = file_path.replace("\\", "/")
    if normalized.startswith("libs/miniaudio/"):
        return True
    if normalized.startswith("libs/stb/"):
        return True
    if normalized == "src/drawing/drawing_image_gdiplus.c" and "DebugEventCallback" in message:
        return True
    if normalized == "src/dialog/dialog_notification_settings.c" and "wFileName" in message and "const array" in message:
        return True
    if normalized == "src/dialog/dialog_notification_audio.c" and ((any(token in message for token in ("selectedFile", "wFileName")) and "const array" in message) or ("Variable 'ext'" in message and "pointer to const" in message)):
        return True
    return False


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

    emitted = 0
    for item in errors:
        severity = item.attrib.get("severity", "warning").lower()
        message = item.attrib.get("msg", "").strip() or item.attrib.get("id", "Cppcheck finding")
        location = item.find("location")
        level = "error" if severity in {"error"} else "warning"
        if location is not None:
            file_path = location.attrib.get("file", "")
            line = location.attrib.get("line", "1")
            if should_ignore(file_path, message):
                continue
            print(f"COPYABLE {level.upper()} {file_path}:{line} {message}")
            print(f"::{level} file={file_path},line={line}::{escape(message)}")
        else:
            print(f"COPYABLE {level.upper()} {message}")
            print(f"::{level}::{escape(message)}")
        emitted += 1
        if emitted >= 50:
            break

    print(emitted)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
