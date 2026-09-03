#!/usr/bin/env python3
"""Generate B7 scene/event/dialogue goldens without calling OpenLegend code."""

from __future__ import annotations

import argparse
from collections import Counter
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
DISPATCH_HELPERS = (
    "sub_2D653", "sub_2CC21", "sub_2D678", "sub_2D841", "sub_2DD45",
    "sub_2DD77", "sub_2DE03", None, "sub_2DE39", "sub_2DE7D", "sub_2DF0E",
    "sub_2E155", "sub_2E1E8", "sub_2E278", "sub_2E28A", "sub_2E659",
    "sub_2E29C", "sub_2E337", "sub_2E2F5", "sub_2E46B", "sub_2E2D7",
    "sub_2E078", "sub_2E536", "sub_2E639", "sub_2EB49", "sub_2ED8D",
    "sub_2DBF4", "sub_2F053", "sub_2F107", "sub_2F136", "sub_2F171",
    "sub_2F34C", "sub_2F39C", "sub_2F3F0", "sub_2F526", "sub_2F62F",
    "sub_2F6C2", "sub_2F6E3", "sub_2F721", "sub_2F890", "sub_2F8AB",
    "sub_2F8D1", "sub_2F966", "sub_2F9B5", "sub_2F9F2", "sub_2FAB7",
    "sub_2FBC0", "sub_2FC9D", "sub_2FDA6", "sub_2FEBF", "sub_2FEDF",
    "sub_2FF87", "sub_2FFB3", "sub_30035", "sub_30094", "sub_300D9",
    "sub_300FF", "sub_301D1", "sub_302E0", "sub_30559", "sub_30A5A",
    "sub_30B45", "sub_30B81", "sub_31284", "sub_312A6", "sub_31945",
    "sub_31C2F", "sub_31C4A",
)
CONDITIONAL_OFFSETS = {
    4: (2, 3), 5: (1, 2), 6: (2, 3), 9: (1, 2), 11: (1, 2),
    16: (2, 3), 18: (2, 3), 20: (1, 2), 28: (4, 5), 29: (4, 5),
    31: (2, 3), 36: (2, 3), 42: (1, 2), 43: (2, 3), 50: (6, 7),
    55: (3, 4), 60: (4, 5), 61: (1, 2),
}
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


def draw_legacy_text(
    pixels: bytearray,
    x: int,
    y: int,
    text: bytes,
    ascii_font: bytes,
    big5_font: bytes,
    shadow: int,
    foreground: int,
) -> None:
    cursor = 0
    while True:
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
            assert len(glyph) == 32 and 0 <= x and x + 16 < 320 and 0 <= y and y + 16 <= 200
            for row in range(16):
                for byte_index in range(2):
                    bits = glyph[row * 2 + byte_index]
                    for bit in range(8):
                        if bits & (0x80 >> bit):
                            offset = (y + row) * 320 + x + byte_index * 8 + bit
                            pixels[offset] = foreground
                            pixels[offset + 1] = shadow
            x += 16
        else:
            glyph_index = 32 if first == ord("_") else first
            glyph = ascii_font[glyph_index * 16:(glyph_index + 1) * 16]
            assert len(glyph) == 16 and 0 <= x and x + 8 < 320 and 0 <= y and y + 16 <= 200
            for row, bits in enumerate(glyph):
                for bit in range(8):
                    if bits & (0x80 >> bit):
                        offset = (y + row) * 320 + x + bit
                        pixels[offset] = foreground
                        pixels[offset + 1] = shadow
            x += 4 if first == ord("_") else 8


def dialogue_pages(text: bytes) -> list[bytes]:
    pages: list[bytes] = []
    page = bytearray()
    line_break_count = 0
    cursor = 0
    while cursor < len(text) and text[cursor] != 0:
        first = text[cursor]
        if first == ord("*"):
            page.append(first)
            line_break_count += 1
            cursor += 1
            if line_break_count == 3:
                pages.append(bytes(page) + b"\0")
                page.clear()
                line_break_count = 0
            continue
        token_size = 2 if first > 0x7F else 1
        page.extend(text[cursor:cursor + token_size])
        cursor += token_size
    pages.append(bytes(page) + b"\0")
    return pages


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


def apply_shadow_mask(base_frame: bytes, mask: bytes, offset: int) -> bytes:
    runs = struct.unpack(f"<{len(mask) // 2}H", mask)
    assert len(mask) % 2 == 0 and sum(runs) >= 64_000
    pixels = bytearray(base_frame)
    destination = 0
    run_index = 0
    if offset < 0:
        remaining = 64_000 + offset
        zero_count = (runs[0] + offset) & 0xFFFFFFFF
        run_index = 1
    else:
        pixels[:offset] = bytes(offset)
        destination = offset
        remaining = 64_000 - offset
        zero_count = runs[0]
        run_index = 1
    while remaining:
        count = min(zero_count, remaining)
        pixels[destination:destination + count] = bytes(count)
        destination += count
        remaining -= count
        if not remaining:
            break
        skip_count = runs[run_index]
        run_index += 1
        if skip_count >= remaining:
            destination += remaining
            remaining = 0
            break
        destination += skip_count
        remaining -= skip_count
        zero_count = runs[run_index]
        run_index += 1
    if offset < 0:
        pixels[destination:destination - offset] = bytes(-offset)
    return bytes(pixels)


def scene_shadow_frames(
    base_frame: bytes, fixed_mask: bytes, shifted_mask: bytes
) -> tuple[bytes, bytes, int, int]:
    state = 1

    def bounded(upper_bound: int) -> int:
        nonlocal state
        state = (state * 0x41C64E6D + 0x3039) & 0xFFFFFFFF
        return ((state >> 16) & 0x7FFF) % upper_bound

    offset = 320 * (bounded(7) - 3) + (bounded(7) - 3)
    return (
        apply_shadow_mask(base_frame, fixed_mask, 0),
        apply_shadow_mask(base_frame, shifted_mask, offset),
        offset,
        state,
    )


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
    base_frame: bytes | None = None,
) -> bytes:
    pixels = bytearray(320 * 200 if base_frame is None else base_frame)
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
            if event >= 0 and event_value(event_words, event, 5) > 0:
                picture = event_value(event_words, event, 7)
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


def advance_event_animation(
    scene_words: tuple[int, ...],
    event_words: tuple[int, ...],
    counter: int,
) -> tuple[int, ...]:
    mutable_events = list(event_words)
    for x in range(64):
        for y in range(64):
            event = scene_value(scene_words, 3, x, y)
            if event < 0:
                continue
            base = event * 11
            first_picture = mutable_events[base + 5]
            if first_picture <= 0:
                continue
            end_picture = mutable_events[base + 6]
            displayed_picture = mutable_events[base + 7]
            delay = mutable_events[base + 8]
            if displayed_picture >= end_picture:
                displayed_picture = first_picture
            if (
                displayed_picture > first_picture
                and counter % 4 == 0
                and displayed_picture < end_picture
            ):
                displayed_picture += 2
            if (
                delay <= counter % 100
                and displayed_picture == first_picture
                and displayed_picture < end_picture
            ):
                displayed_picture += 2
            mutable_events[base + 7] = displayed_picture
    return tuple(mutable_events)


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


def explicit_scene_present_vectors() -> dict[str, object]:
    owners = {
        "0x2D678": [0x2D790],
        "0x2DE7D": [0x2DEF6],
        "0x2DF0E": [0x2E025],
        "0x2ED8D": [0x2EE43, 0x2EE89, 0x2EED2, 0x2EF18, 0x2EF61, 0x2EFA7, 0x2EFF0, 0x2F036],
        "0x2F053": [0x2F09D, 0x2F0DD],
        "0x2F171": [0x2F205, 0x2F229, 0x2F250, 0x2F274, 0x2F29B, 0x2F2BF, 0x2F2E3, 0x2F307, 0x2F342],
        "0x2F3F0": [0x2F51C],
        "0x2F526": [0x2F626],
        "0x2F9F2": [0x2FA82],
        "0x2FAB7": [0x2FBB7],
        "0x2FBC0": [0x2FC94],
        "0x2FC9D": [0x2FD9D],
        "0x2FDA6": [0x2FEB3],
        "0x2FFB3": [0x3002D],
        "0x301D1": [0x301F4, 0x302B2],
        "0x302E0": [0x30365, 0x30397, 0x303B7, 0x303CD, 0x303E3, 0x303F9, 0x3040F, 0x30445, 0x30460],
        "0x30480": [0x304BB, 0x304D7, 0x304F2],
        "0x30B81": [0x30C08],
        "0x312A6": [0x3177A, 0x3178A, 0x317A0],
    }
    categories = {
        "modal_restore_after_wait": [
            0x2D790, 0x2DEF6, 0x2E025, 0x2F51C, 0x2F626,
            0x2FBB7, 0x2FC94, 0x2FD9D, 0x2FEB3, 0x3002D,
        ],
        "animation_delayed_frames": [
            0x2EE43, 0x2EE89, 0x2EED2, 0x2EF18, 0x2EF61, 0x2EFA7, 0x2EFF0, 0x2F036,
            0x2F09D, 0x2F0DD,
            0x2F205, 0x2F229, 0x2F250, 0x2F274, 0x2F29B, 0x2F2BF, 0x2F2E3, 0x2F307,
            0x2FA82, 0x301F4, 0x302B2, 0x30C08,
        ],
        "scripted_walk_final_standing_frame": [0x2F342],
        "tournament_boundaries": [
            0x30365, 0x30397, 0x303B7, 0x303CD, 0x303E3, 0x303F9,
            0x3040F, 0x30445, 0x30460, 0x304BB, 0x304D7, 0x304F2,
        ],
        "shop_feedback_boundaries": [0x3177A, 0x3178A, 0x317A0],
    }
    calls = [address for addresses in owners.values() for address in addresses]
    categorized = [address for addresses in categories.values() for address in addresses]
    assert len(calls) == len(set(calls)) == 48
    assert sorted(calls) == sorted(categorized)
    return {
        "start": "0x2D653",
        "end": "0x2D678",
        "size_bytes": 37,
        "instruction_count": 9,
        "return_value": 0,
        "call_count": len(calls),
        "owner_count": len(owners),
        "owners": {owner: [f"0x{address:X}" for address in addresses]
                   for owner, addresses in owners.items()},
        "categories": {name: [f"0x{address:X}" for address in addresses]
                       for name, addresses in categories.items()},
    }


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
    conditional_targets = 0
    maximum_words = 0
    maximum_word_scripts: list[int] = []
    for script_id, entry in enumerate(entries):
        code = words(entry)
        if len(code) > maximum_words:
            maximum_words = len(code)
            maximum_word_scripts = [script_id]
        elif len(code) == maximum_words:
            maximum_word_scripts.append(script_id)
        pc = 0
        starts: set[int] = set()
        while pc < len(code):
            starts.add(pc)
            opcode = code[pc]
            if opcode == -1:
                terminated += 1
                assert pc + 1 == len(code), (script_id, pc, len(code))
                break
            assert 0 <= opcode < len(WIDTHS), (script_id, pc, opcode)
            width = WIDTHS[opcode]
            assert pc + width <= len(code), (script_id, pc, opcode, width, len(code))
            counts[opcode] += 1
            pc += width
        else:
            raise AssertionError((script_id, "unterminated"))
        pc = 0
        while code[pc] != -1:
            opcode = code[pc]
            width = WIDTHS[opcode]
            for argument in CONDITIONAL_OFFSETS.get(opcode, ()):
                target = pc + width + code[pc + argument]
                assert target in starts, (script_id, pc, opcode, argument, target)
                conditional_targets += 1
            pc += width
    assert len(WIDTHS) == len(DISPATCH_HELPERS) == 68
    assert DISPATCH_HELPERS[7] is None
    return {
        "terminated_scripts": terminated,
        "used_opcodes": [index for index, count in enumerate(counts) if count],
        "unused_opcodes": [index for index, count in enumerate(counts) if not count],
        "counts": counts,
        "instruction_count": sum(counts),
        "maximum_record_words": maximum_words,
        "maximum_record_scripts": maximum_word_scripts,
        "conditional_targets": conditional_targets,
        "dispatch_contract": {
            "instruction_widths": list(WIDTHS),
            "helper_by_opcode": list(DISPATCH_HELPERS),
            "physical_callers": [
                "sub_25BBA:0x25CCB leave_party_script",
                "sub_26B5E:0x26E52 new_game_script_691",
                "sub_29C36:0x29D1F interaction_script",
                "sub_2B308:0x2B3A5 item_script",
                "sub_2B3B4:0x2B475 automatic_script",
            ],
            "caller_return_value": "ignored",
            "terminators": ["word_-1", "opcode_7_execution_flag_clear"],
            "invalid_opcode": "machine_and_modern_keep_pc_unchanged",
            "opcode_62": "sub_30B81_does_not_return_via_sub_30C3D",
        },
    }


def main_loop_dispatch_vectors(world_palette: bytes) -> dict[str, object]:
    cases = [
        ("all", True, True, True, True, True),
        ("up_down_right_menu", False, True, True, True, True),
        ("down_right_menu", False, False, True, True, True),
        ("right_menu", False, False, False, True, True),
        ("menu", False, False, False, False, True),
        ("idle", False, False, False, False, False),
    ]
    dispatch: dict[str, dict[str, object]] = {}
    for name, left, up, down, right, menu in cases:
        if left:
            action = "left"
        elif up:
            action = "up"
        elif down:
            action = "down"
        elif right:
            action = "right"
        elif menu:
            action = "menu"
        else:
            action = "idle"
        dispatch[name] = {
            "held": {"left": left, "up": up, "down": down, "right": right},
            "menu_edge": menu,
            "action": action,
            "menu_edge_consumed": action == "menu",
        }
    assert len(world_palette) == 256 * 3
    palette = [list(world_palette[index:index + 3]) for index in range(0, len(world_palette), 3)]

    def cycle_palette() -> None:
        palette[224:232] = [palette[231], *palette[224:231]]
        palette[244:253] = [palette[252], *palette[244:252]]

    cycle_palette()
    after_first = bytes(component for color in palette for component in color)
    after_five = bytes(component for color in palette for component in color)
    cycle_palette()
    after_sixth = bytes(component for color in palette for component in color)
    return {
        "priority": ["left", "up", "down", "right", "menu", "idle"],
        "dispatch": dispatch,
        "post_action_order": [
            "pending_load_slot",
            "weather",
            "idle_animation_if_active",
            "world_render",
            "present",
            "palette_cycle_after_present_every_five_ticks",
            "wait_for_tick_change",
        ],
        "palette_cycle": {
            "ranges": [[224, 231], [244, 252]],
            "direction": "rotate_right",
            "after_first_present_fnv1a64": fnv1a64(after_first),
            "after_fifth_present_fnv1a64": fnv1a64(after_five),
            "after_sixth_present_fnv1a64": fnv1a64(after_sixth),
            "shared_phase": {
                "world_before_scene": 4,
                "scene_initial": 4,
                "after_scene_tick": 0,
                "world_after_return": 0,
            },
        },
        "shutdown_order": [
            "world_archives",
            "music_fade",
            "audio_system",
            "runtime_platform",
            "exit_zero",
        ],
    }


def rectangle_outline_vectors() -> dict[str, object]:
    x, y, width, height = 55, 62, 40, 40

    def render(color: int) -> bytearray:
        framebuffer = bytearray([7]) * (320 * 200)

        def fill(left: int, top: int, rectangle_width: int, rectangle_height: int) -> None:
            for destination_y in range(top, top + rectangle_height):
                begin = destination_y * 320 + left
                framebuffer[begin:begin + rectangle_width] = bytes([color]) * rectangle_width

        fill(x, y, width, 1)
        fill(x, y, 1, height)
        fill(x + width - 1, y, 1, height)
        fill(x, y + height - 1, width, 1)
        return framebuffer

    normal = render(0)
    selected = render(255)
    assert normal[(y + 1) * 320 + x + 1] == 7
    assert selected[(y + 1) * 320 + x + 1] == 7
    return {
        "entry_range": "0x2D501..0x2D590",
        "instruction_count": 41,
        "caller": "sub_2A186 item grid",
        "caller_addresses": ["0x2A2A0", "0x2A32F"],
        "geometry": [x, y, width, height],
        "grid": {
            "columns": 5,
            "rows": 3,
            "origin": [55, 62],
            "step": [42, 42],
            "positions": [
                [55 + 42 * column, 62 + 42 * row]
                for row in range(3)
                for column in range(5)
            ],
        },
        "draw_order": ["top", "left", "right", "bottom"],
        "pixel_writes": 160,
        "unique_border_pixels": 156,
        "normal_color": 0,
        "normal_frame_fnv1a64": fnv1a64(normal),
        "selected_color": 255,
        "selected_frame_fnv1a64": fnv1a64(selected),
        "interior_preserved": 7,
    }


