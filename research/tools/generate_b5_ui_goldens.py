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


def new_game_wait_screen(background: bytes, frames: list[bytes]) -> bytearray:
    pixels = bytearray(background)
    for y in range(135, 200):
        pixels[y * 320 : (y + 1) * 320] = bytes([0]) * 320
    draw_sprite(pixels, frames[8], 120, 160)
    return pixels


MAIN_MENU_LABELS = (
    bytes.fromhex("c2e5c0f8"),
    bytes.fromhex("b8d1ac72"),
    bytes.fromhex("aaabab7e"),
    bytes.fromhex("aaacba41"),
    bytes.fromhex("c2f7b6a4"),
    bytes.fromhex("a874b2ce"),
)
NAME_PROMPT = bytes.fromhex("bdd0bfe9a44aa96da65720203a")
ZHUYIN_PROMPT = bytes.fromhex("a15daa60adb5a15ea147")
ALNUM_PROMPT = bytes.fromhex("a15dad5ebcc6a15ea147")
NO_NAME_CANDIDATES = bytes.fromhex("b54ca6b9a672")
CANDIDATE_BOTH_PAGES = bytes.fromhex("a1d5a1fea1d6")
CANDIDATE_PREVIOUS_PAGE = bytes.fromhex("a1d5")
CANDIDATE_NEXT_PAGE = bytes.fromhex("a1d6")


def parse_palette(raw: bytes) -> list[tuple[int, int, int]]:
    assert len(raw) == 256 * 3
    return [tuple(raw[offset : offset + 3]) for offset in range(0, len(raw), 3)]


def rgb4_lookup(palette: list[tuple[int, int, int]]) -> list[int]:
    lookup = [0] * 4096
    for red in range(16):
        for green in range(16):
            for blue in range(16):
                best_distance = 30_000
                best_index = 0
                for index, color in enumerate(palette):
                    distance = (
                        (red * 4 + 2 - color[0]) ** 2
                        + (green * 4 + 2 - color[1]) ** 2
                        + (blue * 4 + 2 - color[2]) ** 2
                    )
                    if distance < best_distance:
                        best_distance = distance
                        best_index = index
                lookup[red * 256 + green * 16 + blue] = best_index
    return lookup


def draw_blended_rectangle(
    pixels: bytearray,
    palette: list[tuple[int, int, int]],
    lookup: list[int],
    x: int,
    y: int,
    width: int,
    height: int,
) -> None:
    source = palette[0]
    for target_y in range(y, y + height):
        for target_x in range(x, x + width):
            offset = target_y * 320 + target_x
            target = palette[pixels[offset]]
            red = 3 * source[0] // 32 + 5 * target[0] // 32
            green = 3 * source[1] // 32 + 5 * target[1] // 32
            blue = 3 * source[2] // 32 + 5 * target[2] // 32
            pixels[offset] = lookup[red * 256 + green * 16 + blue]


def fill_rectangle(
    pixels: bytearray, x: int, y: int, width: int, height: int, color: int
) -> None:
    for target_y in range(y, y + height):
        begin = target_y * 320 + x
        pixels[begin : begin + width] = bytes([color]) * width


def draw_rounded_panel(
    pixels: bytearray,
    palette: list[tuple[int, int, int]],
    lookup: list[int],
    x: int,
    y: int,
    width: int,
    height: int,
) -> None:
    assert width > 10 and height > 10
    blend_rectangles = (
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
    )
    for rectangle in blend_rectangles:
        draw_blended_rectangle(pixels, palette, lookup, *rectangle)
    border_rectangles = (
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
    )
    for rectangle in border_rectangles:
        fill_rectangle(pixels, *rectangle, 255)


def big5_glyph_index(code: int) -> int:
    lead = code >> 8
    trail = code & 0xFF
    assert lead >= 0xA1
    if 0x40 <= trail <= 0x7E:
        trail_index = trail - 0x40
    else:
        assert 0xA1 <= trail <= 0xFE
        trail_index = trail - 0x62
    return (lead - 0xA1) * 157 + trail_index


