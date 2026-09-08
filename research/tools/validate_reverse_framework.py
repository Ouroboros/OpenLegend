#!/usr/bin/env python3
"""Validate OpenLegend reverse-engineering inventories and closure claims."""

from __future__ import annotations

import csv
import json
import re
import sys
from collections import Counter
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
REPORT_ROOT = ROOT / "research" / "ida" / "reports"
INVENTORY_ROOT = ROOT / "research" / "inventory"

FUNCTION_CATALOG_COLUMNS = (
    "binary",
    "address",
    "end",
    "size",
    "name",
    "instruction_count",
    "basic_blocks",
    "direct_callers",
    "direct_callees",
    "code_origin",
)
OWNERSHIP_COLUMNS = (
    "binary",
    "address",
    "name",
    "code_origin",
    "module_candidate",
    "candidate_confidence",
    "assignment_basis",
    "direct_callers",
    "direct_callees",
    "follow_up_module",
    "review_status",
)
CLOSURE_COLUMNS = (
    "audit_order",
    "address",
    "name",
    "research_name",
    "function_end",
    "size",
    "closure_status",
    "target_owner",
    "implementation_mapping",
    "evidence",
    "verification",
    "final_review",
    "remaining",
)
MODULE_MAP_COLUMNS = (
    "original_module",
    "target_module",
    "mapping_role",
    "preserved_responsibility",
    "public_inputs",
    "public_outputs",
    "owned_state",
    "allowed_target_dependencies",
    "forbidden_direct_dependencies",
    "first_validation",
    "implementation_order",
    "review_status",
)
STATE_OWNERSHIP_COLUMNS = (
    "state_id",
    "address_or_origin",
    "size_bytes",
    "state_kind",
    "owner_module",
    "owner_basis",
    "initialization_or_creation",
    "writer_modules",
    "reader_modules",
    "destruction_or_end",
    "sharing_contract",
    "evidence_status",
    "review_status",
)
DEPENDENCY_COLUMNS = (
    "dependent_module",
    "provider_module",
    "relationship",
    "shared_state_contract",
    "reverse_dependency_present",
    "review_status",
)
CLOSURE_SOURCES = (
    ("input-font-closure.tsv", "Z_DAT.input_font_xrefs.txt"),
    ("runtime-audio-closure.tsv", "Z_DAT.b4_runtime_xrefs.txt"),
    ("ui-closure.tsv", "Z_DAT.b5_ui_xrefs.txt"),
    ("world-map-closure.tsv", "Z_DAT.b6_world_xrefs.txt"),
    ("scene-event-closure.tsv", "Z_DAT.b7_scene_xrefs.txt"),
    ("battle-closure.tsv", "Z_DAT.b8_battle_xrefs.txt"),
)
PERSISTENCE_CLOSURE_ORDER = (
    "0x20D35",
    "0x20FAF",
    "0x24A02",
    "0x24C8D",
    "0x25AB7",
    "0x25D0E",
    "0x25F87",
    "0x26208",
    "0x265AB",
    "0x26B5E",
    "0x2711A",
    "0x2EB49",
    "0x30C3D",
    "0x31241",
)
EXPECTED_CLOSURE_ROWS = 349
EXPECTED_CLOSURE_FUNCTIONS = 284
EXPECTED_CLOSURE_ORIGINS = Counter({"game_logic": 262, "library_or_platform": 22})
FUNCTION_PATTERN = re.compile(
    r"^FUNCTION\s+\S+\s+research=\S+\s+start=(?P<start>0x[0-9A-Fa-f]+)\s+"
    r"end=0x[0-9A-Fa-f]+\s+size=\d+$"
)

