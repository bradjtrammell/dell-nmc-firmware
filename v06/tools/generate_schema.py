#!/usr/bin/env python3
"""Compile the sanitized OEM metadata-only contract into a C array.

Only names, validation expressions, units and access classifications are emitted.
No live configuration values, credentials or device identifiers may be included.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import re
import sys

VALID = re.compile(r"^[A-Za-z0-9_$+.,:/%|\[\] ()-]{0,180}$")


def c_string(text: str) -> str:
    if not isinstance(text, str) or not VALID.fullmatch(text):
        raise ValueError(f"Invalid metadata token: {repr(text)[:60]}")
    return json.dumps(text, ensure_ascii=True)


def compile_contract(data: dict) -> str:
    if data.get("schemaVersion") != 1:
        raise ValueError("Unsupported OEM schema version")
    groups = data.get("configurationGroups")
    if not isinstance(groups, list) or len(groups) != 24:
        raise ValueError("Expected 24 OEM configuration groups")
    rows = []
    names = set()
    for group in groups:
        group_name = c_string(group["id"])
        hidden = "1" if group.get("hiddenInOEM") is True else "0"
        for item in group["items"]:
            name = item["name"]
            if name in names:
                raise ValueError("Duplicate OEM field name")
            names.add(name)
            access = item["access"]
            if access not in ("RW", "RO", "WO"):
                raise ValueError("Unsupported OEM access code")
            rows.append(
                "    {" + ", ".join((group_name, c_string(name),
                c_string(item["type"]), c_string(access),
                c_string(item.get("unit", "")),
                "1" if item.get("requiresRestart") is True else "0", hidden)) + "},"
            )
    if len(rows) != 302 or data.get("counts", {}).get("configurationItems") != 302:
        raise ValueError("Expected 302 OEM fields")
    return (
        "/* Generated from sanitized v0.6 OEM schema metadata. DO NOT HAND EDIT.\n"
        " * Not derived from live configuration values. No Dell program code linked.\n"
        " */\n"
        '#include "ou_schema.h"\n\n'
        "const ou_schema_field ou_schema_fields[] = {\n"
        + "\n".join(rows) + "\n};\n"
        "const size_t ou_schema_field_count = "
        "sizeof(ou_schema_fields) / sizeof(ou_schema_fields[0]);\n"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("contract", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    data = json.loads(args.contract.read_text(encoding="utf-8"))
    generated = compile_contract(data)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(generated, encoding="utf-8")
    print(f"Generated {len(data['configurationGroups'])} groups and 302 metadata fields -> {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
