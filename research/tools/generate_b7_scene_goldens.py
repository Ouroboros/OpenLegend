#!/usr/bin/env python3
"""Generate B7 scene/event/dialogue goldens without calling OpenLegend code."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

EXTENT = 64
VIEW = 28
WIDTHS = (
    1, 4, 3, 14, 4, 3, 5, 1, 2, 3, 2, 3, 1, 1, 1, 2, 4,
    6, 4, 3, 3, 2, 1, 3, 1, 5, 6, 4, 6, 6, 5, 4, 3, 4,
    3, 5, 4, 2, 5, 2, 2, 4, 3, 4, 7, 3, 3, 3, 3, 3, 8,
    1, 1, 1, 1, 5, 2, 1, 1, 1, 6, 3, 7, 3, 1, 1, 2, 2,
)
FRAME_BASE = (5002, 5016, 5030, 5044)
BLOCKED_LOW = (358, 374, 458, 506, 818, 838, 934, 1016, 1324)
BLOCKED_HIGH = (362, 380, 464, 610, 824, 838, 936, 1022, 1348)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--data-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args()


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def fnv1a64(data: bytes) -> str:
    result = 0xCBF29CE484222325
    for value in data:
        result ^= value
        result = (result * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return f"0x{result:016x}"


def packed(index: bytes, group: bytes) -> list[bytes]:
    ends = struct.unpack(f"<{len(index) // 4}I", index)
    result: list[bytes] = []
    begin = 0
    for end in ends:
        if end == 0 and begin:
            break
        result.append(group[begin:end])
        begin = end
    assert begin == len(group)
    return result


def sentinel(index: bytes, group: bytes) -> list[bytes]:
    ends = struct.unpack(f"<{len(index) // 4}I", index)
    assert ends[-1] == 0
    result: list[bytes] = []
    begin = 0
    for end in ends[:-1]:
        assert begin <= end <= len(group)
        result.append(group[begin:end])
        begin = end
    result.append(group[begin:])
    return result


def draw_sprite(pixels: bytearray, frame: bytes, anchor_x: int, anchor_y: int) -> None:
    assert len(frame) >= 8
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
            for pixel in frame[cursor:cursor + count]:
                destination_y = top + row
                if 0 <= destination_x < 320 and 0 <= destination_y < 200:
                    pixels[destination_y * 320 + destination_x] = pixel
                destination_x += 1
            cursor += count
        assert cursor == row_end
        assert destination_x <= left + width
    assert cursor == len(frame)


def words(data: bytes) -> tuple[int, ...]:
    return struct.unpack(f"<{len(data) // 2}h", data)


def scene_value(scene_words: tuple[int, ...], layer: int, x: int, y: int) -> int:
    return scene_words[layer * 4096 + y * 64 + x]


def event_value(event_words: tuple[int, ...], event: int, field: int) -> int:
    return event_words[event * 11 + field]


def render_scene(
    scene_words: tuple[int, ...],
    event_words: tuple[int, ...],
    sprites: list[bytes],
    player_x: int,
    player_y: int,
    direction: int,
) -> bytes:
    pixels = bytearray(320 * 200)
    origin_x = min(max(player_x - 11, 0), 36)
    origin_y = min(max(player_y - 11, 0), 36)

    def draw(legacy_id: int, anchor_x: int, anchor_y: int) -> None:
        assert legacy_id >= 0 and legacy_id % 2 == 0
        draw_sprite(pixels, sprites[legacy_id // 2], anchor_x, anchor_y)

    for local_x in range(VIEW):
        for local_y in range(VIEW):
            x = origin_x + local_x
            y = origin_y + local_y
            if scene_value(scene_words, 4, x, y) == 0:
                draw(scene_value(scene_words, 0, x, y),
                     18 * (local_x - local_y) + 145,
                     9 * (local_x + local_y) - 81)
    for local_x in range(VIEW):
        for local_y in range(VIEW):
            x = origin_x + local_x
            y = origin_y + local_y
            sx = 18 * (local_x - local_y) + 145
            sy = 9 * (local_x + local_y) - 81
            height = scene_value(scene_words, 4, x, y)
            if height:
                draw(scene_value(scene_words, 0, x, y), sx, sy)
            building = scene_value(scene_words, 1, x, y)
            if building not in (0, 15000):
                draw(building, sx, sy - height)
            event = scene_value(scene_words, 3, x, y)
            if event >= 0:
                picture = event_value(event_words, event, 5)
                if picture > 0:
                    draw(picture, sx, sy - height)
            if x == player_x and y == player_y:
                draw(FRAME_BASE[direction], sx, sy - height)
            decoration = scene_value(scene_words, 2, x, y)
            if decoration:
                draw(decoration, sx, sy - scene_value(scene_words, 5, x, y))
    return bytes(pixels)


def movement_trace(
    scene_words: tuple[int, ...], event_words: tuple[int, ...], x: int, y: int
) -> list[dict[str, object]]:
    result: list[dict[str, object]] = []
    directions = ((1, 0, 1), (0, -1, 0), (-1, 0, 2), (0, 1, 3))
    for dx, dy, direction in directions:
        tx = min(max(x + dx, 0), 63)
        ty = min(max(y + dy, 0), 63)
        earth = scene_value(scene_words, 0, tx, ty)
        blocked = scene_value(scene_words, 1, tx, ty) != 0
        blocked = blocked or scene_value(scene_words, 4, tx, ty) - scene_value(scene_words, 4, x, y) >= 10
        event = scene_value(scene_words, 3, tx, ty)
        blocked = blocked or (event >= 0 and event_value(event_words, event, 0) != 0)
        blocked = blocked or any(low <= earth <= high for low, high in zip(BLOCKED_LOW, BLOCKED_HIGH))
        if not blocked:
            x, y = tx, ty
        result.append({"direction": direction, "x": x, "y": y, "moved": not blocked})
    return result


def opcode_coverage(entries: list[bytes]) -> dict[str, object]:
    counts = [0] * 68
    terminated = 0
    for script_id, entry in enumerate(entries):
        code = words(entry)
        pc = 0
        while pc < len(code):
            opcode = code[pc]
            if opcode == -1:
                terminated += 1
                break
            assert 0 <= opcode < len(WIDTHS), (script_id, pc, opcode)
            counts[opcode] += 1
            pc += WIDTHS[opcode]
        else:
            raise AssertionError((script_id, "unterminated"))
    return {
        "terminated_scripts": terminated,
        "used_opcodes": [index for index, count in enumerate(counts) if count],
        "unused_opcodes": [index for index, count in enumerate(counts) if not count],
        "counts": counts,
        "instruction_count": sum(counts),
    }


def main() -> None:
    args = parse_args()
    root = args.data_root
    scene_maps = packed((root / "ALLSIN.IDX").read_bytes(), (root / "ALLSIN.GRP").read_bytes())
    scene_events = packed((root / "ALLDEF.IDX").read_bytes(), (root / "ALLDEF.GRP").read_bytes())
    talks = packed((root / "TALK.IDX").read_bytes(), (root / "TALK.GRP").read_bytes())
    scripts = packed((root / "KDEF.IDX").read_bytes(), (root / "KDEF.GRP").read_bytes())
    assert len(scene_maps) == 100 and all(len(entry) == 49152 for entry in scene_maps)
    assert len(scene_events) == 100 and all(len(entry) == 4400 for entry in scene_events)
    assert len(talks) == 2977
    assert len(scripts) == 1018

    scene_id = 70
    smap = words(scene_maps[scene_id])
    sevent = words(scene_events[scene_id])
    sprites = sentinel((root / "SDX070").read_bytes(), (root / "SMP070").read_bytes())
    frame = render_scene(smap, sevent, sprites, 44, 29, 1)
    decoded_talks = [bytes(value ^ 0xFF for value in entry[:-1]) + b"\0" for entry in talks]
    coverage = opcode_coverage(scripts)

    output = {
        "format": 1,
        "source": "current DOS assets; independent Python int16le/RLE/KDEF parser",
        "assets": {
            "scene_count": len(scene_maps),
            "scene_map_bytes": sum(map(len, scene_maps)),
            "scene_event_count": len(scene_events),
            "scene_event_bytes": sum(map(len, scene_events)),
            "talk_count": len(talks),
            "script_count": len(scripts),
            "allsin_sha256": sha256((root / "ALLSIN.GRP").read_bytes()),
            "alldef_sha256": sha256((root / "ALLDEF.GRP").read_bytes()),
            "talk_sha256": sha256((root / "TALK.GRP").read_bytes()),
            "kdef_sha256": sha256((root / "KDEF.GRP").read_bytes()),
        },
        "talk": {
            "decoded_stream_sha256": sha256(b"".join(decoded_talks)),
            "record_0_sha256": sha256(decoded_talks[0]),
            "record_0_hex": decoded_talks[0].hex(),
            "record_2976_sha256": sha256(decoded_talks[2976]),
        },
        "kdef": coverage,
        "scene_70": {
            "map_sha256": sha256(scene_maps[scene_id]),
            "events_sha256": sha256(scene_events[scene_id]),
            "earth_fnv1a64": fnv1a64(scene_maps[scene_id][:8192]),
            "event_index_fnv1a64": fnv1a64(scene_maps[scene_id][24576:32768]),
            "initial_x": 44,
            "initial_y": 29,
            "view_origin_x": 33,
            "view_origin_y": 18,
            "initial_frame_fnv1a64": fnv1a64(frame),
            "movement_trace": movement_trace(smap, sevent, 44, 29),
        },
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(output, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(output, ensure_ascii=False, sort_keys=True))


if __name__ == "__main__":
    main()