MODULES = {
    "unresolved",
    "external_crt",
    "external_miles",
    "compat",
    "diagnostics",
    "platform_sdl3",
    "app",
    "resource",
    "input",
    "time",
    "random",
    "audio",
    "model",
    "persistence",
    "render",
    "ui",
    "world",
    "scene",
    "battle",
}
OWNERSHIP_STATUSES = {
    "pending_assignment",
    "manual_reviewed",
    "assembly_exact",
    "platform_adapted",
    "external_boundary",
    "unreachable_current_assets",
}
FINAL_OWNERSHIP_STATUSES = {
    "assembly_exact",
    "platform_adapted",
    "external_boundary",
    "unreachable_current_assets",
}
EXPECTED_FINAL_OWNERSHIP_COUNTS = Counter(
    {
        "assembly_exact": 12,
        "platform_adapted": 282,
        "external_boundary": 283,
    }
)
LIBRARY_START = 0x3E33D
MILES_START = 0x3FAD8
MILES_END = 0x4DE0A
CLOSURE_STATUSES = {
    "pending_mapping",
    "pending_implementation",
    "implemented_pending_review",
    "assembly_exact",
    "platform_adapted",
    "cross_module_handoff",
    "unreachable_current_assets",
    "external_boundary",
}
FINAL_REVIEW_STATUSES = {
    "not_started",
    "in_progress",
    "converged_no_new_differences",
}
PENDING_CLOSURE_STATUSES = {
    "pending_mapping",
    "pending_implementation",
    "implemented_pending_review",
}
CLOSED_CLOSURE_STATUSES = CLOSURE_STATUSES - PENDING_CLOSURE_STATUSES
FRAMEWORK_REVIEW_STATUSES = {
    "pending_mapping",
    "pending_implementation",
    "implemented_pending_review",
    "manual_reviewed",
    "assembly_exact",
    "platform_adapted",
}


def canonical_address(value: str) -> str:
    return f"0x{int(value, 16):X}"


def read_tsv(path: Path, expected_columns: tuple[str, ...]) -> list[dict[str, str]]:
    if not path.exists():
        raise ValueError(f"missing inventory: {path.relative_to(ROOT)}")
    with path.open("r", encoding="utf-8", newline="") as stream:
        contents = stream.read()
        if any(line.endswith("\t") for line in contents.splitlines()):
            raise ValueError(f"{path.relative_to(ROOT)} contains a trailing tab")
        stream.seek(0)
        reader = csv.DictReader(stream, delimiter="\t")
        actual = tuple(reader.fieldnames or ())
        if actual != expected_columns:
            raise ValueError(
                f"{path.relative_to(ROOT)} header mismatch: expected {expected_columns}, got {actual}"
            )
        return list(reader)


def report_catalog_keys() -> set[tuple[str, str]]:
    keys: set[tuple[str, str]] = set()
    for binary in ("Z_COM", "Z_DAT"):
        report = json.loads((REPORT_ROOT / f"{binary}.report.json").read_text(encoding="utf-8"))
        for function in report["functions"]:
            keys.add((binary, canonical_address(function["address"])))
    for _, report_filename in CLOSURE_SOURCES:
        for address in report_closure_addresses(REPORT_ROOT / report_filename):
            keys.add(("Z_DAT", address))
    return keys


def report_closure_addresses(path: Path) -> list[str]:
    result: list[str] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        match = FUNCTION_PATTERN.match(line)
        if match is not None:
            result.append(canonical_address(match.group("start")))
    return result


def unique_keys(
    rows: list[dict[str, str]], columns: tuple[str, ...], label: str
) -> set[tuple[str, ...]]:
    result: set[tuple[str, ...]] = set()
    for row_number, row in enumerate(rows, 2):
        key = tuple(row[column] for column in columns)
        if key in result:
            raise ValueError(f"{label}:{row_number} duplicate key {key}")
        result.add(key)
    return result


def validate_catalog() -> set[tuple[str, str]]:
    rows = read_tsv(INVENTORY_ROOT / "function-catalog.tsv", FUNCTION_CATALOG_COLUMNS)
    keys = unique_keys(rows, ("binary", "address"), "function-catalog.tsv")
    expected = report_catalog_keys()
    if keys != expected:
        missing = sorted(expected - keys)
        extra = sorted(keys - expected)
        raise ValueError(f"function catalog differs from IDA reports: missing={missing}, extra={extra}")
    return keys


