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
Z_DAT_LOAD_BASE = 0x6600
BATTLE_ENTRY_ADDRESS = 0x31C75
BATTLE_ENTRY_END = 0x31DA0
BATTLE_ENTRY_CALL_OFFSETS = (
    0x05, 0x29, 0x3A, 0x3F, 0x6A, 0x72, 0x94,
    0x9C, 0xAC, 0xBC, 0xC4, 0xC9, 0xEE, 0x111,
)
BATTLE_ENTRY_RELOCATION_OFFSETS = (
    0x10, 0x17, 0x23, 0x34, 0x46, 0x4D, 0x55, 0x5D,
    0x65, 0x78, 0x85, 0x8B, 0x90, 0xA3, 0xA8, 0xB7,
    0xD0, 0xD9, 0xE1, 0xE9, 0xF9, 0x103, 0x11C, 0x125,
)
BATTLE_DATA_LOADER_ADDRESS = 0x31DA0
BATTLE_DATA_LOADER_END = 0x31EB9
BATTLE_DATA_LOADER_CALL_OFFSETS = (
    0x05, 0x11, 0x35, 0x48, 0x51, 0x85,
    0x97, 0x9F, 0xB2, 0xC9, 0xDC, 0xE5,
)
BATTLE_DATA_LOADER_RELOCATION_OFFSETS = (
    0x0D, 0x43, 0x5C, 0x63, 0x6A, 0x73, 0x7C,
    0x81, 0x93, 0xA7, 0xAE, 0xC4, 0xD7, 0x102,
)
BATTLE_PARTY_SETUP_ADDRESS = 0x31EB9
BATTLE_PARTY_SETUP_END = 0x3265C
BATTLE_PARTY_SETUP_CALL_OFFSETS = (
    0x005, 0x0C5, 0x178, 0x247, 0x2E9, 0x307, 0x319,
    0x336, 0x358, 0x38B, 0x457, 0x483, 0x49F, 0x4C4,
    0x4FD, 0x5D8, 0x5F4, 0x607, 0x705,
)
BATTLE_ENEMY_SETUP_ADDRESS = 0x3265C
BATTLE_ENEMY_SETUP_END = 0x3271E
BATTLE_ENEMY_SETUP_CALL_OFFSETS = (0x05, 0x6F)
BATTLE_ROUND_LOOP_ADDRESS = 0x3271E
BATTLE_ROUND_LOOP_END = 0x32A51
BATTLE_ROUND_LOOP_CALL_OFFSETS = (
    0x005, 0x020, 0x09D, 0x0AD, 0x0B5, 0x0D1, 0x28B,
    0x29B, 0x2D0, 0x2DB, 0x2EA, 0x2EF, 0x31A,
)
BATTLE_ROUND_LOOP_RELOCATION_OFFSETS = (
    0x01A, 0x027, 0x02D, 0x033, 0x039, 0x03F, 0x048, 0x054, 0x05D,
    0x067, 0x06F, 0x078, 0x084, 0x08D, 0x097, 0x0A4, 0x0A9, 0x0C6,
    0x0CD, 0x0D9, 0x0E2, 0x0EB, 0x101, 0x10E, 0x115, 0x11F, 0x12C,
    0x139, 0x146, 0x150, 0x15D, 0x16A, 0x177, 0x19D, 0x1A9, 0x1B3,
    0x1C6, 0x1CF, 0x1D8, 0x1E1, 0x1E8, 0x1EF, 0x1F7, 0x206, 0x214,
    0x21A, 0x221, 0x227, 0x22D, 0x236, 0x242, 0x24B, 0x255, 0x25D,
    0x266, 0x272, 0x27B, 0x285, 0x292, 0x297, 0x2AC, 0x2B5, 0x2BE,
    0x2C8, 0x2F7, 0x308, 0x310, 0x321, 0x326,
)
BATTLE_SPEED_SORT_ADDRESS = 0x32A51
BATTLE_SPEED_SORT_END = 0x32B78
BATTLE_SPEED_SORT_CALL_OFFSETS = (0x005, 0x101)
BATTLE_SPEED_SORT_RELOCATION_OFFSETS = (
    0x010, 0x02B, 0x038, 0x03F, 0x049, 0x056, 0x063, 0x070, 0x07A,
    0x087, 0x094, 0x0A1, 0x0A8, 0x0B2, 0x0BF, 0x0CC, 0x0D9, 0x0E3,
    0x0F0,
)
BATTLE_COMBATANT_SWAP_ADDRESS = 0x32B78
BATTLE_COMBATANT_SWAP_END = 0x32E2F
BATTLE_COMBATANT_SWAP_CALL_OFFSETS = (0x005, 0x284, 0x29E)
BATTLE_COMBATANT_SWAP_RELOCATION_OFFSETS = (
    0x019, 0x024, 0x02F, 0x03A, 0x044, 0x04F, 0x05A, 0x065, 0x070,
    0x07B, 0x086, 0x091, 0x09C, 0x0AB, 0x0B2, 0x0B9, 0x0C0, 0x0C7,
    0x0CE, 0x0D5, 0x0DC, 0x0E3, 0x0EA, 0x0F1, 0x0F8, 0x0FF, 0x106,
    0x10D, 0x114, 0x11B, 0x122, 0x129, 0x130, 0x137, 0x13E, 0x145,
    0x14C, 0x153, 0x15A, 0x161, 0x16B, 0x175, 0x184, 0x18D, 0x197,
    0x19F, 0x1B4, 0x1BF, 0x1CA, 0x1D4, 0x1DF, 0x1EA, 0x1F5, 0x200,
    0x20B, 0x216, 0x221, 0x22C, 0x233, 0x23A, 0x244, 0x24E, 0x25D,
    0x266, 0x270, 0x278, 0x294, 0x2AE,
)
Z_DAT_EFFECT_FRAME_COUNTS_OFFSET = 324_814
Z_DAT_AI_SPECIAL_ATTACK_OFFSET = 324_920
EFFECT_FRAME_COUNT = 53
AI_SPECIAL_ATTACK_COUNT = 7
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


def relative_call_target(code: bytes, offset: int, address: int) -> int:
    if code[offset] != 0xE8:
        raise ValueError(f"expected near call opcode at {address + offset:#x}")
    displacement = struct.unpack_from("<i", code, offset + 1)[0]
    return address + offset + 5 + displacement


def battle_entry_contract(z_dat_bytes: bytes) -> dict[str, object]:
    raw_offset = BATTLE_ENTRY_ADDRESS - Z_DAT_LOAD_BASE
    raw = z_dat_bytes[raw_offset:raw_offset + BATTLE_ENTRY_END - BATTLE_ENTRY_ADDRESS]
    if len(raw) != 299:
        raise ValueError("Z.DAT does not contain the complete battle entry function")
    call_targets = [
        relative_call_target(raw, offset, BATTLE_ENTRY_ADDRESS)
        for offset in BATTLE_ENTRY_CALL_OFFSETS
    ]
    expected_targets = [
        0x3ED1E, 0x31DA0, 0x31EB9, 0x3265C, 0x3D6E0, 0x3CC97, 0x3D6E0,
        0x3AA85, 0x3D6D1, 0x3E1B2, 0x3271E, 0x3CC97, 0x3D6E0, 0x3E1B2,
    ]
    if call_targets != expected_targets:
        raise ValueError("Z.DAT battle entry call sequence changed")
    relocated = bytearray(raw)
    for offset in BATTLE_ENTRY_RELOCATION_OFFSETS:
        raw_value = struct.unpack_from("<I", raw, offset)[0]
        struct.pack_into("<I", relocated, offset, raw_value + 0x20000)
    caller_sites = (0x2DE15, 0x304CA)
    caller_targets = []
    caller_post_call_bytes = []
    for site in caller_sites:
        site_offset = site - Z_DAT_LOAD_BASE
        caller_targets.append(
            relative_call_target(z_dat_bytes, site_offset, Z_DAT_LOAD_BASE)
        )
        caller_post_call_bytes.append(
            z_dat_bytes[site_offset + 5:site_offset + 11].hex()
        )
    if caller_targets != [BATTLE_ENTRY_ADDRESS, BATTLE_ENTRY_ADDRESS]:
        raise ValueError("Z.DAT battle entry caller target changed")
    if caller_post_call_bytes != ["83c40883f801", "83c40883f801"]:
        raise ValueError("Z.DAT battle entry caller result comparison changed")
    return {
        "address": hex(BATTLE_ENTRY_ADDRESS),
        "end": hex(BATTLE_ENTRY_END),
        "raw_offset": hex(raw_offset),
        "size": len(raw),
        "instruction_count": 72,
        "raw_sha256": sha256(raw),
        "loaded_sha256": sha256(bytes(relocated)),
        "relocation_count": len(BATTLE_ENTRY_RELOCATION_OFFSETS),
        "relocation_delta": 0x20000,
        "call_sites": [hex(BATTLE_ENTRY_ADDRESS + offset) for offset in BATTLE_ENTRY_CALL_OFFSETS],
        "call_targets": [hex(target) for target in call_targets],
        "call_contract": [
            "stack_probe",
            "load_war_and_battlefield",
            "initialize_party",
            "append_enemies",
            "load_wdx_wmp",
            "fade_scene_to_black",
            "load_eft",
            "draw_battlefield",
            "present_black_battlefield",
            "start_battle_music",
            "run_battle_loop",
            "fade_battle_to_black",
            "load_sdx_smp",
            "restore_scene_music",
        ],
        "callers": [hex(site) for site in caller_sites],
        "caller_result_compare": "eax == 1",
        "result_word": {
            "storage": "signed_int16",
            "transient": 0,
            "defeat": 1,
            "victory": 2,
            "return_expression": "result_word - 1",
            "returned_defeat": 0,
            "returned_victory": 1,
        },
        "mode_word": {"during_battle": 2, "after_battle": 1},
        "argument_storage": {"battle_id": "signed_int16", "grant_experience": "signed_int16"},
        "transition_frames": {
            "scene_fade_to_black": 64,
            "black_battlefield_present": 1,
            "battle_initial_fade_after_present": 66,
            "battle_exit_fade_to_black": 64,
        },
    }


def battle_data_loader_contract(z_dat_bytes: bytes) -> dict[str, object]:
    raw_offset = BATTLE_DATA_LOADER_ADDRESS - Z_DAT_LOAD_BASE
    raw = z_dat_bytes[
        raw_offset:raw_offset + BATTLE_DATA_LOADER_END - BATTLE_DATA_LOADER_ADDRESS
    ]
    if len(raw) != 281:
        raise ValueError("Z.DAT does not contain the complete battle data loader")
    call_targets = [
        relative_call_target(raw, offset, BATTLE_DATA_LOADER_ADDRESS)
        for offset in BATTLE_DATA_LOADER_CALL_OFFSETS
    ]
    expected_targets = [
        0x3ED1E, 0x3DA66, 0x3DB07, 0x3DABF, 0x3DB2B, 0x3CF45,
        0x3EF96, 0x20C32, 0x3DA66, 0x3DB07, 0x3DABF, 0x3DB2B,
    ]
    if call_targets != expected_targets:
        raise ValueError("Z.DAT battle data loader call sequence changed")
    relocated = bytearray(raw)
    for offset in BATTLE_DATA_LOADER_RELOCATION_OFFSETS:
        raw_value = struct.unpack_from("<I", raw, offset)[0]
        struct.pack_into("<I", relocated, offset, raw_value + 0x20000)
    caller_site = 0x31C9E
    caller_offset = caller_site - Z_DAT_LOAD_BASE
    if (
        relative_call_target(z_dat_bytes, caller_offset, Z_DAT_LOAD_BASE)
        != BATTLE_DATA_LOADER_ADDRESS
    ):
        raise ValueError("Z.DAT battle data loader caller target changed")
    return {
        "address": hex(BATTLE_DATA_LOADER_ADDRESS),
        "end": hex(BATTLE_DATA_LOADER_END),
        "raw_offset": hex(raw_offset),
        "size": len(raw),
        "instruction_count": 80,
        "raw_sha256": sha256(raw),
        "loaded_sha256": sha256(bytes(relocated)),
        "relocation_count": len(BATTLE_DATA_LOADER_RELOCATION_OFFSETS),
        "relocation_delta": 0x20000,
        "call_sites": [
            hex(BATTLE_DATA_LOADER_ADDRESS + offset)
            for offset in BATTLE_DATA_LOADER_CALL_OFFSETS
        ],
        "call_targets": [hex(target) for target in call_targets],
        "call_contract": [
            "stack_probe",
            "open_war_sta",
            "seek_war_record",
            "read_war_record",
            "close_war_sta",
            "load_warfld_idx_if_cache_miss",
            "print_warfld_idx_error",
            "terminate_after_warfld_idx_error",
            "open_warfld_grp",
            "seek_warfld_entry",
            "read_battlefield_prefix",
            "close_warfld_grp",
        ],
        "callers": [hex(caller_site)],
        "battle_id_storage": "signed_int16",
        "war_record": {
            "size": WAR_RECORD_SIZE,
            "offset_expression": "sign_extend_i16(battle_id) * 186",
            "battlefield_id_word": 6,
        },
        "warfld": {
            "cache_tag": 6,
            "index_layout": "synthetic_zero_then_cumulative_ends",
            "seek_expression": "offsets[sign_extend_i16(battlefield_id)]",
            "battlefield_prefix_size": 0x4000,
        },
        "occupancy": {
            "width": 64,
            "height": 64,
            "loop_order": "y_then_x",
            "fill_value": -1,
        },
    }


