#!/usr/bin/env python3
"""Build deterministic reverse-engineering inventories from headless IDA reports."""

from __future__ import annotations

import csv
import json
import re
from dataclasses import dataclass
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

CLOSURE_SOURCES = (
    ("input-font-closure.tsv", "Z_DAT.input_font_xrefs.txt"),
    ("runtime-audio-closure.tsv", "Z_DAT.b4_runtime_xrefs.txt"),
    ("ui-closure.tsv", "Z_DAT.b5_ui_xrefs.txt"),
    ("world-map-closure.tsv", "Z_DAT.b6_world_xrefs.txt"),
    ("scene-event-closure.tsv", "Z_DAT.b7_scene_xrefs.txt"),
    ("battle-closure.tsv", "Z_DAT.b8_battle_xrefs.txt"),
)

FUNCTION_PATTERN = re.compile(
    r"^FUNCTION\s+(?P<name>\S+)\s+research=(?P<research>\S+)\s+"
    r"start=(?P<start>0x[0-9A-Fa-f]+)\s+end=(?P<end>0x[0-9A-Fa-f]+)\s+"
    r"size=(?P<size>\d+)$"
)


@dataclass(frozen=True)
class FunctionRow:
    binary: str
    address: str
    end: str
    size: str
    name: str
    instruction_count: str
    basic_blocks: str
    direct_callers: str
    direct_callees: str
    code_origin: str

    def as_dict(self) -> dict[str, str]:
        return {column: getattr(self, column) for column in FUNCTION_CATALOG_COLUMNS}


def canonical_address(value: str) -> str:
    return f"0x{int(value, 16):X}"


def read_tsv(path: Path, key_columns: tuple[str, ...]) -> dict[tuple[str, ...], dict[str, str]]:
    if not path.exists():
        return {}
    with path.open("r", encoding="utf-8", newline="") as stream:
        rows = csv.DictReader(stream, delimiter="\t")
        return {tuple(row[column] for column in key_columns): row for row in rows}


def write_tsv(path: Path, columns: tuple[str, ...], rows: list[dict[str, str]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(
            stream, fieldnames=columns, delimiter="\t", lineterminator="\n", extrasaction="ignore"
        )
        writer.writeheader()
        writer.writerows(rows)


def function_catalog() -> list[FunctionRow]:
    result: list[FunctionRow] = []
    for binary in ("Z_COM", "Z_DAT"):
        report = json.loads((REPORT_ROOT / f"{binary}.report.json").read_text(encoding="utf-8"))
        for function in report["functions"]:
            result.append(
                FunctionRow(
                    binary=binary,
                    address=canonical_address(function["address"]),
                    end=canonical_address(function["end"]),
                    size=str(function["size"]),
                    name=function["name"],
                    instruction_count=str(function["instruction_count"]),
                    basic_blocks=str(function["basic_blocks"]),
                    direct_callers=",".join(canonical_address(value) for value in function["callers"]),
                    direct_callees=",".join(canonical_address(value) for value in function["callees"]),
                    code_origin="game_or_library_unclassified",
                )
            )

    known = {(row.binary, row.address) for row in result}
    for _, report_filename in CLOSURE_SOURCES:
        for function in report_functions(REPORT_ROOT / report_filename):
            key = ("Z_DAT", function["address"])
            if key in known:
                continue
            result.append(
                FunctionRow(
                    binary="Z_DAT",
                    address=function["address"],
                    end=function["function_end"],
                    size=function["size"],
                    name=function["name"],
                    instruction_count="",
                    basic_blocks="",
                    direct_callers="",
                    direct_callees="",
                    code_origin="game_or_library_unclassified_report_recovered",
                )
            )
            known.add(key)
    return sorted(result, key=lambda row: (row.binary, int(row.address, 16)))


def build_function_catalog(rows: list[FunctionRow]) -> None:
    write_tsv(
        INVENTORY_ROOT / "function-catalog.tsv",
        FUNCTION_CATALOG_COLUMNS,
        [row.as_dict() for row in rows],
    )


def build_ownership(rows: list[FunctionRow]) -> None:
    path = INVENTORY_ROOT / "module-function-ownership.tsv"
    existing = read_tsv(path, ("binary", "address"))
    output: list[dict[str, str]] = []
    for function in rows:
        key = (function.binary, function.address)
        previous = existing.get(key, {})
        output.append(
            {
                "binary": function.binary,
                "address": function.address,
                "name": function.name,
                "code_origin": previous.get("code_origin", function.code_origin),
                "module_candidate": previous.get("module_candidate", "unresolved"),
                "candidate_confidence": previous.get("candidate_confidence", "unreviewed"),
                "assignment_basis": previous.get("assignment_basis", "mechanical_catalog_only"),
                "direct_callers": function.direct_callers,
                "direct_callees": function.direct_callees,
                "follow_up_module": previous.get("follow_up_module", "framework_assignment"),
                "review_status": previous.get("review_status", "pending_assignment"),
            }
        )
    write_tsv(path, OWNERSHIP_COLUMNS, output)


def report_functions(path: Path) -> list[dict[str, str]]:
    result: list[dict[str, str]] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        match = FUNCTION_PATTERN.match(line)
        if match is None:
            continue
        result.append(
            {
                "name": match.group("name"),
                "research_name": match.group("research"),
                "address": canonical_address(match.group("start")),
                "function_end": canonical_address(match.group("end")),
                "size": match.group("size"),
            }
        )
    return result


def build_closure(filename: str, report_filename: str) -> None:
    path = INVENTORY_ROOT / filename
    existing = read_tsv(path, ("address",))
    functions = report_functions(REPORT_ROOT / report_filename)
    if not functions:
        raise ValueError(f"{report_filename} has no FUNCTION records and cannot define a closure")
    output: list[dict[str, str]] = []
    for audit_order, function in enumerate(functions, 1):
        previous = existing.get((function["address"],), {})
        closure_status = previous.get("closure_status", "pending_mapping")
        if closure_status == "pending_audit":
            closure_status = "pending_mapping"
        remaining = previous.get("remaining", "")
        if not remaining or remaining == "independent assembly review and bidirectional convergence":
            remaining = "map existing implementation or implement, then complete final review"
        output.append(
            {
                "audit_order": str(audit_order),
                **function,
                "closure_status": closure_status,
                "target_owner": previous.get("target_owner", "unresolved"),
                "implementation_mapping": previous.get("implementation_mapping", ""),
                "evidence": previous.get("evidence", ""),
                "verification": previous.get("verification", ""),
                "final_review": previous.get("final_review", "not_started"),
                "remaining": remaining,
            }
        )
    write_tsv(path, CLOSURE_COLUMNS, output)


def main() -> int:
    rows = function_catalog()
    build_function_catalog(rows)
    build_ownership(rows)
    for filename, report_filename in CLOSURE_SOURCES:
        build_closure(filename, report_filename)
    print(
        f"generated {len(rows)} catalog/ownership rows and "
        f"{len(CLOSURE_SOURCES)} closure inventories in {INVENTORY_ROOT}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