def validate_ownership(catalog_keys: set[tuple[str, str]]) -> None:
    rows = read_tsv(INVENTORY_ROOT / "module-function-ownership.tsv", OWNERSHIP_COLUMNS)
    keys = unique_keys(rows, ("binary", "address"), "module-function-ownership.tsv")
    if keys != catalog_keys:
        raise ValueError("module ownership keys do not exactly match function catalog")
    for row_number, row in enumerate(rows, 2):
        if row["module_candidate"] not in MODULES:
            raise ValueError(
                f"module-function-ownership.tsv:{row_number} illegal module "
                f"{row['module_candidate']!r}"
            )
        if row["review_status"] not in OWNERSHIP_STATUSES:
            raise ValueError(
                f"module-function-ownership.tsv:{row_number} illegal review status "
                f"{row['review_status']!r}"
            )
        if row["module_candidate"] == "unresolved":
            raise ValueError(
                f"module-function-ownership.tsv:{row_number} unresolved final owner"
            )
        if row["candidate_confidence"] != "high":
            raise ValueError(
                f"module-function-ownership.tsv:{row_number} final owner is not high confidence"
            )
        if row["assignment_basis"] in {"", "mechanical_catalog_only"}:
            raise ValueError(
                f"module-function-ownership.tsv:{row_number} lacks manual ownership proof"
            )
        if row["review_status"] not in FINAL_OWNERSHIP_STATUSES:
            raise ValueError(
                f"module-function-ownership.tsv:{row_number} non-final review status "
                f"{row['review_status']!r}"
            )
        external_module = row["module_candidate"] in {"external_crt", "external_miles"}
        if external_module != (row["review_status"] == "external_boundary"):
            raise ValueError(
                f"module-function-ownership.tsv:{row_number} external module/status mismatch"
            )
        address = int(row["address"], 16)
        expected_external = row["binary"] == "Z_DAT" and address >= LIBRARY_START
        expected_miles = expected_external and MILES_START <= address < MILES_END
        if expected_external:
            expected_module = "external_miles" if expected_miles else "external_crt"
            if row["module_candidate"] != expected_module:
                raise ValueError(
                    f"module-function-ownership.tsv:{row_number} library boundary mismatch: "
                    f"expected {expected_module!r}"
                )
        elif external_module:
            raise ValueError(
                f"module-function-ownership.tsv:{row_number} external owner below library boundary"
            )
        if external_module and row["code_origin"] != "library_or_platform":
            raise ValueError(
                f"module-function-ownership.tsv:{row_number} external code origin mismatch"
            )
        if not external_module and row["code_origin"] != "game_logic":
            raise ValueError(
                f"module-function-ownership.tsv:{row_number} game code origin mismatch"
            )
        if not row["follow_up_module"]:
            raise ValueError(
                f"module-function-ownership.tsv:{row_number} empty follow-up module"
            )

    status_counts = Counter(row["review_status"] for row in rows)
    if status_counts != EXPECTED_FINAL_OWNERSHIP_COUNTS:
        raise ValueError(
            "module ownership final status counts differ: "
            f"actual={dict(status_counts)} expected={dict(EXPECTED_FINAL_OWNERSHIP_COUNTS)}"
        )
    module_counts = Counter(row["module_candidate"] for row in rows)
    if module_counts["external_crt"] != 91 or module_counts["external_miles"] != 192:
        raise ValueError(
            "external library classification differs: "
            f"external_crt={module_counts['external_crt']} "
            f"external_miles={module_counts['external_miles']}"
        )


def validate_evidence_path(value: str, label: str) -> None:
    path = ROOT / value
    if not path.is_file():
        raise ValueError(f"{label} evidence path does not exist: {value}")