def portrait_archive_vectors(root: Path, ranger: bytes) -> dict[str, object]:
    index = (root / "HDGRP.IDX").read_bytes()
    group = (root / "HDGRP.GRP").read_bytes()
    assert len(index) == 460 and len(index) % 4 == 0
    frames = packed(index, group)
    assert len(frames) == 115

    metadata: list[tuple[int, int, int, int, int, int]] = []
    for frame in frames:
        assert len(frame) >= 8
        width, height, x_offset, y_offset = struct.unpack_from("<HHhh", frame)
        cursor = 8
        run_count = 0
        pixel_count = 0
        for _ in range(height):
            assert cursor < len(frame)
            row_end = cursor + 1 + frame[cursor]
            cursor += 1
            assert row_end <= len(frame)
            x = 0
            while cursor < row_end:
                assert row_end - cursor >= 2
                skip, count = frame[cursor:cursor + 2]
                cursor += 2
                assert count <= row_end - cursor
                x += skip
                assert x + count <= width
                cursor += count
                x += count
                run_count += 1
                pixel_count += count
            assert cursor == row_end
        assert cursor == len(frame)
        metadata.append((width, height, x_offset, y_offset, run_count, pixel_count))

    caller_anchors = [
        [78, 68],
        [25, 71],
        [239, 184],
        [239, 71],
        [25, 184],
        [242, 82],
        [22, 82],
    ]
    render_stream = hashlib.sha256()
    for background, (x, y) in enumerate(caller_anchors, 1):
        for frame in frames:
            pixels = bytearray([background]) * (320 * 200)
            draw_sprite(pixels, frame, x, y)
            render_stream.update(pixels)

    role_head_ids = [
        struct.unpack_from("<h", ranger, 836 + role * 182 + 2)[0]
        for role in range(320)
    ]
    invalid_role_head_ids = [
        [role, head_id]
        for role, head_id in enumerate(role_head_ids)
        if not 0 <= head_id < len(frames)
    ]
    assert not invalid_role_head_ids

    entry_stream = b"".join(struct.pack("<I", len(frame)) + frame for frame in frames)
    metadata_stream = b"".join(
        struct.pack("<HHhhII", *record) for record in metadata
    )
    return {
        "entry_range": "0x2D590..0x2D653",
        "instruction_count": 49,
        "caller_addresses": [
            "0x22AD7", "0x234BF", "0x2CDA0", "0x2CE7F", "0x3C752",
        ],
        "caller_owners": ["sub_22A59", "sub_2CC21", "sub_3C6D3"],
        "resources": ["HDGRP.IDX", "HDGRP.GRP"],
        "direct_head_id_index": True,
        "legacy_sprite_divide_by_two": False,
        "entry_count": len(frames),
        "index_bytes": len(index),
        "group_bytes": len(group),
        "index_sha256": sha256(index),
        "group_sha256": sha256(group),
        "length_prefixed_entry_stream_sha256": sha256(entry_stream),
        "metadata_stream_sha256": sha256(metadata_stream),
        "width_range": [min(record[0] for record in metadata), max(record[0] for record in metadata)],
        "height_range": [min(record[1] for record in metadata), max(record[1] for record in metadata)],
        "x_offset_range": [min(record[2] for record in metadata), max(record[2] for record in metadata)],
        "y_offset_range": [min(record[3] for record in metadata), max(record[3] for record in metadata)],
        "run_count": sum(record[4] for record in metadata),
        "opaque_pixel_count": sum(record[5] for record in metadata),
        "caller_anchors": caller_anchors,
        "all_entries_at_caller_anchors_sha256": render_stream.hexdigest(),
        "role_record_count": len(role_head_ids),
        "role_head_id_range": [min(role_head_ids), max(role_head_ids)],
        "unique_role_head_ids": len(set(role_head_ids)),
        "invalid_role_head_ids": invalid_role_head_ids,
        "return_value": 1,
    }


def state_write_vectors(
    scripts: list[bytes],
    scene_events: list[bytes],
) -> dict[str, object]:
    occurrences: dict[int, list[dict[str, object]]] = {3: [], 4: [], 17: [], 26: []}
    for script_id, payload in enumerate(scripts):
        code = words(payload)
        pc = 0
        while code[pc] != -1:
            opcode = code[pc]
            assert 0 <= opcode < len(WIDTHS)
            if opcode in occurrences:
                arguments = code[pc + 1:pc + WIDTHS[opcode]]
                occurrences[opcode].append({
                    "script": script_id,
                    "pc": pc,
                    "arguments": list(arguments),
                })
            pc += WIDTHS[opcode]

    opcode_3 = occurrences[3]
    opcode_4 = occurrences[4]
    opcode_17 = occurrences[17]
    opcode_26 = occurrences[26]
    opcode_4_stream = b"".join(
        struct.pack("<II3h", row["script"], row["pc"], *row["arguments"])
        for row in opcode_4
    )
    opcode_17_stream = b"".join(
        struct.pack("<II5h", row["script"], row["pc"], *row["arguments"])
        for row in opcode_17
    )
    opcode_4_scripts = {row["script"] for row in opcode_4}
    opcode_4_event_references: list[dict[str, int]] = []
    for scene_id, payload in enumerate(scene_events):
        event_words = words(payload)
        assert len(event_words) == 200 * 11
        for event_id in range(200):
            for field in range(2, 5):
                script_id = event_words[event_id * 11 + field]
                if script_id in opcode_4_scripts:
                    opcode_4_event_references.append({
                        "scene": scene_id,
                        "event": event_id,
                        "field": field,
                        "script": script_id,
                    })
    opcode_4_event_reference_stream = b"".join(
        struct.pack(
            "<IIII",
            row["scene"], row["event"], row["field"], row["script"],
        )
        for row in opcode_4_event_references
    )
    opcode_26_stream = b"".join(
        struct.pack("<II5h", row["script"], row["pc"], *row["arguments"])
        for row in opcode_26
    )
    opcode_26_external = [
        row for row in opcode_26 if row["arguments"][0] != -2
    ]
    coordinate_updates = [
        row for row in opcode_3
        if row["arguments"][11] != -2 or row["arguments"][12] != -2
    ]
    opcode_3_stream = b"".join(
        struct.pack("<II13h", row["script"], row["pc"], *row["arguments"])
        for row in opcode_3
    )
    coordinate_stream = b"".join(
        struct.pack("<II13h", row["script"], row["pc"], *row["arguments"])
        for row in coordinate_updates
    )
    explicit_scenes = sorted({
        row["arguments"][0]
        for row in opcode_3
        if row["arguments"][0] != -2
    })
    assert len(opcode_3) == 2_320
    assert sum(row["arguments"][0] == -2 for row in opcode_3) == 2_009
    assert sum(row["arguments"][1] == -2 for row in opcode_3) == 461
    assert all(row["arguments"][1] != -1 for row in opcode_3)
    assert all(
        0 <= row["arguments"][0] < 100
        for row in opcode_3
        if row["arguments"][0] != -2
    )
    assert all(
        row["arguments"][1] in (-2, -1) or 0 <= row["arguments"][1] < 200
        for row in opcode_3
    )
    assert all(
        row["arguments"][1] not in (-2, -1)
        for row in opcode_3
        if row["arguments"][0] != -2
    )
    assert len(coordinate_updates) == 30
    assert all(
        row["arguments"][0] == -2 and
        row["arguments"][11] != -2 and row["arguments"][12] != -2 and
        0 <= row["arguments"][11] < 64 and 0 <= row["arguments"][12] < 64
        for row in coordinate_updates
    )
    assert coordinate_updates[0] == {
        "script": 147,
        "pc": 0,
        "arguments": [-2, 6, -2, -2, 146, -1, -1, 5398, 5398, 5398, -2, 14, 40],
    }
    assert len(opcode_4) == 167
    assert all(row["pc"] == 0 for row in opcode_4)
    assert all(row["arguments"][1:] == [1, 0] for row in opcode_4)
    assert len({row["arguments"][0] for row in opcode_4}) == 60
    assert min(row["arguments"][0] for row in opcode_4) == 37
    assert max(row["arguments"][0] for row in opcode_4) == 195
    assert sum(row["arguments"][0] == 186 for row in opcode_4) == 100
    assert len(opcode_4_event_references) == 40
    assert all(row["field"] == 3 for row in opcode_4_event_references)
    assert opcode_4_event_references[0] == {
        "scene": 0, "event": 0, "field": 3, "script": 10,
    }
    assert opcode_4_event_references[-1] == {
        "scene": 83, "event": 24, "field": 3, "script": 1014,
    }
    assert len(opcode_17) == 127
    assert opcode_17[0] == {
        "script": 30, "pc": 191, "arguments": [49, 1, 28, 37, 0],
    }
    assert opcode_17[-1] == {
        "script": 1015, "pc": 868, "arguments": [-2, 1, 18, 26, 4062],
    }
    assert [row for row in opcode_17 if row["script"] == 274] == [
        {"script": 274, "pc": 0, "arguments": [-2, 1, 13, 22, 0]},
        {"script": 274, "pc": 6, "arguments": [-2, 1, 12, 22, 2898]},
    ]
    assert sorted({row["arguments"][0] for row in opcode_17}) == [-2, 11, 18, 21, 49, 52, 53, 55]
    assert Counter(row["arguments"][0] for row in opcode_17) == {
        -2: 99, 11: 5, 18: 6, 21: 1, 49: 3, 52: 3, 53: 5, 55: 5,
    }
    assert Counter(row["arguments"][1] for row in opcode_17) == {0: 6, 1: 121}
    assert all(
        (row["arguments"][0] == -2 or 0 <= row["arguments"][0] < 100) and
        0 <= row["arguments"][1] < 6 and
        0 <= row["arguments"][2] < 64 and
        0 <= row["arguments"][3] < 64
        for row in opcode_17
    )
    assert len(opcode_26) == 121
    assert sum(row["arguments"][0] == -2 for row in opcode_26) == 115
    assert all(row["arguments"][1] not in (-2, -1) for row in opcode_26)
    assert Counter(tuple(row["arguments"][2:]) for row in opcode_26) == {
        (0, 0, 1): 21,
        (0, 1, 0): 100,
    }
    assert opcode_26_external == [
        {"script": 95, "pc": 88, "arguments": [73, 2, 0, 0, 1]},
        {"script": 109, "pc": 119, "arguments": [73, 2, 0, 0, 1]},
        {"script": 170, "pc": 49, "arguments": [27, 0, 0, 0, 1]},
        {"script": 176, "pc": 62, "arguments": [27, 0, 0, 0, 1]},
        {"script": 195, "pc": 137, "arguments": [27, 0, 0, 0, 1]},
        {"script": 232, "pc": 62, "arguments": [27, 0, 0, 0, 1]},
    ]

    return {
        "opcode_3_event_fields": {
            "occurrences": len(opcode_3),
            "argument_stream_sha256": sha256(opcode_3_stream),
            "current_scene_occurrences": sum(
                row["arguments"][0] == -2 for row in opcode_3
            ),
            "explicit_scene_occurrences": sum(
                row["arguments"][0] != -2 for row in opcode_3
            ),
            "explicit_scene_ids": explicit_scenes,
            "current_event_occurrences": sum(
                row["arguments"][1] == -2 for row in opcode_3
            ),
            "disable_trigger_event_occurrences": sum(
                row["arguments"][1] == -1 for row in opcode_3
            ),
            "explicit_event_range": [
                min(row["arguments"][1] for row in opcode_3 if row["arguments"][1] >= 0),
                max(row["arguments"][1] for row in opcode_3 if row["arguments"][1] >= 0),
            ],
            "external_scene_event_sentinel_occurrences": sum(
                row["arguments"][0] != -2 and row["arguments"][1] in (-2, -1)
                for row in opcode_3
            ),
            "field_write_counts": [
                sum(row["arguments"][field + 2] != -2 for row in opcode_3)
                for field in range(11)
            ],
            "coordinate_updates": len(coordinate_updates),
            "coordinate_stream_sha256": sha256(coordinate_stream),
            "one_axis_coordinate_updates": sum(
                (row["arguments"][11] == -2) != (row["arguments"][12] == -2)
                for row in coordinate_updates
            ),
            "first_coordinate_update": coordinate_updates[0],
            "coordinate_order": [
                "read_old_x_y",
                "write_selected_event_fields",
                "clear_old_current_scene_event_cell",
                "write_new_current_scene_event_cell",
            ],
        },
        "opcode_4_selected_item_branch": {
            "occurrences": len(opcode_4),
            "argument_stream_sha256": sha256(opcode_4_stream),
            "instruction_pc_values": sorted({row["pc"] for row in opcode_4}),
            "item_id_range": [
                min(row["arguments"][0] for row in opcode_4),
                max(row["arguments"][0] for row in opcode_4),
            ],
            "item_ids": sorted({row["arguments"][0] for row in opcode_4}),
            "repeated_item_ids": [
                {
                    "item_id": item_id,
                    "occurrences": count,
                }
                for item_id, count in sorted(Counter(
                    row["arguments"][0] for row in opcode_4
                ).items())
                if count > 1
            ],
            "true_offsets": sorted({row["arguments"][1] for row in opcode_4}),
            "false_offsets": sorted({row["arguments"][2] for row in opcode_4}),
            "first_occurrence": opcode_4[0],
            "last_occurrence": opcode_4[-1],
            "scene_event_references": len(opcode_4_event_references),
            "scene_event_reference_stream_sha256": sha256(
                opcode_4_event_reference_stream
            ),
            "scene_event_field_indices": sorted({
                row["field"] for row in opcode_4_event_references
            }),
            "referenced_scripts": sorted({
                row["script"] for row in opcode_4_event_references
            }),
            "reference_scenes": sorted({
                row["scene"] for row in opcode_4_event_references
            }),
        },
        "opcode_17_scene_cell": {
            "occurrences": len(opcode_17),
            "stream_encoding": "little_endian_<II5h:script_pc_scene_layer_x_y_value>",
            "argument_stream_sha256": sha256(opcode_17_stream),
            "first_occurrence": opcode_17[0],
            "last_occurrence": opcode_17[-1],
            "current_scene_sentinel": -2,
            "current_scene_occurrences": sum(
                row["arguments"][0] == -2 for row in opcode_17
            ),
            "explicit_scene_occurrences": sum(
                row["arguments"][0] != -2 for row in opcode_17
            ),
            "scene_id_counts": [
                [value, count] for value, count in sorted(Counter(
                    row["arguments"][0] for row in opcode_17
                ).items())
            ],
            "explicit_scene_ids": sorted({
                row["arguments"][0]
                for row in opcode_17
                if row["arguments"][0] != -2
            }),
            "layer_counts": [
                [value, count] for value, count in sorted(Counter(
                    row["arguments"][1] for row in opcode_17
                ).items())
            ],
            "x_range": [
                min(row["arguments"][2] for row in opcode_17),
                max(row["arguments"][2] for row in opcode_17),
            ],
            "y_range": [
                min(row["arguments"][3] for row in opcode_17),
                max(row["arguments"][3] for row in opcode_17),
            ],
            "value_range": [
                min(row["arguments"][4] for row in opcode_17),
                max(row["arguments"][4] for row in opcode_17),
            ],
            "all_current_asset_arguments_valid": True,
            "linear_word_index": "4096*layer + 64*y + x",
            "external_scene_legacy_order": [
                "flush_current_scene", "load_target_scene", "write_target_word",
                "persist_target_scene", "reload_current_scene",
            ],
            "script_274_current_scene_vectors": [
                row for row in opcode_17 if row["script"] == 274
            ],
            "synthetic_external_vector": {
                "active_scene": 70, "target_scene": 69, "layer": 1,
                "x": 2, "y": 3, "value": 456,
                "active_scene_same_cell": 123,
                "active_scene_after": 70,
            },
        },
        "opcode_26_event_script_add": {
            "occurrences": len(opcode_26),
            "argument_stream_sha256": sha256(opcode_26_stream),
            "current_scene_occurrences": sum(
                row["arguments"][0] == -2 for row in opcode_26
            ),
            "explicit_scene_occurrences": len(opcode_26_external),
            "scene_event_occurrence_counts": [
                {
                    "scene": scene,
                    "event": event,
                    "occurrences": count,
                }
                for (scene, event), count in sorted(Counter(
                    (row["arguments"][0], row["arguments"][1])
                    for row in opcode_26
                ).items())
            ],
            "current_event_sentinel_occurrences": sum(
                row["arguments"][1] == -2 for row in opcode_26
            ),
            "event_ids": sorted({row["arguments"][1] for row in opcode_26}),
            "delta_patterns": [
                {
                    "deltas": list(deltas),
                    "occurrences": count,
                }
                for deltas, count in sorted(Counter(
                    tuple(row["arguments"][2:]) for row in opcode_26
                ).items())
            ],
            "external_occurrences": opcode_26_external,
            "updated_fields": [2, 3, 4],
            "addition_width_bits": 16,
        },
    }


