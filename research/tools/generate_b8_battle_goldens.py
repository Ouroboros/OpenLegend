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
Z_DAT_EFFECT_FRAME_COUNTS_OFFSET = 324_814
EFFECT_FRAME_COUNT = 53
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


def wrapping_i16(value: int) -> int:
    value &= 0xFFFF
    return value - 0x10000 if value >= 0x8000 else value


def trunc_div(value: int, divisor: int) -> int:
    quotient = abs(value) // abs(divisor)
    return -quotient if (value < 0) != (divisor < 0) else quotient


def legacy_bounded(state: int, upper_bound: int) -> tuple[int, int]:
    if upper_bound <= 1 or upper_bound > 30_000:
        return 0, state
    state = (state * 0x41C64E6D + 0x3039) & 0xFFFF_FFFF
    return ((state >> 16) & 0x7FFF) % upper_bound, state


def throwing_weapon_vector(
    item: bytes,
    *,
    seed: int,
    hidden_weapon: int,
    hp: int,
    maximum_hp: int,
    hurt: int,
    poison: int,
    anti_poison: int,
) -> dict[str, object]:
    word = lambda index: struct.unpack_from("<h", item, index * 2)[0]
    divisor = 4 if hurt == 0 else 3 if hurt <= 33 else 2 if hurt <= 66 else 1
    damage_random, state = legacy_bounded(seed, 5)
    base_delta = trunc_div(word(45), divisor) - damage_random
    hp_delta = wrapping_i16(trunc_div(base_delta - 2 * hidden_weapon, 3))
    hurt_after = min(99, max(0, wrapping_i16(hurt - trunc_div(hp_delta, 4))))
    hp_after = wrapping_i16(hp + hp_delta)
    if hp_after >= maximum_hp:
        hp_after = maximum_hp
    if hp_after <= 0:
        hp_after = 0
    damage = wrapping_i16(abs(hp_after - hp))

    item_poison = word(47)
    poison_rng: list[int] = []
    if item_poison > 0:
        poison_delta = wrapping_i16(trunc_div(item_poison - hidden_weapon, 2) - anti_poison)
        if anti_poison >= 100 or poison_delta < 0:
            poison_delta = 0
        poison_delta = wrapping_i16(trunc_div(poison_delta, 2))
    else:
        first, state = legacy_bounded(state, 5)
        second, state = legacy_bounded(state, 5)
        poison_rng = [first, second]
        poison_delta = wrapping_i16(trunc_div(item_poison, 2) + first - second)
    poison_after = wrapping_i16(poison + poison_delta)
    if poison_after >= 99:
        poison_after = 99
    if poison_after <= 0:
        poison_after = 0
    return {
        "item_id": word(0),
        "item_type": word(41),
        "effect_id": word(37),
        "add_hp": word(45),
        "add_poison": item_poison,
        "rng_seed": seed,
        "rng_outputs": [damage_random, *poison_rng],
        "rng_state_after": state,
        "hidden_weapon": hidden_weapon,
        "targeting_range": trunc_div(hidden_weapon, 15) + 1,
        "hp_before": hp,
        "maximum_hp": maximum_hp,
        "hurt_before": hurt,
        "poison_before": poison,
        "anti_poison": anti_poison,
        "hp_delta": hp_delta,
        "damage": damage,
        "hp_after": hp_after,
        "hurt_after": hurt_after,
        "poison_after": poison_after,
    }