def battle_setup_machine_contract(z_dat_bytes: bytes) -> dict[str, object]:
    party_raw_offset = BATTLE_PARTY_SETUP_ADDRESS - Z_DAT_LOAD_BASE
    party_raw = z_dat_bytes[
        party_raw_offset:party_raw_offset + BATTLE_PARTY_SETUP_END - BATTLE_PARTY_SETUP_ADDRESS
    ]
    if len(party_raw) != 1955:
        raise ValueError("Z.DAT does not contain the complete battle party setup")
    party_calls = [
        relative_call_target(party_raw, offset, BATTLE_PARTY_SETUP_ADDRESS)
        for offset in BATTLE_PARTY_SETUP_CALL_OFFSETS
    ]
    expected_party_calls = [
        0x3ED1E, 0x3B1E6, 0x3B1E6, 0x3B1E6, 0x29D2D, 0x2CEBF,
        0x3EF4A, 0x3D832, 0x2CEBF, 0x3EF4A, 0x3D832, 0x3D832,
        0x3EF4A, 0x3D832, 0x3EF4A, 0x3EF4A, 0x3D832, 0x3D6D1, 0x3B1E6,
    ]
    if party_calls != expected_party_calls:
        raise ValueError("Z.DAT battle party setup call sequence changed")
    party_caller = 0x31CAF
    if (
        relative_call_target(
            z_dat_bytes, party_caller - Z_DAT_LOAD_BASE, Z_DAT_LOAD_BASE
        )
        != BATTLE_PARTY_SETUP_ADDRESS
    ):
        raise ValueError("Z.DAT battle party setup caller target changed")

    enemy_raw_offset = BATTLE_ENEMY_SETUP_ADDRESS - Z_DAT_LOAD_BASE
    enemy_raw = z_dat_bytes[
        enemy_raw_offset:enemy_raw_offset + BATTLE_ENEMY_SETUP_END - BATTLE_ENEMY_SETUP_ADDRESS
    ]
    if len(enemy_raw) != 194:
        raise ValueError("Z.DAT does not contain the complete battle enemy setup")
    enemy_calls = [
        relative_call_target(enemy_raw, offset, BATTLE_ENEMY_SETUP_ADDRESS)
        for offset in BATTLE_ENEMY_SETUP_CALL_OFFSETS
    ]
    if enemy_calls != [0x3ED1E, 0x3B1E6]:
        raise ValueError("Z.DAT battle enemy setup call sequence changed")
    enemy_caller = 0x31CB4
    if (
        relative_call_target(
            z_dat_bytes, enemy_caller - Z_DAT_LOAD_BASE, Z_DAT_LOAD_BASE
        )
        != BATTLE_ENEMY_SETUP_ADDRESS
    ):
        raise ValueError("Z.DAT battle enemy setup caller target changed")

    return {
        "party": {
            "address": hex(BATTLE_PARTY_SETUP_ADDRESS),
            "end": hex(BATTLE_PARTY_SETUP_END),
            "raw_offset": hex(party_raw_offset),
            "size": len(party_raw),
            "instruction_count": 453,
            "raw_sha256": sha256(party_raw),
            "ida_loaded_sha256": "6743a9d962317bde209fd1a6c36b54a60678d374fa9ca5a90dfa6b9a934feb0f",
            "ida_relocation_count": 144,
            "call_sites": [
                hex(BATTLE_PARTY_SETUP_ADDRESS + offset)
                for offset in BATTLE_PARTY_SETUP_CALL_OFFSETS
            ],
            "call_targets": [hex(target) for target in party_calls],
            "callers": [hex(party_caller)],
            "combatant_slots": 26,
            "combatant_words": 14,
            "party_slots": 6,
            "party_prefix_rule": "slot0 unconditional; first slot 1..5 with signed id <= 0 ends prefix; otherwise 6",
            "fixed_party_words": [15, 20],
            "preset_party_words": [9, 14],
            "preset_rule": "append every non--1 preset before scanning the party prefix only to mark matching mandatory states",
            "party_coordinate_words": [[21, 26], [27, 32]],
            "selection_states": {"unselected": 0, "selected": 1, "mandatory": 2},
            "confirm_index": "party_prefix_length",
            "confirm_requires_nonempty_combatants": True,
        },
        "enemy": {
            "address": hex(BATTLE_ENEMY_SETUP_ADDRESS),
            "end": hex(BATTLE_ENEMY_SETUP_END),
            "raw_offset": hex(enemy_raw_offset),
            "size": len(enemy_raw),
            "instruction_count": 42,
            "raw_sha256": sha256(enemy_raw),
            "ida_loaded_sha256": "dd7d372cd20ca5f93ab279c9f30d6ec874aa1e695bb2b367c4112c11a6c8de3a",
            "ida_relocation_count": 17,
            "call_sites": [
                hex(BATTLE_ENEMY_SETUP_ADDRESS + offset)
                for offset in BATTLE_ENEMY_SETUP_CALL_OFFSETS
            ],
            "call_targets": [hex(target) for target in enemy_calls],
            "callers": [hex(enemy_caller)],
            "enemy_slots": 20,
            "empty_sentinel": -1,
            "enemy_words": [33, 52],
            "enemy_coordinate_words": [[53, 72], [73, 92]],
            "write_order": [
                "role_id", "side", "x", "y", "initial_mode", "sprite", "occupancy", "count",
            ],
            "duplicate_occupancy": "later_write_wins",
        },
    }


def relocated_machine_function_contract(
    z_dat_bytes: bytes,
    *,
    address: int,
    end: int,
    call_offsets: tuple[int, ...],
    expected_call_targets: tuple[int, ...],
    relocation_offsets: tuple[int, ...],
    caller_sites: tuple[int, ...],
    instruction_count: int,
    branch_count: int,
) -> dict[str, object]:
    raw_offset = address - Z_DAT_LOAD_BASE
    raw = z_dat_bytes[raw_offset:raw_offset + end - address]
    if len(raw) != end - address:
        raise ValueError(f"Z.DAT does not contain the complete function at {address:#x}")
    call_targets = tuple(
        relative_call_target(raw, offset, address) for offset in call_offsets
    )
    if call_targets != expected_call_targets:
        raise ValueError(f"Z.DAT call sequence changed at {address:#x}")
    caller_targets = tuple(
        relative_call_target(
            z_dat_bytes, caller_site - Z_DAT_LOAD_BASE, Z_DAT_LOAD_BASE
        )
        for caller_site in caller_sites
    )
    if caller_targets != (address,) * len(caller_sites):
        raise ValueError(f"Z.DAT caller set changed at {address:#x}")
    relocated = bytearray(raw)
    for offset in relocation_offsets:
        raw_value = struct.unpack_from("<I", raw, offset)[0]
        struct.pack_into("<I", relocated, offset, raw_value + 0x20000)
    return {
        "address": hex(address),
        "end": hex(end),
        "raw_offset": hex(raw_offset),
        "size": len(raw),
        "instruction_count": instruction_count,
        "branch_count": branch_count,
        "raw_sha256": sha256(raw),
        "loaded_sha256": sha256(bytes(relocated)),
        "relocation_count": len(relocation_offsets),
        "relocation_delta": 0x20000,
        "call_sites": [hex(address + offset) for offset in call_offsets],
        "call_targets": [hex(target) for target in call_targets],
        "callers": [hex(site) for site in caller_sites],
    }


