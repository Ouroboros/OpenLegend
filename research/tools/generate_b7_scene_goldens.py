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
                picture = event_value(event_words, event, 7)
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
        direction = (2 if delta < 0 else 1) if horizontal else (3 if delta < 0 else 0)
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
        "caller": "sub_2A186 item grid",
        "geometry": [x, y, width, height],
        "draw_order": ["top", "left", "right", "bottom"],
        "normal_color": 0,
        "normal_frame_fnv1a64": fnv1a64(normal),
        "selected_color": 255,
        "selected_frame_fnv1a64": fnv1a64(selected),
        "interior_preserved": 7,
    }


def state_write_vectors(scripts: list[bytes]) -> dict[str, object]:
    occurrences: dict[int, list[dict[str, object]]] = {3: [], 17: [], 26: []}
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
    opcode_17 = occurrences[17]
    opcode_26 = occurrences[26]
    coordinate_updates = [
        row for row in opcode_3
        if row["arguments"][11] != -2 or row["arguments"][12] != -2
    ]
    assert len(opcode_3) == 2_320
    assert len(coordinate_updates) == 30
    assert all(
        row["arguments"][11] != -2 and row["arguments"][12] != -2
        for row in coordinate_updates
    )
    assert coordinate_updates[0] == {
        "script": 147,
        "pc": 0,
        "arguments": [-2, 6, -2, -2, 146, -1, -1, 5398, 5398, 5398, -2, 14, 40],
    }
    assert len(opcode_17) == 127
    assert sorted({row["arguments"][0] for row in opcode_17}) == [-2, 11, 18, 21, 49, 52, 53, 55]
    assert len(opcode_26) == 121
    assert all(row["arguments"][1] != -2 for row in opcode_26)

    return {
        "opcode_3_event_fields": {
            "occurrences": len(opcode_3),
            "explicit_scene_occurrences": sum(
                row["arguments"][0] != -2 for row in opcode_3
            ),
            "coordinate_updates": len(coordinate_updates),
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
        "opcode_17_scene_cell": {
            "occurrences": len(opcode_17),
            "current_scene_occurrences": sum(
                row["arguments"][0] == -2 for row in opcode_17
            ),
            "explicit_scene_ids": sorted({
                row["arguments"][0]
                for row in opcode_17
                if row["arguments"][0] != -2
            }),
            "linear_index": "4096*layer + 64*y + x",
        },
        "opcode_26_event_script_add": {
            "occurrences": len(opcode_26),
            "current_event_sentinel_occurrences": sum(
                row["arguments"][1] == -2 for row in opcode_26
            ),
            "event_ids": sorted({row["arguments"][1] for row in opcode_26}),
            "updated_fields": [2, 3, 4],
            "addition_width_bits": 16,
        },
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
    assert len(opcode_8) == 15
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

    map_words = list(words(scene_maps[70]))
    event_words = list(words(scene_events[70]))
    map_words[3 * 4096 + 29 * 64 + 44] = 199
    event_words[199 * 11 + 5] = first_picture
    event_words[199 * 11 + 6] = end_picture
    event_words[199 * 11 + 7] = 102
    event_words[199 * 11 + 8] = delay
    frame = render_scene(tuple(map_words), tuple(event_words), sprites, 44, 29, 1)
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
        "word7_render_frame_fnv1a64": fnv1a64(frame),
        "interaction_present": {
            "direction": 1,
            "player": [44, 29],
            "target": [45, 29],
            "event_1_script": 825,
            "script_words": list(script_825),
            "outputs": ["present", "notice_style_52"],
        },
        "trigger_field_counts": trigger_counts,
        "item_event_outputs": {
            "no_event": ["stay"],
            "event_script_0": ["present", "stay"],
            "event_script_825": ["present", "notice_style_52"],
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
            "no_event_outputs": ["restore_scene_present", "open_ui_main"],
            "event_script_0_outputs": [
                "restore_scene_present",
                "item_event_present",
                "open_ui_main",
            ],
            "event_script_825_outputs": [
                "restore_scene_present",
                "item_event_present",
                "notice_style_52",
                "open_ui_main",
            ],
            "tick_after_action": "deferred_until_menu_exit",
        },
        "automatic_event_outputs": {
            "event_script_minus_1": ["fallback"],
            "event_script_0": ["present", "stay"],
            "event_script_825": ["present", "notice_style_52"],
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
                    width += 4 if line[cursor] == ord("_") else 8
                    cursor += 1
            if width > maximum_line_width:
                maximum_line_width = width
                maximum_line_talk = talk_id

    styles_present: set[int] = set()
    for script in scripts:
        instructions = words(script)
        program_counter = 0
        while instructions[program_counter] != -1:
            opcode = instructions[program_counter]
            if opcode == 1:
                styles_present.add(instructions[program_counter + 3])
            program_counter += WIDTHS[opcode]

    return {
        "hdgrp_entry_count": len(portraits),
        "hdgrp_sha256": sha256((root / "HDGRP.GRP").read_bytes()),
        "talk_14_page_count": len(dialogue_pages(decoded_talks[14])),
        "maximum_explicit_line_width": maximum_line_width,
        "maximum_explicit_line_talk": maximum_line_talk,
        "styles_present": sorted(styles_present),
        "cases": style_cases,
        "question_prompts": {
            "strings_hex": {key: value.hex() for key, value in question_texts.items()},
            "panel": [61, 40, 187, 27],
            "text_position": [71, 45],
            "colors": [0x05, 0x07],
            "frames": question_frames,
            "join_post_key": "bare_scene_present_before_branch",
            "accepted_key": "uppercase_Y_only",
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
        }

    title_lengths = [
        ranger[97_076 + scene * 52 + 2:97_076 + scene * 52 + 12].find(b"\0")
        for scene in range(84)
    ]
    assert all(length >= 0 for length in title_lengths)
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
        "metadata_title_max_bytes": max(title_lengths),
        "panel": [title_x, 10, title_width, 27],
        "text_position": [title_x + 10, 15],
        "colors": [5, 7],
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
            "state_write_vectors": state_write_vectors(scripts),
            "scene_animation_vectors": scene_animation_vectors(
                scene_maps, scene_events, sprites, scripts
            ),
            "scene_archive_state_vectors": scene_archive_state_vectors(
                ranger, scene_maps, scene_events, scripts
            ),
            "scene_loop_vectors": scene_loop_vectors(ranger, scripts),
            "dialogue_vectors": dialogue_vectors(
                root, frame, style_4_base_frame, palette, decoded_talks, scripts
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
