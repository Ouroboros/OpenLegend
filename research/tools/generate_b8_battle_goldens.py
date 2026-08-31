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
PATH_DIRECTIONS = ((0, -1), (1, 0), (-1, 0), (0, 1))
BLOCKED_TILE_RANGES = (
    (0x0166, 0x016A),
    (0x0176, 0x017C),
    (0x01CA, 0x01D0),
    (0x01FA, 0x0262),
    (0x0332, 0x0338),
    (0x0346, 0x0346),
    (0x03A6, 0x03A8),
    (0x03F8, 0x03FE),
    (0x052C, 0x0544),
)


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


def fnv1a_words(words: list[int]) -> str:
    value = 0xCBF29CE484222325
    for word in words:
        raw = word & 0xFFFF
        for byte in (raw & 0xFF, raw >> 8):
            value ^= byte
            value = value * 0x100000001B3 & 0xFFFFFFFFFFFFFFFF
    return f"0x{value:016x}"


def legacy_path_index(x: int, y: int) -> int | None:
    if x < 0 or y < 0 or x > 64 or y > 64:
        return None
    index = y * 64 + x
    return index if 0 <= index < 4096 else None


def build_path_map(
    field_words: list[int],
    source: tuple[int, int],
    mode: str,
    occupied: set[int] | None = None,
) -> list[int]:
    occupied = occupied or set()
    values: list[int] = []
    for index in range(4096):
        upper_layer = field_words[4096 + index]
        if mode == "targeting":
            values.append(555 if upper_layer != 0 else 254)
        else:
            blocked_ground = any(
                begin <= field_words[index] <= end for begin, end in BLOCKED_TILE_RANGES
            )
            values.append(
                555 if upper_layer != 0 or index in occupied or blocked_ground else 254
            )
    source_index = legacy_path_index(*source)
    if source_index is None:
        raise ValueError(f"invalid path source {source}")
    values[source_index] = 0

    queue = [(0, 0)] * 255
    queue[0] = (0, -1)
    queue[1] = source
    read_index = 0
    write_index = 2
    distance = 0

    def enqueue(coordinate: tuple[int, int]) -> None:
        nonlocal write_index
        queue[write_index] = coordinate
        write_index = (write_index + 1) % 255

    def dequeue() -> tuple[int, int]:
        nonlocal read_index
        coordinate = queue[read_index]
        read_index = (read_index + 1) % 255
        return coordinate

    while True:
        current = dequeue()
        if current[1] < 0:
            distance = (distance + 1) % 128
            enqueue((0, -1))
            current = dequeue()
            if current[1] < 0:
                break
        for dx, dy in PATH_DIRECTIONS:
            next_coord = (current[0] + dx, current[1] + dy)
            next_index = legacy_path_index(*next_coord)
            if next_index is not None and values[next_index] == 254:
                enqueue(next_coord)
                values[next_index] = distance
    return values