def battle_round_machine_contract(
    z_dat_bytes: bytes, ranger_group_bytes: bytes
) -> dict[str, object]:
    round_values = []
    for label, base_speed, equipment_speeds, hurt in (
        ("ordinary", 100, [7, -10], 79),
        ("positive_wrap", 32760, [20], 0),
        ("negative_wrap", -32760, [-20], -79),
    ):
        effective_speed = base_speed
        for item_speed in equipment_speeds:
            effective_speed = wrapping_i16(effective_speed + item_speed)
        value = trunc_div(effective_speed, 15) - trunc_div(hurt, 40)
        round_values.append({
            "label": label,
            "base_speed": base_speed,
            "equipment_add_speed": equipment_speeds,
            "hurt": hurt,
            "effective_speed": effective_speed,
            "round_value": max(value, 0),
        })

    speed_entries = [
        {"label": "equal_a", "speed": 10},
        {"label": "equal_b", "speed": 10},
        {"label": "fast", "speed": 20},
        {"label": "slow", "speed": -3},
    ]
    sorted_entries = [dict(entry) for entry in speed_entries]
    swap_trace = []
    for first in range(len(sorted_entries) - 1):
        for second in range(first + 1, len(sorted_entries)):
            if sorted_entries[first]["speed"] < sorted_entries[second]["speed"]:
                swap_trace.append([first, second])
                sorted_entries[first], sorted_entries[second] = (
                    sorted_entries[second], sorted_entries[first]
                )

    first = [10, 0, 12, 13, 2, 0, 7, 8, 6000, 9, 10, 11, 12, 13]
    second = [20, 1, 12, 13, 3, 0, 17, 18, 6100, 19, 20, 21, 22, 23]
    copied_words = [*range(0, 8), *range(9, 14)]
    exchanged_first = list(first)
    exchanged_second = list(second)
    for word in copied_words:
        exchanged_first[word] = second[word]
    occupancy_writes = [{"slot": 0, "x": second[2], "y": second[3], "value": 0}]
    for word in copied_words:
        exchanged_second[word] = first[word]
    occupancy_writes.append({"slot": 1, "x": first[2], "y": first[3], "value": 1})
    sprite_globals = [
        struct.unpack_from("<h", z_dat_bytes, address - Z_DAT_LOAD_BASE)[0]
        for address in (0x556CC, 0x556D4)
    ]
    sprite_results = []
    for words in (exchanged_first, exchanged_second):
        head = struct.unpack_from("<h", ranger_group_bytes, 836 + words[0] * 182 + 2)[0]
        words[8] = wrapping_i16(head * 8 + sprite_globals[0] * 2 + sprite_globals[1] + words[4] * 2)
        sprite_results.append(words[8])

    outcome_calls = {
        site: relative_call_target(z_dat_bytes, site - Z_DAT_LOAD_BASE, Z_DAT_LOAD_BASE)
        for site in (0x3B300, 0x3B371, 0x3B379, 0x3B37E)
    }
    if list(outcome_calls.values()) != [0x3AA85, 0x3D6D1, 0x20C32, 0x3B387]:
        raise ValueError("Z.DAT result display/wait/settlement boundary changed")
    shared_exit = z_dat_bytes[0x35407 - Z_DAT_LOAD_BASE:0x3540E - Z_DAT_LOAD_BASE]
    if shared_exit.hex() != "83c4045f5e5bc3":
        raise ValueError("Z.DAT battle loop shared return changed")

    return {
        "round_loop": {
            **relocated_machine_function_contract(
                z_dat_bytes,
                address=BATTLE_ROUND_LOOP_ADDRESS,
                end=BATTLE_ROUND_LOOP_END,
                call_offsets=BATTLE_ROUND_LOOP_CALL_OFFSETS,
                expected_call_targets=(
                    0x3ED1E, 0x32A51, 0x3AA85, 0x3D6D1, 0x3CD17,
                    0x32A51, 0x3AA85, 0x3D6D1, 0x32E59, 0x33599,
                    0x3B238, 0x3C672, 0x3C563,
                ),
                relocation_offsets=BATTLE_ROUND_LOOP_RELOCATION_OFFSETS,
                caller_sites=(0x31D39,),
                instruction_count=166,
                branch_count=27,
            ),
            "round_value_vectors": round_values,
            "sequence": [
                "initial_sort", "initial_actor_present", "initial_fade",
                "capture_tick", "round_sort_once", "clear_render_globals",
                "compute_round_values", "actor_loop", "round_status",
                "wait_until_tick_differs", "repeat_or_return",
            ],
            "confirm_state_offsets": [0x0D, 0x20, 0x96],
            "confirm_keys": ["enter", "space", "keypad_insert"],
            "confirm_effect": "sample current key states before each slot hidden check; clear all three and disable automatic mode before actor rendering",
            "shared_exit": {"address": "0x35407", "end": "0x3540e", "bytes": shared_exit.hex()},
            "released_confirmation": "a key released before the slot boundary does not cancel automatic mode",
            "hidden_actor": "skip dispatch, then evaluate outcome and clear hidden AI targets",
            "action_result_six": "decrement actor index before common post-action calls",
            "outcome_boundary": [
                "evaluate_result", "result_screen_present", "wait_for_key",
                "complete_post_battle_settlement_and_messages",
                "clear_hidden_ai_targets", "apply_round_status_once",
                "wait_until_tick_differs_without_redraw", "return_to_battle_entry",
            ],
            "outcome_callee_boundary_calls": {
                hex(site): hex(target) for site, target in outcome_calls.items()
            },
        },
        "speed_sort": {
            **relocated_machine_function_contract(
                z_dat_bytes,
                address=BATTLE_SPEED_SORT_ADDRESS,
                end=BATTLE_SPEED_SORT_END,
                call_offsets=BATTLE_SPEED_SORT_CALL_OFFSETS,
                expected_call_targets=(0x3ED1E, 0x32B78),
                relocation_offsets=BATTLE_SPEED_SORT_RELOCATION_OFFSETS,
                caller_sites=(0x3273E, 0x327EF),
                instruction_count=69,
                branch_count=9,
            ),
            "algorithm": "pairwise exchange sort, fixed first slot against every later slot, descending signed effective speed",
            "swap_condition": "first_speed < second_speed",
            "equal_speed_order": "no direct swap on equality; indirect exchanges may reverse equal-speed actors",
            "input": speed_entries,
            "swap_trace": swap_trace,
            "output": sorted_entries,
        },
        "combatant_swap": {
            **relocated_machine_function_contract(
                z_dat_bytes,
                address=BATTLE_COMBATANT_SWAP_ADDRESS,
                end=BATTLE_COMBATANT_SWAP_END,
                call_offsets=BATTLE_COMBATANT_SWAP_CALL_OFFSETS,
                expected_call_targets=(0x3ED1E, 0x3B1E6, 0x3B1E6),
                relocation_offsets=BATTLE_COMBATANT_SWAP_RELOCATION_OFFSETS,
                caller_sites=(0x32B52, 0x3AA30),
                instruction_count=131,
                branch_count=4,
            ),
            "copied_word_indexes": copied_words,
            "excluded_word_index": 8,
            "occupancy_write_order": "first_slot_then_second_slot",
            "sprite_recompute_order": "first_slot_then_second_slot",
            "vector": {
                "input": [first, second],
                "asset_initial_sprite_globals": sprite_globals,
                "asset_head_role_ids": [20, 10],
                "delegated_sprite_results": sprite_results,
                "occupancy_writes": occupancy_writes,
                "final": [exchanged_first, exchanged_second],
                "same_coordinate_final_occupancy": 1,
            },
        },
    }


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


def fnv1a_bytes(data: bytes | bytearray) -> str:
    value = 0xCBF29CE484222325
    for byte in data:
        value ^= byte
        value = value * 0x100000001B3 & 0xFFFFFFFFFFFFFFFF
    return f"0x{value:016x}"


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


def ai_throwing_weapon_vector(
    item: bytes,
    *,
    seed: int,
    hidden_weapon: int,
    hp: int,
    maximum_hp: int,
    hurt: int,
    poison: int,
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
    poison_delta = (
        wrapping_i16(trunc_div(item_poison - hidden_weapon, 2))
        if item_poison < 0
        else item_poison
    )
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
        "rng_outputs": [damage_random],
        "rng_state_after": state,
        "hidden_weapon": hidden_weapon,
        "hp_before": hp,
        "maximum_hp": maximum_hp,
        "hurt_before": hurt,
        "poison_before": poison,
        "hp_delta": hp_delta,
        "damage": damage,
        "hp_after": hp_after,
        "hurt_after": hurt_after,
        "poison_delta": poison_delta,
        "poison_after": poison_after,
    }


def shared_item_effect_vector(
    item: bytes,
    *,
    seed: int,
    role_words: list[int],
) -> dict[str, object]:
    item_word = lambda index: struct.unpack_from("<h", item, index * 2)[0]
    role = list(role_words)
    before = list(role)
    deltas = [0] * 23
    outputs: list[int] = []
    state = seed
    hidden_weapon = role[54]

    item_hp = item_word(45)
    if item_hp:
        old_hp = role[17]
        if item_hp > 0:
            value, state = legacy_bounded(state, 10)
            outputs.append(value)
            hp_delta = item_hp - trunc_div(role[19], 2) + value
            if hp_delta < 0:
                value, state = legacy_bounded(state, 5)
                outputs.append(value)
                hp_delta = value + 5
            role[19] = wrapping_i16(role[19] - trunc_div(item_hp, 4))
            role[19] = min(99, max(0, role[19]))
        else:
            value, state = legacy_bounded(state, 10)
            outputs.append(value)
            damage_base = item_hp + 50 - trunc_div(role[19], 2) - value
            if damage_base > 0:
                value, state = legacy_bounded(state, 5)
                outputs.append(value)
                damage_base = -5 - value
            hp_delta = trunc_div(damage_base - 3 * hidden_weapon, 3)
            role[19] = wrapping_i16(role[19] - trunc_div(hp_delta, 10))
            role[19] = min(99, max(0, role[19]))
        role[17] = wrapping_i16(role[17] + hp_delta)
        role[17] = min(role[18], max(0, role[17]))
        deltas[0] = wrapping_i16(role[17] - old_hp)

    if item_word(46):
        role[18] = wrapping_i16(role[18] + item_word(46))
        role[18] = min(999, max(0, role[18]))
        if role[17] >= role[18]:
            role[17] = role[18]
        deltas[1] = item_word(46)

    item_poison = item_word(47)
    if item_poison:
        old_poison = role[20]
        if item_poison > 0:
            poison_delta = trunc_div(hidden_weapon + item_poison, 2) - role[49]
            if role[49] >= 100 or poison_delta < 0:
                poison_delta = 0
            poison_delta = trunc_div(poison_delta, 2)
        else:
            first, state = legacy_bounded(state, 5)
            second, state = legacy_bounded(state, 5)
            outputs.extend((first, second))
            poison_delta = trunc_div(item_poison, 2) + first - second
        role[20] = wrapping_i16(role[20] + poison_delta)
        role[20] = min(99, max(0, role[20]))
        deltas[2] = wrapping_i16(role[20] - old_poison)

    if item_word(48):
        old_value = role[21]
        role[21] = wrapping_i16(role[21] + item_word(48))
        role[21] = min(100, max(0, role[21]))
        deltas[3] = wrapping_i16(role[21] - old_value)

    if item_word(49) == 2:
        role[40] = 2
        deltas[4] = 2

    if item_word(50):
        old_value = role[41]
        role[41] = wrapping_i16(role[41] + item_word(50))
        role[41] = min(role[42], max(0, role[41]))
        deltas[5] = wrapping_i16(role[41] - old_value)

    if item_word(51):
        role[42] = wrapping_i16(role[42] + item_word(51))
        role[42] = min(999, max(0, role[42]))
        if role[41] >= role[42]:
            role[41] = role[42]
        deltas[6] = item_word(51)

    for index in range(13):
        delta = item_word(52 + index)
        role[43 + index] = wrapping_i16(role[43 + index] + delta)
        deltas[7 + index] = delta
    deltas[20] = item_word(65)
    deltas[21] = item_word(66)
    role[57] = wrapping_i16(role[57] + item_word(67))
    deltas[22] = item_word(67)

    selected_names = {
        17: "hp",
        18: "maximum_hp",
        19: "hurt",
        20: "poison",
        21: "physical_power",
        40: "mp_type",
        41: "mp",
        42: "maximum_mp",
        43: "attack",
        44: "speed",
        45: "defence",
        49: "anti_poison",
        54: "hidden_weapon",
        56: "morality",
        57: "attack_with_poison",
        58: "attack_twice",
    }
    return {
        "item_id": item_word(0),
        "item_type": item_word(41),
        "rng_seed": seed,
        "rng_outputs": outputs,
        "rng_state_after": state,
        "deltas": deltas,
        "effect_count": sum(value != 0 for value in deltas),
        "role_before": {name: before[index] for index, name in selected_names.items()},
        "role_after": {name: role[index] for index, name in selected_names.items()},
    }


def ai_request_vectors() -> dict[str, object]:
    return {
        "entry_aliases": ["request_medicine", "request_detox"],
        "target_slot": 1,
        "target": [13, 23],
        "positive_round_value": {
            "round_value": 3,
            "next_step": "move",
            "movement_mode": 0,
            "movement_value": 0,
        },
        "zero_round_value": {
            "round_value": 0,
            "next_step": "automatic_attack",
            "movement_called": False,
        },
        "negative_round_value": {
            "round_value": -1,
            "next_step": "automatic_attack",
            "movement_called": False,
        },
        "restore_request_target_before_attack": True,
        "automatic_attack_after_move_or_skip": True,
        "outer_marks_action_done_after_handler": True,
        "rng_consumed_before_automatic_attack": False,
    }


def ai_support_vectors() -> dict[str, object]:
    medicine = 30
    detoxification = 45
    medicine_range = trunc_div(medicine, 15) + 1
    detox_range = trunc_div(detoxification, 15) + 1
    allied_total = wrapping_i16(30_000 + 30_000 + 10_000 + 10_000)
    allied_count = 2
    doubled_average = trunc_div(2 * allied_total, allied_count)
    return {
        "medicine": {
            "ability": medicine,
            "targeting_range": medicine_range,
            "direct_distance": medicine_range,
            "direct_next_step": "apply_support",
        },
        "detox": {
            "ability": detoxification,
            "targeting_range": detox_range,
            "direct_distance": detox_range,
            "direct_next_step": "apply_support",
        },
        "out_of_range_positive_round": {
            "round_value": 3,
            "range_checks_before_move": 1,
            "movement_mode": 1,
            "movement_value_source": "targeting_range",
            "next_step": "move",
        },
        "out_of_range_zero_round": {
            "round_value": 0,
            "range_checks": 2,
            "movement_called": False,
        },
        "fallback": {
            "allied_total_wrapped": allied_total,
            "allied_count": allied_count,
            "doubled_allied_average": doubled_average,
            "actor_attack_30000_doubled": 60_000,
            "actor_attack_30000_next_step": "automatic_attack",
            "actor_attack_7232_next_step": "rest",
            "strict_greater_for_attack": True,
        },
        "restore_same_target_after_move": True,
        "rng_consumed_before_support_or_fallback": False,
        "outer_marks_action_done_after_handler": True,
    }