def validate_framework_tables() -> None:
    module_rows = read_tsv(INVENTORY_ROOT / "rewrite-module-map.tsv", MODULE_MAP_COLUMNS)
    unique_keys(module_rows, ("target_module",), "rewrite-module-map.tsv")
    for row_number, row in enumerate(module_rows, 2):
        if row["target_module"] not in MODULES - {"unresolved", "external_crt", "external_miles"}:
            raise ValueError(
                f"rewrite-module-map.tsv:{row_number} illegal target module {row['target_module']!r}"
            )
        if row["review_status"] not in FRAMEWORK_REVIEW_STATUSES:
            raise ValueError(
                f"rewrite-module-map.tsv:{row_number} illegal review status {row['review_status']!r}"
            )
        for column in ("mapping_role", "preserved_responsibility", "owned_state", "first_validation"):
            if not row[column]:
                raise ValueError(f"rewrite-module-map.tsv:{row_number} empty {column}")

    state_rows = read_tsv(
        INVENTORY_ROOT / "module-state-ownership.tsv", STATE_OWNERSHIP_COLUMNS
    )
    unique_keys(state_rows, ("state_id",), "module-state-ownership.tsv")
    for row_number, row in enumerate(state_rows, 2):
        if row["owner_module"] not in MODULES - {"unresolved", "external_crt", "external_miles"}:
            raise ValueError(
                f"module-state-ownership.tsv:{row_number} illegal owner {row['owner_module']!r}"
            )
        if row["review_status"] not in FRAMEWORK_REVIEW_STATUSES:
            raise ValueError(
                f"module-state-ownership.tsv:{row_number} illegal review status "
                f"{row['review_status']!r}"
            )
        for column in (
            "address_or_origin",
            "state_kind",
            "owner_basis",
            "initialization_or_creation",
            "destruction_or_end",
            "sharing_contract",
            "evidence_status",
        ):
            if not row[column]:
                raise ValueError(f"module-state-ownership.tsv:{row_number} empty {column}")

    dependency_rows = read_tsv(INVENTORY_ROOT / "module-dependencies.tsv", DEPENDENCY_COLUMNS)
    unique_keys(
        dependency_rows,
        ("dependent_module", "provider_module"),
        "module-dependencies.tsv",
    )
    for row_number, row in enumerate(dependency_rows, 2):
        if row["dependent_module"] not in MODULES or row["provider_module"] not in MODULES:
            raise ValueError(f"module-dependencies.tsv:{row_number} illegal module")
        if row["reverse_dependency_present"] not in {"yes", "no"}:
            raise ValueError(
                f"module-dependencies.tsv:{row_number} reverse dependency must be yes/no"
            )
        if row["review_status"] not in FRAMEWORK_REVIEW_STATUSES:
            raise ValueError(
                f"module-dependencies.tsv:{row_number} illegal review status {row['review_status']!r}"
            )


