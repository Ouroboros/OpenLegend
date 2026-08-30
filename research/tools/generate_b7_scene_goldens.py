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


def rgb4_lookup(palette: list[tuple[int, int, int]]) -> list[int]:
    result: list[int] = []
    for red in range(16):
        for green in range(16):
            for blue in range(16):
                target = (red * 4 + 2, green * 4 + 2, blue * 4 + 2)
                best_distance = 30_000
                best_index = 0
                for index, color in enumerate(palette):
                    distance = sum((target[channel] - color[channel]) ** 2 for channel in range(3))
                    if distance < best_distance:
                        best_distance = distance
                        best_index = index
                result.append(best_index)
    return result


def draw_translucent(
    pixels: bytearray,
    frame: bytes,
    anchor_x: int,
    anchor_y: int,
    weight: int,
    palette: list[tuple[int, int, int]],
    lookup: list[int],
) -> None:
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
            for source_index in frame[cursor:cursor + count]:
                destination_y = top + row
                if 0 <= destination_x < 320 and 0 <= destination_y < 200:
                    destination_offset = destination_y * 320 + destination_x
                    source = palette[source_index]
                    destination = palette[pixels[destination_offset]]
                    components = tuple(
                        source[channel] * weight // 32
                        + destination[channel] * (8 - weight) // 32
                        for channel in range(3)
                    )
                    pixels[destination_offset] = lookup[
                        components[0] * 256 + components[1] * 16 + components[2]
                    ]
                destination_x += 1
            cursor += count
        assert cursor == row_end
        assert destination_x <= left + width
    assert cursor == len(frame)


def weather_frame(
    base_frame: bytes,
    cloud_archive: list[bytes],
    palette: list[tuple[int, int, int]],
    ticks: int,
) -> tuple[bytes, list[dict[str, int]], int]:
    state = 1

    def bounded(upper_bound: int) -> int:
        nonlocal state
        if upper_bound <= 1 or upper_bound > 30_000:
            return 0
        state = (state * 0x41C64E6D + 0x3039) & 0xFFFFFFFF
        return ((state >> 16) & 0x7FFF) % upper_bound

    particles = [{"kind": bounded(4)} for _ in range(3)]
    for particle in particles:
        particle["weight"] = bounded(3) + 6
    for particle in particles:
        particle["x"] = bounded(100) - 300
    for index, particle in enumerate(particles):
        particle["y"] = -3000 - index * 1000 if bounded(2) else bounded(50) + index * 75
    for _ in range(1, ticks):
        for particle in particles:
            particle["x"] += 1
    output = bytearray(base_frame)
    lookup = rgb4_lookup(palette)
    for particle in particles:
        if particle["y"] > -1000:
            draw_translucent(
                output,
                cloud_archive[particle["kind"]],
                particle["x"],
                particle["y"],
                particle["weight"],
                palette,
                lookup,
            )
    return bytes(output), particles, state


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
    view_origin: tuple[int, int] | None = None,
    player_picture: int | None = None,
) -> bytes:
    pixels = bytearray(320 * 200)
    if view_origin is None:
        origin_x = min(max(player_x - 11, 0), 36)
        origin_y = min(max(player_y - 11, 0), 36)
    else:
        origin_x, origin_y = view_origin

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
                picture = FRAME_BASE[direction] if player_picture is None else player_picture
                if picture not in (0, -86):
                    draw(picture, sx, sy - height)
            decoration = scene_value(scene_words, 2, x, y)
            if decoration:
                draw(decoration, sx, sy - scene_value(scene_words, 5, x, y))
    return bytes(pixels)


def pan_trace(
    scene_words: tuple[int, ...],
    event_words: tuple[int, ...],
    sprites: list[bytes],
    player_x: int,
    player_y: int,
    direction: int,
    arguments: tuple[int, int, int, int],
) -> list[dict[str, object]]:
    source_x, source_y, target_x, target_y = arguments
    origin_x = min(max(player_x - 11, 0), 36)
    origin_y = min(max(player_y - 11, 0), 36)
    result: list[dict[str, object]] = []
    step_x = -1 if target_x < source_x else 1
    for x in range(source_x, target_x, step_x):
        origin_x = min(max(x - 11, 0), 36)
        frame = render_scene(
            scene_words, event_words, sprites, player_x, player_y, direction,
            (origin_x, origin_y),
        )
        result.append({
            "view_origin_x": origin_x,
            "view_origin_y": origin_y,
            "frame_fnv1a64": fnv1a64(frame),
        })
    step_y = -1 if target_y < source_y else 1
    for y in range(source_y, target_y, step_y):
        origin_y = min(max(y - 11, 0), 36)
        frame = render_scene(
            scene_words, event_words, sprites, player_x, player_y, direction,
            (origin_x, origin_y),
        )
        result.append({
            "view_origin_x": origin_x,
            "view_origin_y": origin_y,
            "frame_fnv1a64": fnv1a64(frame),
        })
    return result


