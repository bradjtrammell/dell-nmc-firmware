#!/usr/bin/env python3
"""Extract H910P OEM schema-only metadata to JSON without network access.

Never serializes OEM object values or device identifiers. It does not access
an NMC, read its serial port, or modify any captured files.
"""
from __future__ import annotations

import argparse
from collections import Counter
from pathlib import Path
import json
import re
import sys
import xml.etree.ElementTree as ET

MAX_XML = 1_048_576
PATH_RE = re.compile(r"^[A-Za-z][A-Za-z0-9_.\[\]-]{0,160}$")
SAFE_ACCESS = {"RW", "RO", "WO", ""}
SAFE_FORMAT = re.compile(r"^[A-Za-z0-9_.,:/%\[\] ()|+$-]{1,180}$")


class ContractError(ValueError):
    pass


def tag_name(tag: str) -> str:
    return tag.rsplit("}", 1)[-1]


def parse(path: Path, expected_root: str) -> ET.Element:
    if not path.is_file():
        raise ContractError(f"Missing capture: {path.name}")
    if path.stat().st_size > MAX_XML:
        raise ContractError(f"Capture exceeds 1 MiB: {path.name}")
    try:
        root = ET.fromstring(path.read_bytes())
    except ET.ParseError as exc:
        raise ContractError(f"Malformed capture: {path.name}: {exc}") from exc
    if tag_name(root.tag) != expected_root:
        raise ContractError(f"Incorrect XML root for {path.name}")
    return root


def object_key(value: str, description: str) -> str:
    if not PATH_RE.fullmatch(value):
        raise ContractError(f"Unexpected {description}; refuse to include untrusted values")
    return value


def clean_format(value: str) -> str:
    if value == "":
        return ""
    if not SAFE_FORMAT.fullmatch(value):
        raise ContractError("Unexpected OEM type expression; refuse to drop type metadata")
    return value


def build_contract(directory: Path) -> dict:
    product = parse(directory / "product.xml", "PRODUCT_INFO")
    descriptor = parse(directory / "descriptor.xml", "SYSTEM_CONFIG")
    telemetry = parse(directory / "ups.xml", "GET_OBJECT")

    groups = []
    seen_fields: set[str] = set()
    access_totals: Counter = Counter()
    for group in descriptor.iter():
        if tag_name(group.tag) != "GROUP":
            continue
        group_id = object_key(group.get("id", ""), "group ID")
        items = []
        for item in group.iter():
            if tag_name(item.tag) != "ITEM":
                continue
            name = object_key(item.get("object", ""), "configuration object")
            access = item.get("access", "")
            if access not in SAFE_ACCESS:
                raise ContractError(f"Unexpected access mode for {name}")
            if name in seen_fields:
                raise ContractError(f"Duplicate configuration item: {name}")
            seen_fields.add(name)
            access_totals[access or "other"] += 1
            items.append({
                "name": name,
                "access": access or "other",
                "type": clean_format(item.get("type", "")),
                "requiresRestart": item.get("restart") == "TRUE",
                "unit": clean_format(item.get("unit", "")),
            })
        groups.append({"id": group_id, "hiddenInOEM": group.get("hidden") == "TRUE", "items": items})

    telemetry_names: list[str] = []
    telemetry_seen: set[str] = set()
    for obj in telemetry.iter():
        if tag_name(obj.tag) != "OBJECT":
            continue
        name = object_key(obj.get("name", ""), "telemetry object")
        if name in telemetry_seen:
            raise ContractError(f"Duplicate telemetry object: {name}")
        telemetry_seen.add(name)
        telemetry_names.append(name)

    # Prove snapshot name coverage without reading or serializing live values.
    field_names = seen_fields
    observed_names = telemetry_seen
    type_counts = Counter(
        item["type"].split(":", 1)[0]
        for group in groups for item in group["items"]
        if item["type"]
    )
    coverage = {
        "descriptorItemsObserved": len(field_names & observed_names),
        "descriptorItemsMissing": len(field_names - observed_names),
        "additionalObservedObjects": len(observed_names - field_names),
        "observedNamePrefixes": dict(sorted(Counter(name.split(".", 1)[0] for name in observed_names).items())),
    }

    # Capability tag names describe the OEM interface but not any live values,
    # authenticated URLs, identification attributes or secret fields.
    capability_tags = sorted({tag_name(item.tag) for item in product.iter()})
    return {
        "schemaVersion": 1,
        "source": "H910P OEM read-only schema capture",
        "purpose": "Offline v0.6 migration inventory; NOT a native v0.6 firmware API",
        "classification": "metadata only; no live values",
        "counts": {
            "groups": len(groups),
            "configurationItems": len(seen_fields),
            "telemetryObjects": len(telemetry_names),
            "access": dict(sorted(access_totals.items())),
            "typeFamilies": dict(sorted(type_counts.items())),
            "coverage": coverage,
        },
        "capabilityTags": capability_tags,
        "configurationGroups": groups,
        "telemetryObjectNames": telemetry_names,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture_dir", type=Path, help="Existing private H910P discovery directory")
    parser.add_argument("--output", type=Path, required=True, help="Destination sanitized JSON path")
    args = parser.parse_args(argv)
    try:
        contract = build_contract(args.capture_dir)
        target = args.output.resolve()
        inputs = {(args.capture_dir / (name + ".xml")).resolve() for name in ("product", "descriptor", "ups")}
        if target in inputs:
            raise ContractError("Refusing to overwrite a raw capture")
        if target.exists():
            raise ContractError("Refusing to overwrite an existing JSON contract")
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(json.dumps(contract, indent=2, ensure_ascii=True) + "\n", encoding="utf-8")
        target.chmod(0o600)
        print(json.dumps({"output": str(target), "counts": contract["counts"]}, indent=2))
    except (ContractError, OSError) as exc:
        print(f"Capture analysis refused: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())