def battle_request_vectors(scripts: list[bytes]) -> dict[str, object]:
    occurrences: list[tuple[int, int, int, int, int, int]] = []
    for script_id, payload in enumerate(scripts):
        code = words(payload)
        program_counter = 0
        while code[program_counter] != -1:
            opcode = code[program_counter]
            if opcode == 6:
                occurrences.append((
                    script_id,
                    program_counter,
                    code[program_counter + 1],
                    code[program_counter + 2],
                    code[program_counter + 3],
                    code[program_counter + 4],
                ))
            program_counter += WIDTHS[opcode]

    stream = b"".join(struct.pack("<II4h", *row) for row in occurrences)
    battle_ids = [row[2] for row in occurrences]
    true_offsets = Counter(row[3] for row in occurrences)
    false_offsets = Counter(row[4] for row in occurrences)
    offset_pairs = Counter((row[3], row[4]) for row in occurrences)
    get_experience = Counter(row[5] for row in occurrences)
    assert len(occurrences) == 145
    assert occurrences[0] == (2, 14, 0, 0, 74, 1)
    assert occurrences[-1] == (1015, 686, 134, 3, 0, 0)
    assert min(battle_ids) == 0 and max(battle_ids) == 135
    assert all(0 <= battle_id < 140 for battle_id in battle_ids)
    assert get_experience == Counter({0: 115, 1: 30})
    assert all(row[3] >= 0 and row[4] >= 0 for row in occurrences)

    return {
        "occurrences": len(occurrences),
        "stream_encoding": "little_endian_<II4h:script_pc_battle_true_false_get_exp>",
        "parameter_stream_sha256": sha256(stream),
        "first": list(occurrences[0]),
        "last": list(occurrences[-1]),
        "battle_id_range": [min(battle_ids), max(battle_ids)],
        "unique_battle_ids": len(set(battle_ids)),
        "invalid_battle_ids_for_war_sta": [],
        "true_offset_counts": {
            str(offset): count for offset, count in sorted(true_offsets.items())
        },
        "false_offset_counts": {
            str(offset): count for offset, count in sorted(false_offsets.items())
        },
        "offset_pair_counts": {
            f"{true_offset},{false_offset}": count
            for (true_offset, false_offset), count in sorted(offset_pairs.items())
        },
        "all_offsets_nonnegative": True,
        "get_experience_counts": {
            str(value): count for value, count in sorted(get_experience.items())
        },
        "program_counter_formula": "old_pc + 5 + selected_offset",
    }


def join_role_vectors(scripts: list[bytes], ranger: bytes) -> dict[str, object]:
    occurrences: list[tuple[int, int, int]] = []
    for script_id, payload in enumerate(scripts):
        code = words(payload)
        program_counter = 0
        while code[program_counter] != -1:
            opcode = code[program_counter]
            if opcode == 10:
                occurrences.append((script_id, program_counter, code[program_counter + 1]))
            program_counter += WIDTHS[opcode]

    stream = b"".join(struct.pack("<IIh", *row) for row in occurrences)
    role_ids = sorted({row[2] for row in occurrences})
    role_records = []
    for role_id in role_ids:
        record = words(ranger[836 + role_id * 182:836 + (role_id + 1) * 182])
        role_records.append({
            "role_id": role_id,
            "equipment": list(record[23:25]),
            "practice_item": record[61],
            "item_experience": record[62],
            "carrying": [
                [record[83 + slot], record[87 + slot]] for slot in range(4)
            ],
        })
    carrying = [pair for record in role_records for pair in record["carrying"]]
    nonempty = [pair for pair in carrying if pair[0] != -1]
    role_counts = Counter(row[2] for row in occurrences)
    assert len(occurrences) == 80
    assert occurrences[0] == (10, 160, 1)
    assert occurrences[-1] == (999, 50, 76)
    assert role_ids == [
        1, 2, 9, 16, 17, 25, 26, 28, 29, 35, 36, 37, 38,
        44, 45, 47, 48, 49, 51, 53, 54, 58, 59, 61, 63, 76,
    ]
    assert all(0 <= role_id < 320 for role_id in role_ids)
    assert all(item_id == -1 or 0 <= item_id < 200 for item_id, _ in carrying)
    assert all(count >= 0 for _, count in carrying)
    assert words(scripts[11])[50:52] == (10, 1)

    return {
        "occurrences": len(occurrences),
        "stream_encoding": "little_endian_<IIh:script_pc_role>",
        "parameter_stream_sha256": sha256(stream),
        "first": list(occurrences[0]),
        "last": list(occurrences[-1]),
        "role_id_range": [min(role_ids), max(role_ids)],
        "role_ids": role_ids,
        "role_counts": {str(role_id): count for role_id, count in sorted(role_counts.items())},
        "all_role_ids_valid": True,
        "baseline_role_records": role_records,
        "baseline_nonempty_carrying_slots": len(nonempty),
        "baseline_carrying_item_ids": sorted({pair[0] for pair in nonempty}),
        "baseline_carrying_counts": sorted({pair[1] for pair in nonempty}),
        "script_11": {
            "script_id": 11,
            "program_counter": 50,
            "role_id": 1,
            "carrying": role_records[0]["carrying"],
        },
        "team_scan_slots": [1, 2, 3, 4, 5],
        "empty_team_value": "signed_le_zero",
        "carrying_slot_order": [0, 1, 2, 3],
        "empty_carrying_item": -1,
        "notice_format_big5_hex": "b16fa8ec257300",
        "notice_format_cp950": "得到%s",
        "post_notice_fields": [-1, 0],
        "cleared_role_fields": [23, 24, 61, 62],
        "cleared_role_values": [-1, -1, -1, 0],
        "item_user_writes": 0,
        "program_counter_formula": "old_pc + 2",
    }


def fade_from_black_vectors(scripts: list[bytes]) -> dict[str, object]:
    occurrences: list[tuple[int, int]] = []
    for script_id, payload in enumerate(scripts):
        code = words(payload)
        program_counter = 0
        while code[program_counter] != -1:
            opcode = code[program_counter]
            if opcode == 13:
                occurrences.append((script_id, program_counter))
            program_counter += WIDTHS[opcode]

    stream = b"".join(struct.pack("<II", *row) for row in occurrences)
    assert len(occurrences) == 346
    assert occurrences[0] == (2, 20)
    assert occurrences[-1] == (1016, 128)
    return {
        "occurrences": len(occurrences),
        "stream_encoding": "little_endian_<II:script_pc>",
        "position_stream_sha256": sha256(stream),
        "first": list(occurrences[0]),
        "last": list(occurrences[-1]),
        "machine_callers": [
            {"address": "0x2C5BB", "owner": "sub_2C319", "role": "opcode_13"},
            {"address": "0x30381", "owner": "sub_302E0", "role": "interround"},
            {"address": "0x3044A", "owner": "sub_302E0", "role": "finale"},
            {"address": "0x304DC", "owner": "sub_30480", "role": "battle_victory"},
        ],
        "tournament_success_fades": {
            "battle_victory": 15,
            "interround": 4,
            "finale": 1,
            "total": 20,
        },
        "program_counter_formula": "old_pc + 1",
    }


def fade_to_black_vectors(scripts: list[bytes]) -> dict[str, object]:
    occurrences: list[tuple[int, int]] = []
    for script_id, payload in enumerate(scripts):
        code = words(payload)
        program_counter = 0
        while code[program_counter] != -1:
            opcode = code[program_counter]
            if opcode == 14:
                occurrences.append((script_id, program_counter))
            program_counter += WIDTHS[opcode]

    stream = b"".join(struct.pack("<II", *row) for row in occurrences)
    assert len(occurrences) == 171
    assert occurrences[0] == (10, 129)
    assert occurrences[-1] == (1016, 70)
    return {
        "occurrences": len(occurrences),
        "stream_encoding": "little_endian_<II:script_pc>",
        "position_stream_sha256": sha256(stream),
        "first": list(occurrences[0]),
        "last": list(occurrences[-1]),
        "machine_callers": [
            {"address": "0x2C5C5", "owner": "sub_2C319", "role": "opcode_14"},
            {"address": "0x3036A", "owner": "sub_302E0", "role": "interround"},
            {"address": "0x30414", "owner": "sub_302E0", "role": "finale"},
        ],
        "tournament_success_fades": {
            "interround": 4,
            "finale": 1,
            "total": 5,
        },
        "interround_post_fade_delay": {
            "legacy_counter": 300,
            "legacy_delay_ticks": 8,
            "result_wait_ticks": 9,
            "additional_wait_ticks_after_final_frame": 8,
            "placement": "after_complete_fade_at_black",
        },
        "program_counter_formula": "old_pc + 1",
    }


def party_contains_vectors(scripts: list[bytes]) -> dict[str, object]:
    occurrences: list[tuple[int, int, int, int, int]] = []
    for script_id, payload in enumerate(scripts):
        code = words(payload)
        program_counter = 0
        while code[program_counter] != -1:
            opcode = code[program_counter]
            if opcode == 16:
                occurrences.append(
                    (script_id, program_counter, code[program_counter + 1],
                     code[program_counter + 2], code[program_counter + 3]))
            program_counter += WIDTHS[opcode]

    stream = b"".join(struct.pack("<IIhhh", *row) for row in occurrences)
    role_counts = Counter(row[2] for row in occurrences)
    true_offsets = Counter(row[3] for row in occurrences)
    false_offsets = Counter(row[4] for row in occurrences)
    assert len(occurrences) == 80
    assert occurrences[0] == (27, 0, 1, 1, 0)
    assert occurrences[-1] == (914, 35, 35, 0, 1)
    assert min(role_counts) == 1 and max(role_counts) == 76 and len(role_counts) == 17
    return {
        "occurrences": len(occurrences),
        "stream_encoding": "little_endian_<IIhhh:script_pc_role_true_false>",
        "parameter_stream_sha256": sha256(stream),
        "first": list(occurrences[0]),
        "last": list(occurrences[-1]),
        "role_counts": [[value, role_counts[value]] for value in sorted(role_counts)],
        "true_offset_counts": [[value, true_offsets[value]] for value in sorted(true_offsets)],
        "false_offset_counts": [[value, false_offsets[value]] for value in sorted(false_offsets)],
        "scan_slots": [0, 1, 2, 3, 4, 5],
        "stops_at_nonpositive": False,
        "program_counter_formula": "old_pc + 4 + selected_signed_offset",
        "synthetic_vectors": {
            "late_match_after_gap": {
                "team": [0, -1, -1, -1, -1, 55],
                "role": 55,
                "true_offset": 0,
                "false_offset": 3,
                "selected_offset": 0,
            },
            "miss": {
                "team": [0, 1, 2, 3, 4, 5],
                "role": 55,
                "true_offset": 0,
                "false_offset": 3,
                "selected_offset": 3,
            },
        },
    }


def party_tail_condition_vectors(scripts: list[bytes]) -> dict[str, object]:
    occurrences: list[tuple[int, int, int, int]] = []
    for script_id, payload in enumerate(scripts):
        code = words(payload)
        program_counter = 0
        while code[program_counter] != -1:
            opcode = code[program_counter]
            if opcode == 20:
                occurrences.append(
                    (script_id, program_counter, code[program_counter + 1],
                     code[program_counter + 2]))
            program_counter += WIDTHS[opcode]

    stream = b"".join(struct.pack("<IIhh", *row) for row in occurrences)
    true_offsets = Counter(row[2] for row in occurrences)
    false_offsets = Counter(row[3] for row in occurrences)
    assert len(occurrences) == 82
    assert occurrences[0] == (10, 110, 0, 6)
    assert occurrences[-1] == (999, 19, 0, 6)
    return {
        "occurrences": len(occurrences),
        "stream_encoding": "little_endian_<IIhh:script_pc_true_false>",
        "parameter_stream_sha256": sha256(stream),
        "first": list(occurrences[0]),
        "last": list(occurrences[-1]),
        "true_offset_counts": [[value, true_offsets[value]] for value in sorted(true_offsets)],
        "false_offset_counts": [[value, false_offsets[value]] for value in sorted(false_offsets)],
        "tested_slot": 5,
        "condition": "signed_team_slot_5 > 0",
        "scans_other_slots": False,
        "program_counter_formula": "old_pc + 3 + selected_signed_offset",
        "script_11_vectors": [
            {"team": [1, 2, 3, 4, 5, -1], "selected": "false", "talk_id": 30},
            {"team": [1, 2, 3, 4, 5, 0], "selected": "false", "talk_id": 30},
            {"team": [1, 2, -1, 4, 5, 9], "selected": "true", "talk_id": 175},
        ],
    }


def inventory_presence_vectors(scripts: list[bytes]) -> dict[str, object]:
    occurrences: list[tuple[int, int, int, int, int]] = []
    for script_id, payload in enumerate(scripts):
        code = words(payload)
        program_counter = 0
        while code[program_counter] != -1:
            opcode = code[program_counter]
            if opcode == 18:
                occurrences.append(
                    (script_id, program_counter, code[program_counter + 1],
                     code[program_counter + 2], code[program_counter + 3]))
            program_counter += WIDTHS[opcode]

    stream = b"".join(struct.pack("<IIhhh", *row) for row in occurrences)
    assert occurrences == [(37, 0, 173, 0, 15), (38, 0, 173, 0, 20)]
    return {
        "occurrences": len(occurrences),
        "stream_encoding": "little_endian_<IIhhh:script_pc_item_true_false>",
        "parameter_stream_sha256": sha256(stream),
        "all": [list(row) for row in occurrences],
        "scan_slot_range": [0, 199],
        "item_word_stride_bytes": 4,
        "stops_at_first_match": True,
        "reads_count": False,
        "program_counter_formula": "old_pc + 4 + selected_signed_offset",
        "script_37_vectors": [
            {"matching_slot": 0, "count": 0, "selected": "true", "result": "stay"},
            {"matching_slot": None, "selected": "false", "result": "dialogue_139"},
            {"matching_slot": 199, "count": -32768, "selected": "true", "result": "stay"},
        ],
    }