def picture_animation_trace(
    scene_words: tuple[int, ...],
    event_words: tuple[int, ...],
    sprites: list[bytes],
    player_x: int,
    player_y: int,
    direction: int,
    event_index: int,
    start_frame: int,
    end_frame: int,
) -> list[dict[str, object]]:
    mutable_events = list(event_words)
    result: list[dict[str, object]] = []
    for frame_id in range(start_frame, end_frame + 1, 2):
        for field in (5, 6, 7):
            mutable_events[event_index * 11 + field] = frame_id
        frame = render_scene(
            scene_words, tuple(mutable_events), sprites,
            player_x, player_y, direction,
        )
        result.append({
            "picture": frame_id,
            "frame_fnv1a64": fnv1a64(frame),
        })
    return result


def dual_picture_animation_trace(
    scene_words: tuple[int, ...],
    event_words: tuple[int, ...],
    sprites: list[bytes],
    player_x: int,
    player_y: int,
    direction: int,
    view_origin: tuple[int, int],
    arguments: tuple[int, int, int, int, int, int],
    player_picture: int | None = None,
) -> list[dict[str, object]]:
    first_event, first_picture, first_end, second_event, second_picture, _ = arguments
    mutable_events = list(event_words)
    result: list[dict[str, object]] = []
    while first_picture <= first_end:
        for event_index, picture in (
            (first_event, first_picture), (second_event, second_picture)
        ):
            assert event_index >= 0
            for field in (5, 6, 7):
                mutable_events[event_index * 11 + field] = picture
        frame = render_scene(
            scene_words, tuple(mutable_events), sprites,
            player_x, player_y, direction, view_origin, player_picture,
        )
        result.append({
            "first_picture": first_picture,
            "second_picture": second_picture,
            "frame_fnv1a64": fnv1a64(frame),
        })
        first_picture += 2
        second_picture += 2
    return result


def three_statue_animation_trace(
    scene_words: tuple[int, ...],
    event_words: tuple[int, ...],
    sprites: list[bytes],
    player_x: int,
    player_y: int,
    direction: int,
) -> list[dict[str, object]]:
    mutable_events = list(event_words)
    result: list[dict[str, object]] = []
    player_picture = 0
    for player_picture in range(7664, 7676, 2):
        frame = render_scene(
            scene_words, tuple(mutable_events), sprites,
            player_x, player_y, direction,
            player_picture=player_picture,
        )
        result.append({
            "phase": 0,
            "player_picture": player_picture,
            "event_pictures": [
                mutable_events[event * 11 + 5] for event in (2, 3, 4)
            ],
            "frame_fnv1a64": fnv1a64(frame),
        })
    for value in range(0, 58, 2):
        if player_picture < 7688:
            player_picture = value + 7676
        pictures = (value + 7690, value + 7748, value + 7806)
        for event_index, picture in zip((2, 3, 4), pictures):
            for field in (5, 6, 7):
                mutable_events[event_index * 11 + field] = picture
        frame = render_scene(
            scene_words, tuple(mutable_events), sprites,
            player_x, player_y, direction,
            player_picture=player_picture,
        )
        result.append({
            "phase": 1,
            "player_picture": player_picture,
            "event_pictures": list(pictures),
            "frame_fnv1a64": fnv1a64(frame),
        })
    return result