def rest_vector(
    *,
    seed: int,
    speed: int,
    round_value: int,
    physical_power: int,
    hp: int,
    maximum_hp: int,
    mp: int,
    maximum_mp: int,
) -> dict[str, object]:
    first, state = legacy_bounded(seed, 3)
    physical_power = wrapping_i16(
        physical_power + first + (3 if round_value == trunc_div(speed, 10) else 2)
    )
    if physical_power > 100:
        physical_power = 100
    outputs = [first]
    if physical_power >= 30:
        bound = trunc_div(physical_power, 10) - 2
        second, state = legacy_bounded(state, bound)
        third, state = legacy_bounded(state, bound)
        outputs.extend([second, third])
        hp = min(maximum_hp, wrapping_i16(hp + second + 3))
        mp = min(maximum_mp, wrapping_i16(mp + third + 3))
    return {
        "rng_seed": seed,
        "rng_outputs": outputs,
        "rng_state_after": state,
        "speed": speed,
        "round_value": round_value,
        "physical_power_after": physical_power,
        "hp_after": hp,
        "mp_after": mp,
        "action_done": 1,
    }


def battle_render_plan_vector(
    field_words: list[int], setup: dict[str, object]
) -> dict[str, object]:
    view_x = 15
    view_y = 17
    path_limit = 5
    primary_cursor = (27, 26)
    secondary_cursor = (25, 27)
    effect_cell = 26 * 64 + 26
    path_values = [0] * 4096
    path_values[25 * 64 + 25] = 10
    path_values[24 * 64 + 24] = 555
    effects = [0] * 4096
    effects[effect_cell] = 1
    occupancy = [-1] * 4096
    combatants: list[dict[str, int]] = []
    for write in setup["static_occupancy_writes"]:
        role_id = int(write["role_id"])
        initial_mode = 2 if write["side"] == "party" else 1
        combatants.append(
            {
                "role_id": role_id,
                "sprite": 8 * (role_id % 17) + 5106 + 2 * initial_mode,
                "damage": 17 if int(write["occupancy_index"]) == effect_cell else 0,
            }
        )
        occupancy[int(write["occupancy_index"])] = int(write["slot"])

    commands: list[list[int]] = []

    def append(
        kind: int,
        map_x: int,
        map_y: int,
        screen_x: int,
        screen_y: int,
        sprite_id: int = 0,
        overlay_variant: int = 0,
        style: int = 0,
        value: int = 0,
    ) -> None:
        commands.append(
            [
                kind,
                map_x,
                map_y,
                screen_x,
                screen_y,
                sprite_id,
                overlay_variant,
                wrapping_i16(style),
                value,
            ]
        )

    for local_x in range(32):
        for local_y in range(32):
            map_x = local_x + view_x
            map_y = local_y + view_y
            cell = map_y * 64 + map_x
            append(
                0,
                map_x,
                map_y,
                18 * local_x - 18 * local_y + 145,
                9 * local_x + 9 * local_y - 81,
                field_words[cell],
            )

    for local_x in range(32):
        for local_y in range(32):
            map_x = local_x + view_x
            map_y = local_y + view_y
            cell = map_y * 64 + map_x
            sprite_x = 18 * local_x - 18 * local_y + 145
            overlay_x = sprite_x - 18
            screen_y = 9 * local_x + 9 * local_y - 81
            if path_values[cell] > path_limit and path_values[cell] != 555:
                append(1, map_x, map_y, overlay_x, screen_y, overlay_variant=0, style=3)
            if (map_x, map_y) == primary_cursor:
                append(1, map_x, map_y, overlay_x, screen_y, overlay_variant=0, style=2)
            if (map_x, map_y) == secondary_cursor:
                append(1, map_x, map_y, overlay_x, screen_y, overlay_variant=1, style=3)
            object_sprite = field_words[4096 + cell]
            if object_sprite not in (0, 15000):
                append(0, map_x, map_y, sprite_x, screen_y, object_sprite)
            occupant = occupancy[cell]
            if occupant >= 0:
                combatant = combatants[occupant]
                if effects[cell] == 1:
                    append(2, map_x, map_y, sprite_x, screen_y, combatant["sprite"], style=47)
                else:
                    append(0, map_x, map_y, sprite_x, screen_y, combatant["sprite"])
            if effects[cell] == 1:
                append(0, map_x, map_y, sprite_x, screen_y, 8)
            if occupant >= 0 and effects[cell] == 1:
                append(
                    3,
                    map_x,
                    map_y,
                    overlay_x,
                    9 * local_x + 9 * local_y - 145,
                    overlay_variant=1,
                    style=0x9193,
                    value=combatants[occupant]["damage"],
                )

    words = [wrapping_i16(value) for command in commands for value in command]
    counts = {str(kind): sum(command[0] == kind for command in commands) for kind in range(4)}
    target_commands = [command for command in commands if command[1:3] == [26, 26]]
    return {
        "battle_id": 4,
        "view": [view_x, view_y],
        "path_limit": path_limit,
        "primary_cursor": list(primary_cursor),
        "secondary_cursor": list(secondary_cursor),
        "effect_cell": [26, 26],
        "command_count": len(commands),
        "command_kind_counts": counts,
        "command_hash": fnv1a_words(words),
        "first_command": commands[0],
        "target_commands": target_commands,
        "zero_path_limit_vector": {
            "secondary_cursor_visible": False,
            "command_count": len(commands) - counts["1"],
            "cursor_overlay_count": 0,
        },
    }


