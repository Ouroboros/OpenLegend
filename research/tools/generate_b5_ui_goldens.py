#!/usr/bin/env python3
"""Generate B5 title/menu and protagonist-roll goldens from original assets.

This independent oracle intentionally does not import or execute OpenLegend code.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def fnv1a64(data: bytes | bytearray) -> str:
    value = 0xCBF29CE484222325
    for byte in data:
        value ^= byte
        value = (value * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return f"{value:016x}"


def archive_entries(index_bytes: bytes, group_bytes: bytes) -> list[bytes]:
    assert len(index_bytes) % 4 == 0
    result: list[bytes] = []
    begin = 0
    for (end,) in struct.iter_unpack("<I", index_bytes):
        assert begin <= end <= len(group_bytes)
        result.append(group_bytes[begin:end])
        begin = end
    assert begin == len(group_bytes)
    return result


def draw_sprite(pixels: bytearray, frame: bytes, anchor_x: int, anchor_y: int) -> None:
    width, height, x_offset, y_offset = struct.unpack_from("<HHhh", frame)
    left = anchor_x - x_offset
    top = anchor_y - y_offset
    cursor = 8
    for row in range(height):
        row_size = frame[cursor]
        cursor += 1
        row_end = cursor + row_size
        x = left
        while cursor < row_end:
            skip = frame[cursor]
            count = frame[cursor + 1]
            cursor += 2
            x += skip
            run = frame[cursor : cursor + count]
            cursor += count
            for pixel in run:
                if 0 <= top + row < 200 and 0 <= x < 320:
                    pixels[(top + row) * 320 + x] = pixel
                x += 1
        assert cursor == row_end
        assert x - left <= width
    assert cursor == len(frame)


def title_screen(background: bytes, frames: list[bytes], selection: int) -> bytearray:
    pixels = bytearray(background)
    draw_sprite(pixels, frames[0], 117, 137)
    draw_sprite(pixels, frames[1 + selection], 117, 137 + 20 * selection)
    return pixels


def title_load_screen(background: bytes, frames: list[bytes], slot: int) -> bytearray:
    pixels = bytearray(background)
    draw_sprite(pixels, frames[4], 117, 137)
    draw_sprite(pixels, frames[5 + slot], 117, 137 + 20 * slot)
    return pixels


def title_wait_screen(background: bytes, frames: list[bytes]) -> bytearray:
    pixels = bytearray(background)
    for y in range(135, 135 + 65):
        pixels[y * 320 + 115 : y * 320 + 250] = bytes([0]) * 135
    draw_sprite(pixels, frames[8], 120, 160)
    return pixels


class LegacyRandom:
    def __init__(self, state: int):
        self.state = state & 0xFFFFFFFF
        self.consumed = 0

    def next(self) -> int:
        self.state = (self.state * 0x41C64E6D + 0x3039) & 0xFFFFFFFF
        self.consumed += 1
        return (self.state >> 16) & 0x7FFF

    def bounded(self, upper_bound: int) -> int:
        assert 2 <= upper_bound <= 30000
        return self.next() % upper_bound


def protagonist_roll(seed: int, level: int) -> dict[str, int | str]:
    rng = LegacyRandom(seed)
    result: dict[str, int | str] = {
        "mp_type": rng.bounded(2),
        "maximum_mp": rng.bounded(20) + 21,
    }
    for field in (
        "attack",
        "speed",
        "defence",
        "medicine",
        "use_poison",
        "detoxification",
        "anti_poison",
        "fist",
        "sword",
        "knife",
        "unusual",
        "hidden_weapon",
    ):
        result[field] = rng.bounded(10) + 21
    result["increased_life"] = rng.bounded(5) + 3
    result["maximum_hp"] = int(result["increased_life"]) * 3 * level + 29
    iq_bucket = rng.bounded(10)
    if iq_bucket <= 1:
        result["iq"] = rng.bounded(35) + 30
    elif iq_bucket <= 7:
        result["iq"] = rng.bounded(20) + 60
    else:
        result["iq"] = rng.bounded(20) + 75
    result["hp"] = result["maximum_hp"]
    result["mp"] = result["maximum_mp"]
    result["rng_consumed"] = rng.consumed
    result["rng_state"] = f"{rng.state:08x}"
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--data-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    title_index = (args.data_root / "title.idx").read_bytes()
    title_group = (args.data_root / "title.grp").read_bytes()
    title_big = (args.data_root / "title.big").read_bytes()
    palette = (args.data_root / "mmap.col").read_bytes()
    ranger = (args.data_root / "RANGER.GRP").read_bytes()
    assert len(title_big) == 320 * 200
    assert len(palette) == 256 * 3
    assert len(ranger) == 114242
    frames = archive_entries(title_index, title_group)
    assert len(frames) == 9
    protagonist = ranger[836 : 836 + 182]
    (level,) = struct.unpack_from("<h", protagonist, 15 * 2)

    output = {
        "oracle": "independent Python little-endian/RLE implementation",
        "assets": {
            "title.idx": {"bytes": len(title_index), "sha256": sha256(title_index)},
            "title.grp": {"bytes": len(title_group), "sha256": sha256(title_group)},
            "title.big": {"bytes": len(title_big), "sha256": sha256(title_big)},
            "mmap.col": {"bytes": len(palette), "sha256": sha256(palette)},
        },
        "title_pixels_fnv1a64": {
            "main_selection_0": fnv1a64(title_screen(title_big, frames, 0)),
            "main_selection_1": fnv1a64(title_screen(title_big, frames, 1)),
            "main_selection_2": fnv1a64(title_screen(title_big, frames, 2)),
            "load_slot_0": fnv1a64(title_load_screen(title_big, frames, 0)),
            "load_slot_1": fnv1a64(title_load_screen(title_big, frames, 1)),
            "load_slot_2": fnv1a64(title_load_screen(title_big, frames, 2)),
            "please_wait": fnv1a64(title_wait_screen(title_big, frames)),
        },
        "runtime_ui_regression_fnv1a64": {
            "source": "C++ framebuffer regression lock using baseline seed 0 and protagonist name A",
            "status_selector": "c680e9dc6259c18c",
            "status_page_0": "754d417908729f49",
            "status_page_1": "3fb535c659854c04",
            "items_page_0": "1f2c81326be42838",
        },
        "title_navigation": {
            "main_labels": ["new_game", "load", "exit"],
            "slot_labels_big5_hex": ["a440", "a447", "a454"],
            "down_key": "98",
            "up_key": "9e",
            "confirm_keys": ["0d", "20", "96"],
            "escape_key": "1b",
            "title_escape_effect": "none",
            "load_escape_effect": "return_to_main_selection_1",
        },
        "protagonist_level_from_baseline": level,
        "protagonist_rolls": {
            f"{seed:08x}": protagonist_roll(seed, level)
            for seed in (0, 1, 0x12345678, 0xFFFFFFFF)
        },
        "protagonist_cheat": {
            "sequence": "BABERUTH",
            "mp_type": 2,
            "maximum_mp": 40,
            "attack": 30,
            "speed": 30,
            "defence": 30,
            "medicine": 30,
            "use_poison": 30,
            "detoxification": 30,
            "anti_poison": 30,
            "fist": 30,
            "sword": 30,
            "knife": 30,
            "unusual": 30,
            "hidden_weapon": 30,
            "increased_life": 10,
            "maximum_hp": 50,
            "iq": 100,
            "rng_consumed_for_matching_key": 0,
        },
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(output, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