def ai_movement_plan_vector(
    field_words: list[int],
    occupied: set[int],
    *,
    source: tuple[int, int],
    target: tuple[int, int],
    round_value: int,
    mode: int,
    range_value: int,
) -> dict[str, object]:
    targeting = build_path_map(field_words, target, "targeting")
    target_distance = targeting[source[1] * 64 + source[0]]
    within_turn_range = target_distance - round_value <= range_value
    destination = target
    selected_layer = -1
    movement_builds = 0
    selection = "generic_reachable_neighbor"
    values = targeting

    if mode == 2 and within_turn_range or mode == 3:
        selection = "aligned_range_layer" if mode == 2 else "range_layer"
        values = build_path_map(field_words, target, "movement", occupied)
        movement_builds += 1
        layer = range_value
        while True:
            found = False
            best_distance = 1000
            best = (0, 0)
            for x in range(64):
                for y in range(64):
                    if values[y * 64 + x] != layer:
                        continue
                    if mode == 2 and x != target[0] and y != target[1]:
                        continue
                    found = True
                    distance = abs(x - source[0]) + abs(y - source[1])
                    if distance < best_distance:
                        best = (x, y)
                        best_distance = distance
            if found:
                destination = best
                selected_layer = layer
                break
            layer = wrapping_i16(layer - 1)
            if layer == 0:
                break
    else:
        values = build_path_map(field_words, source, "movement", occupied)
        movement_builds += 1
        cursor = target
        for _ in range(4096):
            found = False
            for dx, dy in PATH_DIRECTIONS:
                candidate = (cursor[0] + dx, cursor[1] + dy)
                values = build_path_map(field_words, source, "movement", occupied)
                movement_builds += 1
                index = legacy_path_index(*candidate)
                value = values[index] if index is not None else 555
                if value < 128 and (candidate[0] == source[0] or candidate[1] == source[1]):
                    cursor = candidate
                    found = True
                    break
            if not found:
                for dx, dy in PATH_DIRECTIONS:
                    candidate = (cursor[0] + dx, cursor[1] + dy)
                    values = build_path_map(field_words, source, "movement", occupied)
                    movement_builds += 1
                    index = legacy_path_index(*candidate)
                    value = values[index] if index is not None else 555
                    if value < 128:
                        cursor = candidate
                        found = True
                        break
            if found:
                break
            x, y = cursor
            if x > source[0] and x > 0:
                x -= 1
            elif x < source[0] and x < 63:
                x += 1
            elif y > source[1] and y > 0:
                y -= 1
            elif y < source[1] and y < 63:
                y += 1
            cursor = (x, y)
            if cursor == source:
                break
        destination = cursor

    first_index = legacy_path_index(*destination)
    first_reachable = destination != source and first_index is not None and values[first_index] < 128
    second_reachable = False
    marked = False
    first_step = None
    if first_reachable:
        values = build_path_map(field_words, source, "movement", occupied)
        movement_builds += 1
        second_reachable = values[destination[1] * 64 + destination[0]] < 128
        if second_reachable:
            marked = mark_path(values, source, destination)
            if marked:
                first_step = next(
                    (
                        [source[0] + dx, source[1] + dy]
                        for dx, dy in PATH_DIRECTIONS
                        if 0 <= source[0] + dx < 64
                        and 0 <= source[1] + dy < 64
                        and values[(source[1] + dy) * 64 + source[0] + dx] == 250
                    ),
                    None,
                )
    return {
        "mode": mode,
        "range": range_value,
        "round_value": round_value,
        "selection": selection,
        "source": list(source),
        "target": list(target),
        "target_distance": target_distance,
        "within_turn_range": within_turn_range,
        "destination": list(destination),
        "selected_distance_layer": selected_layer,
        "movement_map_build_count": movement_builds,
        "first_reachability_passed": first_reachable,
        "second_reachability_passed": second_reachable,
        "path_marked": marked,
        "marked_path_hash": fnv1a_words(values),
        "first_step": first_step,
    }


def ai_movement_vectors(
    field_words: list[int], occupied: set[int]
) -> dict[str, object]:
    source = (26, 24)
    target = (26, 26)
    return {
        "battle_id": 4,
        "battlefield_id": 2,
        "direction_order": ["up", "right", "left", "down"],
        "mode_0": ai_movement_plan_vector(
            field_words, occupied, source=source, target=target,
            round_value=1, mode=0, range_value=0,
        ),
        "mode_1": ai_movement_plan_vector(
            field_words, occupied, source=source, target=target,
            round_value=8, mode=1, range_value=1,
        ),
        "mode_2": ai_movement_plan_vector(
            field_words, occupied, source=source, target=target,
            round_value=20, mode=2, range_value=3,
        ),
        "mode_3": ai_movement_plan_vector(
            field_words, occupied, source=source, target=target,
            round_value=20, mode=3, range_value=3,
        ),
        "first_step_state": {
            "source_path_after_step": 255,
            "old_occupancy": -1,
            "new_occupancy": 0,
            "round_value": [8, 7],
            "speed_div_10": 8,
            "physical_power": [10, 9],
            "render_required": True,
            "present_required": True,
            "wait_ticks": 40,
        },
        "mode_0_round_exhaustion_stops_after_first_step": True,
    }


def player_cursor_vectors(
    field_words: list[int], occupied: set[int]
) -> dict[str, object]:
    source = (26, 24)
    destination = (26, 25)
    occupied_target = (26, 26)
    movement = build_path_map(field_words, source, "movement", occupied)
    targeting = build_path_map(field_words, source, "targeting", occupied)
    movement_target_value = movement[occupied_target[1] * 64 + occupied_target[0]]
    targeting_target_value = targeting[occupied_target[1] * 64 + occupied_target[0]]
    marked = movement.copy()
    path_marked = mark_path(marked, source, destination)
    return {
        "battle_id": 4,
        "battlefield_id": 2,
        "source": list(source),
        "path_limit": 2,
        "input_priority": ["down", "right", "left", "up", "cancel", "activate"],
        "movement": {
            "source_activation_selects": False,
            "first_down": list(destination),
            "occupied_hover": list(occupied_target),
            "occupied_path_value": movement_target_value,
            "occupied_value_over_limit_but_occupancy_allows_hover":
                movement_target_value > 2 and occupied_target[1] * 64 + occupied_target[0] in occupied,
            "occupied_activation_selects": False,
            "selected": list(destination),
            "path_marked": path_marked,
            "first_step": list(destination),
            "round_value": [2, 1],
            "speed_div_10": 2,
            "physical_power": [10, 9],
            "render_required": True,
            "present_required": True,
            "wait_ticks": 40,
        },
        "targeting": {
            "occupied_path_value": targeting_target_value,
            "path_limit": targeting_target_value,
            "occupied_activation_selects": targeting_target_value >= 0,
        },
        "cancel": {
            "path_limit_after": 0,
            "cancel_word_after": 1,
            "wrapper_return": -1,
        },
    }