def animation_vectors(effect_counts: list[int]) -> dict[str, object]:
    magic_type = 2
    actor_frame_count = 4
    effect_start = 2
    magic_dispatch_frame = 4
    total_frames = 19
    sprite_base = 100 + 4 * (2 + 3)
    direction = 1
    sprite = 0
    effect_frame = -2 + 2 * sum(effect_counts[:2])
    effect_visible = False
    effect_dispatched = False
    magic_dispatched = False
    magic_words: list[int] = []
    for frame in range(total_frames):
        sprite_updated = frame < actor_frame_count
        if sprite_updated:
            sprite = wrapping_i16(
                2 * actor_frame_count * direction + 2 * sprite_base + 2 * frame
            )
        dispatch_effect = False
        if frame >= effect_start:
            effect_visible = True
            effect_frame = wrapping_i16(effect_frame + 2)
            if not effect_dispatched:
                effect_dispatched = True
                dispatch_effect = True
        dispatch_magic = False
        if frame >= magic_dispatch_frame and not magic_dispatched:
            magic_dispatched = True
            dispatch_magic = True
        magic_words.extend(
            [
                sprite,
                effect_frame,
                17,
                int(sprite_updated),
                int(effect_visible),
                int(dispatch_magic),
                int(dispatch_effect),
            ]
        )

    standalone_words: list[int] = []
    standalone_frame = 2 * sum(effect_counts[:2])
    for _ in range(effect_counts[2]):
        standalone_words.extend([standalone_frame, 17, 1])
        standalone_frame = wrapping_i16(standalone_frame + 2)

    damage_words: list[int] = []
    suppressed_words: list[int] = []
    for frame in range(10):
        damage_words.extend([frame, 1, int(frame < 4)])
        suppressed_words.extend([frame, 1, 0])
    return {
        "magic": {
            "magic_type": magic_type,
            "effect_id": 2,
            "fight_frame_count": 100,
            "direction": direction,
            "actor_frame_count": actor_frame_count,
            "effect_start_frame": effect_start,
            "magic_dispatch_frame": magic_dispatch_frame,
            "frame_count": total_frames,
            "first_sprite": 248,
            "last_sprite": 254,
            "first_effect_frame": 48,
            "last_effect_frame": 80,
            "frame_hash": fnv1a_words(magic_words),
        },
        "standalone_effect": {
            "effect_id": 2,
            "magic_sample_id": 13,
            "prelude_wait_ticks": 100,
            "frame_count": effect_counts[2],
            "first_effect_frame": 48,
            "last_effect_frame": 80,
            "frame_hash": fnv1a_words(standalone_words),
        },
        "damage_display": {
            "frame_count": 10,
            "wait_ticks": 1,
            "flash_frames": [0, 1, 2, 3],
            "frame_hash": fnv1a_words(damage_words),
            "suppressed_hash": fnv1a_words(suppressed_words),
        },
    }


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
    z_dat_bytes = (data_root / "Z.DAT").read_bytes()
    effect_end = Z_DAT_EFFECT_FRAME_COUNTS_OFFSET + EFFECT_FRAME_COUNT * 2
    if effect_end > len(z_dat_bytes):
        raise ValueError("Z.DAT does not contain the battle effect frame-count table")
    effect_table_bytes = z_dat_bytes[Z_DAT_EFFECT_FRAME_COUNTS_OFFSET:effect_end]
    effect_counts = list(struct.unpack(f"<{EFFECT_FRAME_COUNT}h", effect_table_bytes))
    if any(value <= 0 for value in effect_counts):
        raise ValueError("battle effect frame-count table contains a non-positive value")

    ranger_group_bytes = (data_root / "RANGER.GRP").read_bytes()
    magic_bytes = ranger_group_bytes[101_444:114_092]
    if len(magic_bytes) != 93 * 136:
        raise ValueError("RANGER.GRP does not contain 93 complete magic records")
    magic_effect_ids = [
        struct.unpack_from("<h", magic_bytes, index * 136 + 13 * 2)[0]
        for index in range(93)
    ]
    item_bytes = ranger_group_bytes[59_076:97_076]
    if len(item_bytes) != 200 * 190:
        raise ValueError("RANGER.GRP does not contain 200 complete item records")
    item_records = [item_bytes[index * 190:(index + 1) * 190] for index in range(200)]
    poisoned_throw = throwing_weapon_vector(
        item_records[102],
        seed=1,
        hidden_weapon=20,
        hp=100,
        maximum_hp=200,
        hurt=40,
        poison=10,
        anti_poison=5,
    )
    plain_throw = throwing_weapon_vector(
        item_records[96],
        seed=2,
        hidden_weapon=20,
        hp=100,
        maximum_hp=200,
        hurt=0,
        poison=10,
        anti_poison=0,
    )
    if poisoned_throw["item_id"] != 102 or poisoned_throw["item_type"] != 4:
        raise ValueError("RANGER.GRP item 102 is not the expected throwing weapon")
    if plain_throw["item_id"] != 96 or plain_throw["item_type"] != 4:
        raise ValueError("RANGER.GRP item 96 is not the expected throwing weapon")

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

    render_plan = battle_render_plan_vector(
        list(struct.unpack("<8192h", warfld_entries[int(setup_records[4]["battlefield_id"])][:16384])),
        setup_records[4],
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
                "magic_selection_vector": {
                    "magic_ids": [6, 0, 5, 0, 7, 0, 0, 0, 0, 0],
                    "current_mp": 6,
                    "need_mp": {"slot_0": 6, "slot_2": 4, "slot_4": 7},
                    "learned_count": 3,
                    "available_slots": [0, 2, -1, -1, -1, -1, -1, -1, -1, -1],
                    "available_count": 2,
                    "initial_cursor": 0,
                    "initial_state_hash": "0xc254d2cd83d7da76",
                    "cursor_after_next_next_previous": 1,
                    "selected_slot": 2,
                    "cancel_out_flag": 1,
                    "panel_width": "17*learned_count+10",
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
                "hp_damage_vector": {
                    "attack": 30,
                    "magic_attack": 30,
                    "defence": 5,
                    "rng_bounds": [20, 20],
                    "rng_values": [18, 18],
                    "rng_state_after": 2524885223,
                    "distance": 1,
                    "damage": 30,
                    "cost_scale": 3,
                    "exact_hp_before": 30,
                    "exact_kill_bonus": 0,
                    "underkill_hp_before": 29,
                    "underkill_level": 4,
                    "underkill_attack_counter": 46,
                    "hurt_after": 3,
                    "poison_after": 4,
                    "fallback_vector": {
                        "allied_knowledge": 162,
                        "enemy_knowledge": 164,
                        "equipment_attack": 6,
                        "equipment_defence": 2,
                        "special_bonus": 3,
                        "rng_bounds": [20, 20, 4, 4],
                        "rng_values": [18, 18, 1, 3],
                        "rng_state_after": 3295386429,
                        "distance": 11,
                        "damage": 14,
                    },
                },
                "poison_vector": {
                    "use_poison": 80,
                    "targeting_range": 6,
                    "target_anti_poison": 20,
                    "target_poison_before": 90,
                    "raw_amount": 15,
                    "applied_amount": 9,
                    "target_poison_after": 99,
                    "direction_after": 3,
                    "effect_hash": "0xab559939923b4f74",
                    "effect_kind": 2,
                    "negative_amount_clamped": 0,
                    "maximum_amount": 99,
                    "action_done": 1,
                    "attack_counter_after": 1,
                    "physical_power_before": 1,
                    "physical_power_after": 0,
                    "empty_cell_marked": True,
                    "friendly_cell_marked": False,
                },
                "detox_vector": {
                    "detoxification": 80,
                    "targeting_range": 6,
                    "rng_seed": 1,
                    "rng_bounds": [10, 10],
                    "rng_outputs": [8, 8],
                    "rng_state_after": 2_524_885_223,
                    "target_poison_before": 90,
                    "raw_amount": 26,
                    "applied_amount": 26,
                    "target_poison_after": 64,
                    "direction_after": 3,
                    "effect_hash": "0xab559939923b4f74",
                    "effect_kind": 3,
                    "threshold_poison": 41,
                    "threshold_detoxification": 20,
                    "threshold_amount": 0,
                    "maximum_amount": 99,
                    "poison_100_after": 100,
                    "poison_101_after": 99,
                    "action_done": 1,
                    "attack_counter_after": 1,
                    "physical_power_before": 1,
                    "physical_power_after": 0,
                    "empty_cell_marked": True,
                    "enemy_cell_marked": False,
                },
                "medicine_vector": {
                    "medicine": 80,
                    "targeting_range": 6,
                    "rng_seed": 1,
                    "rng_bound": 5,
                    "rng_output": 3,
                    "rng_state_after": 1_103_527_590,
                    "target_hp_before": 100,
                    "target_maximum_hp": 200,
                    "target_hurt_before": 40,
                    "hurt_band_amounts": {"25": 67, "26": 63, "51": 56, "76": 43},
                    "healed_amount": 63,
                    "target_hp_after": 163,
                    "target_hurt_after": 0,
                    "threshold_medicine": 20,
                    "threshold_hurt": 41,
                    "threshold_healed_amount": 0,
                    "capped_healed_amount": 10,
                    "direction_after": 3,
                    "effect_hash": "0xab559939923b4f74",
                    "effect_kind": 4,
                    "action_done": 1,
                    "attack_counter_after": 1,
                    "physical_power_before": 51,
                    "physical_power_after_value": 49,
                    "physical_power_after_action": 47,
                    "low_physical_power": 49,
                    "low_physical_power_consumes_rng": False,
                    "empty_cell_marked": True,
                    "enemy_cell_marked": False,
                    "animation_effect_id": 0,
                    "damage_flash_suppressed": True,
                },
                "battle_item_vector": {
                    "menu_filter": {
                        "requested_type": 4,
                        "accepted_types": [3, 4],
                        "inventory_count_ignored": True,
                        "selected_type_4_opens_targeting": True,
                        "result_1_marks_action_done": True,
                    },
                    "poisoned_throw": poisoned_throw,
                    "plain_throw": plain_throw,
                    "targeting": {
                        "direction_after": 3,
                        "effect_hash": "0xab559939923b4f74",
                        "friendly_cell_marked": False,
                        "empty_cell_marked": True,
                        "friendly_consumes_item": False,
                        "empty_consumes_item": False,
                    },
                    "inventory": {
                        "before": [[102, 1], [97, 2], [10, 0], [-1, 0]],
                        "after_consuming_slot_0": [[97, 2], [10, 0], [-1, 0], [-1, 0]],
                        "decrement_wraps_to_int16": True,
                    },
                },
                "wait_auto_render_vector": {
                    "wait_order_before": [10, 20, 30, 40],
                    "wait_source_slot": 1,
                    "wait_order_after": [10, 30, 40, 20],
                    "wait_return_slot": 3,
                    "automatic_flag_after": 1,
                    "automatic_sequence": ["render", "present", "set_flag", "ai"],
                    "render_plan": render_plan,
                },
                "rest_vector": {
                    "ready": rest_vector(
                        seed=1,
                        speed=60,
                        round_value=6,
                        physical_power=50,
                        hp=95,
                        maximum_hp=100,
                        mp=48,
                        maximum_mp=50,
                    ),
                    "tired": rest_vector(
                        seed=1,
                        speed=60,
                        round_value=5,
                        physical_power=25,
                        hp=95,
                        maximum_hp=100,
                        mp=48,
                        maximum_mp=50,
                    ),
                },
                "mp_damage_vector": {
                    "rng_seed": 1,
                    "rng_bounds": [3, 3, 10, 3, 3],
                    "rng_values": [2, 1, 3, 1, 1],
                    "rng_state_after": 4182499122,
                    "add_mp": 20,
                    "hurt_mp": 15,
                    "actor_mp_after": 23,
                    "actor_max_mp_after": 23,
                    "target_mp_after": 35,
                    "drained": 15,
                },
                "area_vectors": {
                    "square": {
                        "scan_order": "x outer, y inner",
                        "center": [26, 26],
                        "radius": 1,
                        "friendly_skip": [25, 25],
                        "marked_cells": 8,
                        "effect_hash": "0xe5f47b0a810ce2bd",
                        "direction_after": 3,
                        "target_distance": 2,
                        "damage": 29,
                        "effect_kind": 1,
                    },
                    "cross": {
                        "scan_order": ["up", "down", "left", "right"],
                        "source": [26, 24],
                        "radius": 2,
                        "friendly_skip": [26, 25],
                        "marked_cells": 7,
                        "effect_hash": "0x3144c415023d9464",
                        "direction_unchanged": 2,
                        "hurt_type_ignored": 1,
                        "damage": 29,
                        "effect_kind": 1,
                    },
                    "mp_square": {
                        "center": [26, 26],
                        "radius": 0,
                        "marked_cells": 1,
                        "effect_hash": "0xab559939923b4f74",
                        "damage": 15,
                        "effect_kind": 3,
                        "last_hp_cost_scale_unchanged": 3,
                    },
                    "line": {
                        "direction_map": {
                            "0": [0, -1],
                            "1": [1, 0],
                            "2": [-1, 0],
                            "3": [0, 1],
                        },
                        "range": 2,
                        "effect_hash": "0xae7c1e4e161ac125",
                        "friendly_skip_hash": "0xab559939923b4f74",
                        "invalid_direction_hash": "0xb9d103fd6854a325",
                        "continues_after_friendly": True,
                        "hurt_type_ignored": 1,
                        "damage": 29,
                        "effect_kind": 1,
                    },
                },
                "animation": {
                    "effect_frame_table": {
                        "z_dat_file_offset": Z_DAT_EFFECT_FRAME_COUNTS_OFFSET,
                        "entry_count": len(effect_counts),
                        "bytes_sha256": sha256(effect_table_bytes),
                        "word_hash": fnv1a_words(effect_counts),
                        "values": effect_counts,
                    },
                    "ranger_magic_effect_ids": {
                        "record_count": len(magic_effect_ids),
                        "minimum": min(magic_effect_ids),
                        "maximum": max(magic_effect_ids),
                        "unique": sorted(set(magic_effect_ids)),
                    },
                    "vectors": animation_vectors(effect_counts),
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