def party_rest_vectors(scripts: list[bytes]) -> dict[str, object]:
    occurrences: list[tuple[int, int]] = []
    for script_id, payload in enumerate(scripts):
        code = words(payload)
        program_counter = 0
        while code[program_counter] != -1:
            opcode = code[program_counter]
            if opcode == 12:
                occurrences.append((script_id, program_counter))
            program_counter += WIDTHS[opcode]

    stream = b"".join(struct.pack("<II", *row) for row in occurrences)
    script_931 = words(scripts[931])
    assert occurrences == [
        (235, 26), (480, 26), (502, 36), (575, 26),
        (658, 26), (664, 26), (931, 10),
    ]
    assert script_931 == (
        11, 1, 0, 7, 1, 2843, 0, 1, 0, 14, 12, 40, 3, 0, 13,
        1, 2844, 0, 1, 0, -1,
    )

    return {
        "occurrences": len(occurrences),
        "stream_encoding": "little_endian_<II:script_pc>",
        "parameter_stream_sha256": sha256(stream),
        "positions": [list(row) for row in occurrences],
        "script_931": {
            "words": list(script_931),
            "rest_question_pc": 0,
            "party_rest_pc": 10,
        },
        "team_end_scan_slots": [1, 2, 3, 4, 5],
        "team_end_rule": "first_signed_nonpositive_else_6",
        "processed_slots": "[0,team_end)",
        "eligible_hurt": "signed_less_than_33",
        "eligible_poison": "equals_0",
        "write_order": ["hurt=0", "physical_power=100", "mp=maximum_mp", "hp=maximum_hp"],
        "program_counter_formula": "old_pc + 1",
    }


def leave_role_vectors(scripts: list[bytes], ranger: bytes) -> dict[str, object]:
    occurrences: list[tuple[int, int, int]] = []
    opcode_59: list[tuple[int, int]] = []
    for script_id, payload in enumerate(scripts):
        code = words(payload)
        program_counter = 0
        while code[program_counter] != -1:
            opcode = code[program_counter]
            if opcode == 21:
                occurrences.append((script_id, program_counter, code[program_counter + 1]))
            elif opcode == 59:
                opcode_59.append((script_id, program_counter))
            program_counter += WIDTHS[opcode]

    stream = b"".join(struct.pack("<IIh", *row) for row in occurrences)
    role_ids = sorted({row[2] for row in occurrences})
    populated_personal_items = []
    for role_id in role_ids:
        record = words(ranger[836 + role_id * 182:836 + (role_id + 1) * 182])
        for field in (23, 24, 61):
            item_id = record[field]
            if item_id == -1:
                continue
            assert 0 <= item_id < 200
            item = words(ranger[59_076 + item_id * 190:59_076 + (item_id + 1) * 190])
            populated_personal_items.append({
                "role_id": role_id,
                "role_field": field,
                "item_id": item_id,
                "item_user": item[38],
            })
    role_counts = Counter(row[2] for row in occurrences)
    script_950 = words(scripts[950])
    header = words(ranger[:836])
    assert len(occurrences) == 35
    assert occurrences[0] == (320, 358, 26)
    assert occurrences[-1] == (998, 4, 76)
    assert role_ids == [
        1, 2, 9, 16, 17, 25, 26, 28, 29, 35, 36, 37, 38,
        44, 45, 47, 48, 49, 51, 53, 54, 58, 59, 61, 63, 76,
    ]
    assert all(0 <= role_id < 320 for role_id in role_ids)
    assert opcode_59 == [(932, 38)]
    assert script_950 == (
        1, 2707, 0, 1, 21, 1,
        3, 0, 0, 1, 1, 951, -1, -1, 5166, 5166, 5166, 0, -2, -2,
        -1,
    )
    assert populated_personal_items == [
        {"role_id": 44, "role_field": 24, "item_id": 123, "item_user": -1},
        {"role_id": 58, "role_field": 23, "item_id": 106, "item_user": -1},
    ]

    return {
        "occurrences": len(occurrences),
        "stream_encoding": "little_endian_<IIh:script_pc_role>",
        "parameter_stream_sha256": sha256(stream),
        "first": list(occurrences[0]),
        "last": list(occurrences[-1]),
        "role_id_range": [min(role_ids), max(role_ids)],
        "role_ids": role_ids,
        "role_counts": {str(role_id): count for role_id, count in sorted(role_counts.items())},
        "all_role_ids_valid": True,
        "script_950": {
            "words": list(script_950),
            "role_id": 1,
            "program_counter": 4,
        },
        "baseline_team": list(header[12:18]),
        "baseline_team_index_6_alias_inventory_item_0": header[18],
        "baseline_personal_item_slots": len(role_ids) * 3,
        "baseline_populated_personal_items": populated_personal_items,
        "team_scan_slots": [1, 2, 3, 4, 5],
        "team_remove_matches": "first_only_then_shift_left",
        "team_tail_value": -1,
        "cleanup_when_team_role_missing": True,
        "personal_role_fields": [23, 24, 61, 62],
        "personal_role_values": [-1, -1, -1, 0],
        "item_user_field": 38,
        "item_user_value": -1,
        "opcode_59_callers": [
            {
                "script_id": 932,
                "program_counter": 38,
                "source_indices": [6, 5, 4, 3, 2, 1],
                "index_6_source": "inventory_item_0",
                "positive_roles_only": True,
            }
        ],
        "program_counter_formula": "old_pc + 2",
    }


def basic_helper_vectors(scripts: list[bytes]) -> dict[str, object]:
    script_2 = words(scripts[2])
    script_28 = words(scripts[28])
    script_149 = words(scripts[149])
    script_235 = words(scripts[235])
    script_328 = words(scripts[328])
    script_420 = words(scripts[420])
    script_434 = words(scripts[434])
    script_445 = words(scripts[445])
    script_464 = words(scripts[464])
    script_673 = words(scripts[673])
    script_692 = words(scripts[692])
    assert script_2[90:92] == (56, 1)
    assert script_28[65:68] == (23, 4, 99)
    assert script_149[59:61] == (37, -5)
    assert script_235[30:35] == (19, 14, 14, 40, 3)
    assert script_328[:4] == (36, 2, 6, 0)
    assert script_420[24:26] == (39, 75)
    assert script_434[:5] == (38, -2, 0, 990, 994)
    assert script_445[24:27] == (42, 6, 0)
    assert script_464[28:33] == (55, 2, -1, 14, 0)
    assert script_673[74:77] == (34, 0, 3)
    assert script_692 == (51, -1)

    random_state = (0x41C64E6D + 0x3039) & 0xFFFFFFFF
    random_talk_id = 2547 + (((random_state >> 16) & 0x7FFF) % 18)

    def wrapped_add(value: int, delta: int) -> int:
        bits = (value + delta) & 0xFFFF
        return bits if bits < 0x8000 else bits - 0x10000

    def wrapped_clamped_add(value: int, delta: int) -> int:
        return min(max(wrapped_add(value, delta), 0), 100)

    return {
        "opcode_19_and_40_script_235": {
            "arguments": list(script_235[30:35]),
            "scene_position": [14, 14],
            "view_origin": [3, 3],
            "direction": 3,
            "player_frame": 5044,
        },
        "opcode_23_script_28": {
            "arguments": list(script_28[65:68]),
            "role_4_use_poison": 99,
        },
        "opcode_34_script_673": {
            "arguments": list(script_673[74:77]),
            "cases": [
                {"before": value, "after": wrapped_clamped_add(value, 3)}
                for value in (99, 32766)
            ],
        },
        "opcode_36_script_328": {
            "arguments": list(script_328[:4]),
            "cases": [
                {"sexual": 2, "talk_id": 1123},
                {"sexual": 1, "talk_id": 1122},
            ],
        },
        "opcode_37_script_149": {
            "arguments": list(script_149[59:61]),
            "cases": [
                {"before": value, "after": wrapped_clamped_add(value, -5)}
                for value in (3, -32768)
            ],
        },
        "opcode_38_script_434": {
            "arguments": list(script_434[:5]),
            "changed_indices": [0, 63, 4095],
            "unchanged_index": 64,
            "unchanged_value": 123,
        },
        "opcode_39_script_420": {
            "arguments": list(script_420[24:26]),
            "scene_75_entrance_condition": 0,
        },
        "opcode_42_script_445": {
            "arguments": list(script_445[24:27]),
            "cases": [
                {"female_present": False, "talk_id": 1575},
                {"female_present": True, "last_party_slot": -1, "talk_id": 1574},
            ],
        },
        "opcode_55_script_464": {
            "arguments": list(script_464[28:33]),
            "cases": [
                {"event_1_before": -1, "event_1_after": -1, "current_picture": 1234},
                {"event_1_before": 999, "event_1_after": 862, "current_picture": 1234},
            ],
        },
        "opcode_51_script_692": {
            "arguments": list(script_692[:1]),
            "seed": 1,
            "random_state": f"0x{random_state:08x}",
            "talk_id": random_talk_id,
            "head_id": 114,
            "style": 0,
        },
        "opcode_56_script_2": {
            "arguments": list(script_2[90:92]),
            "cases": [
                {"fame_before": 199, "fame_after": 200, "book_event_1": 932},
                {"fame_before": 32767, "fame_after": wrapped_add(32767, 1), "book_event_1": -1},
            ],
        },
    }


def scene_loop_vectors(ranger: bytes, scripts: list[bytes]) -> dict[str, object]:
    metadata = [
        words(ranger[97_076 + scene * 52:97_076 + (scene + 1) * 52])
        for scene in range(84)
    ]
    assert all(len(record) == 26 for record in metadata)
    jumps = [
        {
            "scene": scene,
            "target": record[8],
            "trigger": [record[22], record[23]],
            "use_jump_entrance": record[10] == 0 and record[11] == 0,
            "normal_entrance": [record[14], record[15]],
            "jump_entrance": [record[24], record[25]],
        }
        for scene, record in enumerate(metadata)
        if record[8] >= 0
    ]
    exit_cells = [
        [scene, record[16 + index], record[19 + index]]
        for scene, record in enumerate(metadata)
        for index in range(3)
    ]
    opcode_8 = []
    for script_id, payload in enumerate(scripts):
        code = words(payload)
        pc = 0
        while code[pc] != -1:
            opcode = code[pc]
            assert 0 <= opcode < len(WIDTHS)
            if opcode == 8:
                opcode_8.append({"script": script_id, "pc": pc, "music": code[pc + 1]})
            pc += WIDTHS[opcode]
    opcode_8_stream = b"".join(
        struct.pack("<IIh", row["script"], row["pc"], row["music"])
        for row in opcode_8
    )
    opcode_8_music_counts = Counter(row["music"] for row in opcode_8)
    assert len(opcode_8) == 15
    assert opcode_8[0] == {"script": 30, "pc": 241, "music": 3}
    assert opcode_8[-1] == {"script": 635, "pc": 119, "music": 3}
    assert opcode_8_music_counts == Counter({3: 15})
    return {
        "entry_outputs": [
            "fade_from_black",
            "scene_title",
            "present",
            "auto_event_check",
        ],
        "loop_order": [
            "event_animation",
            "one_input_action",
            "scene_render_present",
            "periodic_update_if_remainder_1",
            "auto_event",
            "exit_cells",
            "internal_jump",
            "wait_tick_change",
        ],
        "input_priority": [
            "left",
            "up",
            "down",
            "right",
            "interact",
            "main_ui",
            "weather_disable",
            "idle_update",
        ],
        "periodic_update_ticks_first_20": [tick for tick in range(1, 21) if tick % 5 == 1],
        "exit_outputs_after_auto_event": [
            "auto_event_outputs",
            "fade_to_black",
            "return_world",
        ],
        "internal_jump_outputs": [
            "fade_to_black",
            "load_target_scene",
            "fade_from_black",
            "scene_title",
            "present",
            "auto_event_check",
        ],
        "world_direction_after_scene": [3, 2, 1, 0],
        "metadata_jump_count": len(jumps),
        "metadata_jumps": jumps,
        "metadata_exit_cell_count": len(exit_cells),
        "metadata_nonnegative_exit_cell_count": sum(
            1 for _, x, y in exit_cells if x >= 0 and y >= 0
        ),
        "scene_music_samples": {
            str(scene): {
                "exit": metadata[scene][6],
                "entrance": metadata[scene][7],
            }
            for scene in (7, 53, 70, 71)
        },
        "music_dispatch": {
            "metadata_compares_current_music": True,
            "opcode_8_override_compares_current_music": False,
            "opcode_66_compares_current_music": False,
        },
        "opcode_8_exit_music_summary": {
            "occurrences": len(opcode_8),
            "stream_encoding": "little_endian_<IIh:script_pc_music>",
            "parameter_stream_sha256": sha256(opcode_8_stream),
            "first": opcode_8[0],
            "last": opcode_8[-1],
            "music_counts": {
                str(music): count
                for music, count in sorted(opcode_8_music_counts.items())
            },
        },
        "opcode_8_exit_music_overrides": opcode_8,
    }