def draw_big5_text(
    pixels: bytearray,
    font: bytes,
    x: int,
    y: int,
    text: bytes,
    right_shadow: int,
    foreground: int,
) -> None:
    assert len(text) % 2 == 0
    for offset in range(0, len(text), 2):
        code = text[offset] << 8 | text[offset + 1]
        glyph_offset = big5_glyph_index(code) * 32
        glyph = font[glyph_offset : glyph_offset + 32]
        assert len(glyph) == 32
        for row in range(16):
            for byte_index in range(2):
                bits = glyph[row * 2 + byte_index]
                for bit_index in range(8):
                    if bits & (0x80 >> bit_index):
                        column = byte_index * 8 + bit_index
                        target = (y + row) * 320 + x + column
                        pixels[target] = foreground
                        pixels[target + 1] = right_shadow
        x += 16


def game_menu_screen(
    font: bytes,
    palette: list[tuple[int, int, int]],
    lookup: list[int],
    count: int,
    selection: int,
) -> bytearray:
    pixels = bytearray(
        ((index % 320) * 13 + (index // 320) * 7) & 0xFF
        for index in range(320 * 200)
    )
    draw_rounded_panel(pixels, palette, lookup, 20, 18, 42, 12 + 20 * count)
    for index in range(count):
        draw_big5_text(pixels, font, 24, 25 + 20 * index, MAIN_MENU_LABELS[index], 0x21, 0x23)
    draw_big5_text(
        pixels, font, 24, 25 + 20 * selection, MAIN_MENU_LABELS[selection], 0x63, 0x66
    )
    return pixels


def draw_ascii_glyph(
    pixels: bytearray,
    font: bytes,
    x: int,
    y: int,
    code: int,
    right_shadow: int,
    foreground: int,
) -> None:
    glyph = font[code * 16 : code * 16 + 16]
    assert len(glyph) == 16
    for row, bits in enumerate(glyph):
        for bit_index in range(8):
            if bits & (0x80 >> bit_index):
                target = (y + row) * 320 + x + bit_index
                pixels[target] = foreground
                pixels[target + 1] = right_shadow


def renderable_big5(font: bytes, lead: int, trail: int) -> bool:
    if lead < 0xA1:
        return False
    if 0x40 <= trail <= 0x7E:
        trail_index = trail - 0x40
    elif 0xA1 <= trail <= 0xFE:
        trail_index = trail - 0x62
    else:
        return False
    glyph_index = (lead - 0xA1) * 157 + trail_index
    return glyph_index * 32 + 32 <= len(font)


def draw_legacy_text(
    pixels: bytearray,
    ascii_font: bytes,
    big5_font: bytes,
    x: int,
    y: int,
    text: bytes,
    right_shadow: int,
    foreground: int,
) -> None:
    offset = 0
    while offset < len(text) and text[offset] != 0:
        if text[offset] > 0x7F:
            if offset + 1 >= len(text):
                break
            if renderable_big5(big5_font, text[offset], text[offset + 1]):
                draw_big5_text(
                    pixels,
                    big5_font,
                    x,
                    y,
                    text[offset : offset + 2],
                    right_shadow,
                    foreground,
                )
            x += 16
            offset += 2
        else:
            draw_ascii_glyph(
                pixels, ascii_font, x, y, text[offset], right_shadow, foreground
            )
            x += 8
            offset += 1


def zhuyin_label(kind: int, value: int) -> bytes:
    if kind == 1 and 1 <= value <= 21:
        return bytes((0xA3, (0x73 + value) if value <= 11 else (0x95 + value)))
    if kind == 2 and 1 <= value <= 3:
        return bytes((0xA3, 0xB7 + value))
    if kind == 3 and 1 <= value <= 13:
        return bytes((0xA3, 0xAA + value))
    if kind == 4 and 1 <= value <= 4:
        return bytes((0xA3, 0xBB if value == 1 else 0xBB + value))
    return b""


def lookup_name_candidates(
    cfont: bytes, initial: int, medial: int, final: int, tone: int
) -> tuple[int, list[bytes]]:
    table_index = initial * 5 + tone
    begin = struct.unpack_from("<H", cfont, table_index * 2)[0]
    end = struct.unpack_from("<H", cfont, (table_index + 1) * 2)[0]
    packed = (medial << 4) | final
    try:
        match = cfont.index(bytes((packed,)), begin, end)
    except ValueError:
        return 0, []
    candidate_begin = match + 1
    cursor = candidate_begin
    while cursor < len(cfont) and cfont[cursor] >= 0x40:
        cursor += 1
    count = (cursor - candidate_begin) // 2
    return candidate_begin, [
        cfont[candidate_begin + index * 2 : candidate_begin + index * 2 + 2]
        for index in range(count)
    ]


def name_entry_screen(
    title_big: bytes,
    ascii_font: bytes,
    big5_font: bytes,
    cfont: bytes,
    *,
    mode: int = 0,
    name: bytes = b"",
    display_name: bytes | None = None,
    composition: tuple[int, int, int, int] = (0, 0, 0, 0),
    candidates: list[bytes] | None = None,
    candidate_begin: int = 0,
    candidate_page: int = 0,
    no_candidates: bool = False,
    cursor_color: int = 7,
    accepted: bool = False,
) -> bytearray:
    pixels = bytearray(title_big)
    fill_rectangle(pixels, 0, 140, 320, 60, 0)
    active_candidates = candidates or []
    draw_legacy_text(pixels, ascii_font, big5_font, 48, 141, NAME_PROMPT, 0x15, 0x17)
    draw_legacy_text(
        pixels,
        ascii_font,
        big5_font,
        3,
        161,
        ZHUYIN_PROMPT if mode == 0 else ALNUM_PROMPT,
        0x19 if active_candidates else 0x15,
        0x17,
    )
    draw_legacy_text(
        pixels,
        ascii_font,
        big5_font,
        158,
        141,
        name if display_name is None else display_name,
        0x03,
        0x05,
    )
    if not accepted and len(name) < 6:
        fill_rectangle(pixels, 158 + len(name) * 8, 156, 8, 1, cursor_color)

    if active_candidates:
        for visible_index in range(8):
            global_index = candidate_page * 8 + visible_index
            if global_index >= 0:
                if global_index >= len(active_candidates):
                    break
                candidate = active_candidates[global_index]
            else:
                offset = candidate_begin + global_index * 2
                if offset < 0 or offset + 1 >= len(cfont):
                    break
                candidate = cfont[offset : offset + 2]
            draw_legacy_text(
                pixels,
                ascii_font,
                big5_font,
                30 * (visible_index + 1),
                180,
                bytes((ord("1") + visible_index,)) + candidate,
                0x19,
                0x17,
            )
        has_next = candidate_page < 0 or (candidate_page + 1) * 8 < len(active_candidates)
        if candidate_page == 0 and has_next:
            draw_legacy_text(
                pixels,
                ascii_font,
                big5_font,
                300,
                180,
                CANDIDATE_NEXT_PAGE,
                0x19,
                0x17,
            )
        elif candidate_page != 0 and has_next:
            draw_legacy_text(
                pixels,
                ascii_font,
                big5_font,
                270,
                180,
                CANDIDATE_BOTH_PAGES,
                0x19,
                0x17,
            )
        elif candidate_page != 0:
            draw_legacy_text(
                pixels,
                ascii_font,
                big5_font,
                300,
                180,
                CANDIDATE_PREVIOUS_PAGE,
                0x19,
                0x17,
            )
        return pixels

    for index, value in enumerate(composition):
        label = zhuyin_label(index + 1, value)
        if label:
            draw_legacy_text(
                pixels,
                ascii_font,
                big5_font,
                100 + index * 20,
                161,
                label,
                0x15,
                0x17,
            )
    if no_candidates:
        draw_legacy_text(
            pixels,
            ascii_font,
            big5_font,
            240,
            161,
            NO_NAME_CANDIDATES,
            0x05,
            0x07,
        )
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
    ascii_font = (args.data_root / "FONT.X16").read_bytes()
    big5_font = (args.data_root / "FONT.C16").read_bytes()
    cfont = (args.data_root / "CFONT").read_bytes()
    ranger = (args.data_root / "RANGER.GRP").read_bytes()
    assert len(title_big) == 320 * 200
    assert len(palette) == 256 * 3
    assert len(ascii_font) == 128 * 16
    assert len(big5_font) % 32 == 0
    assert len(cfont) == 29674
    assert len(ranger) == 114242
    frames = archive_entries(title_index, title_group)
    assert len(frames) == 9
    protagonist = ranger[836 : 836 + 182]
    (level,) = struct.unpack_from("<h", protagonist, 15 * 2)
    parsed_palette = parse_palette(palette)
    panel_lookup = rgb4_lookup(parsed_palette)
    candidate_begin, name_candidates = lookup_name_candidates(cfont, 12, 1, 10, 0)
    assert len(name_candidates) == 20
    no_match_begin, no_match_candidates = lookup_name_candidates(cfont, 1, 0, 0, 1)
    assert no_match_begin == 0 and not no_match_candidates

    output = {
        "oracle": "independent Python little-endian/RLE implementation",
        "assets": {
            "title.idx": {"bytes": len(title_index), "sha256": sha256(title_index)},
            "title.grp": {"bytes": len(title_group), "sha256": sha256(title_group)},
            "title.big": {"bytes": len(title_big), "sha256": sha256(title_big)},
            "mmap.col": {"bytes": len(palette), "sha256": sha256(palette)},
            "FONT.X16": {"bytes": len(ascii_font), "sha256": sha256(ascii_font)},
            "FONT.C16": {"bytes": len(big5_font), "sha256": sha256(big5_font)},
            "CFONT": {"bytes": len(cfont), "sha256": sha256(cfont)},
        },
        "title_pixels_fnv1a64": {
            "main_selection_0": fnv1a64(title_screen(title_big, frames, 0)),
            "main_selection_1": fnv1a64(title_screen(title_big, frames, 1)),
            "main_selection_2": fnv1a64(title_screen(title_big, frames, 2)),
            "load_slot_0": fnv1a64(title_load_screen(title_big, frames, 0)),
            "load_slot_1": fnv1a64(title_load_screen(title_big, frames, 1)),
            "load_slot_2": fnv1a64(title_load_screen(title_big, frames, 2)),
            "please_wait": fnv1a64(title_wait_screen(title_big, frames)),
            "new_game_wait": fnv1a64(new_game_wait_screen(title_big, frames)),
        },
        "game_menu_pixels_fnv1a64": {
            "source": "independent synthetic indexed background plus current mmap.col/FONT.C16",
            "world_selection_0": fnv1a64(
                game_menu_screen(big5_font, parsed_palette, panel_lookup, 6, 0)
            ),
            "scene_selection_3": fnv1a64(
                game_menu_screen(big5_font, parsed_palette, panel_lookup, 4, 3)
            ),
        },
        "name_entry_pixels_fnv1a64": {
            "source": "independent machine-coordinate FONT.X16/FONT.C16/CFONT renderer with bounded invalid-glyph platform adaptation",
            "initial_zhuyin_cursor_7": fnv1a64(
                name_entry_screen(title_big, ascii_font, big5_font, cfont)
            ),
            "composition_rup_cursor_7": fnv1a64(
                name_entry_screen(
                    title_big,
                    ascii_font,
                    big5_font,
                    cfont,
                    composition=(12, 1, 10, 0),
                )
            ),
            "candidates_rup_page_0_cursor_7": fnv1a64(
                name_entry_screen(
                    title_big,
                    ascii_font,
                    big5_font,
                    cfont,
                    candidates=name_candidates,
                    candidate_begin=candidate_begin,
                )
            ),
            "candidates_rup_page_minus_1_cursor_7": fnv1a64(
                name_entry_screen(
                    title_big,
                    ascii_font,
                    big5_font,
                    cfont,
                    candidates=name_candidates,
                    candidate_begin=candidate_begin,
                    candidate_page=-1,
                )
            ),
            "no_candidates_initial_1_tone_1_cursor_7": fnv1a64(
                name_entry_screen(
                    title_big,
                    ascii_font,
                    big5_font,
                    cfont,
                    composition=(1, 0, 0, 1),
                    no_candidates=True,
                )
            ),
            "alphanumeric_a_cursor_7": fnv1a64(
                name_entry_screen(
                    title_big,
                    ascii_font,
                    big5_font,
                    cfont,
                    mode=1,
                    name=b"A",
                )
            ),
            "alphanumeric_single_backspace_ghost_cursor_7": fnv1a64(
                name_entry_screen(
                    title_big,
                    ascii_font,
                    big5_font,
                    cfont,
                    mode=1,
                    name=b"",
                    display_name=b"A",
                )
            ),
            "accepted_alphanumeric_a": fnv1a64(
                name_entry_screen(
                    title_big,
                    ascii_font,
                    big5_font,
                    cfont,
                    mode=1,
                    name=b"A",
                    accepted=True,
                )
            ),
        },
        "runtime_ui_regression_fnv1a64": {
            "source": "C++ framebuffer regression lock using baseline seed 0 and protagonist name A",
            "status_selector": "85fc6aad255a1c1b",
            "status_page_0": "76cb48686954de6d",
            "status_page_1": "a51f2adebe80d31f",
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