def mark_path(values: list[int], source: tuple[int, int], target: tuple[int, int]) -> bool:
    target_index = legacy_path_index(*target)
    if target_index is None or not 0 <= values[target_index] < 254:
        return False
    current = target
    distance = values[target_index]
    values[target_index] = 250
    for _ in range(4096):
        if current == source:
            return True
        distance = (distance + 127) % 128
        for dx, dy in PATH_DIRECTIONS:
            previous = (current[0] + dx, current[1] + dy)
            if not (0 <= previous[0] < 64 and 0 <= previous[1] < 64):
                continue
            previous_index = legacy_path_index(*previous)
            if previous_index is not None and values[previous_index] == distance:
                values[previous_index] = 250
                current = previous
                break
        else:
            return False
    return False


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

    warfld_index_bytes = (data_root / "WARFLD.IDX").read_bytes()
    warfld_group_bytes = (data_root / "WARFLD.GRP").read_bytes()
    warfld_entries = cumulative_entries(warfld_index_bytes, warfld_group_bytes)
    warfld = archive_record(data_root / "WARFLD.IDX", data_root / "WARFLD.GRP")
    if warfld["entry_count"] != 26:
        raise ValueError(f"expected 26 WARFLD entries, got {warfld['entry_count']}")

    pathing_records: list[dict[str, object]] = []
    for battle_id in (0, 93):
        setup = setup_records[battle_id]
        party = next(
            write for write in setup["static_occupancy_writes"] if write["side"] == "party"
        )
        enemy = next(
            write for write in setup["static_occupancy_writes"] if write["side"] == "enemy"
        )
        source = (int(party["x"]), int(party["y"]))
        target = (int(enemy["x"]), int(enemy["y"]))
        field_bytes = warfld_entries[int(setup["battlefield_id"])][:16384]
        field_words = list(struct.unpack("<8192h", field_bytes))
        movement = build_path_map(field_words, source, "movement")
        occupied_coordinate = (source[0] + 1, source[1])
        movement_occupied = build_path_map(
            field_words,
            source,
            "movement",
            {occupied_coordinate[1] * 64 + occupied_coordinate[0]},
        )
        targeting = build_path_map(field_words, source, "targeting")
        targeting_before_mark = fnv1a_words(targeting)
        path_marked = mark_path(targeting, source, target)
        first_marked_step = next(
            (
                [source[0] + dx, source[1] + dy]
                for dx, dy in PATH_DIRECTIONS
                if 0 <= source[0] + dx < 64
                and 0 <= source[1] + dy < 64
                and targeting[(source[1] + dy) * 64 + source[0] + dx] == 250
            ),
            None,
        )
        pathing_records.append(
            {
                "battle_id": battle_id,
                "source": list(source),
                "target": list(target),
                "movement_hash": fnv1a_words(movement),
                "occupied_coordinate": list(occupied_coordinate),
                "movement_occupied_hash": fnv1a_words(movement_occupied),
                "targeting_hash": targeting_before_mark,
                "targeting_marked": path_marked,
                "targeting_marked_hash": fnv1a_words(targeting),
                "first_marked_step": first_marked_step,
                "target_distance": build_path_map(field_words, source, "targeting")[
                    target[1] * 64 + target[0]
                ],
            }
        )

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
            "round_order": {
                "role_speed_word": 44,
                "role_equipment_words": [23, 24],
                "item_add_speed_word": 53,
                "effective_speed": "int16(role.speed + each equipped item.add_speed)",
                "sort": "signed descending; equal values do not swap",
                "swap_words": [0, 1, 2, 3, 4, 5, 6, 7, 9, 10, 11, 12, 13],
                "recomputed_word": 8,
                "occupancy_hidden_word": 5,
                "round_value_word": 6,
                "round_value": "max(0, effective_speed/15 - role.hurt/40), signed truncation toward zero",
                "vectors": {
                    "stable_equal": {
                        "input": [
                            {"role_id": 1, "speed": 10, "item_add_speed": 30},
                            {"role_id": 3, "speed": 40, "item_add_speed": 0},
                        ],
                        "expected_role_order": [1, 3],
                    },
                    "swap_hidden_round": {
                        "input_role_order": [3, 1],
                        "effective_speeds": [41, 50],
                        "hurt": [200, 40],
                        "hidden": [1, 0],
                        "expected_role_order": [1, 3],
                        "expected_round_values": [2, 0],
                        "expected_occupancy": {"party_26_24": 0, "enemy_26_26": -1},
                    },
                },
            },
            "result_codes": {
                "ongoing": 0,
                "no_party_alive": 1,
                "no_enemy_alive": 2,
                "both_empty": 2,
                "public_return": "raw_result - 1",
            },
            "attack_entry": {
                "learned_magic_rule": "count role magic ids > 0",
                "single_learned_slot_bug": {
                    "magic_ids": [0, 0, 5, 0, 0, 0, 0, 0, 0, 0],
                    "learned_count": 1,
                    "selected_slot": 0,
                    "selected_magic_id": 0,
                    "rng_consumed": False,
                },
                "profile_vector": {
                    "slot": 2,
                    "magic_id": 5,
                    "experience": 299,
                    "level_index": 2,
                    "select_distance": 7,
                    "attack_distance": 3,
                    "area_type": 2,
                    "hurt_type": 1,
                    "attack_count": 2,
                    "need_mp": 4,
                },
                "commit_vector": {
                    "rng_seed": 1,
                    "rng_bound": 2,
                    "rng_state_after": 1103527590,
                    "experience_after": 300,
                    "leveled": True,
                    "cost_scale": 3,
                    "mp_before": 3,
                    "mp_after": 0,
                    "action_done": 1,
                    "attack_counter_after": 2,
                    "physical_power_after_finish": 0,
                    "experience_cap": 999,
                },
            },
            "pathing": {
                "directions": [[0, -1], [1, 0], [-1, 0], [0, 1]],
                "blocked_tile_ranges": [list(value) for value in BLOCKED_TILE_RANGES],
                "unvisited": 254,
                "blocked": 555,
                "marked": 250,
                "consumed": 255,
                "queue_slots": 255,
                "distance_modulus": 128,
                "movement_step": {
                    "source": [32, 20],
                    "marked_first_steps": [[31, 20], [30, 20]],
                    "slot": 0,
                    "role_head_word": 0,
                    "role_speed": 50,
                    "physical_power_before": 1,
                    "round_value_before": 5,
                    "first_direction": 2,
                    "first_sprite": 5110,
                    "physical_power_after_first": 0,
                    "round_value_after_first": 4,
                    "round_value_after_second": 3,
                    "stop_vectors": {
                        "destination_equal": True,
                        "destination_different": False,
                        "manhattan_13_in_range": True,
                        "manhattan_13_aligned": False,
                        "round_value_zero": True,
                    },
                    "consumed_value": 255,
                },
                "records": pathing_records,
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