def scene_animation_vectors(
    scene_maps: list[bytes],
    scene_events: list[bytes],
    sprites: list[bytes],
    scripts: list[bytes],
) -> dict[str, object]:
    duplicate_scenes = []
    trigger_counts = {
        "event_2": {"minus_one": 0, "other_nonpositive": 0, "positive": 0},
        "event_3": {"minus_one": 0, "other_nonpositive": 0, "positive": 0},
    }
    for payload in scene_events:
        event_words = words(payload)
        for event in range(200):
            for name, field in (("event_2", 3), ("event_3", 4)):
                value = event_words[event * 11 + field]
                category = "minus_one" if value == -1 else (
                    "positive" if value > 0 else "other_nonpositive"
                )
                trigger_counts[name][category] += 1

    event_index_values = [
        event
        for payload in scene_maps
        for event in words(payload)[3 * 4096:4 * 4096]
    ]
    automatic_event_cell_counts = {
        "event_minus_one": 0,
        "event_3_minus_one": 0,
        "event_3_zero": 0,
        "event_3_positive": 0,
    }
    for scene_id, payload in enumerate(scene_maps):
        event_words = words(scene_events[scene_id])
        for event in words(payload)[3 * 4096:4 * 4096]:
            if event == -1:
                automatic_event_cell_counts["event_minus_one"] += 1
                continue
            event_3 = event_words[event * 11 + 4]
            category = "event_3_minus_one" if event_3 == -1 else (
                "event_3_positive" if event_3 > 0 else "event_3_zero"
            )
            automatic_event_cell_counts[category] += 1
    positive_event_3_scripts = sorted({
        event_words[event * 11 + 4]
        for payload in scene_events
        for event_words in (words(payload),)
        for event in range(200)
        if event_words[event * 11 + 4] > 0
    })
    event_3_item_context_scripts = set()
    for script_id in positive_event_3_scripts:
        script_words = words(scripts[script_id])
        pc = 0
        while pc < len(script_words):
            opcode = script_words[pc]
            if opcode == -1:
                break
            if opcode == 4:
                event_3_item_context_scripts.add(script_id)
            if opcode < 0 or opcode >= len(WIDTHS):
                break
            pc += WIDTHS[opcode]
    assert automatic_event_cell_counts == {
        "event_minus_one": 408342,
        "event_3_minus_one": 1153,
        "event_3_zero": 6,
        "event_3_positive": 99,
    }
    assert len(positive_event_3_scripts) == 54
    assert event_3_item_context_scripts == set()

    vertical_alias_values = []
    for payload in scene_maps:
        map_words = words(payload)
        vertical_alias_values.extend(map_words[3 * 4096 - 64:3 * 4096])
        vertical_alias_values.extend(map_words[4 * 4096:4 * 4096 + 64])
    positive_event_2_scripts = sorted({
        event_words[event * 11 + 3]
        for payload in scene_events
        for event_words in (words(payload),)
        for event in range(200)
        if event_words[event * 11 + 3] > 0
    })
    outer_control_opcodes = set()
    for script_id in positive_event_2_scripts:
        script_words = words(scripts[script_id])
        pc = 0
        while pc < len(script_words):
            opcode = script_words[pc]
            if opcode == -1:
                break
            if opcode in (6, 15, 24):
                outer_control_opcodes.add(opcode)
            if opcode < 0 or opcode >= len(WIDTHS):
                break
            pc += WIDTHS[opcode]
    assert min(event_index_values) == -1
    assert max(event_index_values) == 140
    assert all(-1 <= value < 200 for value in event_index_values)
    assert len(vertical_alias_values) == 12800
    assert min(vertical_alias_values) == 0
    assert max(vertical_alias_values) == 36
    assert all(0 <= value < 200 for value in vertical_alias_values)
    assert len(positive_event_2_scripts) == 38
    assert outer_control_opcodes == set()

    for scene_id, payload in enumerate(scene_maps):
        counts: dict[int, int] = {}
        for event in words(payload)[3 * 4096:4 * 4096]:
            if event >= 0:
                counts[event] = counts.get(event, 0) + 1
        if any(count > 1 for count in counts.values()):
            duplicate_scenes.append(scene_id)
    assert duplicate_scenes == []

    displayed_picture = 102
    first_picture = 100
    end_picture = 110
    counter = 4
    delay = 99
    for _ in range(2):
        if displayed_picture >= end_picture:
            displayed_picture = first_picture
        if displayed_picture > first_picture and counter % 4 == 0 and displayed_picture < end_picture:
            displayed_picture += 2
        if delay <= counter % 100 and displayed_picture == first_picture and displayed_picture < end_picture:
            displayed_picture += 2
    assert displayed_picture == 106

    def advance_one(
        first: int, end: int, displayed: int, picture_delay: int, animation_counter: int
    ) -> int:
        if first <= 0:
            return displayed
        if displayed >= end:
            displayed = first
        if displayed > first and animation_counter % 4 == 0 and displayed < end:
            displayed += 2
        if (
            picture_delay <= animation_counter % 100
            and displayed == first
            and displayed < end
        ):
            displayed += 2
        return displayed

    condition_vectors = [
        {
            "name": "nonpositive_first_skips",
            "counter": 0,
            "before": [0, 110, 108, -1],
            "displayed_after": advance_one(0, 110, 108, -1, 0),
        },
        {
            "name": "end_resets_before_delayed_start",
            "counter": 0,
            "before": [200, 210, 210, 1],
            "displayed_after": advance_one(200, 210, 210, 1, 0),
        },
        {
            "name": "end_resets_then_signed_delay_starts",
            "counter": 0,
            "before": [300, 310, 310, -1],
            "displayed_after": advance_one(300, 310, 310, -1, 0),
        },
        {
            "name": "delay_starts_at_counter_one",
            "counter": 1,
            "before": [400, 410, 400, 1],
            "displayed_after": advance_one(400, 410, 400, 1, 1),
        },
    ]
    assert [case["displayed_after"] for case in condition_vectors] == [108, 200, 302, 402]

    map_words = list(words(scene_maps[70]))
    event_words = list(words(scene_events[70]))
    map_words[3 * 4096 + 29 * 64 + 44] = 199
    event_words[199 * 11 + 5] = first_picture
    event_words[199 * 11 + 6] = end_picture
    event_words[199 * 11 + 7] = 102
    event_words[199 * 11 + 8] = delay
    frame = render_scene(tuple(map_words), tuple(event_words), sprites, 44, 29, 1)
    entry_displayed_picture = 102
    if entry_displayed_picture > first_picture and entry_displayed_picture < end_picture:
        entry_displayed_picture += 2
    event_words[199 * 11 + 7] = entry_displayed_picture
    entry_frame = render_scene(tuple(map_words), tuple(event_words), sprites, 44, 29, 1)
    event_words[199 * 11 + 5] = 0
    word5_zero_frame = render_scene(tuple(map_words), tuple(event_words), sprites, 44, 29, 1)
    script_825 = words(scripts[825])
    assert script_825 == (52, -1)
    return {
        "asset_duplicate_event_scenes": duplicate_scenes,
        "synthetic_duplicate_event": {
            "event_index": 199,
            "cell_count": 2,
            "counter": counter,
            "first_picture": first_picture,
            "end_picture": end_picture,
            "displayed_before": 102,
            "displayed_after": displayed_picture,
            "delay": delay,
        },
        "condition_vectors": condition_vectors,
        "counter_call_order": [
            "scene_entry_scan_at_zero",
            "main_tick_increment_mod_1000_then_scan",
            "scene_jump_scan_at_zero",
        ],
        "word7_render_frame_fnv1a64": fnv1a64(frame),
        "entry_scan_displayed_picture": entry_displayed_picture,
        "entry_scanned_word7_render_frame_fnv1a64": fnv1a64(entry_frame),
        "word5_zero_word7_positive_frame_fnv1a64": fnv1a64(word5_zero_frame),
        "interaction_present": {
            "player": [44, 29],
            "direction_targets": {
                "0": [44, 28],
                "1": [45, 29],
                "2": [43, 29],
                "3": [44, 30],
            },
            "event_1_script": 825,
            "script_words": list(script_825),
            "picture_vector_direction": 1,
            "player_picture_before": 5018,
            "player_picture_at_event_present": 5016,
            "function_order_with_event": [
                "probe_front_event",
                "set_direction_base_picture",
                "render_scene",
                "present",
                "write_event_context",
                "run_positive_script",
            ],
            "positive_script_caller_outputs": [
                "event_present",
                "notice_style_52",
                "notice_restore_present",
                "scene_tail_present",
                "stay",
            ],
            "nonpositive_script_caller_outputs": [
                "event_present",
                "scene_tail_present",
                "stay",
            ],
            "empty_target_caller_outputs": ["scene_tail_present", "stay"],
        },
        "trigger_field_counts": trigger_counts,
        "item_event_asset_domain": {
            "event_index_min": min(event_index_values),
            "event_index_max": max(event_index_values),
            "event_index_invalid_count": sum(
                value < -1 or value >= 200 for value in event_index_values
            ),
            "vertical_alias_sample_count": len(vertical_alias_values),
            "vertical_alias_min": min(vertical_alias_values),
            "vertical_alias_max": max(vertical_alias_values),
            "vertical_alias_invalid_count": sum(
                value < 0 or value >= 200 for value in vertical_alias_values
            ),
            "positive_event_2_script_ids": positive_event_2_scripts,
            "outer_control_opcodes": [6, 15, 24],
            "outer_control_opcodes_present": sorted(outer_control_opcodes),
        },
        "item_event_outputs": {
            "no_event": ["stay"],
            "event_script_0": ["present", "stay"],
            "event_script_825": [
                "present",
                "notice_style_52",
                "notice_restore_present",
                "stay",
            ],
        },
        "item_menu_call_chain": {
            "selected_value": "inventory_item_id",
            "required_item_type": 0,
            "player": [44, 29],
            "direction_targets": {
                "0": [44, 28],
                "1": [45, 29],
                "2": [43, 29],
                "3": [44, 30],
            },
            "boundary_linear_aliases": {
                "up_from_y0": {"layer": "decoration", "coordinate": [44, 63]},
                "right_from_x63": {"layer": "event_index", "coordinate": [0, 30]},
                "left_from_x0": {"layer": "event_index", "coordinate": [63, 28]},
                "down_from_y63": {"layer": "building_height", "coordinate": [44, 0]},
            },
            "scene_menu_outputs": {
                "no_event": ["restore_scene_present", "open_ui_main_scene"],
                "event_script_0": [
                    "restore_scene_present",
                    "item_event_present",
                    "open_ui_main_scene",
                ],
                "event_script_825": [
                    "restore_scene_present",
                    "item_event_present",
                    "notice_style_52",
                    "notice_restore_present",
                    "open_ui_main_scene",
                ],
            },
            "world_menu_outputs": {
                "no_event": ["restore_world_present", "open_ui_main_world"],
                "event_script_0": [
                    "restore_world_present",
                    "item_event_present_over_world_buffer",
                    "open_ui_main_world",
                ],
                "event_script_825": [
                    "restore_world_present",
                    "item_event_present_over_world_buffer",
                    "notice_style_52",
                    "notice_restore_present",
                    "open_ui_main_world",
                ],
                "requires_retained_scene_state": True,
            },
            "tick_after_action": "deferred_until_menu_exit",
        },
        "automatic_event_outputs": {
            "event_index_minus_1": ["fallback"],
            "event_script_minus_1": ["fallback"],
            "event_script_minus_2": ["present", "stay"],
            "event_script_0": ["present", "stay"],
            "event_script_825": [
                "present",
                "notice_style_52",
                "notice_restore_present",
                "stay",
            ],
            "function_order_with_trigger": [
                "probe_current_event",
                "probe_event_3",
                "set_direction_base_picture",
                "render_scene",
                "present",
                "reread_event_context_without_item_write",
                "run_positive_script",
            ],
            "player_picture_before": 5018,
            "player_picture_at_event_present": 5016,
            "event_item_context_before": 123,
            "event_item_context_after": 123,
            "asset_cell_counts": automatic_event_cell_counts,
            "positive_script_ids": positive_event_3_scripts,
            "positive_scripts_with_opcode_4": sorted(event_3_item_context_scripts),
            "callers": [
                "sub_26B5E:0x26FF0 post_title_new_game_tick",
                "sub_28E40:0x28F53 scene_entry_after_title",
                "sub_28E40:0x290EE main_tick_after_present_and_periodic_update",
                "sub_28E40:0x292AB scene_jump_after_title",
            ],
            "caller_return_value": "ignored_before_transition_checks",
        },
    }


def scene_archive_state_vectors(
    ranger: bytes,
    scene_maps: list[bytes],
    scene_events: list[bytes],
    scripts: list[bytes],
) -> dict[str, object]:
    scene_id = 70
    metadata = struct.unpack_from("<26h", ranger, 97_076 + scene_id * 52)
    map_words = words(scene_maps[scene_id])
    event_cell_count = sum(
        value != -1 for value in map_words[3 * 4096:4 * 4096]
    )
    script_436 = words(scripts[436])
    assert script_436[:14] == (
        3, 7, 6, 0, 0, -1, -1, -1, -1, -1, -1, 0, -2, -2
    )
    external_event_mutations = 0
    external_map_mutations = 0
    for payload in scripts:
        script = words(payload)
        index = 0
        while index < len(script) and script[index] != -1:
            opcode = script[index]
            assert 0 <= opcode < len(WIDTHS)
            width = WIDTHS[opcode]
            assert index + width <= len(script)
            if opcode == 3 and script[index + 1] != -2:
                external_event_mutations += 1
            if opcode == 38 and script[index + 1] != -2:
                external_map_mutations += 1
            index += width
    assert external_event_mutations == 311
    assert external_map_mutations == 0
    event_before = list(words(scene_events[7])[6 * 11:7 * 11])
    event_after = event_before.copy()
    for field, value in enumerate(script_436[3:14]):
        if value != -2:
            event_after[field] = value
    return {
        "scene_70_asset_entry": {
            "normal": [metadata[14], metadata[15]],
            "jump": [metadata[24], metadata[25]],
        },
        "synthetic_entry_vectors": [
            {
                "jump": False,
                "coordinate": [0, 63],
                "view_origin": [0, 36],
                "direction": 2,
                "player_frame": 5030,
            },
            {
                "jump": True,
                "coordinate": [63, 0],
                "view_origin": [36, 0],
                "direction": 2,
                "player_frame": 5030,
            },
        ],
        "event_cell_count": event_cell_count,
        "external_mutation_counts": {
            "event": external_event_mutations,
            "map": external_map_mutations,
        },
        "external_event_script_436": {
            "arguments": list(script_436[:14]),
            "scene_id": 7,
            "event_index": 6,
            "before": event_before,
            "after": event_after,
        },
    }