def scripted_walk_trace(
    scene_words: tuple[int, ...],
    event_words: tuple[int, ...],
    sprites: list[bytes],
    player_x: int,
    player_y: int,
    arguments: tuple[int, int, int, int],
) -> list[dict[str, object]]:
    source_x, source_y, target_x, target_y = arguments
    x, y = player_x, player_y
    walk_offset = 0
    result: list[dict[str, object]] = []

    def step(horizontal: bool, delta: int) -> None:
        nonlocal x, y, walk_offset
        walk_offset += 2
        if walk_offset > 12:
            walk_offset = 2
        direction = (2 if delta < 0 else 1) if horizontal else (0 if delta < 0 else 3)
        tx = min(max(x + (delta if horizontal else 0), 0), 63)
        ty = min(max(y + (0 if horizontal else delta), 0), 63)
        earth = scene_value(scene_words, 0, tx, ty)
        blocked = scene_value(scene_words, 1, tx, ty) != 0
        blocked = blocked or scene_value(scene_words, 4, tx, ty) - scene_value(scene_words, 4, x, y) >= 10
        event = scene_value(scene_words, 3, tx, ty)
        blocked = blocked or (event >= 0 and event_value(event_words, event, 0) != 0)
        blocked = blocked or any(low <= earth <= high for low, high in zip(BLOCKED_LOW, BLOCKED_HIGH))
        if not blocked:
            x, y = tx, ty
        picture = FRAME_BASE[direction] + walk_offset
        frame = render_scene(
            scene_words, event_words, sprites, x, y, direction,
            player_picture=picture,
        )
        result.append({
            "x": x,
            "y": y,
            "direction": direction,
            "player_picture": picture,
            "wait_ticks": 3,
            "frame_fnv1a64": fnv1a64(frame),
        })

    step_x = -1 if target_x < source_x else 1
    for _ in range(source_x, target_x, step_x):
        step(True, step_x)
    step_y = -1 if target_y < source_y else 1
    for _ in range(source_y, target_y, step_y):
        step(False, step_y)
    return result


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

    weather_scene_id = 5
    weather_map = words(scene_maps[weather_scene_id])
    weather_events = words(scene_events[weather_scene_id])
    weather_sprites = sentinel((root / "SDX005").read_bytes(), (root / "SMP005").read_bytes())
    ranger = (root / "RANGER.GRP").read_bytes()
    metadata_offset = 97_076 + weather_scene_id * 52
    metadata = struct.unpack_from("<26h", ranger, metadata_offset)
    weather_x, weather_y = metadata[14], metadata[15]
    weather_base = render_scene(weather_map, weather_events, weather_sprites, weather_x, weather_y, 1)
    cloud_archive = packed((root / "CLOUD.IDX").read_bytes(), (root / "CLOUD.GRP").read_bytes())
    palette_bytes = (root / "MMAP.COL").read_bytes()
    palette = [tuple(palette_bytes[offset:offset + 3]) for offset in range(0, len(palette_bytes), 3)]
    weather_ticks = 300
    weather_output, weather_particles, weather_random_state = weather_frame(
        weather_base, cloud_archive, palette, weather_ticks
    )
    decoded_talks = [bytes(value ^ 0xFF for value in entry[:-1]) + b"\0" for entry in talks]
    coverage = opcode_coverage(scripts)
    script_30 = words(scripts[30])
    assert script_30[:5] == (25, 41, 31, 34, 31)
    opcode_25_script_30 = pan_trace(
        smap, sevent, sprites, 44, 29, 1, script_30[1:5]
    )

    animation_scene_id = 53
    animation_map = words(scene_maps[animation_scene_id])
    animation_events = words(scene_events[animation_scene_id])
    animation_sprites = sentinel(
        (root / "SDX053").read_bytes(), (root / "SMP053").read_bytes()
    )
    animation_trigger_event = 4
    animation_x = event_value(animation_events, animation_trigger_event, 9)
    animation_y = event_value(animation_events, animation_trigger_event, 10)
    script_535 = words(scripts[535])
    assert script_535[:4] == (27, 3, 6342, 6348)
    opcode_27_script_535 = picture_animation_trace(
        animation_map,
        animation_events,
        animation_sprites,
        animation_x,
        animation_y,
        1,
        script_535[1],
        script_535[2],
        script_535[3],
    )

    walk_scene_id = 39
    walk_map = words(scene_maps[walk_scene_id])
    walk_events = words(scene_events[walk_scene_id])
    walk_sprites = sentinel(
        (root / "SDX039").read_bytes(), (root / "SMP039").read_bytes()
    )
    script_343 = words(scripts[343])
    assert script_343[30:35] == (30, 28, 24, 28, 19)
    opcode_30_script_343 = scripted_walk_trace(
        walk_map, walk_events, walk_sprites, 28, 24, script_343[31:35]
    )

    script_534 = words(scripts[534])
    assert script_534[120:127] == (44, 1, 6486, 6520, 2, 6450, 6484)
    opcode_44_script_534 = dual_picture_animation_trace(
        animation_map,
        animation_events,
        animation_sprites,
        animation_x,
        animation_y,
        1,
        (12, 9),
        script_534[121:127],
    )

    statue_scene_id = 14
    statue_map = words(scene_maps[statue_scene_id])
    statue_events = words(scene_events[statue_scene_id])
    statue_sprites = sentinel(
        (root / "SDX014").read_bytes(), (root / "SMP014").read_bytes()
    )
    script_655 = words(scripts[655])
    assert script_655[47] == 57
    opcode_57_script_655 = three_statue_animation_trace(
        statue_map, statue_events, statue_sprites, 32, 15, 1
    )

    ending_scene_id = 83
    ending_map = words(scene_maps[ending_scene_id])
    ending_events = words(scene_events[ending_scene_id])
    ending_sprites = sentinel(
        (root / "SDX083").read_bytes(), (root / "SMP083").read_bytes()
    )
    script_1017 = words(scripts[1017])
    assert script_1017[5:12] == (62, 0, 8054, 8128, 1, 8130, 8204)
    opcode_62_script_1017 = dual_picture_animation_trace(
        ending_map,
        ending_events,
        ending_sprites,
        22,
        41,
        1,
        (11, 30),
        script_1017[6:12],
        -86,
    )

    script_932 = words(scripts[932])
    assert script_932[38] == 59
    opcode_59_targets = [
        [0, 0], [49, 2], [4, 1], [44, 0], [44, 1], [37, 5],
        [30, 0], [59, 0], [40, 3], [56, 1], [1, 7], [1, 8], [1, 10],
        [40, 7], [40, 8], [77, 0], [54, 0], [62, 3], [62, 4],
        [60, 2], [60, 15], [52, 1], [61, 0], [61, 8], [78, 0],
        [18, 0], [18, 1], [69, 0], [69, 1], [45, 0], [52, 2],
        [42, 6], [42, 7], [8, 8], [7, 6], [80, 1],
    ]

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
        "kdef": {
            **coverage,
            "opcode_25_script_30": {
                "arguments": list(script_30[1:5]),
                "frames": opcode_25_script_30,
            },
            "opcode_27_script_535": {
                "scene_id": animation_scene_id,
                "player_x": animation_x,
                "player_y": animation_y,
                "arguments": list(script_535[1:4]),
                "frames": opcode_27_script_535,
            },
            "opcode_30_script_343": {
                "scene_id": walk_scene_id,
                "arguments": list(script_343[31:35]),
                "frames": opcode_30_script_343,
            },
            "opcode_44_script_534": {
                "scene_id": animation_scene_id,
                "arguments": list(script_534[121:127]),
                "frames": opcode_44_script_534,
            },
            "opcode_57_script_655": {
                "scene_id": statue_scene_id,
                "player_x": 32,
                "player_y": 15,
                "frames": opcode_57_script_655,
            },
            "opcode_62_script_1017": {
                "scene_id": ending_scene_id,
                "player_x": 22,
                "player_y": 41,
                "arguments": list(script_1017[6:12]),
                "frames": opcode_62_script_1017,
            },
            "opcode_59_script_932": {
                "script_id": 932,
                "program_counter": 38,
                "party_source_indices": [6, 5, 4, 3, 2, 1],
                "index_6_source": "inventory_item_0",
                "disabled_event_fields": [0, 0, -1, -1, -1, -1, -1, -1, 0],
                "targets": opcode_59_targets,
            },
        },
        "scene_5_weather": {
            "initial_x": weather_x,
            "initial_y": weather_y,
            "base_frame_fnv1a64": fnv1a64(weather_base),
            "ticks": weather_ticks,
            "frame_fnv1a64": fnv1a64(weather_output),
            "particles": weather_particles,
            "random_state": f"0x{weather_random_state:08x}",
        },
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