def post_battle_progression_vectors(total_experience: int) -> dict[str, object]:
    thresholds = [
        0, 50, 150, 300, 500, 750, 1050, 1400, 1800, 2250,
        2750, 3850, 5050, 6350, 7750, 9250, 10850, 12550, 14350, 16750,
        18250, 21400, 24700, 28150, 31750, 35500, 39400, 43450, 47650, 52000,
    ]
    old_level = 1
    experience = 150
    new_level = old_level
    for level in range(old_level, 30):
        if experience >= thresholds[level]:
            new_level = level + 1
    level_count = new_level - old_level
    state = 1
    growth_zero_based, state = legacy_bounded(state, 6)
    hp_random, state = legacy_bounded(state, 3)
    skill_words = {
        "medicine": 21,
        "use_poison": 20,
        "detoxification": 22,
        "fist": 23,
        "sword": 24,
        "knife": 25,
    }
    skill_after: dict[str, int] = {}
    for name, value in skill_words.items():
        if value > 20:
            addition, state = legacy_bounded(state, 3)
            value = min(100, wrapping_i16(value + addition))
        skill_after[name] = value
    hidden_addition, state = legacy_bounded(state, 3)
    growth_roll = growth_zero_based + 1

    craft_state = 1
    craft_picks: list[int] = []
    while True:
        choice, craft_state = legacy_bounded(craft_state, 5)
        craft_picks.append(choice)
        if choice == 0:
            break
    product_zero_based, craft_state = legacy_bounded(craft_state, 3)
    return {
        "level_thresholds": thresholds,
        "level_up": {
            "seed": 1,
            "old_level": old_level,
            "experience": experience,
            "new_level": new_level,
            "levels_gained": level_count,
            "growth_roll": growth_roll,
            "maximum_hp": 100 + (hp_random + 2) * 3 * level_count,
            "maximum_mp": 80 + (9 - growth_roll) * 4 * level_count,
            "primary_stats": [36, 36, 36],
            "skills": skill_after,
            "hidden_weapon": min(100, 26 + hidden_addition),
            "rng_state_after": state,
        },
        "practice": {
            "iq": 60,
            "factor": 3,
            "need_experience": 10,
            "existing_magic_level": 199,
            "magic_rank": 1,
            "required_experience": 60,
            "maximum_hp": [100, 110],
            "maximum_mp": [80, 100],
            "attack": [30, 100],
            "morality": [50, 0],
            "attack_twice": [0, 1],
            "attack_with_poison": [0, 5],
            "magic_level": [199, 299],
            "item_experience_after": 0,
        },
        "craft": {
            "seed": 1,
            "iq": 60,
            "factor": 3,
            "required_experience": 30,
            "eligible_recipes": [0],
            "selection_rng": craft_picks,
            "selected_recipe": 0,
            "product_count_added": product_zero_based + 1,
            "material_count": [3, 1],
            "product_count": [4, 4 + product_zero_based + 1],
            "rng_state_after": craft_state,
        },
        "settlement": {
            "battle_id": 2,
            "total_experience": total_experience,
            "living_party_count": 1,
            "shared_experience": total_experience,
            "living_reward": [5, wrapping_i16(5 + total_experience)],
            "dead_reward": 7,
            "dead_hp_floor_divisor": 5,
            "dead_physical_power_floor": 10,
            "enemy_restore": {
                "hp_to_maximum": True,
                "mp_to_maximum": True,
                "hurt": 0,
                "poison": 0,
                "physical_power": 100,
            },
        },
        "round_status_damage": {
            "condition": "hurt>0 || (poison>0 && hp>0 && physical_power>0 && hidden==0)",
            "hurt_divisor": 20,
            "poison_divisor": 10,
            "negative_hp_floor": 1,
            "negative_physical_power_floor": 1,
            "zero_hp_is_not_floored": True,
            "hurt_priority_vector": {
                "hp": [0, 1],
                "hurt": 20,
                "poison": 0,
                "physical_power": [-1, 1],
                "hidden": 1,
            },
            "poison_vector": {
                "hp": [100, 98],
                "hurt": 0,
                "poison": 20,
                "physical_power": 100,
                "hidden": 0,
            },
        },
        "ai_target_cleanup": {
            "fields": ["ai_target", "ai_poison_target"],
            "clear_only_when_hidden_equals": 1,
            "hidden_one": [1, -1],
            "hidden_two": [1, 1],
            "inactive_slot_within_26": [2, -1],
            "negative_target_preserved": [-1, -1],
            "true_out_of_bounds_preserved": [26, 26],
        },
        "status_panel": {
            "party_offset": 0,
            "enemy_offset": 220,
            "panel_origin_party": [220, 19],
            "panel_origin_enemy": [0, 19],
            "portrait_origin_party": [242, 82],
            "portrait_origin_enemy": [22, 82],
            "name_null_byte_2_x": 262,
            "hurt_colors": {"0_to_33": 1797, "34_to_66": 3600, "67_plus": 5142},
            "poison_colors": {"zero": 8993, "1_to_49": 12338, "50_plus": 13623},
            "mp_type_colors": {"0": 20558, "1": 1797, "2": 26211},
            "invalid_mp_type_reuses_poison_color": True,
        },
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


def ai_entry_vectors() -> dict[str, object]:
    def consume(seed: int, bounds: list[int]) -> tuple[list[int], int]:
        outputs: list[int] = []
        state = seed
        for bound in bounds:
            value, state = legacy_bounded(state, bound)
            outputs.append(value)
        return outputs, state

    poisoned_outputs, poisoned_state = consume(1, [10])
    low_mp_outputs, low_mp_state = consume(1, [10, 10, 10])
    medicine_outputs, medicine_state = consume(1, [10, 10, 10])
    detox_outputs, detox_state = consume(1, [10, 10, 10])
    cleared_wait_outputs, cleared_wait_state = consume(1, [10, 10, 50])
    attack_outputs, attack_state = consume(1, [10, 10, 50])
    escape_outputs, escape_state = consume(10, [10, 10])
    frozen_outputs, frozen_state = consume(1, [10, 10, 10, 50])
    return {
        "prelude": {
            "allied_total": 330,
            "opponent_total": 220,
            "allied_count": 3,
            "opponent_count": 2,
            "order": ["render", "present", "wait"],
            "wait_ticks": 300,
        },
        "priority": [
            "low_hp",
            "poisoned",
            "low_mp",
            "medicine_target",
            "detox_target",
            "escape",
            "offensive",
        ],
        "wait": {"physical_power": 9, "rng_consumed": False, "action": 7},
        "wait_cleared_by_low_hp_no_choice": {
            "physical_power": 9,
            "hp": 10,
            "rng_bounds_after_low_hp": [10, 10, 50],
            "rng_outputs": cleared_wait_outputs,
            "rng_state_after": cleared_wait_state,
            "action": 0,
        },
        "poisoned": {
            "poison": 100,
            "rng_bounds": [10],
            "rng_outputs": poisoned_outputs,
            "rng_state_after": poisoned_state,
            "action": 4,
        },
        "low_mp": {
            "mp": 0,
            "maximum_mp": 100,
            "rng_bounds": [10, 10, 10],
            "rng_outputs": low_mp_outputs,
            "rng_state_after": low_mp_state,
            "action": 6,
        },
        "medicine": {
            "medicine": 80,
            "requested_target": 1,
            "rng_bounds": [10, 10, 10],
            "rng_outputs": medicine_outputs,
            "rng_state_after": medicine_state,
            "action": 5,
        },
        "detox": {
            "detoxification": 80,
            "requested_target": 1,
            "rng_bounds": [10, 10, 10],
            "rng_outputs": detox_outputs,
            "rng_state_after": detox_state,
            "action": 4,
        },
        "escape": {
            "seed": 10,
            "hp": 19,
            "rng_bounds": [10, 10],
            "rng_outputs": escape_outputs,
            "rng_state_after": escape_state,
            "action": 11,
        },
        "attack": {
            "mp": 5,
            "maximum_mp": 5,
            "minimum_need_mp": 5,
            "rng_bounds": [10, 10, 50],
            "rng_outputs": attack_outputs,
            "rng_state_after": attack_state,
            "writes_action_code": False,
            "action": 2,
        },
        "frozen_prelude": {
            "totals_before_present": [330, 220, 3, 2],
            "mutated_totals_during_wait": [1530, 620, 3, 2],
            "rng_bounds": [10, 10, 10, 50],
            "rng_outputs": frozen_outputs,
            "rng_state_after": frozen_state,
            "action_with_frozen_totals": 2,
            "action_if_totals_were_recomputed": 5,
        },
        "handler_by_action": {
            "0": "rest",
            "1": "move",
            "2": "attack",
            "3": "use_poison",
            "4": "detox",
            "5": "medicine",
            "6": "item",
            "7": "rest",
            "8": "request_medicine",
            "9": "request_detox",
            "10": "throwing_weapon",
            "11": "escape",
        },
        "action_done_written_after_handler": True,
    }


def ai_selector_vectors() -> dict[str, object]:
    medicine_outputs: list[int] = []
    state = 1
    for _ in range(3):
        value, state = legacy_bounded(state, 10)
        medicine_outputs.append(value)

    detox_outputs: list[int] = []
    detox_state = 1
    for _ in range(3):
        value, detox_state = legacy_bounded(detox_state, 10)
        detox_outputs.append(value)

    poison_gate, poison_state = legacy_bounded(1, 50)
    poison_roll, poison_state = legacy_bounded(poison_state, 150)
    throwing_gate, throwing_state = legacy_bounded(1, 50)
    throwing_roll, throwing_state = legacy_bounded(throwing_state, 100)
    attack_gate, attack_state = legacy_bounded(1, 50)
    return {
        "action_codes": {
            "none": 0,
            "move": 1,
            "attack": 2,
            "use_poison": 3,
            "detox": 4,
            "medicine": 5,
            "item": 6,
            "wait": 7,
            "request_medicine": 8,
            "request_detox": 9,
            "throwing_weapon": 10,
            "escape": 11,
        },
        "low_hp": {
            "self": {"medicine": 21, "hurt": 50, "physical_power": 50, "action": 5},
            "self_minimum": {
                "medicine": 20,
                "hurt": 49,
                "physical_power": 50,
                "action": 5,
            },
            "self_strict_reject": {
                "medicine": 20,
                "hurt": 50,
                "physical_power": 50,
                "action": 0,
                "writes_action_code": False,
            },
            "self_power_reject": {
                "medicine": 100,
                "hurt": 50,
                "physical_power": 49,
                "action": 0,
            },
            "inventory": {
                "add_hp": 1,
                "inventory_slot": 2,
                "quantity": 0,
                "action": 6,
            },
            "enemy_carried": {
                "add_hp": 1,
                "carried_slot": 2,
                "quantity": 0,
                "action": 6,
            },
            "request": {"hurt": 80, "ally_medicine": 51, "target_slot": 1, "action": 8},
            "hidden_request_skip": {
                "hurt": 80,
                "hidden_slot": 1,
                "ally_medicine": 51,
                "target_slot": 2,
                "action": 8,
            },
        },
        "poisoned": {
            "self": {"detoxification": 22, "poison": 51, "physical_power": 51, "action": 4},
            "self_minimum": {
                "detoxification": 21,
                "poison": 50,
                "physical_power": 51,
                "action": 4,
            },
            "self_skill_reject": {
                "detoxification": 20,
                "poison": 49,
                "physical_power": 51,
                "action": 0,
                "writes_action_code": False,
            },
            "self_relative_reject": {
                "detoxification": 21,
                "poison": 51,
                "physical_power": 51,
                "action": 0,
            },
            "self_power_reject": {
                "detoxification": 100,
                "poison": 50,
                "physical_power": 50,
                "action": 0,
            },
            "party_item_property_word": 56,
            "party_ignores_negative_add_poison_word": 47,
            "enemy_carried_item_property_word": 47,
            "party_item": {"inventory_slot": 1, "quantity": 0, "action": 6},
            "enemy_carried": {"side": -1, "carried_slot": 2, "quantity": 0, "action": 6},
            "party_item_slot": 1,
            "enemy_carried_slot": 2,
            "item_action": 6,
            "request": {
                "poison": 80,
                "ally_detoxification": 51,
                "target_slot": 1,
                "action": 9,
            },
            "hidden_request_skip": {
                "poison": 80,
                "hidden_slot": 1,
                "ally_detoxification": 51,
                "target_slot": 2,
                "action": 9,
            },
            "different_side_skip": {
                "poison": 80,
                "different_side_slot": 1,
                "ally_detoxification": 51,
                "target_slot": 2,
                "action": 9,
            },
            "request_skill_reject": {
                "poison": 49,
                "ally_detoxification": 20,
                "action": 0,
                "writes_action_code": False,
            },
            "request_relative_reject": {
                "poison": 51,
                "ally_detoxification": 21,
                "action": 0,
            },
        },
        "low_mp": {"add_mp_word": 50, "inventory_slot": 3, "action": 6},
        "medicine_target": {
            "hp": 24,
            "maximum_hp": 100,
            "rng_bounds": [10, 10, 10],
            "rng_outputs": medicine_outputs,
            "rng_state_after": state,
            "target_slot": 1,
            "action": 5,
        },
        "detox_target": {
            "poison": 35,
            "rng_bounds": [10, 10, 10],
            "rng_outputs": detox_outputs,
            "rng_state_after": detox_state,
            "target_slot": 1,
            "action": 4,
        },
        "offensive": {
            "aid": {
                "allied_total": 1002,
                "opponent_total": 400,
                "opponent_count": 2,
                "actor_hp_plus_attack": 2,
                "missing_hp": [100, 300],
                "target_slot": 2,
                "action": 5,
                "rng_consumed": False,
            },
            "poison": {
                "use_poison": 100,
                "attack": 10,
                "rng_bounds": [50, 150],
                "rng_outputs": [poison_gate, poison_roll],
                "rng_state_after": poison_state,
                "action": 3,
            },
            "party_throwing": {
                "add_hp": -100,
                "attack": 10,
                "hidden_weapon": 100,
                "rng_bounds": [50, 100],
                "rng_outputs": [throwing_gate, throwing_roll],
                "rng_state_after": throwing_state,
                "inventory_slot": 4,
                "action": 10,
            },
            "attack": {
                "physical_power": 100,
                "current_mp": 5,
                "minimum_need_mp": 5,
                "rng_bounds": [50],
                "rng_outputs": [attack_gate],
                "rng_state_after": attack_state,
                "writes_action_code": False,
                "action": 2,
            },
        },
    }


def sentinel_entries(index_bytes: bytes, group_bytes: bytes) -> list[bytes]:
    offsets = struct.unpack(f"<{len(index_bytes) // 4}I", index_bytes)
    if not offsets or offsets[-1] != 0:
        raise ValueError("sentinel archive is missing trailing zero")
    entries: list[bytes] = []
    begin = 0
    for end in offsets[:-1]:
        if end < begin or end > len(group_bytes):
            raise ValueError("sentinel archive has invalid offset")
        entries.append(group_bytes[begin:end])
        begin = end
    entries.append(group_bytes[begin:])
    return entries


def draw_battle_sprite(
    pixels: bytearray,
    frame: bytes,
    anchor_x: int,
    anchor_y: int,
    mode: str = "normal",
    value: int = 0,
    palette: list[tuple[int, int, int]] | None = None,
    rgb4_lookup: list[int] | None = None,
) -> None:
    if len(frame) < 8:
        return
    width, height, x_offset, y_offset = struct.unpack_from("<HHhh", frame)
    cursor = 8
    left = anchor_x - x_offset
    top = anchor_y - y_offset
    for row in range(height):
        row_size = frame[cursor]
        cursor += 1
        row_end = cursor + row_size
        destination_x = left
        while cursor < row_end:
            skip = frame[cursor]
            count = frame[cursor + 1]
            cursor += 2
            destination_x += skip
            for source in frame[cursor:cursor + count]:
                destination_y = top + row
                if 0 <= destination_x < 320 and 0 <= destination_y < 200:
                    offset = destination_y * 320 + destination_x
                    if mode == "tint":
                        pixels[offset] = value
                    elif mode == "blend":
                        assert palette is not None and rgb4_lookup is not None
                        destination = pixels[offset]
                        source_rgb = palette[source]
                        destination_rgb = palette[destination]
                        components = tuple(
                            value * source_rgb[index] // 32
                            + (8 - value) * destination_rgb[index] // 32
                            for index in range(3)
                        )
                        pixels[offset] = rgb4_lookup[
                            components[0] * 256 + components[1] * 16 + components[2]
                        ]
                    else:
                        pixels[offset] = source
                destination_x += 1
            cursor += count
        if cursor != row_end or destination_x > left + width:
            raise ValueError("malformed battle RLE row")
    if cursor != len(frame):
        raise ValueError("battle RLE frame has trailing bytes")


def draw_battle_text(
    pixels: bytearray,
    x: int,
    y: int,
    text: bytes,
    ascii_font: bytes,
    big5_font: bytes,
    packed_colors: int,
) -> None:
    shadow = packed_colors & 0xFF
    foreground = packed_colors >> 8 & 0xFF
    cursor = 0
    while cursor < len(text):
        first = text[cursor]
        cursor += 1
        if first == 0:
            return
        if first > 0x7F:
            second = text[cursor]
            cursor += 1
            trail = second - 0x40 if 0x40 <= second <= 0x7E else second - 0x62
            glyph_index = (first - 0xA1) * 157 + trail
            glyph = big5_font[glyph_index * 32:(glyph_index + 1) * 32]
            glyph_width = 16
            stride = 2
        else:
            glyph_index = 32 if first == ord("_") else first
            glyph = ascii_font[glyph_index * 16:(glyph_index + 1) * 16]
            glyph_width = 8
            stride = 1
        for row in range(16):
            for byte_index in range(stride):
                bits = glyph[row * stride + byte_index]
                for bit in range(8):
                    if bits & (0x80 >> bit):
                        target_x = x + byte_index * 8 + bit
                        if 0 <= target_x < 319 and 0 <= y + row < 200:
                            offset = (y + row) * 320 + target_x
                            pixels[offset] = foreground
                            pixels[offset + 1] = shadow
        x += 4 if first == ord("_") else glyph_width


def battle_pixel_hashes(
    root: Path,
    battlefield_id: int,
    commands: list[list[int]],
) -> tuple[str, str, bytes]:
    battlefield = sentinel_entries(
        (root / f"WDX{battlefield_id:03d}").read_bytes(),
        (root / f"WMP{battlefield_id:03d}").read_bytes(),
    )
    cloud = cumulative_entries(
        (root / "CLOUD.IDX").read_bytes(), (root / "CLOUD.GRP").read_bytes()
    )
    portraits = cumulative_entries(
        (root / "HDGRP.IDX").read_bytes(), (root / "HDGRP.GRP").read_bytes()
    )
    palette_bytes = (root / "MMAP.COL").read_bytes()
    palette = [tuple(palette_bytes[index:index + 3]) for index in range(0, 768, 3)]
    rgb4_lookup: list[int] = []
    for red in range(16):
        for green in range(16):
            for blue in range(16):
                target = (red * 4 + 2, green * 4 + 2, blue * 4 + 2)
                rgb4_lookup.append(min(
                    range(256),
                    key=lambda index: sum(
                        (target[channel] - palette[index][channel]) ** 2
                        for channel in range(3)
                    ),
                ))
    ascii_font = (root / "FONT.X16").read_bytes()
    big5_font = (root / "FONT.C16").read_bytes()
    pixels = bytearray(320 * 200)
    for command in commands:
        kind, _, _, screen_x, screen_y, sprite_id, variant, style, value = command
        if kind in (0, 2):
            if sprite_id < 0 or sprite_id > 0x7FFE or sprite_id & 1:
                continue
            index = sprite_id // 2
            if index >= len(battlefield):
                continue
            draw_battle_sprite(
                pixels,
                battlefield[index],
                screen_x,
                screen_y,
                "tint" if kind == 2 else "normal",
                style & 0xFF,
            )
        elif kind == 1:
            draw_battle_sprite(
                pixels,
                cloud[4 + variant],
                screen_x,
                screen_y,
                "blend",
                style,
                palette,
                rgb4_lookup,
            )
        elif kind == 3:
            sign = b"-" if variant < 0 else b"+"
            draw_battle_text(
                pixels,
                screen_x,
                screen_y,
                sign + str(value).encode("ascii") + b"\0",
                ascii_font,
                big5_font,
                style & 0xFFFF,
            )
    battle_hash = fnv1a_bytes(pixels)
    battle_pixels = bytes(pixels)

    panel_x, panel_y, panel_width, panel_height = 220, 19, 100, 140

    def blend_rectangle(x: int, y: int, width: int, height: int) -> None:
        for destination_y in range(max(y, 0), min(y + height, 200)):
            for destination_x in range(max(x, 0), min(x + width, 320)):
                offset = destination_y * 320 + destination_x
                destination = pixels[offset]
                source_rgb = palette[0]
                destination_rgb = palette[destination]
                components = tuple(
                    source_rgb[index] // 8 + destination_rgb[index] // 8
                    for index in range(3)
                )
                pixels[offset] = rgb4_lookup[
                    components[0] * 256 + components[1] * 16 + components[2]
                ]

    for rectangle in (
        (panel_x + 5, panel_y, panel_width - 10, 1),
        (panel_x + 4, panel_y + 1, panel_width - 8, 1),
        (panel_x + 3, panel_y + 2, panel_width - 6, 1),
        (panel_x + 2, panel_y + 3, panel_width - 4, 1),
        (panel_x + 1, panel_y + 4, panel_width - 2, 1),
        (panel_x, panel_y + 5, panel_width, panel_height - 10),
        (panel_x + 1, panel_y + panel_height - 5, panel_width - 2, 1),
        (panel_x + 2, panel_y + panel_height - 4, panel_width - 4, 1),
        (panel_x + 3, panel_y + panel_height - 3, panel_width - 6, 1),
        (panel_x + 4, panel_y + panel_height - 2, panel_width - 8, 1),
        (panel_x + 5, panel_y + panel_height - 1, panel_width - 10, 1),
    ):
        blend_rectangle(*rectangle)
    for left, top, width, height in (
        (panel_x + 5, panel_y + 1, panel_width - 10, 1),
        (panel_x + 4, panel_y + 2, 1, 2),
        (panel_x + panel_width - 5, panel_y + 2, 1, 2),
        (panel_x + 2, panel_y + 4, 2, 1),
        (panel_x + panel_width - 4, panel_y + 4, 2, 1),
        (panel_x + 1, panel_y + 5, 1, panel_height - 10),
        (panel_x + panel_width - 2, panel_y + 5, 1, panel_height - 10),
        (panel_x + 2, panel_y + panel_height - 5, 2, 1),
        (panel_x + panel_width - 4, panel_y + panel_height - 5, 2, 1),
        (panel_x + 4, panel_y + panel_height - 4, 1, 2),
        (panel_x + panel_width - 5, panel_y + panel_height - 4, 1, 2),
        (panel_x + 5, panel_y + panel_height - 2, panel_width - 10, 1),
    ):
        for row in range(top, top + height):
            pixels[row * 320 + left:row * 320 + left + width] = bytes([0xFF]) * width
    draw_battle_sprite(pixels, portraits[1], 242, 82)
    labels = (
        (225, 101, bytes.fromhex("ca5ea44f20"), 0x2321),
        (262, 101, b"  0", 0x0705),
        (285, 101, b"/", 0x6663),
        (292, 101, b"100", 0x2321),
        (225, 118, bytes.fromhex("a5cda95220"), 0x2321),
        (262, 118, b"  0", 0x0705),
        (285, 118, b"/", 0x6663),
        (292, 118, b"  0", 0x2321),
        (225, 135, bytes.fromhex("a4baa44f20"), 0x2321),
        (262, 135, b"  0", 0x504E),
        (285, 135, b"/", 0x504E),
        (292, 135, b"  0", 0x504E),
    )
    for x, y, text, colors in labels:
        draw_battle_text(
            pixels, x, y, text + b"\0", ascii_font, big5_font, colors
        )
    return battle_hash, fnv1a_bytes(pixels), battle_pixels


def battle_session_vector(root: Path, field_words: list[int]) -> dict[str, object]:
    palette_bytes = (root / "MMAP.COL").read_bytes()
    palette = [tuple(palette_bytes[index:index + 3]) for index in range(0, 768, 3)]
    rgb4_lookup: list[int] = []
    for red in range(16):
        for green in range(16):
            for blue in range(16):
                target = (red * 4 + 2, green * 4 + 2, blue * 4 + 2)
                rgb4_lookup.append(min(
                    range(256),
                    key=lambda index: sum(
                        (target[channel] - palette[index][channel]) ** 2
                        for channel in range(3)
                    ),
                ))
    ascii_font = (root / "FONT.X16").read_bytes()
    big5_font = (root / "FONT.C16").read_bytes()
    pixels = bytearray(index % 251 for index in range(320 * 200))

    def blend_rectangle(x: int, y: int, width: int, height: int) -> None:
        for destination_y in range(y, y + height):
            for destination_x in range(x, x + width):
                offset = destination_y * 320 + destination_x
                destination_rgb = palette[pixels[offset]]
                source_rgb = palette[0]
                components = tuple(
                    source_rgb[index] // 8 + destination_rgb[index] // 8
                    for index in range(3)
                )
                pixels[offset] = rgb4_lookup[
                    components[0] * 256 + components[1] * 16 + components[2]
                ]

    def draw_panel(x: int, y: int, width: int, height: int) -> None:
        for rectangle in (
            (x + 5, y, width - 10, 1),
            (x + 4, y + 1, width - 8, 1),
            (x + 3, y + 2, width - 6, 1),
            (x + 2, y + 3, width - 4, 1),
            (x + 1, y + 4, width - 2, 1),
            (x, y + 5, width, height - 10),
            (x + 1, y + height - 5, width - 2, 1),
            (x + 2, y + height - 4, width - 4, 1),
            (x + 3, y + height - 3, width - 6, 1),
            (x + 4, y + height - 2, width - 8, 1),
            (x + 5, y + height - 1, width - 10, 1),
        ):
            blend_rectangle(*rectangle)
        for left, top, rectangle_width, rectangle_height in (
            (x + 5, y + 1, width - 10, 1),
            (x + 4, y + 2, 1, 2),
            (x + width - 5, y + 2, 1, 2),
            (x + 2, y + 4, 2, 1),
            (x + width - 4, y + 4, 2, 1),
            (x + 1, y + 5, 1, height - 10),
            (x + width - 2, y + 5, 1, height - 10),
            (x + 2, y + height - 5, 2, 1),
            (x + width - 4, y + height - 5, 2, 1),
            (x + 4, y + height - 4, 1, 2),
            (x + width - 5, y + height - 4, 1, 2),
            (x + 5, y + height - 2, width - 10, 1),
        ):
            for row in range(top, top + rectangle_height):
                begin = row * 320 + left
                pixels[begin:begin + rectangle_width] = bytes([0xFF]) * rectangle_width

    draw_panel(64, 17, 180, 30)
    draw_battle_text(
        pixels,
        69,
        25,
        bytes.fromhex("bdd0bfefbedcb0d1bb50bed4b0aba4a7a448aaab00"),
        ascii_font,
        big5_font,
        0x0705,
    )
    draw_panel(64, 48, 66, 70)
    draw_battle_text(pixels, 95, 55, b"A\0", ascii_font, big5_font, 0x6663)
    draw_battle_text(pixels, 67, 55, b"*\0", ascii_font, big5_font, 0x0705)
    draw_battle_text(pixels, 95, 75, b"C\0", ascii_font, big5_font, 0x2321)
    draw_battle_text(
        pixels,
        83,
        95,
        bytes.fromhex("b5b2a7f400"),
        ascii_font,
        big5_font,
        0x2321,
    )
    selection_hash = fnv1a_bytes(pixels)

    occupancy = [-1] * 4096
    combatants = (
        (0, 30, 24, 5110),
        (1, 30, 22, 5118),
        (2, 30, 26, 5126),
        (4, 24, 24, 5140),
    )
    for slot, (_, x, y, _) in enumerate(combatants):
        occupancy[y * 64 + x] = slot

    def render_commands(
        view_x: int, view_y: int, actor_cursor_visible: bool = False
    ) -> list[list[int]]:
        commands: list[list[int]] = []
        for local_x in range(32):
            for local_y in range(32):
                map_x = local_x + view_x
                map_y = local_y + view_y
                screen_x = 18 * local_x - 18 * local_y + 145
                screen_y = 9 * local_x + 9 * local_y - 81
                commands.append([
                    0, map_x, map_y, screen_x, screen_y,
                    field_words[map_y * 64 + map_x], 0, 0, 0,
                ])
        for local_x in range(32):
            for local_y in range(32):
                map_x = local_x + view_x
                map_y = local_y + view_y
                cell = map_y * 64 + map_x
                screen_x = 18 * local_x - 18 * local_y + 145
                screen_y = 9 * local_x + 9 * local_y - 81
                # 3AC0C..3AC7B: 556F0==1 draws CLOUD frame5/style3 at
                # E6EE4/E6EE2 before upper terrain and the occupant.
                if actor_cursor_visible and (map_x, map_y) == combatants[0][1:3]:
                    commands.append([
                        1, map_x, map_y, screen_x - 18, screen_y, 0, 1, 3, 0,
                    ])
                object_sprite = field_words[4096 + cell]
                if object_sprite not in (0, 15000):
                    commands.append([
                        0, map_x, map_y, screen_x, screen_y,
                        object_sprite, 0, 0, 0,
                    ])
                occupant = occupancy[cell]
                if occupant >= 0:
                    commands.append([
                        0, map_x, map_y, screen_x, screen_y,
                        combatants[occupant][3], 0, 0, 0,
                    ])
        return commands

    initial_view_x, initial_view_y = 0, 0
    initial_commands = render_commands(initial_view_x, initial_view_y)
    initial_hash, _, _ = battle_pixel_hashes(root, 1, initial_commands)
    action_commands = render_commands(19, 13, actor_cursor_visible=True)
    _, _, action_pixels = battle_pixel_hashes(root, 1, action_commands)
    pixels = bytearray(action_pixels)
    draw_panel(20, 19, 42, 180)
    action_labels = (
        bytes.fromhex("b2beb0ca"),
        bytes.fromhex("a7f0c0bb"),
        bytes.fromhex("a5ceac72"),
        bytes.fromhex("b8d1ac72"),
        bytes.fromhex("c2e5c0f8"),
        bytes.fromhex("aaabab7e"),
        bytes.fromhex("b5a5abdd"),
        bytes.fromhex("aaacba41"),
        bytes.fromhex("a5f0aea7"),
        bytes.fromhex("a6dbb0ca"),
    )
    for ordinal, label in enumerate(action_labels):
        draw_battle_text(
            pixels,
            25,
            24 + 17 * ordinal,
            label + b"\0",
            ascii_font,
            big5_font,
            0x2321,
        )
    draw_battle_text(
        pixels, 25, 24, action_labels[0] + b"\0", ascii_font, big5_font, 0x6663
    )
    draw_panel(220, 19, 100, 140)
    portraits = cumulative_entries(
        (root / "HDGRP.IDX").read_bytes(), (root / "HDGRP.GRP").read_bytes()
    )
    draw_battle_sprite(pixels, portraits[0], 242, 82)
    for x, y, text, colors in (
        (266, 84, b"A", 0x0705),
        (225, 101, bytes.fromhex("ca5ea44f20"), 0x2321),
        (262, 101, b" 60", 0x0705),
        (285, 101, b"/", 0x6663),
        (292, 101, b"100", 0x2321),
        (225, 118, bytes.fromhex("a5cda95220"), 0x2321),
        (262, 118, b"  0", 0x0705),
        (285, 118, b"/", 0x6663),
        (292, 118, b"  0", 0x2321),
        (225, 135, bytes.fromhex("a4baa44f20"), 0x2321),
        (262, 135, b" 25", 0x504E),
        (285, 135, b"/", 0x504E),
        (292, 135, b"  0", 0x504E),
    ):
        draw_battle_text(
            pixels, x, y, text + b"\0", ascii_font, big5_font, colors
        )
    action_menu_hash = fnv1a_bytes(pixels)
    return {
        "battle_id": 2,
        "party_prefix_length": 2,
        "selection_states": [2, 0],
        "selection_cursor": 0,
        "selection_pixel_hash": selection_hash,
        "selected_role_ids": [0, 1, 2],
        "combatant_role_ids": [0, 1, 2, 4],
        "initial_view": [initial_view_x, initial_view_y],
        "initial_command_count": len(initial_commands),
        "initial_pixel_hash": initial_hash,
        "action_menu": {
            "availability": [1] * 10,
            "available_count": 10,
            "cursor": 0,
            "selected_action": -1,
            "actor_cursor": list(combatants[0][1:3]),
            "actor_cursor_visible": True,
            "pixel_hash": action_menu_hash,
        },
    }


def battle_render_plan_vector(
    root: Path, field_words: list[int], setup: dict[str, object]
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
    battle_hash, status_hash, _ = battle_pixel_hashes(
        root, int(setup["battlefield_id"]), commands
    )
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
        "pixel_hash": battle_hash,
        "status_panel_pixel_hash": status_hash,
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


def ai_escape_vector(field_words: list[int]) -> dict[str, object]:
    combatants = [
        {"side": 0, "x": 10, "y": 20},
        {"side": 0, "x": 11, "y": 21},
        {"side": 0, "x": 12, "y": 22},
        {"side": 1, "x": 13, "y": 23},
        {"side": 1, "x": 14, "y": 24},
    ]
    source = (10, 20)
    occupied = {c["y"] * 64 + c["x"] for c in combatants}
    values = build_path_map(field_words, source, "movement", occupied)
    round_value = 3
    best_score = 0
    destination: list[int] | None = None
    for x in range(64):
        for y in range(64):
            if values[y * 64 + x] != round_value:
                continue
            score = sum(
                abs(x - c["x"]) + abs(y - c["y"])
                for c in combatants
                if c["side"] != 0
            )
            if score > best_score:
                best_score = score
                destination = [x, y]
    return {
        "battle_id": 3,
        "battlefield_id": 2,
        "source": list(source),
        "round_value": round_value,
        "combatants": combatants,
        "scan_order": ["x", "y"],
        "strictly_greater_replaces": True,
        "destination": destination,
        "maximum_enemy_distance_sum": best_score,
        "rest_after_move_for_escape_action": True,
        "rest_after_move_for_item_reposition": False,
    }


def ai_attack_handler_vectors(
    z_dat_bytes: bytes, field_words: list[int]
) -> dict[str, object]:
    table_end = Z_DAT_AI_SPECIAL_ATTACK_OFFSET + AI_SPECIAL_ATTACK_COUNT * 6
    if table_end > len(z_dat_bytes):
        raise ValueError("Z.DAT does not contain the AI special-attack table")
    table_bytes = z_dat_bytes[Z_DAT_AI_SPECIAL_ATTACK_OFFSET:table_end]
    table_words = struct.unpack(f"<{AI_SPECIAL_ATTACK_COUNT * 3}h", table_bytes)
    table = [
        {
            "weapon_id": table_words[index * 3],
            "magic_id": table_words[index * 3 + 1],
            "bonus": table_words[index * 3 + 2],
        }
        for index in range(AI_SPECIAL_ATTACK_COUNT)
    ]
    magic_roll, state = legacy_bounded(9, 2)
    target_roll, state = legacy_bounded(state, 10)
    targeting = build_path_map(field_words, (10, 20), "targeting")
    distance_3 = targeting[23 * 64 + 13]
    distance_4 = targeting[24 * 64 + 14]
    adjacent_targeting = build_path_map(field_words, (10, 20), "targeting")
    return {
        "special_attack_table": {
            "z_dat_file_offset": Z_DAT_AI_SPECIAL_ATTACK_OFFSET,
            "entry_count": AI_SPECIAL_ATTACK_COUNT,
            "bytes_sha256": sha256(table_bytes),
            "entries": table,
        },
        "magic_then_target_rng": {
            "seed": 9,
            "learned_magic_count": 2,
            "magic_slot": magic_roll,
            "target_bound": 10,
            "target_roll": target_roll,
            "state_after": state,
            "call_order": ["automatic_magic_slot", "attack_target_strategy"],
        },
        "range_rules": {
            "target_distance": distance_3,
            "area_0": {"movement_mode": 1, "range": 6, "in_range": True},
            "area_1_diagonal": {"movement_mode": 2, "range": 6, "in_range": False},
            "area_1_aligned": {"movement_mode": 2, "distance": 1, "in_range": True},
            "area_2_diagonal": {"movement_mode": 2, "range": 6, "in_range": False},
            "area_3": {"movement_mode": 1, "range": 6, "in_range": True},
            "unsupported_area": {"movement_mode": 0, "in_range": False},
        },
        "branch_order": {
            "initial_target_distances": [distance_3, distance_4],
            "already_in_range": "attack",
            "out_of_range_with_no_round_value": "finish_without_rest",
            "out_of_range_with_round_value": "move",
            "after_move_original_target_first": True,
            "after_move_reselect_strategy": "nearest",
            "attack_call_automatic_flag": 1,
            "handler_writes_action_done_after_step": True,
            "reselected_adjacent_distance": adjacent_targeting[20 * 64 + 11],
            "reselected_adjacent_result": "attack",
            "reselected_distance_6_result_for_range_1": "rest",
        },
    }


def ai_item_handler_vectors(field_words: list[int]) -> dict[str, object]:
    targeting = build_path_map(field_words, (10, 20), "targeting")
    strongest_roll, strongest_state = legacy_bounded(9, 10)
    return {
        "consumable": {
            "calls_relocation_before_use": True,
            "relocation_rest_after_move": False,
            "source": [10, 20],
            "round_value": 3,
            "destination": [7, 20],
            "maximum_enemy_distance_sum": 20,
            "use_mode": 0,
            "next_step_with_destination": "move",
            "next_step_after_relocation": "use_item",
            "use_after_relocation_even_without_destination": True,
        },
        "throwing_weapon": {
            "target_selector_runs_before_range": True,
            "actor_hidden_weapon": 80,
            "targeting_range": trunc_div(80, 15) + 1,
            "movement_mode": 1,
            "use_mode": 1,
            "nearest": {
                "target_slot": 3,
                "distance": targeting[23 * 64 + 13],
                "rng_consumed": False,
                "result": "use_item",
                "range_checks": 1,
            },
            "strongest": {
                "rng_seed": 9,
                "rng_output": strongest_roll,
                "rng_state_after": strongest_state,
                "attacks": [30, 50],
                "target_slot": 4,
                "distance": targeting[24 * 64 + 14],
                "round_positive_result": "move",
                "range_checks_before_move": 1,
                "no_position_change_result": "attack_fallback",
                "range_checks_after_resume": 2,
                "moved_source": [13, 23],
                "moved_distance": 2,
                "moved_result": "use_item",
            },
            "round_zero_out_of_range": {
                "result": "attack_fallback",
                "range_checks": 2,
            },
            "stale_target_bug": {
                "strategy_gate_hit": True,
                "eligible_attacks": [0, 0],
                "target_written": False,
                "preexisting_target_slot": 4,
                "target_slot_used": 4,
            },
            "post_move_target_reselected": False,
        },
        "item_source_by_side": {
            "party_side_0": "inventory_slot",
            "enemy_nonzero_side": "role_carried_slot",
            "inventory_count_checked_before_handler": False,
        },
        "carried_item_remove": {
            "removed_slot": 1,
            "item_ids_before": [5, 6, 7, 8],
            "counts_before": [1, 2, 3, 4],
            "item_ids_after": [5, 7, 8, -1],
            "counts_after": [1, 3, 4, 0],
        },
        "outer_ai_marks_action_done_after_handler": True,
    }


def ai_poison_handler_vectors(field_words: list[int]) -> dict[str, object]:
    targeting = build_path_map(field_words, (10, 20), "targeting")
    high_iq_roll, high_iq_state = legacy_bounded(9, 10)
    allied_total = wrapping_i16(0 + 10)
    allied_total = wrapping_i16(allied_total + 100)
    allied_total = wrapping_i16(allied_total + 10)
    allied_total = wrapping_i16(allied_total + 100)
    allied_total = wrapping_i16(allied_total + 10)
    allied_total = wrapping_i16(allied_total + 100)
    allied_count = 3
    doubled_average = trunc_div(2 * allied_total, allied_count)
    return {
        "eligibility": {
            "different_side": True,
            "hidden_equals_zero": True,
            "target_poison_strictly_below": 95,
            "target_anti_poison_strictly_below_actor_use_poison": True,
        },
        "high_iq_strongest": {
            "iq_strictly_above": 60,
            "rng_seed": 9,
            "rng_output": high_iq_roll,
            "rng_state_after": high_iq_state,
            "rng_threshold": 7,
            "attacks": [30, 50],
            "target_slot": 4,
        },
        "stale_distance_first_eligible_bug": {
            "strongest_attacks": [0, 0],
            "stale_target_slot": 4,
            "stale_target_distance": targeting[24 * 64 + 14],
            "candidate_distances_ignored": [
                targeting[23 * 64 + 13],
                targeting[24 * 64 + 14],
            ],
            "target_slot": 3,
            "strict_tie_keeps_first": True,
            "rebuilds_targeting_map_for_each_eligible_candidate": True,
            "no_eligible_target_does_not_read_stale_slot": True,
        },
        "range_and_round": {
            "actor_use_poison": 80,
            "targeting_range": trunc_div(80, 15) + 1,
            "target_distance": targeting[23 * 64 + 13],
            "round_zero_in_range": {"result": "poison", "range_checks": 1},
            "round_positive_in_range": {"result": "move_mode_3", "range_checks_before_move": 1},
            "round_zero_out_of_range": {
                "result": "average_attack_fallback",
                "range_checks": 2,
            },
            "round_negative_in_range": {
                "result": "poison_after_second_range_check",
                "range_checks": 2,
            },
        },
        "out_of_range_fallback": {
            "allied_total_i16": allied_total,
            "allied_count_i16": allied_count,
            "doubled_allied_average": doubled_average,
            "target_attack_50_doubled": 100,
            "target_attack_50_result": "rest",
            "target_attack_200_doubled": 400,
            "target_attack_200_result": "attack",
            "comparison": "strictly_greater",
        },
        "selector_initializes_word12_to": -1,
        "no_target_result": "attack",
        "outer_ai_marks_action_done_after_handler": True,
    }


def ai_attack_target_vectors(field_words: list[int]) -> dict[str, object]:
    combatants = [
        {"side": 0, "x": 10, "y": 20},
        {"side": 0, "x": 11, "y": 21},
        {"side": 0, "x": 12, "y": 22},
        {"side": 1, "x": 13, "y": 23},
        {"side": 1, "x": 14, "y": 24},
    ]
    attacks = [10, 10, 10, 30, 50]
    visible_enemies = [i for i, c in enumerate(combatants) if c["side"] != 0]
    strongest = max(visible_enemies, key=lambda i: attacks[i])
    weakest = min(visible_enemies, key=lambda i: attacks[i])
    targeting = build_path_map(field_words, (10, 20), "targeting")
    nearest = min(
        visible_enemies,
        key=lambda i: targeting[combatants[i]["y"] * 64 + combatants[i]["x"]],
    )
    seed9_roll, seed9_state = legacy_bounded(9, 10)
    cascade_first, cascade_state = legacy_bounded(1, 10)
    cascade_second, cascade_state = legacy_bounded(cascade_state, 10)
    return {
        "strongest": {
            "morality": 75,
            "rng_output": seed9_roll,
            "rng_state_after": seed9_state,
            "attacks": attacks[3:],
            "target_slot": strongest,
        },
        "weakest": {
            "morality": 25,
            "rng_output": seed9_roll,
            "rng_state_after": seed9_state,
            "attacks": attacks[3:],
            "target_slot": weakest,
        },
        "specialist_medicine": {
            "iq": 70,
            "medicine": [30, 10],
            "target_slot": 3,
        },
        "specialist_detox_fallback_bug": {
            "ally_use_poison": 21,
            "detoxification": [30, 0],
            "attacks": [50, 10],
            "provisional_detox_target": 3,
            "final_target_slot": 4,
        },
        "nearest": {
            "targeting_distances": [
                targeting[c["y"] * 64 + c["x"]] for c in combatants[3:]
            ],
            "target_slot": nearest,
            "rng_consumed": False,
        },
        "failed_high_morality_then_failed_iq": {
            "rng_outputs": [cascade_first, cascade_second],
            "rng_state_after": cascade_state,
            "target_slot": nearest,
        },
        "selected_strongest_with_no_positive_attack": {
            "attacks": [0, 0],
            "target_slot": -1,
            "falls_back": False,
        },
        "strict_comparisons_preserve_first_tie": True,
        "hidden_opponents_skipped": True,
    }


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
    ai_poisoned_throw = ai_throwing_weapon_vector(
        item_records[102],
        seed=1,
        hidden_weapon=20,
        hp=100,
        maximum_hp=200,
        hurt=40,
        poison=10,
    )
    ai_plain_throw = ai_throwing_weapon_vector(
        item_records[96],
        seed=2,
        hidden_weapon=20,
        hp=100,
        maximum_hp=200,
        hurt=0,
        poison=10,
    )
    shared_role = [0] * 91
    shared_role[17] = 100
    shared_role[18] = 200
    shared_role[19] = 40
    shared_role[20] = 50
    shared_role[21] = 30
    shared_role[40] = 0
    shared_role[41] = 10
    shared_role[42] = 100
    shared_role[49] = 5
    shared_role[54] = 20
    shared_item_19 = shared_item_effect_vector(
        item_records[19], seed=1, role_words=shared_role
    )
    display_role = list(shared_role)
    display_role[58] = 4
    shared_item_91 = shared_item_effect_vector(
        item_records[91], seed=1, role_words=display_role
    )
    shared_item_40 = shared_item_effect_vector(
        item_records[40], seed=1, role_words=shared_role
    )
    if poisoned_throw["item_id"] != 102 or poisoned_throw["item_type"] != 4:
        raise ValueError("RANGER.GRP item 102 is not the expected throwing weapon")
    if plain_throw["item_id"] != 96 or plain_throw["item_type"] != 4:
        raise ValueError("RANGER.GRP item 96 is not the expected throwing weapon")
    if shared_item_19["item_id"] != 19 or shared_item_19["item_type"] != 3:
        raise ValueError("RANGER.GRP item 19 is not the expected shared-use item")
    if shared_item_91["item_id"] != 91 or shared_item_40["item_id"] != 40:
        raise ValueError("RANGER.GRP shared item bug vectors changed identity")

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

    movement_setup = setup_records[4]
    movement_field_bytes = warfld_entries[int(movement_setup["battlefield_id"])][:16384]
    movement_field_words = list(struct.unpack("<8192h", movement_field_bytes))
    movement_occupied = {
        int(write["occupancy_index"])
        for write in movement_setup["static_occupancy_writes"]
    }
    movement_vectors = ai_movement_vectors(movement_field_words, movement_occupied)
    cursor_vectors = player_cursor_vectors(movement_field_words, movement_occupied)
    post_battle_vectors = post_battle_progression_vectors(
        struct.unpack_from("<h", war_records[2], 7 * 2)[0]
    )

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

    battle_session = battle_session_vector(
        data_root,
        list(struct.unpack("<8192h", warfld_entries[int(setup_records[2]["battlefield_id"])][:16384])),
    )
    render_plan = battle_render_plan_vector(
        data_root,
        list(struct.unpack("<8192h", warfld_entries[int(setup_records[4]["battlefield_id"])][:16384])),
        setup_records[4],
    )
    battle3_field_words = list(
        struct.unpack("<8192h", warfld_entries[int(setup_records[3]["battlefield_id"])][:16384])
    )
    escape_vector = ai_escape_vector(battle3_field_words)
    attack_target_vectors = ai_attack_target_vectors(battle3_field_words)
    attack_handler_vectors = ai_attack_handler_vectors(z_dat_bytes, battle3_field_words)
    poison_handler_vectors = ai_poison_handler_vectors(battle3_field_words)
    item_handler_vectors = ai_item_handler_vectors(battle3_field_words)

    return {
        "format": "openlegend-b8-battle-goldens-v1",
        "battle_entry": battle_entry_contract(z_dat_bytes),
        "battle_data_loader": battle_data_loader_contract(z_dat_bytes),
        "battle_setup_machine": battle_setup_machine_contract(z_dat_bytes),
        "battle_round_machine": battle_round_machine_contract(z_dat_bytes, ranger_group_bytes),
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
                    "ai_throwing": {
                        "poisoned_state_vector": ai_poisoned_throw,
                        "plain_state_vector": ai_plain_throw,
                        "effect_cell_marked": True,
                        "actor_direction_unchanged": True,
                        "action_done_before_outer_ai_finish": 0,
                        "party_inventory_before": [[102, 1], [97, 2]],
                        "party_inventory_after": [[97, 2], [-1, 0]],
                        "enemy_carried_before": [[102, 1], [97, 2], [-1, 0], [-1, 0]],
                        "enemy_carried_after": [[97, 2], [-1, 0], [-1, 0], [-1, 0]],
                    },
                    "ai_item_effect": {
                        "multi_effect_item_19": shared_item_19,
                        "display_only_item_91": shared_item_91,
                        "mp_type_item_40": shared_item_40,
                        "presentation": {
                            "panel_rect": [70, 18, 148],
                            "panel_height_formula": "20 * effect_count + 30",
                            "item_name_position": [75, 25],
                            "row_label_x": 75,
                            "row_sign_x": 155,
                            "row_value_x": 187,
                            "row_y_formula": "18 * visible_index + 45",
                            "battle_redraw_when_effect_count_positive": True,
                            "wait_for_input_when_effect_count_positive": True,
                            "post_effect_tick_changes": 340 // 40 + 1,
                        },
                        "effect_cell_marked": True,
                        "zero_effect_item_still_consumed": True,
                        "actor_direction_unchanged": True,
                        "action_done_before_outer_ai_finish": 0,
                        "party_inventory_before": [[19, 1], [2, 3]],
                        "party_inventory_after": [[2, 3], [-1, 0]],
                        "enemy_carried_before": [[19, 1], [2, 3], [-1, 0], [-1, 0]],
                        "enemy_carried_after": [[2, 3], [-1, 0], [-1, 0], [-1, 0]],
                    },
                },
                "ai_selector_vectors": ai_selector_vectors(),
                "ai_entry_vectors": ai_entry_vectors(),
                "ai_request_vectors": ai_request_vectors(),
                "ai_support_vectors": ai_support_vectors(),
                "ai_movement_vectors": movement_vectors,
                "player_cursor_vectors": cursor_vectors,
                "post_battle_progression_vectors": post_battle_vectors,
                "ai_escape_vector": escape_vector,
                "ai_attack_target_vectors": attack_target_vectors,
                "ai_attack_handler_vectors": attack_handler_vectors,
                "ai_poison_handler_vectors": poison_handler_vectors,
                "ai_item_handler_vectors": item_handler_vectors,
                "battle_session_vector": battle_session,
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