def validate_closure(
    filename: str,
    report_filename: str | None,
    zdat_addresses: set[str],
    fixed_order: tuple[str, ...] = (),
) -> int:
    rows = read_tsv(INVENTORY_ROOT / filename, CLOSURE_COLUMNS)
    keys = unique_keys(rows, ("address",), filename)
    if report_filename is None:
        expected_order = list(fixed_order)
        source_label = "fixed finite B9 boundary"
    else:
        expected_order = report_closure_addresses(REPORT_ROOT / report_filename)
        source_label = f"{report_filename} FUNCTION entries"
    expected = {(address,) for address in expected_order}
    if keys != expected:
        raise ValueError(f"{filename} does not exactly match {source_label}")
    if [row["address"] for row in rows] != expected_order:
        raise ValueError(f"{filename} audit order differs from {source_label}")
    expected_audit_order = [str(index) for index in range(1, len(rows) + 1)]
    if [row["audit_order"] for row in rows] != expected_audit_order:
        raise ValueError(f"{filename} audit_order column is not sequential")

    pending = 0
    for row_number, row in enumerate(rows, 2):
        label = f"{filename}:{row_number}"
        if row["address"] not in zdat_addresses:
            raise ValueError(f"{label} address is absent from Z_DAT function catalog")
        if row["closure_status"] not in CLOSURE_STATUSES:
            raise ValueError(f"{label} illegal closure status {row['closure_status']!r}")
        if row["target_owner"] not in MODULES:
            raise ValueError(f"{label} illegal target owner {row['target_owner']!r}")
        if row["final_review"] not in FINAL_REVIEW_STATUSES:
            raise ValueError(f"{label} illegal final review status {row['final_review']!r}")
        if row["closure_status"] in PENDING_CLOSURE_STATUSES:
            pending += 1
            if not row["remaining"]:
                raise ValueError(f"{label} pending status must state remaining work")
            if row["closure_status"] == "implemented_pending_review" and not row[
                "implementation_mapping"
            ]:
                raise ValueError(f"{label} implemented_pending_review lacks implementation mapping")
            if row["final_review"] == "converged_no_new_differences":
                raise ValueError(f"{label} pending row cannot claim converged final review")
            continue
        if not row["implementation_mapping"]:
            raise ValueError(f"{label} closed row lacks implementation mapping")
        if not row["evidence"]:
            raise ValueError(f"{label} closed row lacks evidence")
        validate_evidence_path(row["evidence"], label)
        if not row["verification"]:
            raise ValueError(f"{label} closed row lacks verification")
        if row["final_review"] != "converged_no_new_differences":
            raise ValueError(f"{label} closed row lacks converged final review")
        if row["remaining"]:
            raise ValueError(f"{label} closed row still declares remaining work")
    return pending


def main() -> int:
    try:
        catalog_keys = validate_catalog()
        validate_ownership(catalog_keys)
        validate_framework_tables()
        zdat_addresses = {address for binary, address in catalog_keys if binary == "Z_DAT"}
        pending_by_file = {
            filename: validate_closure(filename, report_filename, zdat_addresses)
            for filename, report_filename in CLOSURE_SOURCES
        }
        pending_by_file["persistence-closure.tsv"] = validate_closure(
            "persistence-closure.tsv", None, zdat_addresses, PERSISTENCE_CLOSURE_ORDER
        )
        closure_tables = {
            filename: read_tsv(INVENTORY_ROOT / filename, CLOSURE_COLUMNS)
            for filename in pending_by_file
        }
        closure_rows = sum(len(rows) for rows in closure_tables.values())
        if closure_rows != EXPECTED_CLOSURE_ROWS:
            raise ValueError(
                f"closure row count mismatch: expected {EXPECTED_CLOSURE_ROWS}, got {closure_rows}"
            )
        closure_addresses = {
            row["address"] for rows in closure_tables.values() for row in rows
        }
        if len(closure_addresses) != EXPECTED_CLOSURE_FUNCTIONS:
            raise ValueError(
                "closure physical function count mismatch: "
                f"expected {EXPECTED_CLOSURE_FUNCTIONS}, got {len(closure_addresses)}"
            )
        ownership_rows = read_tsv(
            INVENTORY_ROOT / "module-function-ownership.tsv", OWNERSHIP_COLUMNS
        )
        zdat_origins = {
            row["address"]: row["code_origin"]
            for row in ownership_rows
            if row["binary"] == "Z_DAT"
        }
        closure_origins = Counter(zdat_origins[address] for address in closure_addresses)
        if closure_origins != EXPECTED_CLOSURE_ORIGINS:
            raise ValueError(
                "closure code-origin counts differ: "
                f"actual={dict(closure_origins)} expected={dict(EXPECTED_CLOSURE_ORIGINS)}"
            )
    except (OSError, ValueError, csv.Error, json.JSONDecodeError) as error:
        print(f"reverse framework validation failed: {error}", file=sys.stderr)
        return 1

    print(
        f"reverse framework valid: {len(catalog_keys)} catalog/ownership rows, "
        f"{closure_rows} closure rows/{len(closure_addresses)} physical functions"
    )
    for filename, pending in pending_by_file.items():
        print(f"{filename}: {pending} pending or unverified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
