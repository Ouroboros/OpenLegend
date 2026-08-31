#!/usr/bin/env python3
"""Generate independent B8 battle asset goldens from original read-only files."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
from pathlib import Path


WAR_RECORD_SIZE = 186
FIGHT_PATTERN = re.compile(r"^FIGHT(?P<id>\d{3})\.IDX$", re.IGNORECASE)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def cumulative_entries(index_bytes: bytes, group_bytes: bytes) -> list[bytes]:
    if len(index_bytes) % 4 != 0:
        raise ValueError("IDX size is not divisible by four")
    offsets = struct.unpack(f"<{len(index_bytes) // 4}I", index_bytes)
    entries: list[bytes] = []
    start = 0
    for end in offsets:
        if end < start or end > len(group_bytes):
            raise ValueError(f"invalid cumulative offset {end} after {start}")
        entries.append(group_bytes[start:end])
        start = end
    if start != len(group_bytes):
        raise ValueError(f"final cumulative offset {start} != group size {len(group_bytes)}")
    return entries


def archive_record(index_path: Path, group_path: Path) -> dict[str, object]:
    index_bytes = index_path.read_bytes()
    group_bytes = group_path.read_bytes()
    entries = cumulative_entries(index_bytes, group_bytes)
    return {
        "index_size": len(index_bytes),
        "index_sha256": sha256(index_bytes),
        "group_size": len(group_bytes),
        "group_sha256": sha256(group_bytes),
        "entry_count": len(entries),
        "entry_sizes": [len(entry) for entry in entries],
        "entry_sha256": [sha256(entry) for entry in entries],
    }


def battle_setup_record(battle_id: int, record: bytes) -> dict[str, object]:
    words = list(struct.unpack("<93h", record))
    preset_party = words[9:15]
    fixed_party = words[15:21]
    party_x = words[21:27]
    party_y = words[27:33]
    enemies = words[33:53]
    enemy_x = words[53:73]
    enemy_y = words[73:93]
    use_fixed_party = any(role_id != -1 for role_id in fixed_party)
    active_party = fixed_party if use_fixed_party else preset_party
    writes: list[dict[str, int | str]] = []
    slot = 0
    for source_index, role_id in enumerate(active_party):
        if role_id == -1:
            continue
        writes.append(
            {
                "slot": slot,
                "side": "party",
                "source_index": source_index,
                "role_id": role_id,
                "x": party_x[source_index],
                "y": party_y[source_index],
                "occupancy_index": party_y[source_index] * 64 + party_x[source_index],
            }
        )
        slot += 1
    for source_index, role_id in enumerate(enemies):
        if role_id == -1:
            continue
        writes.append(
            {
                "slot": slot,
                "side": "enemy",
                "source_index": source_index,
                "role_id": role_id,
                "x": enemy_x[source_index],
                "y": enemy_y[source_index],
                "occupancy_index": enemy_y[source_index] * 64 + enemy_x[source_index],
            }
        )
        slot += 1
    duplicate_writes: list[dict[str, int]] = []
    previous_slots: dict[int, int] = {}
    for write in writes:
        occupancy_index = int(write["occupancy_index"])
        if occupancy_index in previous_slots:
            duplicate_writes.append(
                {
                    "occupancy_index": occupancy_index,
                    "previous_slot": previous_slots[occupancy_index],
                    "replacement_slot": int(write["slot"]),
                }
            )
        previous_slots[occupancy_index] = int(write["slot"])
    return {
        "battle_id": battle_id,
        "battlefield_id": words[6],
        "music_id": words[8],
        "preset_party_ids": preset_party,
        "fixed_party_ids": fixed_party,
        "party_x": party_x,
        "party_y": party_y,
        "enemy_ids": enemies,
        "enemy_x": enemy_x,
        "enemy_y": enemy_y,
        "active_party_source": "fixed" if use_fixed_party else "preset",
        "active_party_count": sum(role_id != -1 for role_id in active_party),
        "enemy_count": sum(role_id != -1 for role_id in enemies),
        "static_combatant_count": len(writes),
        "static_occupancy_writes": writes,
        "duplicate_occupancy_writes": duplicate_writes,
    }


def build(data_root: Path) -> dict[str, object]:
    war_bytes = (data_root / "WAR.STA").read_bytes()
    if len(war_bytes) % WAR_RECORD_SIZE != 0:
        raise ValueError("WAR.STA is not a whole number of 186-byte records")
    war_records = [
        war_bytes[offset:offset + WAR_RECORD_SIZE]
        for offset in range(0, len(war_bytes), WAR_RECORD_SIZE)
    ]
    setup_records = [
        battle_setup_record(battle_id, record)
        for battle_id, record in enumerate(war_records)
    ]
    for record in setup_records:
        for write in record["static_occupancy_writes"]:
            if not (0 <= int(write["x"]) < 64 and 0 <= int(write["y"]) < 64):
                raise ValueError(
                    f"battle {record['battle_id']} has out-of-range static coordinate {write}"
                )
        if int(record["static_combatant_count"]) > 26:
            raise ValueError(f"battle {record['battle_id']} exceeds 26 combatant slots")

    fight_ids: list[int] = []
    fight_packages: list[dict[str, object]] = []
    for index_path in sorted(data_root.glob("FIGHT*.IDX"), key=lambda path: path.name.upper()):
        match = FIGHT_PATTERN.match(index_path.name)
        if match is None:
            continue
        fight_id = int(match.group("id"))
        group_path = data_root / f"FIGHT{fight_id:03d}.GRP"
        if not group_path.is_file():
            raise ValueError(f"missing {group_path.name}")
        record = archive_record(index_path, group_path)
        fight_ids.append(fight_id)
        fight_packages.append({"id": fight_id, **record})
    if len(fight_packages) != 92:
        raise ValueError(f"expected 92 FIGHT packages, got {len(fight_packages)}")

    warfld = archive_record(data_root / "WARFLD.IDX", data_root / "WARFLD.GRP")
    if warfld["entry_count"] != 26:
        raise ValueError(f"expected 26 WARFLD entries, got {warfld['entry_count']}")

    return {
        "format": "openlegend-b8-battle-goldens-v1",
        "war_sta": {
            "record_size": WAR_RECORD_SIZE,
            "record_count": len(war_records),
            "file_size": len(war_bytes),
            "file_sha256": sha256(war_bytes),
            "record_sha256": [sha256(record) for record in war_records],
        },
        "warfld": warfld,
        "battle_setup": {
            "combatant_slot_count": 26,
            "combatant_words_per_slot": 14,
            "combatant_bytes_per_slot": 28,
            "initial_words": [-1, -1, 0, 0, 0, 0, 0, 0, 5098, 0, 0, -1, -1, 0],
            "sprite_word": {
                "role_head_word": 1,
                "archive_frame_offset": 0,
                "base": 5106,
                "empty_role_head_word": -1,
                "formula": "int16(8*role_head_word + 5106 + 2*initial_mode)",
            },
            "party_prefix_rule": "slot0 unconditional; first slot 1..5 with signed id <= 0 ends prefix; otherwise 6",
            "selection_states": {
                "unselected": 0,
                "selected": 1,
                "mandatory": 2,
                "confirm_index": "party_prefix_length",
            },
            "fixed_records": sum(
                record["active_party_source"] == "fixed" for record in setup_records
            ),
            "preset_records": sum(
                record["active_party_source"] == "preset" for record in setup_records
            ),
            "max_active_party_count": max(
                int(record["active_party_count"]) for record in setup_records
            ),
            "max_enemy_count": max(int(record["enemy_count"]) for record in setup_records),
            "max_static_combatant_count": max(
                int(record["static_combatant_count"]) for record in setup_records
            ),
            "records_with_duplicate_occupancy": [
                int(record["battle_id"])
                for record in setup_records
                if record["duplicate_occupancy_writes"]
            ],
            "battlefield_ids": sorted(
                {int(record["battlefield_id"]) for record in setup_records}
            ),
            "music_ids": sorted({int(record["music_id"]) for record in setup_records}),
            "records": setup_records,
        },
        "fight_packages": {
            "count": len(fight_packages),
            "ids": fight_ids,
            "missing_ids_between_min_max": sorted(
                set(range(min(fight_ids), max(fight_ids) + 1)) - set(fight_ids)
            ),
            "total_frames": sum(int(package["entry_count"]) for package in fight_packages),
            "packages": fight_packages,
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--data-root", type=Path, default=Path(".."))
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("research/evidence/battle-goldens.json"),
    )
    args = parser.parse_args()
    result = build(args.data_root)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print(f"wrote {args.output}")
    print(f"WAR records: {result['war_sta']['record_count']}")
    print(f"WARFLD entries: {result['warfld']['entry_count']}")
    print(f"FIGHT packages: {result['fight_packages']['count']}")
    print(f"FIGHT frames: {result['fight_packages']['total_frames']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