def dialogue_vectors(
    root: Path,
    base_frame: bytes,
    style_4_base_frame: bytes,
    weather_base_frame: bytes,
    palette: list[tuple[int, int, int]],
    decoded_talks: list[bytes],
    scripts: list[bytes],
) -> dict[str, object]:
    ascii_font = (root / "FONT.X16").read_bytes()
    big5_font = (root / "FONT.C16").read_bytes()
    portraits = packed((root / "HDGRP.IDX").read_bytes(), (root / "HDGRP.GRP").read_bytes())
    lookup = rgb4_lookup(palette)

    def blend(pixels: bytearray, x: int, y: int, width: int, height: int) -> None:
        source = palette[0]
        for py in range(y, y + height):
            for px in range(x, x + width):
                offset = py * 320 + px
                destination = palette[pixels[offset]]
                components = tuple(
                    source[channel] // 8 + destination[channel] // 8
                    for channel in range(3)
                )
                pixels[offset] = lookup[
                    components[0] * 256 + components[1] * 16 + components[2]
                ]

    def fill(pixels: bytearray, x: int, y: int, width: int, height: int) -> None:
        for py in range(y, y + height):
            pixels[py * 320 + x:py * 320 + x + width] = bytes([0xFF]) * width

    def background(pixels: bytearray, x: int, y: int, width: int, height: int) -> None:
        for left, top, w, h in (
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
            blend(pixels, left, top, w, h)

    def border(pixels: bytearray, x: int, y: int, width: int, height: int) -> None:
        for left, top, w, h in (
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
            fill(pixels, left, top, w, h)

    layouts = {
        0: ((94, 17), (23, 12)),
        1: ((8, 130), (237, 125)),
        2: ((94, 17), None),
        3: ((8, 130), None),
        4: ((8, 17), (237, 12)),
        5: ((94, 130), (23, 125)),
    }

    def draw_text_linear(
        pixels: bytearray,
        x: int,
        y: int,
        text: bytes,
        right_shadow: int = 0x17,
        foreground: int = 0x15,
    ) -> None:
        cursor = 0
        while text[cursor] != 0:
            first = text[cursor]
            cursor += 1
            if first > 0x7F:
                second = text[cursor]
                cursor += 1
                trail = second - 0x40 if 0x40 <= second <= 0x7E else second - 0x62
                glyph_index = (first - 0xA1) * 157 + trail
                glyph = big5_font[glyph_index * 32:(glyph_index + 1) * 32]
                glyph_width = 16
            else:
                glyph = ascii_font[first * 16:(first + 1) * 16]
                glyph_width = 8
            assert len(glyph) == glyph_width * 2
            for row in range(16):
                for byte_index in range(glyph_width // 8):
                    bits = glyph[row * glyph_width // 8 + byte_index]
                    for bit in range(8):
                        if bits & (0x80 >> bit):
                            offset = (y + row) * 320 + x + byte_index * 8 + bit
                            assert 0 <= offset and offset + 1 < len(pixels)
                            pixels[offset] = foreground
                            pixels[offset + 1] = right_shadow
            x += glyph_width

    def render_case(
        talk_id: int,
        head_id: int,
        style: int,
        case_base_frame: bytes | None = None,
        view_origin: tuple[int, int] = (33, 18),
    ) -> dict[str, object]:
        panel_position, portrait_position = layouts[style]
        panel_x, panel_y = panel_position
        page_frames: list[str] = []
        pages = dialogue_pages(decoded_talks[talk_id])
        for page in pages:
            pixels = bytearray(base_frame if case_base_frame is None else case_base_frame)
            background(pixels, panel_x, panel_y, 218, 57)
            border(pixels, panel_x, panel_y, 218, 57)
            if portrait_position is not None:
                portrait_x, portrait_y = portrait_position
                background(pixels, portrait_x, portrait_y, 60, 62)
                draw_sprite(pixels, portraits[head_id], portrait_x + 2, portrait_y + 59)
                border(pixels, portrait_x, portrait_y, 60, 62)
            for line_index, line in enumerate(page[:-1].split(b"*")):
                if not line:
                    continue
                draw_text_linear(
                    pixels,
                    panel_x + 13,
                    panel_y + 3 + line_index * 17,
                    line + b"\0",
                )
            page_frames.append(fnv1a64(pixels))
        return {
            "talk_id": talk_id,
            "head_id": head_id,
            "style": style,
            "view_origin": list(view_origin),
            "page_count": len(pages),
            "page_hex": [page.hex() for page in pages],
            "panel": [panel_x, panel_y, 218, 57],
            "portrait_panel": None if portrait_position is None else [*portrait_position, 60, 62],
            "portrait_anchor": None if portrait_position is None else [portrait_position[0] + 2, portrait_position[1] + 59],
            "text_position": [panel_x + 13, panel_y + 3],
            "colors": [0x17, 0x15],
            "frame_fnv1a64": page_frames,
        }

    question_texts = {
        "battle": bytes.fromhex(
            "ac 4f a7 5f bb 50 a4 a7 b9 4c a9 db a1 5d a2 e7 a1 fe a2 dc a1 5e 00"
        ),
        "join": bytes.fromhex(
            "ac 4f a7 5f ad 6e a8 44 a5 5b a4 4a a1 5d a2 e7 a1 fe a2 dc a1 5e 00"
        ),
        "rest": bytes.fromhex(
            "ac 4f a7 5f a6 ed b1 4a b9 4c a9 5d a1 5d a2 e7 a1 fe a2 dc a1 5e 00"
        ),
    }
    question_frames: dict[str, str] = {}
    for question, text in question_texts.items():
        pixels = bytearray(base_frame)
        background(pixels, 61, 40, 187, 27)
        border(pixels, 61, 40, 187, 27)
        draw_text_linear(pixels, 71, 45, text, 0x05, 0x07)
        question_frames[question] = fnv1a64(pixels)

    opcode_5_occurrences: list[tuple[int, int, int, int]] = []
    opcode_9_occurrences: list[tuple[int, int, int, int]] = []
    opcode_11_occurrences: list[tuple[int, int, int, int]] = []
    for script_id, script in enumerate(scripts):
        instructions = words(script)
        program_counter = 0
        while instructions[program_counter] != -1:
            opcode = instructions[program_counter]
            if opcode in (5, 9, 11):
                occurrence = (
                    script_id,
                    program_counter,
                    instructions[program_counter + 1],
                    instructions[program_counter + 2],
                )
                if opcode == 5:
                    opcode_5_occurrences.append(occurrence)
                elif opcode == 9:
                    opcode_9_occurrences.append(occurrence)
                else:
                    opcode_11_occurrences.append(occurrence)
            program_counter += WIDTHS[opcode]
    opcode_5_stream = b"".join(
        struct.pack("<II2h", *row) for row in opcode_5_occurrences
    )
    opcode_5_offset_counts = Counter(
        (row[2], row[3]) for row in opcode_5_occurrences
    )
    opcode_5_exceptional = [
        {
            "script": script_id,
            "program_counter": program_counter,
            "true_offset": true_offset,
            "false_offset": false_offset,
        }
        for script_id, program_counter, true_offset, false_offset in opcode_5_occurrences
        if false_offset != 0
    ]
    assert len(opcode_5_occurrences) == 43
    assert opcode_5_occurrences[0] == (2, 5, 1, 0)
    assert opcode_5_occurrences[-1] == (637, 39, 1, 0)
    assert [(row["script"], row["true_offset"], row["false_offset"])
            for row in opcode_5_exceptional] == [(307, 0, 52), (308, 0, 52)]

    opcode_9_stream = b"".join(
        struct.pack("<II2h", *row) for row in opcode_9_occurrences
    )
    opcode_9_true_counts = Counter(row[2] for row in opcode_9_occurrences)
    opcode_9_false_counts = Counter(row[3] for row in opcode_9_occurrences)
    opcode_9_offset_counts = Counter(
        (row[2], row[3]) for row in opcode_9_occurrences
    )
    opcode_9_exceptional = [
        {
            "script": script_id,
            "program_counter": program_counter,
            "true_offset": true_offset,
            "false_offset": false_offset,
        }
        for script_id, program_counter, true_offset, false_offset in opcode_9_occurrences
        if false_offset != 0
    ]
    assert len(opcode_9_occurrences) == 81
    assert opcode_9_occurrences[0] == (10, 101, 1, 0)
    assert opcode_9_occurrences[-1] == (999, 5, 6, 0)
    assert [(row["script"], row["true_offset"], row["false_offset"])
            for row in opcode_9_exceptional] == [
                (304, 0, 47), (306, 0, 47), (307, 0, 42), (308, 0, 42)
            ]

    opcode_11_stream = b"".join(
        struct.pack("<II2h", *row) for row in opcode_11_occurrences
    )
    opcode_11_offset_counts = Counter(
        (row[2], row[3]) for row in opcode_11_occurrences
    )
    assert len(opcode_11_occurrences) == 7
    assert opcode_11_occurrences[0] == (235, 5, 1, 0)
    assert opcode_11_occurrences[-1] == (931, 0, 1, 0)
    assert set(opcode_11_offset_counts) == {(1, 0)}

    progress_menu_items = (
        bytes.fromhex("b8 fc a4 4a b6 69 ab d7 a4 40 00"),
        bytes.fromhex("b8 fc a4 4a b6 69 ab d7 a4 47 00"),
        bytes.fromhex("b8 fc a4 4a b6 69 ab d7 a4 54 00"),
        bytes.fromhex("c2 f7 b6 7d ba ce c4 b1 a5 68 00"),
    )
    exit_prompt = bytes.fromhex(
        "af 75 ad 6e c2 f7 b6 7d b9 43 c0 b8 a1 5d a2 e7 a1 fe a2 dc a1 5e 00"
    )

    def draw_progress_menu(pixels: bytearray, selection: int) -> None:
        background(pixels, 109, 40, 101, 90)
        border(pixels, 109, 40, 101, 90)
        for index, text in enumerate(progress_menu_items):
            draw_text_linear(pixels, 119, 45 + index * 20, text, 0x21, 0x23)
        draw_text_linear(
            pixels, 119, 45 + selection * 20,
            progress_menu_items[selection], 0x63, 0x66,
        )

    def draw_exit_confirmation(pixels: bytearray) -> None:
        background(pixels, 71, 140, 177, 31)
        border(pixels, 71, 140, 177, 31)
        draw_text_linear(pixels, 75, 145, exit_prompt, 0x05, 0x07)

    load_menu_pixels = bytearray(320 * 200)
    load_menu_frames = {"black": fnv1a64(load_menu_pixels)}
    draw_progress_menu(load_menu_pixels, 0)
    load_menu_frames["selection_0"] = fnv1a64(load_menu_pixels)
    draw_progress_menu(load_menu_pixels, 2)
    load_menu_frames["up_from_0_to_2"] = fnv1a64(load_menu_pixels)
    draw_progress_menu(load_menu_pixels, 3)
    load_menu_frames["down_from_2_to_3"] = fnv1a64(load_menu_pixels)
    draw_exit_confirmation(load_menu_pixels)
    load_menu_frames["quit_confirmation"] = fnv1a64(load_menu_pixels)
    draw_progress_menu(load_menu_pixels, 3)
    load_menu_frames["non_y_returns_with_prompt_pixels"] = fnv1a64(load_menu_pixels)

    style_cases = {
        "style_0_script_1_pc_0": render_case(0, 1, 0),
        "style_1_script_1_pc_5": render_case(1, 0, 1),
        "style_2_script_244_pc_0": render_case(796, 200, 2),
        "style_4_script_142_pc_10": render_case(
            547, 77, 4, style_4_base_frame, (33, 14)
        ),
        "long_line_script_515_pc_65": render_case(1841, 0, 1),
        "weather_script_18_pc_0": render_case(
            2960, 93, 0, weather_base_frame, (6, 36)
        ),
    }
    assert words(scripts[1])[:9] == (1, 0, 1, 0, 0, 1, 1, 0, 1)
    assert words(scripts[244])[:5] == (1, 796, 200, 2, 0)
    assert words(scripts[142])[10:14] == (1, 547, 77, 4)
    assert words(scripts[515])[65:69] == (1, 1841, 0, 1)

    maximum_line_width = 0
    maximum_line_talk = -1
    for talk_id, text in enumerate(decoded_talks):
        for line in text[:-1].split(b"*"):
            width = 0
            cursor = 0
            while cursor < len(line):
                if line[cursor] > 0x7F:
                    width += 16
                    cursor += 2
                else:
                    width += 8
                    cursor += 1
            if width > maximum_line_width:
                maximum_line_width = width
                maximum_line_talk = talk_id

    styles_present: set[int] = set()
    style_counts: dict[int, int] = {}
    opcode_1_count = 0
    invalid_talk_ids: list[tuple[int, int, int]] = []
    invalid_portrait_ids: list[tuple[int, int, int]] = []
    for script_id, script in enumerate(scripts):
        instructions = words(script)
        program_counter = 0
        while instructions[program_counter] != -1:
            opcode = instructions[program_counter]
            if opcode == 1:
                talk_id = instructions[program_counter + 1]
                head_id = instructions[program_counter + 2]
                style = instructions[program_counter + 3]
                opcode_1_count += 1
                styles_present.add(style)
                style_counts[style] = style_counts.get(style, 0) + 1
                if not 0 <= talk_id < len(decoded_talks):
                    invalid_talk_ids.append((script_id, program_counter, talk_id))
                if not 2 <= style <= 3 and not 0 <= head_id < len(portraits):
                    invalid_portrait_ids.append((script_id, program_counter, head_id))
            program_counter += WIDTHS[opcode]

    page_count_distribution: dict[int, int] = {}
    maximum_page_count = 0
    maximum_page_talks: list[int] = []
    empty_final_page_talks: list[int] = []
    for talk_id, text in enumerate(decoded_talks):
        page_count = len(dialogue_pages(text))
        page_count_distribution[page_count] = page_count_distribution.get(page_count, 0) + 1
        if page_count > maximum_page_count:
            maximum_page_count = page_count
            maximum_page_talks = [talk_id]
        elif page_count == maximum_page_count:
            maximum_page_talks.append(talk_id)
        if text[-2:-1] == b"*" and text[:-1].count(b"*") % 3 == 0:
            empty_final_page_talks.append(talk_id)

    return {
        "hdgrp_entry_count": len(portraits),
        "hdgrp_sha256": sha256((root / "HDGRP.GRP").read_bytes()),
        "talk_14_page_count": len(dialogue_pages(decoded_talks[14])),
        "maximum_explicit_line_width": maximum_line_width,
        "maximum_explicit_line_talk": maximum_line_talk,
        "styles_present": sorted(styles_present),
        "opcode_1_asset_domain": {
            "occurrences": opcode_1_count,
            "style_counts": {str(style): style_counts[style] for style in sorted(style_counts)},
            "invalid_talk_ids": invalid_talk_ids,
            "invalid_portrait_ids_for_drawn_styles": invalid_portrait_ids,
        },
        "pagination_asset_domain": {
            "page_count_distribution": {
                str(count): page_count_distribution[count]
                for count in sorted(page_count_distribution)
            },
            "maximum_page_count": maximum_page_count,
            "maximum_page_talks": maximum_page_talks,
            "empty_final_page_talks": empty_final_page_talks,
        },
        "render_policy": {
            "first_page": "reuse_caller_framebuffer_without_scene_redraw",
            "repeated_same_page": "restore_frozen_page_base",
            "later_pages": "redraw_scene_once_before_each_page",
            "final_key": "leave_final_dialogue_framebuffer",
            "weather_random_draws": {
                "first_page": 0,
                "repeated_same_page": 0,
                "later_page_first_render": 2,
            },
        },
        "physical_callers": [
            "sub_2C319:0x2C3ED opcode_1",
            "sub_2FF87:0x2FFA6 random_talk",
            "sub_302E0:0x3035D/0x3038F/0x303AF/0x303C5/0x303DB/0x303F1/0x30407/0x30458 tournament",
            "sub_30480:0x304B3/0x304EA tournament_challenger",
            "sub_312A6:0x31359/0x31798 shop",
        ],
        "portrait_blend_contract": {
            "entry_range": "0x2D1CD..0x2D372",
            "instruction_count": 163,
            "strict_dimension_gate": "width > 10 && height > 10",
            "rectangles": [
                ["x+5", "y", "w-10", "1"],
                ["x+4", "y+1", "w-8", "1"],
                ["x+3", "y+2", "w-6", "1"],
                ["x+2", "y+3", "w-4", "1"],
                ["x+1", "y+4", "w-2", "1"],
                ["x", "y+5", "w", "h-10"],
                ["x+1", "y+h-5", "w-2", "1"],
                ["x+2", "y+h-4", "w-4", "1"],
                ["x+3", "y+h-3", "w-6", "1"],
                ["x+4", "y+h-2", "w-8", "1"],
                ["x+5", "y+h-1", "w-10", "1"],
            ],
            "portrait_size": [60, 62],
            "blended_pixel_count": 3660,
            "caller_addresses": ["0x2CD87", "0x2CE66"],
            "source_palette_index": 0,
            "blend_style": 4,
            "ignored_border_index": 255,
            "positions": [[23, 12], [237, 125], [237, 12], [23, 125]],
        },
        "portrait_border_contract": {
            "entry_range": "0x2D372..0x2D501",
            "instruction_count": 153,
            "strict_dimension_gate": "width > 10 && height > 10",
            "rectangles": [
                ["x+5", "y+1", "w-10", "1"],
                ["x+4", "y+2", "1", "2"],
                ["x+w-5", "y+2", "1", "2"],
                ["x+2", "y+4", "2", "1"],
                ["x+w-4", "y+4", "2", "1"],
                ["x+1", "y+5", "1", "h-10"],
                ["x+w-2", "y+5", "1", "h-10"],
                ["x+2", "y+h-5", "2", "1"],
                ["x+w-4", "y+h-5", "2", "1"],
                ["x+4", "y+h-4", "1", "2"],
                ["x+w-5", "y+h-4", "1", "2"],
                ["x+5", "y+h-2", "w-10", "1"],
            ],
            "portrait_size": [60, 62],
            "border_pixel_count": 220,
            "caller_addresses": ["0x2CDBC", "0x2CE9B"],
            "color_index": 255,
            "draw_order": "after_portrait_sprite",
            "positions": [[23, 12], [237, 125], [237, 12], [23, 125]],
        },
        "cases": style_cases,
        "question_prompts": {
            "strings_hex": {key: value.hex() for key, value in question_texts.items()},
            "string_sha256": {
                key: sha256(value) for key, value in question_texts.items()
            },
            "panel": [61, 40, 187, 27],
            "text_position": [71, 45],
            "colors": [0x05, 0x07],
            "frames": question_frames,
            "join_post_key": "bare_scene_present_before_branch",
            "accepted_key": "uppercase_Y_only",
            "non_y_key": "false_offset_without_filter_loop",
            "battle_post_key": "branch_without_scene_redraw_or_extra_present",
            "rest_post_key": "branch_without_scene_redraw_or_extra_present",
            "opcode_5_asset_domain": {
                "occurrences": len(opcode_5_occurrences),
                "stream_encoding": "little_endian_<II2h:script_pc_true_false>",
                "parameter_stream_sha256": sha256(opcode_5_stream),
                "first": list(opcode_5_occurrences[0]),
                "last": list(opcode_5_occurrences[-1]),
                "offset_pair_counts": {
                    f"{true_offset},{false_offset}": count
                    for (true_offset, false_offset), count in sorted(opcode_5_offset_counts.items())
                },
                "exceptional_false_offset_calls": opcode_5_exceptional,
                "program_counter_formula": "old_pc + 3 + selected_offset",
            },
            "opcode_9_asset_domain": {
                "occurrences": len(opcode_9_occurrences),
                "stream_encoding": "little_endian_<II2h:script_pc_true_false>",
                "parameter_stream_sha256": sha256(opcode_9_stream),
                "first": list(opcode_9_occurrences[0]),
                "last": list(opcode_9_occurrences[-1]),
                "true_offset_counts": {
                    str(offset): count
                    for offset, count in sorted(opcode_9_true_counts.items())
                },
                "false_offset_counts": {
                    str(offset): count
                    for offset, count in sorted(opcode_9_false_counts.items())
                },
                "offset_pair_counts": {
                    f"{true_offset},{false_offset}": count
                    for (true_offset, false_offset), count in sorted(opcode_9_offset_counts.items())
                },
                "exceptional_false_offset_calls": opcode_9_exceptional,
                "program_counter_formula": "old_pc + 3 + selected_offset",
            },
            "opcode_11_asset_domain": {
                "occurrences": len(opcode_11_occurrences),
                "stream_encoding": "little_endian_<II2h:script_pc_true_false>",
                "parameter_stream_sha256": sha256(opcode_11_stream),
                "first": list(opcode_11_occurrences[0]),
                "last": list(opcode_11_occurrences[-1]),
                "offset_pair_counts": {
                    f"{true_offset},{false_offset}": count
                    for (true_offset, false_offset), count in sorted(opcode_11_offset_counts.items())
                },
                "program_counter_formula": "old_pc + 3 + selected_offset",
            },
        },
        "opcode_24_load_menu": {
            "items_hex": [item.hex() for item in progress_menu_items],
            "exit_prompt_hex": exit_prompt.hex(),
            "menu_panel": [109, 40, 101, 90],
            "item_positions": [[119, 45 + index * 20] for index in range(4)],
            "confirmation_panel": [71, 140, 177, 31],
            "confirmation_text": [75, 145],
            "normal_colors": [0x21, 0x23],
            "selected_colors": [0x63, 0x66],
            "confirmation_colors": [0x05, 0x07],
            "up_from_zero": 2,
            "frames": load_menu_frames,
        },
        "out_of_range_opcode": {
            "opcode": 68,
            "jump_table_base": "0x2c209",
            "indexed_dword_address": "0x2c319",
            "indexed_dword_value": "0x00004068",
            "program_counter_delta_before_indirect_transfer": 0,
            "modern_safe_adapter": "stable_no_progress",
            "following_instruction_must_not_execute": [2, 123, 1],
        },
    }


def status_notice_vectors(
    root: Path,
    base_frame: bytes,
    palette: list[tuple[int, int, int]],
    scripts: list[bytes],
    ranger: bytes,
) -> dict[str, object]:
    ascii_font = (root / "FONT.X16").read_bytes()
    big5_font = (root / "FONT.C16").read_bytes()
    lookup = rgb4_lookup(palette)

    def blend(pixels: bytearray, x: int, y: int, width: int, height: int) -> None:
        source = palette[0]
        for py in range(y, y + height):
            for px in range(x, x + width):
                offset = py * 320 + px
                destination = palette[pixels[offset]]
                components = tuple(
                    source[channel] // 8 + destination[channel] // 8
                    for channel in range(3)
                )
                pixels[offset] = lookup[
                    components[0] * 256 + components[1] * 16 + components[2]
                ]

    def fill(pixels: bytearray, x: int, y: int, width: int, height: int) -> None:
        for py in range(y, y + height):
            pixels[py * 320 + x:py * 320 + x + width] = bytes([0xFF]) * width

    def panel(pixels: bytearray, x: int, y: int, width: int, height: int) -> None:
        for left, top, w, h in (
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
            blend(pixels, left, top, w, h)
        for left, top, w, h in (
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
            fill(pixels, left, top, w, h)

    cases = (
        (52, 825, 7, 54, 212, bytes.fromhex("a741b27ba662aabaab7ebc77abfcbcc6acb0"), 5),
        (53, 828, 123, 50, 220, bytes.fromhex("a741b27ba662add3a448c16eb1e6abfcbcc6acb0"), 4),
    )
    output: dict[str, object] = {}
    for opcode, script_id, value, x, width, prefix, field_width in cases:
        assert words(scripts[script_id]) == (opcode, -1)
        text = prefix + f"{value:{field_width}d}".encode("ascii") + b"\0"
        pixels = bytearray(base_frame)
        panel(pixels, x, 40, width, 27)
        draw_legacy_text(pixels, x + 10, 45, text, ascii_font, big5_font, 0x05, 0x07)
        output[f"opcode_{opcode}_script_{script_id}"] = {
            "value": value,
            "text_hex": text.hex(),
            "panel": [x, 40, width, 27],
            "text_position": [x + 10, 45],
            "colors": [5, 7],
            "frame_fnv1a64": fnv1a64(pixels),
            "post_ack_outputs": ["present", "stay"] if opcode == 52 else ["stay"],
        }

    opcode_2_occurrences: list[dict[str, int | str]] = []
    for script_id, payload in enumerate(scripts):
        code = words(payload)
        pc = 0
        previous_opcode: int | None = None
        while code[pc] != -1:
            opcode = code[pc]
            assert 0 <= opcode < len(WIDTHS)
            if opcode == 2:
                opcode_2_occurrences.append({
                    "script": script_id,
                    "pc": pc,
                    "item_id": code[pc + 1],
                    "count": code[pc + 2],
                    "previous_opcode": "start" if previous_opcode is None else previous_opcode,
                    "next_opcode": code[pc + 3],
                })
            previous_opcode = opcode
            pc += WIDTHS[opcode]
    assert len(opcode_2_occurrences) == 325
    legal_item_ids = sorted({int(row["item_id"]) for row in opcode_2_occurrences} | {143})
    assert len(legal_item_ids) == 148 and legal_item_ids[0] == 1 and legal_item_ids[-1] == 197

    item_records = [
        ranger[59_076 + item_id * 190:59_076 + (item_id + 1) * 190]
        for item_id in range(200)
    ]
    item_name_lengths = [record[2:22].find(b"\0") for record in item_records]
    assert all(length >= 0 for length in item_name_lengths)
    assert min(item_name_lengths) == 4 and max(item_name_lengths) == 16
    item_name_length_counts = {
        str(length): item_name_lengths.count(length)
        for length in sorted(set(item_name_lengths))
    }

    def item_notice(item_id: int) -> dict[str, object]:
        name_length = item_name_lengths[item_id]
        name = item_records[item_id][2:2 + name_length]
        text = bytes.fromhex("b1 6f a8 ec") + name + b"\0"
        x = 150 - (4 * name_length + 16)
        width = 8 * name_length + 52
        pixels = bytearray(base_frame)
        panel(pixels, x, 40, width, 27)
        draw_legacy_text(
            pixels, x + 10, 45, text, ascii_font, big5_font, 0x05, 0x07
        )
        return {
            "item_id": item_id,
            "name_hex": name.hex(),
            "name_byte_length": name_length,
            "text_hex": text.hex(),
            "panel": [x, 40, width, 27],
            "text_position": [x + 10, 45],
            "colors": [5, 7],
            "frame_fnv1a64": fnv1a64(pixels),
        }

    def wrap_word(value: int) -> int:
        bits = value & 0xFFFF
        return bits if bits < 0x8000 else bits - 0x10000

    def add_item(
        slots: list[tuple[int, int]], item_id: int, count: int
    ) -> list[tuple[int, int]]:
        result = slots.copy()
        found = False
        for index, (slot_item, slot_count) in enumerate(result):
            if slot_item == item_id:
                result[index] = (slot_item, wrap_word(slot_count + count))
                found = True
        if not found:
            for index, (slot_item, slot_count) in enumerate(result):
                if slot_item == -1:
                    result[index] = (item_id, wrap_word(slot_count + count))
                    break
        return result

    def book_event_ready(fame: int, slots: list[tuple[int, int]]) -> bool:
        ids = {item_id for item_id, _ in slots}
        return fame >= 200 and 189 not in ids and all(
            item_id in ids for item_id in range(144, 158)
        )

    duplicate_before = [(109, 32767), (109, -32768), (88, 4)] + [(-1, 0)] * 197
    duplicate_after = add_item(duplicate_before, 109, 1)
    residual_before = [(50 + index, 1) for index in range(5)] + [(-1, 9)] + [(-1, 0)] * 194
    residual_after = add_item(residual_before, 57, 1)
    full_before = [(0, 7)] * 200
    full_after = add_item(full_before, 109, 1)
    all_books = [(item_id, 0) for item_id in range(144, 158)] + [(-1, 0)] * 186
    blocked_by_letter = all_books.copy()
    blocked_by_letter[14] = (189, 0)
    missing_last_book = all_books.copy()
    missing_last_book[13] = (-1, 0)
    added_last_book = add_item(missing_last_book, 157, 1)
    assert duplicate_after[:3] == [(109, -32768), (109, -32767), (88, 4)]
    assert residual_after[5] == (57, 10)
    assert full_after == full_before
    assert not book_event_ready(199, all_books)
    assert book_event_ready(200, all_books)
    assert not book_event_ready(200, blocked_by_letter)
    assert not book_event_ready(200, missing_last_book)
    assert book_event_ready(200, added_last_book)

    occurrence_stream = b"".join(
        struct.pack(
            "<4h",
            int(row["script"]), int(row["pc"]),
            int(row["item_id"]), int(row["count"]),
        )
        for row in opcode_2_occurrences
    )
    legal_notice_stream = bytearray()
    legal_notices = {item_id: item_notice(item_id) for item_id in legal_item_ids}
    for item_id in legal_item_ids:
        legal_notice_stream.extend(bytes.fromhex(str(legal_notices[item_id]["frame_fnv1a64"])[2:]))
    output["opcode_2_item_add"] = {
        "entry_range": "0x2D678..0x2D841",
        "size_bytes": 457,
        "instruction_count": 134,
        "caller_addresses": ["0x2C40F", "0x3046C"],
        "caller_owners": ["sub_2C319", "sub_302E0"],
        "return_value": 0,
        "inventory_slots": 200,
        "matching_slots_updated": "all",
        "new_slot": "first_item_id_minus_one",
        "new_slot_count": "wrapping_add_existing_count",
        "remove_or_compact_on_nonpositive_count": False,
        "full_inventory_still_shows_notice": True,
        "opcode_2_occurrences": len(opcode_2_occurrences),
        "occurrence_stream_sha256": sha256(occurrence_stream),
        "unique_item_ids": legal_item_ids,
        "count_values": {
            str(value): count
            for value, count in sorted(Counter(int(row["count"]) for row in opcode_2_occurrences).items())
        },
        "previous_opcode_counts": {
            str(value): count
            for value, count in sorted(
                Counter(row["previous_opcode"] for row in opcode_2_occurrences).items(),
                key=lambda item: str(item[0]),
            )
        },
        "next_opcode_counts": {
            str(value): count
            for value, count in sorted(Counter(int(row["next_opcode"]) for row in opcode_2_occurrences).items())
        },
        "all_item_names_nul_terminated": True,
        "item_name_length_counts": item_name_length_counts,
        "item_name_byte_range": [min(item_name_lengths), max(item_name_lengths)],
        "notice_prefix_hex": "b16fa8ec",
        "notice_prefix_cp950": "得到",
        "notice_uses_item_name_only": True,
        "notice_background": "preserve_caller_framebuffer",
        "notice_repeated_render": "restore_frozen_caller_framebuffer",
        "notice_random_draws": 0,
        "machine_outputs": ["notice_present", "wait_any_key", "scene_present"],
        "modern_outputs": ["notice", "present"],
        "sample_notices": {
            str(item_id): legal_notices[item_id] for item_id in (109, 131, 143)
        },
        "all_legal_notice_hashes_sha256": sha256(bytes(legal_notice_stream)),
        "inventory_vectors": {
            "duplicate_wrapping_after": [list(value) for value in duplicate_after[:3]],
            "residual_empty_slot_after": list(residual_after[5]),
            "full_inventory_unchanged": full_after == full_before,
        },
        "book_event_gate": {
            "fame_threshold": 200,
            "required_item_ids": list(range(144, 158)),
            "blocking_item_id": 189,
            "counts_ignored": True,
            "fame_199": book_event_ready(199, all_books),
            "fame_200": book_event_ready(200, all_books),
            "blocking_item_count_zero": book_event_ready(200, blocked_by_letter),
            "missing_last_book": book_event_ready(200, missing_last_book),
            "add_last_book_same_call": book_event_ready(200, added_last_book),
            "event_change": [70, 11, 1, 1, 932, -1, -1, 7968, 7968, 7968, -2, -2, -2],
        },
        "tournament_reward": {
            "item_id": 143,
            "count": 1,
            "runs_book_event_gate_before_notice": True,
        },
    }

    title_lengths = [
        ranger[97_076 + scene * 52 + 2:97_076 + scene * 52 + 12].find(b"\0")
        for scene in range(84)
    ]
    assert all(length >= 0 for length in title_lengths)
    title_length_counts = {
        str(length): title_lengths.count(length)
        for length in sorted(set(title_lengths))
    }
    assert title_length_counts == {"4": 13, "6": 47, "8": 24}
    assert all(length % 2 == 0 for length in title_lengths)
    title_field = ranger[97_076 + 70 * 52 + 2:97_076 + 70 * 52 + 12]
    title_length = title_field.find(b"\0")
    if title_length < 0:
        title_length = len(title_field)
    title = title_field[:title_length] + b"\0"
    title_x = 150 - 4 * title_length
    title_width = 8 * title_length + 20
    title_pixels = bytearray(base_frame)
    panel(title_pixels, title_x, 10, title_width, 27)
    draw_legacy_text(
        title_pixels, title_x + 10, 15, title, ascii_font, big5_font, 0x05, 0x07
    )
    output["scene_70_title"] = {
        "text_hex": title.hex(),
        "byte_length": title_length,
        "all_metadata_titles_nul_terminated": True,
        "metadata_title_length_counts": title_length_counts,
        "metadata_title_max_bytes": max(title_lengths),
        "panel": [title_x, 10, title_width, 27],
        "text_position": [title_x + 10, 15],
        "colors": [5, 7],
        "function_order": [
            "copy_metadata_name_from_byte_2",
            "measure_nul_terminated_byte_length",
            "draw_panel_on_existing_scene_frame",
            "draw_name",
            "present",
            "clear_last_key_and_wait_for_any_nonzero_key",
            "render_scene",
            "present",
        ],
        "physical_callers": [
            "sub_28E40:0x28F4B normal_scene_entry",
            "sub_28E40:0x292A3 internal_scene_jump",
        ],
        "caller_next_step": "sub_2B3B4 automatic_event_check",
        "overlay_render_policy": "restore_frozen_pre_title_frame_before_each_overlay_render",
        "weather_random_draws": {
            "title_overlay": 0,
            "repeated_title_overlay": 0,
            "post_key_scene_redraw": 2,
        },
        "outputs": ["scene_title", "present", "auto_event_check"],
        "frame_fnv1a64": fnv1a64(title_pixels),
    }
    return output


def death_menu_sequence(
    root: Path,
    palette: list[tuple[int, int, int]],
    ranger: bytes,
) -> dict[str, object]:
    image = (root / "DEAD.BIG").read_bytes()
    assert len(image) == 64000
    ascii_font = (root / "FONT.X16").read_bytes()
    big5_font = (root / "FONT.C16").read_bytes()
    lookup = rgb4_lookup(palette)
    menu_items = (
        bytes.fromhex("b8 fc a4 4a b6 69 ab d7 a4 40 00"),
        bytes.fromhex("b8 fc a4 4a b6 69 ab d7 a4 47 00"),
        bytes.fromhex("b8 fc a4 4a b6 69 ab d7 a4 54 00"),
        bytes.fromhex("c2 f7 b6 7d ba ce c4 b1 a5 68 00"),
    )
    location = bytes.fromhex("a6 62 a6 61 b2 79 aa ba ac 59 b3 42 00")
    missing = bytes.fromhex("b7 ed a6 61 a4 48 a4 66 aa ba a5 a2 c2 dc bc c6 00")
    another = bytes.fromhex("a4 53 a6 68 a4 46 a4 40 b5 a7 a1 44 a1 44 a1 44 00")
    exit_prompt = bytes.fromhex(
        "af 75 ad 6e c2 f7 b6 7d b9 43 c0 b8 a1 5d a2 e7 a1 fe a2 dc a1 5e 00"
    )

    def blend(pixels: bytearray, x: int, y: int, width: int, height: int) -> None:
        source = palette[0]
        for py in range(y, y + height):
            for px in range(x, x + width):
                offset = py * 320 + px
                destination = palette[pixels[offset]]
                components = tuple(
                    source[channel] * 4 // 32 + destination[channel] * 4 // 32
                    for channel in range(3)
                )
                pixels[offset] = lookup[
                    components[0] * 256 + components[1] * 16 + components[2]
                ]

    def fill(pixels: bytearray, x: int, y: int, width: int, height: int) -> None:
        for py in range(y, y + height):
            pixels[py * 320 + x:py * 320 + x + width] = bytes([255]) * width

    def panel(pixels: bytearray, x: int, y: int, width: int, height: int) -> None:
        for left, top, w, h in (
            (x + 5, y, width - 10, 1), (x + 4, y + 1, width - 8, 1),
            (x + 3, y + 2, width - 6, 1), (x + 2, y + 3, width - 4, 1),
            (x + 1, y + 4, width - 2, 1), (x, y + 5, width, height - 10),
            (x + 1, y + height - 5, width - 2, 1),
            (x + 2, y + height - 4, width - 4, 1),
            (x + 3, y + height - 3, width - 6, 1),
            (x + 4, y + height - 2, width - 8, 1),
            (x + 5, y + height - 1, width - 10, 1),
        ):
            blend(pixels, left, top, w, h)
        for left, top, w, h in (
            (x + 5, y + 1, width - 10, 1), (x + 4, y + 2, 1, 2),
            (x + width - 5, y + 2, 1, 2), (x + 2, y + 4, 2, 1),
            (x + width - 4, y + 4, 2, 1), (x + 1, y + 5, 1, height - 10),
            (x + width - 2, y + 5, 1, height - 10),
            (x + 2, y + height - 5, 2, 1),
            (x + width - 4, y + height - 5, 2, 1),
            (x + 4, y + height - 4, 1, 2),
            (x + width - 5, y + height - 4, 1, 2),
            (x + 5, y + height - 2, width - 10, 1),
        ):
            fill(pixels, left, top, w, h)

    def render(selection: int, confirm: bool = False) -> bytes:
        pixels = bytearray(image)
        name = ranger[836 + 8:836 + 8 + 10] + b"\0"
        date = b"  1996/ 1/ 1  \0"
        draw_legacy_text(pixels, 97, 46, name, ascii_font, big5_font, 0x6E, 0x6C)
        draw_legacy_text(pixels, 190, 8, date, ascii_font, big5_font, 0x17, 0x15)
        draw_legacy_text(pixels, 190, 28, location, ascii_font, big5_font, 0x17, 0x15)
        draw_legacy_text(pixels, 190, 48, missing, ascii_font, big5_font, 0x17, 0x15)
        draw_legacy_text(pixels, 190, 68, another, ascii_font, big5_font, 0x17, 0x15)
        panel(pixels, 205, 90, 101, 90)
        for index, item in enumerate(menu_items):
            draw_legacy_text(pixels, 215, 95 + index * 20, item, ascii_font, big5_font, 0x21, 0x23)
        draw_legacy_text(
            pixels, 215, 95 + selection * 20, menu_items[selection],
            ascii_font, big5_font, 0x63, 0x66,
        )
        if confirm:
            panel(pixels, 71, 180, 177, 20)
            draw_legacy_text(pixels, 75, 182, exit_prompt, ascii_font, big5_font, 0x05, 0x07)
        return bytes(pixels)

    return {
        "image_sha256": sha256(image),
        "fixed_date": [1996, 1, 1],
        "selected_frame_fnv1a64": [fnv1a64(render(index)) for index in range(4)],
        "confirm_frame_fnv1a64": fnv1a64(render(3, True)),
        "cleared_frame_fnv1a64": fnv1a64(bytes(64000)),
        "menu_items_hex": [item.hex() for item in menu_items],
    }


def ending_sequence(root: Path) -> dict[str, object]:
    words_archive = packed(
        (root / "ENDWORD.IDX").read_bytes(), (root / "ENDWORD.GRP").read_bytes()
    )
    kend_archive = packed(
        (root / "KEND.IDX").read_bytes(), (root / "KEND.GRP").read_bytes()
    )
    assert len(words_archive) == 23
    assert len(kend_archive) == 221
    assert all(len(frame) == 64000 for frame in kend_archive)

    def render_words(entries: tuple[tuple[int, int, int], ...]) -> bytes:
        pixels = bytearray(64000)
        for legacy_id, x, y in entries:
            draw_sprite(pixels, words_archive[legacy_id // 2], x, y)
        return bytes(pixels)

    title = render_words(((0, 94, 90),))
    word_scroll = []
    for index in (0, 50, 150, 250, 350, 442):
        word_scroll.append({
            "index": index,
            "first_y": 210 - index,
            "second_y": 313 - index,
            "frame_fnv1a64": fnv1a64(render_words((
                (2, 44, 210 - index), (4, 44, 313 - index)
            ))),
        })

    credit_ids = tuple(range(6, 46, 2))
    credit_x = (
        60, 60, 60, 60, 115, 115, 115, 115, 115, 115,
        115, 115, 115, 115, 115, 115, 115, 105, 135, 56,
    )
    credit_y = (
        210, 386, 551, 698, 950, 1111, 1255, 1391, 1557, 1718,
        1772, 1938, 2087, 2195, 2335, 2479, 2642, 2743, 3055, 3301,
    )
    credits_setup = render_words(tuple(
        (credit_ids[index], credit_x[index], credit_y[index]) for index in range(4)
    ))
    credits_scroll = []
    for index in (0, 100, 500, 1000, 2000, 3000, 3243):
        credits_scroll.append({
            "index": index,
            "last_y": credit_y[-1] - index,
            "frame_fnv1a64": fnv1a64(render_words(tuple(
                (credit_ids[item], credit_x[item], credit_y[item] - index)
                for item in range(len(credit_ids))
            ))),
        })

    return {
        "palette_sha256": sha256((root / "ENDCOL.COL").read_bytes()),
        "title_frame_fnv1a64": fnv1a64(title),
        "title_delay_ticks": 2000 // 40 + 1,
        "word_scroll_frame_count": 443,
        "word_scroll_delay_ticks": 100 // 40 + 1,
        "word_scroll_samples": word_scroll,
        "kend_frame_count": len(kend_archive),
        "kend_samples": [
            {"index": index, "frame_fnv1a64": fnv1a64(kend_archive[index])}
            for index in (0, 1, 220)
        ],
        "credits_setup_fnv1a64": fnv1a64(credits_setup),
        "credits_scroll_frame_count": 3244,
        "credits_scroll_delay_ticks": 100 // 40 + 1,
        "credits_scroll_samples": credits_scroll,
        "wait_key_count": 2,
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
    fixed_shadow_mask = (root / "3_shadow.msk").read_bytes()
    shifted_shadow_mask = (root / "4_shadow.msk").read_bytes()
    weather_output, shifted_weather_output, shadow_offset, weather_random_state = scene_shadow_frames(
        weather_base, fixed_shadow_mask, shifted_shadow_mask
    )
    palette_bytes = (root / "MMAP.COL").read_bytes()
    palette = [tuple(palette_bytes[offset:offset + 3]) for offset in range(0, len(palette_bytes), 3)]
    buffer_preservation = []
    for render_scene_id in range(84):
        render_metadata = struct.unpack_from("<26h", ranger, 97_076 + render_scene_id * 52)
        render_x, render_y = render_metadata[14], render_metadata[15]
        render_map_words = words(scene_maps[render_scene_id])
        render_event_words = words(scene_events[render_scene_id])
        render_sprites = sentinel(
            (root / f"SDX{render_scene_id:03d}").read_bytes(),
            (root / f"SMP{render_scene_id:03d}").read_bytes(),
        )
        zero_frame = render_scene(
            render_map_words, render_event_words, render_sprites, render_x, render_y, 1
        )
        seeded_frame = render_scene(
            render_map_words,
            render_event_words,
            render_sprites,
            render_x,
            render_y,
            1,
            base_frame=bytes([255]) * 64_000,
        )
        different_positions = [
            [index % 320, index // 320]
            for index, (left, right) in enumerate(zip(zero_frame, seeded_frame, strict=True))
            if left != right
        ]
        if different_positions:
            buffer_preservation.append({
                "scene": render_scene_id,
                "entrance": [render_x, render_y],
                "different_pixels": len(different_positions),
                "different_positions": different_positions,
                "zero_frame_fnv1a64": fnv1a64(zero_frame),
                "seed255_frame_fnv1a64": fnv1a64(seeded_frame),
            })
    assert [
        (entry["scene"], entry["different_pixels"]) for entry in buffer_preservation
    ] == [(4, 3), (44, 2), (80, 2)]
    decoded_talks = [bytes(value ^ 0xFF for value in entry[:-1]) + b"\0" for entry in talks]
    coverage = opcode_coverage(scripts)
    script_30 = words(scripts[30])
    assert script_30[:5] == (25, 41, 31, 34, 31)
    opcode_25_script_30 = pan_trace(
        smap, sevent, sprites, 44, 29, 1, script_30[1:5]
    )
    script_142 = words(scripts[142])
    assert script_142[:5] == (25, 30, 33, 30, 24)
    opcode_25_script_142 = pan_trace(
        smap, sevent, sprites, 44, 29, 1, script_142[1:5]
    )
    assert opcode_25_script_142[-1]["view_origin_x"] == 33
    assert opcode_25_script_142[-1]["view_origin_y"] == 14
    style_4_base_frame = render_scene(
        smap, sevent, sprites, 44, 29, 1, (33, 14)
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
    walk_events = advance_event_animation(
        walk_map, words(scene_events[walk_scene_id]), 1
    )
    walk_sprites = sentinel(
        (root / "SDX039").read_bytes(), (root / "SMP039").read_bytes()
    )
    script_343 = words(scripts[343])
    assert script_343[30:35] == (30, 28, 24, 28, 19)
    opcode_30_script_343 = scripted_walk_trace(
        walk_map, walk_events, walk_sprites, 28, 24, script_343[31:35]
    )
    walk_final = opcode_30_script_343[-1]
    walk_final_direction = int(walk_final["direction"])
    walk_final_frame = render_scene(
        walk_map,
        walk_events,
        walk_sprites,
        int(walk_final["x"]),
        int(walk_final["y"]),
        walk_final_direction,
        player_picture=FRAME_BASE[walk_final_direction],
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

    script_936 = words(scripts[936])
    assert script_936[54] == 58
    tournament_state = 1
    tournament_indices: list[int] = []
    for group in range(5):
        chosen: set[int] = set()
        while len(chosen) < 3:
            tournament_state = (tournament_state * 0x41C64E6D + 0x3039) & 0xFFFFFFFF
            opponent = ((tournament_state >> 16) & 0x7FFF) % 6
            if opponent in chosen:
                continue
            chosen.add(opponent)
            tournament_indices.append(group * 6 + opponent)

    script_938 = words(scripts[938])
    script_939 = words(scripts[939])
    assert script_938 == (64, -1)
    assert script_939 == (65, -1)
    shop_cases = [
        {"scene": 0, "shop": 0, "close_events": []},
        {"scene": 1, "shop": 0, "close_events": [17, 18]},
        {"scene": 3, "shop": 1, "close_events": [15, 16]},
        {"scene": 40, "shop": 2, "close_events": [21, 22]},
        {"scene": 60, "shop": 3, "close_events": [17, 18]},
        {"scene": 61, "shop": 4, "close_events": [10, 11, 12]},
    ]
    merchant_hide_cases = [
        {"scene": 0, "events": []},
        {"scene": 1, "events": [16, 17, 18]},
        {"scene": 3, "events": [14, 15, 16]},
        {"scene": 40, "events": [20, 21, 22]},
        {"scene": 60, "events": [16, 17, 18]},
        {"scene": 61, "events": [9, 10, 11, 12]},
    ]

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
            "ranger_sha256": sha256(ranger),
        },
        "talk": {
            "decoded_stream_sha256": sha256(b"".join(decoded_talks)),
            "record_0_sha256": sha256(decoded_talks[0]),
            "record_0_hex": decoded_talks[0].hex(),
            "record_2976_sha256": sha256(decoded_talks[2976]),
        },
        "kdef": {
            **coverage,
            "opcode_6_battle_requests": battle_request_vectors(scripts),
            "opcode_10_join_role": join_role_vectors(scripts, ranger),
            "opcode_12_party_rest": party_rest_vectors(scripts),
            "opcode_13_fade_from_black": fade_from_black_vectors(scripts),
            "opcode_14_fade_to_black": fade_to_black_vectors(scripts),
            "opcode_16_party_contains": party_contains_vectors(scripts),
            "opcode_18_inventory_presence": inventory_presence_vectors(scripts),
            "opcode_20_party_tail_condition": party_tail_condition_vectors(scripts),
            "opcode_21_leave_role": leave_role_vectors(scripts, ranger),
            "explicit_scene_present": explicit_scene_present_vectors(),
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
                "final_standing_present": {
                    "x": int(walk_final["x"]),
                    "y": int(walk_final["y"]),
                    "direction": walk_final_direction,
                    "player_picture": FRAME_BASE[walk_final_direction],
                    "wait_ticks": 1,
                    "frame_fnv1a64": fnv1a64(walk_final_frame),
                },
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
                "ending": ending_sequence(root),
            },
            "opcode_58_script_936": {
                "script_id": 936,
                "program_counter": 54,
                "seed": 1,
                "selected_indices": tournament_indices,
                "battle_ids": [102 + index for index in tournament_indices],
                "talk_ids": [2854 + index for index in tournament_indices],
                "head_ids": [
                    8, 21, 23, 31, 32, 43, 7, 11, 14, 20, 33, 34, 10, 12, 19,
                    22, 56, 68, 13, 55, 62, 67, 70, 71, 26, 57, 60, 64, 3, 69,
                ],
                "final_rng_state": f"0x{tournament_state:08x}",
                "delay_300_ticks": 300 // 40 + 1,
                "disabled_event_range": [24, 72],
                "reward_item": 143,
                "death_menu": death_menu_sequence(root, palette, ranger),
            },
            "opcode_64_script_938": {
                "script_id": 938,
                "shop_cases": shop_cases,
                "currency_item": 174,
                "success_talk": 2976,
                "failure_talk": 2975,
                "purchase_feedback_outputs": [
                    "present",
                    "dialogue",
                    "present",
                    "stay",
                ],
                "close_event_3": 939,
            },
            "opcode_65_script_939": {
                "script_id": 939,
                "hide_cases": merchant_hide_cases,
                "hide_fields": [0, 0, -1, -1, -1, -1, -1, -1],
                "seed_1_random_value": 3,
                "activated_target": [60, 16],
                "activated_fields": [1, 1, 938, -1, -1, 8256, 8256, 8256],
            },
            "basic_helper_vectors": basic_helper_vectors(scripts),
            "main_loop_dispatch_vectors": main_loop_dispatch_vectors(palette_bytes),
            "rectangle_outline_vectors": rectangle_outline_vectors(),
            "portrait_archive_vectors": portrait_archive_vectors(root, ranger),
            "state_write_vectors": state_write_vectors(scripts, scene_events),
            "scene_animation_vectors": scene_animation_vectors(
                scene_maps, scene_events, sprites, scripts
            ),
            "scene_archive_state_vectors": scene_archive_state_vectors(
                ranger, scene_maps, scene_events, scripts
            ),
            "scene_loop_vectors": scene_loop_vectors(ranger, scripts),
            "dialogue_vectors": dialogue_vectors(
                root,
                frame,
                style_4_base_frame,
                weather_output,
                palette,
                decoded_talks,
                scripts,
            ),
            "status_notice_vectors": status_notice_vectors(
                root, frame, palette, scripts, ranger
            ),
            "opcode_59_script_932": {
                "script_id": 932,
                "program_counter": 38,
                "party_source_indices": [6, 5, 4, 3, 2, 1],
                "index_6_source": "inventory_item_0",
                "disabled_event_fields": [0, 0, -1, -1, -1, -1, -1, -1, 0],
                "targets": opcode_59_targets,
            },
        },
        "scene_render_buffer_preservation": {
            "scene_archive_count": 84,
            "scenes_with_uncovered_pixels": buffer_preservation,
        },
        "scene_5_shadow": {
            "initial_x": weather_x,
            "initial_y": weather_y,
            "base_frame_fnv1a64": fnv1a64(weather_base),
            "shadow_counter": 0,
            "rng_upper_bounds": [7, 7],
            "random_offset": shadow_offset,
            "random_state_after_render": f"0x{weather_random_state:08x}",
            "fixed_mask_bytes": len(fixed_shadow_mask),
            "fixed_mask_sha256": sha256(fixed_shadow_mask),
            "fixed_mask_frame_fnv1a64": fnv1a64(weather_output),
            "shifted_mask_bytes": len(shifted_shadow_mask),
            "shifted_mask_sha256": sha256(shifted_shadow_mask),
            "shifted_mask_frame_fnv1a64": fnv1a64(shifted_weather_output),
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
