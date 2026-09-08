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
    saw_positive_end = False
    zero_tail = False
    for (end,) in struct.iter_unpack("<I", index_bytes):
        if zero_tail:
            assert end == 0
            continue
        if end == 0 and saw_positive_end:
            zero_tail = True
            continue
        assert begin <= end <= len(group_bytes)
        result.append(group_bytes[begin:end])
        begin = end
        saw_positive_end = saw_positive_end or end != 0
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


def outline_rectangle(
    pixels: bytearray, x: int, y: int, width: int, height: int, color: int
) -> None:
    fill_rectangle(pixels, x, y, width, 1, color)
    fill_rectangle(pixels, x, y, 1, height, color)
    fill_rectangle(pixels, x + width - 1, y, 1, height, color)
    fill_rectangle(pixels, x, y + height - 1, width, 1, color)


def signed_word_at(data: bytes | bytearray, offset: int) -> int:
    return struct.unpack_from("<h", data, offset)[0]


def fixed_c_string(data: bytes | bytearray, offset: int, size: int) -> bytes:
    return bytes(data[offset : offset + size]).split(b"\0", 1)[0]


def game_menu_items_screen(
    ascii_font: bytes,
    big5_font: bytes,
    palette: list[tuple[int, int, int]],
    lookup: list[int],
    item_frames: list[bytes],
    ranger: bytes | bytearray,
    inventory_slots: list[int],
    page: int = 0,
    row: int = 0,
    column: int = 0,
    context: str = "world",
) -> bytearray:
    assert len(ranger) == 114242
    assert len(inventory_slots) == 200
    pixels = bytearray(
        ((index % 320) * 13 + (index // 320) * 7) & 0xFF
        for index in range(320 * 200)
    )
    for panel in ((45, 2, 230, 23), (45, 27, 230, 23), (45, 52, 230, 145)):
        draw_rounded_panel(pixels, palette, lookup, *panel)

    metric = 200
    for slot in range(200):
        if signed_word_at(ranger, 2 * (18 + 2 * slot)) == -1:
            metric = slot + 1
            break
    if metric > 5 * (page + 3):
        for rectangle in (
            (267, 175, 2, 1),
            (266, 174, 4, 1),
            (265, 173, 6, 1),
            (264, 172, 8, 1),
            (266, 161, 4, 11),
        ):
            fill_rectangle(pixels, *rectangle, 99)
    if metric > 15 and page > 0:
        for rectangle in (
            (267, 72, 2, 1),
            (266, 73, 4, 1),
            (265, 74, 6, 1),
            (264, 75, 8, 1),
            (266, 76, 4, 11),
        ):
            fill_rectangle(pixels, *rectangle, 99)

    for grid_row in range(3):
        for grid_column in range(5):
            x = 55 + 42 * grid_column
            y = 62 + 42 * grid_row
            outline_rectangle(pixels, x, y, 40, 40, 0)
            list_index = 5 * (page + grid_row) + grid_column
            inventory_slot = inventory_slots[list_index]
            if not 0 <= inventory_slot < 200:
                continue
            item_id = signed_word_at(ranger, 2 * (18 + 2 * inventory_slot))
            if 0 <= item_id < len(item_frames):
                draw_sprite(pixels, item_frames[item_id], x, y)

    outline_rectangle(pixels, 55 + 42 * column, 62 + 42 * row, 40, 40, 255)
    selected_index = 5 * (page + row) + column
    selected_slot = inventory_slots[selected_index]
    if not 0 <= selected_slot < 200:
        return pixels
    item_id = signed_word_at(ranger, 2 * (18 + 2 * selected_slot))
    if not 0 <= item_id < 200:
        return pixels
    item_offset = 59076 + item_id * 190
    selector = signed_word_at(ranger, item_offset + 80)
    name = fixed_c_string(ranger, item_offset + (2 if selector == 0 else 22), 20)
    item_type = signed_word_at(ranger, item_offset + 82)
    user = signed_word_at(ranger, item_offset + 76)
    name_center = 140 if item_type in (1, 2) and user > 0 else 160
    draw_legacy_text(
        pixels, ascii_font, big5_font, name_center - 4 * len(name), 5, name, 5, 7
    )

    if signed_word_at(ranger, item_offset) == 182:
        coordinate_words = (
            (signed_word_at(ranger, 8), signed_word_at(ranger, 10))
            if context == "scene"
            else (signed_word_at(ranger, 4), signed_word_at(ranger, 6))
        )
        coordinate_text = (
            bytes.fromhex("a448a15d")
            + f"{coordinate_words[0]:3d}".encode("ascii")
            + bytes.fromhex("a141")
            + f"{coordinate_words[1]:3d}".encode("ascii")
            + bytes.fromhex("a15eb2eea15d")
            + f"{signed_word_at(ranger, 14):3d}".encode("ascii")
            + bytes.fromhex("a141")
            + f"{signed_word_at(ranger, 16):3d}".encode("ascii")
            + bytes.fromhex("a15e")
        )
        draw_legacy_text(
            pixels, ascii_font, big5_font, 48, 30, coordinate_text, 0x21, 0x23
        )
    else:
        description = fixed_c_string(ranger, item_offset + 42, 30)
        draw_legacy_text(
            pixels,
            ascii_font,
            big5_font,
            160 - 4 * len(description),
            30,
            description,
            0x21,
            0x23,
        )

    if user >= 0:
        role_name = fixed_c_string(ranger, 836 + user * 182 + 8, 10)
        draw_legacy_text(
            pixels,
            ascii_font,
            big5_font,
            205,
            5,
            b"(" + role_name + b")",
            0x21,
            0x23,
        )
    quantity = signed_word_at(ranger, 2 * (19 + 2 * selected_slot))
    if quantity > 1:
        draw_legacy_text(pixels, ascii_font, big5_font, 215, 5, b"X", 0x21, 0x23)
        draw_legacy_text(
            pixels,
            ascii_font,
            big5_font,
            235,
            5,
            f"{quantity:2d}".encode("ascii"),
            0x63,
            0x66,
        )
    return pixels


def item_draw_pixel_vectors(
    ascii_font: bytes,
    big5_font: bytes,
    palette: list[tuple[int, int, int]],
    lookup: list[int],
    item_frames: list[bytes],
    ranger: bytes,
) -> dict[str, str]:
    def set_word(data: bytearray, byte_offset: int, value: int) -> None:
        struct.pack_into("<h", data, byte_offset, value)

    def clear_inventory(data: bytearray) -> None:
        for slot in range(200):
            set_word(data, 2 * (18 + 2 * slot), -1)
            set_word(data, 2 * (19 + 2 * slot), 0)

    def set_inventory(data: bytearray, slot: int, item_id: int, quantity: int) -> None:
        set_word(data, 2 * (18 + 2 * slot), item_id)
        set_word(data, 2 * (19 + 2 * slot), quantity)

    def set_string(data: bytearray, byte_offset: int, size: int, value: bytes) -> None:
        assert len(value) < size
        data[byte_offset : byte_offset + size] = value + bytes(size - len(value))

    def render(data: bytearray, slots: list[int], context: str = "world") -> str:
        slots = slots + [-1] * (200 - len(slots))
        return fnv1a64(
            game_menu_items_screen(
                ascii_font,
                big5_font,
                palette,
                lookup,
                item_frames,
                data,
                slots,
                context=context,
            )
        )

    fifteen = bytearray(ranger)
    clear_inventory(fifteen)
    item_ids = (0, 2, 10, 21)
    for slot in range(15):
        set_inventory(fifteen, slot, item_ids[slot % len(item_ids)], 1)

    coordinate = bytearray(ranger)
    clear_inventory(coordinate)
    set_inventory(coordinate, 0, 182, 2)
    for word, value in ((2, -7), (3, 23), (7, -4), (8, 99)):
        set_word(coordinate, 2 * word, value)

    alternate = bytearray(ranger)
    clear_inventory(alternate)
    set_inventory(alternate, 0, 0, 1)
    item_offset = 59076
    set_string(alternate, item_offset + 2, 20, b"A")
    set_string(alternate, item_offset + 22, 20, b"BC")
    set_string(alternate, item_offset + 42, 30, b"DESC")
    for offset, value in ((0, 0), (76, 1), (80, 1), (82, 1)):
        set_word(alternate, item_offset + offset, value)
    set_string(alternate, 836 + 182 + 8, 10, b"R")

    strict_user = bytearray(alternate)
    set_string(strict_user, item_offset + 2, 20, b"A")
    for offset, value in ((76, 0), (80, 0), (82, 1)):
        set_word(strict_user, item_offset + offset, value)
    set_string(strict_user, 836 + 8, 10, b"Q")

    empty = bytearray(ranger)
    clear_inventory(empty)
    return {
        "fifteen_items_page0_draws_down": render(fifteen, list(range(15))),
        "item182_world_coordinates_quantity2": render(coordinate, [0]),
        "alternate_name_type1_positive_role": render(alternate, [0]),
        "primary_name_type1_role_zero": render(strict_user, [0]),
        "invalid_selection_border_only": render(empty, []),
    }


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


def game_menu_item_entry_machine(z_dat: bytes) -> dict[str, object]:
    loaded_start = 0x2A0D9
    raw = z_dat[0x23AD9:0x23B0F]
    assert len(raw) == 54
    assert raw.hex() == (
        "6810000000e83b4c0100c6056b1b0300006a00e81e00000083c404"
        "6a006a006a00e88700000083c40c6aff6a00e86107000083c408c3"
    )

    call_offsets = (5, 19, 33, 45)
    call_targets = []
    for offset in call_offsets:
        assert raw[offset] == 0xE8
        (displacement,) = struct.unpack_from("<i", raw, offset + 1)
        call_targets.append(loaded_start + offset + 5 + displacement)
    assert call_targets == [0x3ED1E, 0x2A10F, 0x2A186, 0x2A86C]

    (raw_last_key_address,) = struct.unpack_from("<I", raw, 12)
    assert raw_last_key_address == 0x31B6B
    loaded = bytearray(raw)
    struct.pack_into("<I", loaded, 12, raw_last_key_address + 0x20000)
    assert sha256(raw) == "1fd36ede5cb17507e24e83bc883074f6205a911ee34359aad6514b6280a2f383"
    assert sha256(loaded) == "ed5269e0ae12171d13068702b67735cd621e9984a90560a9da73e0bf1949bc2e"

    caller = z_dat[0x1ACC0:0x1AE96]
    assert len(caller) == 470
    assert sha256(caller) == "d50c9629b28d74a41330b5641d7fc86755c722a46768c66a97fbe078f5d2cfb0"
    for offset, target in (
        (0x128, 0x31B6B),
        (0x12F, 0x31B7A),
        (0x136, 0x31B8D),
        (0x13D, 0x31C03),
    ):
        assert caller[offset : offset + 2] == b"\xC6\x05"
        assert struct.unpack_from("<I", caller, offset + 2)[0] == target
        assert caller[offset + 6] == 0
    caller_call_offset = 0x21433 - 0x212C0
    assert caller[caller_call_offset] == 0xE8
    (caller_displacement,) = struct.unpack_from("<i", caller, caller_call_offset + 1)
    assert 0x21433 + 5 + caller_displacement == loaded_start

    contract = {
        "operation_order": [
            "stack_probe_16",
            "clear_last_key",
            "reset_items_0",
            "draw_items_0_0_0",
            "select_items_0_minus_1",
            "return_selector_eax",
        ],
        "caller": {
            "address": "0x212c0:0x21433",
            "selection": 2,
            "clears_before_call": [
                "last_key",
                "enter_state",
                "space_state",
                "insert_state",
            ],
            "return_value": "ignored_then_jump_main_menu_loop",
        },
        "input_owner": {
            "last_key_write": "unconditional_zero_before_delegated_calls",
            "internal_last_key_reads": 0,
            "delegated_ui_owners": ["0x2a10f", "0x2a186", "0x2a86c"],
        },
    }
    contract_sha256 = sha256(
        json.dumps(contract, sort_keys=True, separators=(",", ":")).encode("utf-8")
    )
    return {
        "raw_range": "Z.DAT[0x23ad9:0x23b0f]",
        "loaded_range": "0x2a0d9..0x2a10f",
        "size_bytes": len(raw),
        "instruction_count": 16,
        "basic_block_count": 1,
        "branch_count": 0,
        "call_offsets": [f"0x{loaded_start + offset:x}" for offset in call_offsets],
        "call_targets": [f"0x{target:x}" for target in call_targets],
        "fixups": ["0x2a0e5"],
        "relocation_delta": "0x20000",
        "raw_sha256": sha256(raw),
        "loaded_sha256": sha256(loaded),
        "normalized_loaded_equals_raw": True,
        "entry_xref": "sub_212c0:0x21433",
        "external_internal_entries": [],
        "local_return": "0x2a10e",
        "caller_raw_sha256": sha256(caller),
        "contract": contract,
        "contract_sha256": contract_sha256,
    }


def game_menu_item_reset_machine(z_dat: bytes, ranger: bytes) -> dict[str, object]:
    loaded_start = 0x2A10F
    raw = z_dat[0x23B0F:0x23B86]
    assert len(raw) == 119
    assert raw.hex() == (
        "680c000000e8054c010053568b74240cb990010000baffffffffb8c0270b00"
        "e85d4d010031db31d289d0c1e0026683b82cfe0600007c340fbf802cfe06"
        "0069c0be0000000fbf889627080039f1741385f6740f6683b89627080003"
        "750e83fe0475096689145dc0270b00434281fac80000007cb45e5bc3"
    )
    assert sha256(raw) == "ed0ea131ea4ad455f838bd2a0886c36f9570633e4bdd512caa84612c10643cae"

    fixup_offsets = (0x1B, 0x30, 0x3A, 0x47, 0x56, 0x66)
    loaded = bytearray(raw)
    fixups = []
    for offset in fixup_offsets:
        (raw_address,) = struct.unpack_from("<I", raw, offset)
        struct.pack_into("<I", loaded, offset, raw_address + 0x20000)
        fixups.append(
            {
                "site": f"0x{loaded_start + offset:x}",
                "raw": f"0x{raw_address:x}",
                "loaded": f"0x{raw_address + 0x20000:x}",
            }
        )
    assert sha256(loaded) == "c14a05e9ecf2998e8362891eb4edb8a0788be7ec2819bcd744c0e93e7a7cacd4"

    calls = ((0x05, 0x3ED1E), (0x1F, 0x3EE90))
    for offset, target in calls:
        assert raw[offset] == 0xE8
        (displacement,) = struct.unpack_from("<i", raw, offset + 1)
        assert loaded_start + offset + 5 + displacement == target

    entry_caller = z_dat[0x23AD9:0x23B0F]
    battle_caller = z_dat[0x33C9C:0x33D0B]
    assert sha256(entry_caller) == "1fd36ede5cb17507e24e83bc883074f6205a911ee34359aad6514b6280a2f383"
    assert sha256(battle_caller) == "eb358abcceab4d2b8c735da754e73ff00bc611237a8cf23087e2475bfffad5c7"
    assert entry_caller[0x11:0x18] == bytes.fromhex("6a00e81e000000")
    assert battle_caller[0x0E:0x15] == bytes.fromhex("6a04e85efefeff")

    fill_callee = z_dat[0x38890:0x388C1]
    assert len(fill_callee) == 49
    assert sha256(fill_callee) == "8bd4126f640ec6f565a8e4069e0dbd027d32ee12da3ec9d6ff52ce2151aa7365"

    def signed_word(value: int) -> int:
        value &= 0xFFFF
        return value - 0x10000 if value >= 0x8000 else value

    def machine_filter(
        filter_value: int,
        inventory: list[tuple[int, int]],
        item_types: dict[int, int],
    ) -> list[int]:
        assert len(inventory) == 200
        output = [-1] * 200
        output_count = 0
        for slot, (item_id, _quantity) in enumerate(inventory):
            item_id = signed_word(item_id)
            if item_id < 0:
                continue
            item_type = signed_word(item_types[item_id])
            if (
                item_type == filter_value
                or filter_value == 0
                or (item_type == 3 and filter_value == 4)
            ):
                output[output_count] = slot
                output_count += 1
        return output

    header = struct.unpack("<418h", ranger[:836])
    baseline_inventory = [
        (header[18 + slot * 2], header[19 + slot * 2]) for slot in range(200)
    ]
    baseline_types = {
        item_id: struct.unpack_from("<h", ranger, 59076 + item_id * 190 + 82)[0]
        for item_id, _quantity in baseline_inventory
        if item_id >= 0
    }
    baseline_zero = machine_filter(0, baseline_inventory, baseline_types)
    baseline_four = machine_filter(4, baseline_inventory, baseline_types)
    assert baseline_zero[:5] == [0, 1, 2, 3, -1]
    assert baseline_four[:5] == [0, 1, 2, 3, -1]
    assert baseline_four[4:] == [-1] * 196

    synthetic_inventory = [(-1, 0)] * 200
    synthetic_types = {10: 4, 11: 3, 12: 2, 13: 4, 14: -1, 15: 0, 16: 3}
    for slot, entry in {
        0: (10, 1),
        1: (11, 0),
        2: (12, 0),
        3: (-32768, 9),
        4: (13, -32768),
        5: (10, 7),
        6: (14, 1),
        7: (15, 1),
        8: (16, -1),
    }.items():
        synthetic_inventory[slot] = entry
    synthetic_zero = machine_filter(0, synthetic_inventory, synthetic_types)
    synthetic_four = machine_filter(4, synthetic_inventory, synthetic_types)
    assert synthetic_zero[:9] == [0, 1, 2, 4, 5, 6, 7, 8, -1]
    assert synthetic_four[:6] == [0, 1, 4, 5, 8, -1]

    full_inventory = [
        (slot % 17, 0 if slot % 2 else -32768) for slot in range(200)
    ]
    full_types = {item_id: 4 for item_id in range(17)}
    full_output = machine_filter(4, full_inventory, full_types)
    assert full_output == list(range(200))

    contract = {
        "input": "full 32-bit filter; actual callers pass 0 and 4",
        "reset": "fill 400 output bytes with 0xff, yielding 200 signed-word -1 sentinels",
        "scan": "inventory slots 0..199 in ascending order",
        "item_id": "signed word; negative values skipped; quantity word never read",
        "item_type": "signed word at item record byte +82 (word 41), stride 190",
        "include": "item_type == filter or filter == 0 or (item_type == 3 and filter == 4)",
        "output": "accepted inventory slot indices as signed words in stable prefix order",
        "return": "unstable EAX ignored/overwritten by both callers",
        "owner_boundary": "stack probe, fill callee, both callers, draw and selector remain independent",
    }
    contract_sha256 = sha256(
        json.dumps(contract, sort_keys=True, separators=(",", ":")).encode("utf-8")
    )
    assert contract_sha256 == "b821c125ab79c95c0cd6e9074e6a6b9d4c67cef202e40573fe402a79f24fdb6d"
    vectors = {
        "baseline_filter_0": baseline_zero,
        "baseline_filter_4": baseline_four,
        "synthetic_filter_0": synthetic_zero,
        "synthetic_filter_4": synthetic_four,
        "full_200_filter_4": full_output,
    }
    vectors_sha256 = sha256(
        json.dumps(vectors, sort_keys=True, separators=(",", ":")).encode("utf-8")
    )
    assert vectors_sha256 == "3fd01787c6d22cd4414b79ca7a947ee75769f49e12a01ad13703a56877fcb76b"

    return {
        "raw_range": "Z.DAT[0x23b0f:0x23b86]",
        "loaded_range": "0x2a10f..0x2a186",
        "size_bytes": len(raw),
        "instruction_count": 34,
        "basic_block_count": 9,
        "conditional_branch_count": 6,
        "unconditional_jump_count": 0,
        "calls": [
            {"site": f"0x{loaded_start + offset:x}", "target": f"0x{target:x}"}
            for offset, target in calls
        ],
        "fixups": fixups,
        "relocation_delta": "0x20000",
        "raw_sha256": sha256(raw),
        "loaded_sha256": sha256(loaded),
        "normalized_loaded_equals_raw": True,
        "entry_xrefs": ["sub_2a0d9:0x2a0ec", "sub_3a29c:0x3a2ac"],
        "external_internal_entries": [],
        "local_return": "0x2a185",
        "caller_filters": {"sub_2a0d9": 0, "sub_3a29c": 4},
        "caller_raw_sha256": {
            "sub_2a0d9": sha256(entry_caller),
            "sub_3a29c": sha256(battle_caller),
        },
        "fill_callee": {
            "range": "0x3ee90..0x3eec1",
            "size_bytes": len(fill_callee),
            "instruction_count": 24,
            "raw_sha256": sha256(fill_callee),
            "call_registers": {"eax": "0xd27c0", "ecx": 400, "edx": "0xffffffff"},
        },
        "contract": contract,
        "contract_sha256": contract_sha256,
        "vectors": vectors,
        "vectors_sha256": vectors_sha256,
    }


def game_menu_item_draw_machine(z_dat: bytes, ranger: bytes) -> dict[str, object]:
    loaded_start = 0x2A186
    raw = z_dat[0x23B86:0x2414C]
    assert len(raw) == 1478
    assert sha256(raw) == "4343589cfb549849f9e6215c269b3ea45804d94bf6918068a3fc48ec4f5dcec0"
    fixup_sites = (
        0x2A19C, 0x2A1C7, 0x2A1D8, 0x2A1E9, 0x2A1F8, 0x2A219, 0x2A23A,
        0x2A288, 0x2A2BD, 0x2A2C7, 0x2A2D1, 0x2A2D9, 0x2A2DE, 0x2A2E5,
        0x2A2EE, 0x2A310, 0x2A34F, 0x2A359, 0x2A367, 0x2A375, 0x2A380,
        0x2A389, 0x2A399, 0x2A39E, 0x2A3AD, 0x2A3B5, 0x2A3C0, 0x2A3D5,
        0x2A3DD, 0x2A3EA, 0x2A3F4, 0x2A412, 0x2A41A, 0x2A427, 0x2A436,
        0x2A43B, 0x2A459, 0x2A45E, 0x2A473, 0x2A478, 0x2A487, 0x2A48F,
        0x2A49A, 0x2A4AF, 0x2A4B7, 0x2A4C4, 0x2A4CE, 0x2A4EC, 0x2A4F4,
        0x2A501, 0x2A510, 0x2A515, 0x2A52D, 0x2A532, 0x2A565, 0x2A56D,
        0x2A57A, 0x2A585, 0x2A58D, 0x2A594, 0x2A59A, 0x2A59F, 0x2A5A4,
        0x2A5B8, 0x2A5BD, 0x2A5C8, 0x2A5D1, 0x2A5D6, 0x2A5E5, 0x2A5ED,
        0x2A5F8, 0x2A610, 0x2A615, 0x2A645, 0x2A64D, 0x2A65A, 0x2A664,
        0x2A66F, 0x2A678, 0x2A67D, 0x2A691, 0x2A696, 0x2A6C3, 0x2A6CB,
        0x2A6DA, 0x2A6DF, 0x2A6F5, 0x2A6FD, 0x2A703, 0x2A708, 0x2A71C,
        0x2A721, 0x2A736, 0x2A73B,
    )
    assert len(fixup_sites) == 94
    assert sha256(",".join(f"{site:x}" for site in fixup_sites).encode()) == (
        "4b666d15915fc77743e3e31c6156ea131c385b4e0481b9534033256c3d87148d"
    )
    loaded = bytearray(raw)
    for site in fixup_sites:
        offset = site - loaded_start
        raw_address = struct.unpack_from("<I", raw, offset)[0]
        struct.pack_into("<I", loaded, offset, raw_address + 0x20000)
    assert sha256(loaded) == "c782243a08c8bd7711ba165b88639e11dfa721ecd61fb46c33cf11e4b42bf0ce"
    normalized = bytearray(loaded)
    for site in fixup_sites:
        offset = site - loaded_start
        loaded_address = struct.unpack_from("<I", normalized, offset)[0]
        struct.pack_into("<I", normalized, offset, loaded_address - 0x20000)
    assert bytes(normalized) == raw

    calls = {
        "sub_3ED1E": [0x2A18B],
        "sub_2558B": [0x2A1CE],
        "sub_29D2D": [0x2A1DF],
        "sub_3AA85": [0x2A1F0],
        "sub_2CEBF": [0x2A20E, 0x2A22F, 0x2A253],
        "sub_2A74C": [0x2A26D],
        "sub_2A7E8": [0x2A27E],
        "sub_2D501": [0x2A2A0, 0x2A32F],
        "sub_3D643": [0x2A2F7],
        "sub_3EF4A": [0x2A3A2, 0x2A47C, 0x2A5A8, 0x2A5DA, 0x2A681, 0x2A70C],
        "sub_3EF7D": [0x2A3C8, 0x2A4A2, 0x2A600],
        "sub_3D832": [0x2A543, 0x2A626, 0x2A6A1, 0x2A6EA, 0x2A72C],
        "sub_3D6D1": [0x2A73F],
    }
    for sites in calls.values():
        for site in sites:
            offset = site - loaded_start
            assert raw[offset] == 0xE8
    assert sum(map(len, calls.values())) == 27

    header = struct.unpack("<418h", ranger[:836])
    inventory = [(header[18 + 2 * slot], header[19 + 2 * slot]) for slot in range(200)]
    baseline_ids = [item_id for item_id, _ in inventory if item_id >= 0]
    assert baseline_ids == [0, 2, 10, 21]
    baseline_item_types = {
        item_id: struct.unpack_from("<h", ranger, 59076 + item_id * 190 + 82)[0]
        for item_id in baseline_ids
    }
    assert baseline_item_types == {0: 3, 2: 3, 10: 3, 21: 3}

    contract = {
        "arguments": "cdecl (page,row,column); wrappers pass 0,0,0",
        "background": "context 0 world, 1 scene, 2 battle, otherwise no redraw",
        "panels": [[45, 2, 230, 23], [45, 27, 230, 23], [45, 52, 230, 145]],
        "metric": "first exact -1 inventory slot plus one, else 200",
        "arrows": "down iff metric > 5*(page+3); up iff metric > 15 and page > 0",
        "grid": "row-major 3x5; origin 55,62; step 42; border 40x40 color 0",
        "mapping": "word_D27C0 logical list position -> signed real inventory slot",
        "icon": "frame = 2*item_id + word_54508 at cell origin",
        "selection": "selected border color 255 before selected-entry validation",
        "record": "190-byte item stride; id+0 names+2/+22 description+42 role+76 selector+80 type+82",
        "name": "selector chooses name; x 140 or 160 minus 4*byte_length; y 5",
        "special": "id 182 formats 人（%3d，%3d）船（%3d，%3d） at 48,30",
        "description": "ordinary +42 string centered at x=160-4*byte_length,y=30",
        "role": "nonnegative role index formats 182-byte-stride role name +8 as (%s) at 205,5",
        "quantity": "signed >1 only: X at 215,5 and %2d at 235,5",
        "present": "exactly once on every path after rendering",
        "return": "unstable EAX ignored by all three callers",
        "boundary": "arrow/background/primitive/string/present callees, reset, selector and callers remain independent",
    }
    contract_sha256 = sha256(
        json.dumps(
            contract, ensure_ascii=False, sort_keys=True, separators=(",", ":")
        ).encode("utf-8")
    )
    assert contract_sha256 == "e2512968dbf346d44b6856095c8f21ea87d0af23cd5626d8fdc40d629fc7f4b1"
    vectors = {
        "metric_exact_15_items": {
            "inventory_prefix": [0] * 15 + [-1],
            "metric": 16,
            "page": 0,
            "down": True,
            "up": False,
        },
        "metric_other_negative_does_not_stop": {
            "inventory_prefix": [0, -2, 1, -1],
            "metric": 4,
        },
        "metric_full_table": {"metric": 200},
        "grid_last_cell_page_2": {
            "page": 2,
            "row": 2,
            "column": 4,
            "logical_index": 24,
            "origin": [223, 146],
        },
        "selected_invalid": {
            "border": [55, 62, 40, 40, 255],
            "details": [],
            "present": 1,
        },
        "detail_cases": {
            "alternate_type1_role1_name_len2_x": 132,
            "primary_type1_role0_name_len1_x": 156,
            "item182_description": [48, 30],
            "quantity1_draws": False,
            "quantity2_draws": True,
        },
    }
    vectors_sha256 = sha256(
        json.dumps(vectors, sort_keys=True, separators=(",", ":")).encode("utf-8")
    )
    return {
        "raw_range": "Z.DAT[0x23b86:0x2414c]",
        "loaded_range": "0x2a186..0x2a74c",
        "size_bytes": len(raw),
        "instruction_count": 379,
        "basic_block_count": 47,
        "conditional_branch_count": 25,
        "unconditional_jump_count": 6,
        "fixup_count": len(fixup_sites),
        "fixup_sites_sha256": sha256(",".join(f"{site:x}" for site in fixup_sites).encode()),
        "raw_sha256": sha256(raw),
        "loaded_sha256": sha256(loaded),
        "normalized_loaded_equals_raw": True,
        "calls": {name: [f"0x{site:x}" for site in sites] for name, sites in calls.items()},
        "call_count": sum(map(len, calls.values())),
        "entry_xrefs": ["sub_2a0d9:0x2a0fa", "sub_2a86c:0x2a8da", "sub_3a29c:0x3a2ba"],
        "external_internal_entries": [],
        "local_return": "0x2a74b",
        "caller_raw_sha256": {
            "sub_2a0d9": sha256(z_dat[0x23AD9:0x23B0F]),
            "sub_2a86c": sha256(z_dat[0x2426C:0x24C27]),
            "sub_3a29c": sha256(z_dat[0x33C9C:0x33D0B]),
        },
        "adjacent_arrow_helper_raw_sha256": {
            "sub_2a74c": sha256(z_dat[0x2414C:0x241E8]),
            "sub_2a7e8": sha256(z_dat[0x241E8:0x2426C]),
        },
        "baseline_item_types_at_byte_82": baseline_item_types,
        "contract": contract,
        "contract_sha256": contract_sha256,
        "vectors": vectors,
        "vectors_sha256": vectors_sha256,
    }


def game_menu_item_selector_machine(z_dat: bytes) -> dict[str, object]:
    loaded_start = 0x2A86C
    raw = z_dat[0x2426C:0x24C27]
    assert len(raw) == 2491
    assert sha256(raw) == "7494f3fde6bf4003b02830c6fb99e3899e2332bb3ccf94367f4eaf254347ae9f"

    fixup_offsets = (
        0x21, 0x5C, 0x7F, 0xA5, 0xBA, 0xD1, 0xE6, 0xFE, 0x116, 0x12C,
        0x134, 0x141, 0x14F, 0x155, 0x169, 0x177, 0x17F, 0x190, 0x1A1,
        0x1AF, 0x1B4, 0x1CD, 0x1D5, 0x1E5, 0x210, 0x224, 0x246, 0x24F,
        0x268, 0x270, 0x27D, 0x28B, 0x293, 0x2A0, 0x2AA, 0x2B2, 0x2BF,
        0x2CC, 0x2D6, 0x2E3, 0x2ED, 0x2FA, 0x303, 0x30B, 0x319, 0x326,
        0x32D, 0x33E, 0x345, 0x351, 0x359, 0x366, 0x370, 0x378, 0x385,
        0x392, 0x39C, 0x3A9, 0x3B3, 0x3C0, 0x3C9, 0x3D1, 0x3DF, 0x3EC,
        0x3F3, 0x3FF, 0x410, 0x41F, 0x43E, 0x443, 0x457, 0x45C, 0x46E,
        0x473, 0x487, 0x497, 0x4A5, 0x4B6, 0x4C5, 0x4E4, 0x4E9, 0x4FD,
        0x502, 0x513, 0x518, 0x52C, 0x531, 0x543, 0x548, 0x556, 0x562,
        0x593, 0x5A5, 0x5AD, 0x5BA, 0x5C1, 0x5D5, 0x5DD, 0x5EA, 0x5F7,
        0x604, 0x612, 0x623, 0x632, 0x651, 0x65D, 0x666, 0x67F, 0x687,
        0x694, 0x69E, 0x6AD, 0x6BA, 0x6CA, 0x6DB, 0x6EA, 0x709, 0x70E,
        0x722, 0x727, 0x738, 0x73D, 0x751, 0x756, 0x768, 0x76D, 0x77B,
        0x787, 0x797, 0x7A4, 0x7BD, 0x7C5, 0x7D3, 0x7E0, 0x7E7, 0x802,
        0x80B, 0x815, 0x822, 0x834, 0x83E, 0x84B, 0x853, 0x860, 0x867,
        0x86E, 0x875, 0x87C, 0x885, 0x893, 0x8A4, 0x8B3, 0x8D2, 0x8D7,
        0x8EB, 0x8F0, 0x900, 0x922, 0x92A, 0x935, 0x94B, 0x953, 0x95B,
        0x972, 0x97C, 0x99E, 0x9AE,
    )
    loaded = bytearray(raw)
    for offset in fixup_offsets:
        (raw_address,) = struct.unpack_from("<I", raw, offset)
        struct.pack_into("<I", loaded, offset, raw_address + 0x20000)
    assert sha256(loaded) == "813c4e3369973a4ed502e1c2193d4574d8f64340aadc0623769a07d092b4fd98"

    calls = (
        (0x005, 0x3ED1E), (0x06E, 0x2A186), (0x186, 0x2558B),
        (0x197, 0x29D2D), (0x1A8, 0x3AA85), (0x1B8, 0x3D6D1),
        (0x217, 0x2B288), (0x231, 0x22090), (0x254, 0x2BD8B),
        (0x406, 0x2558B), (0x417, 0x29D2D), (0x435, 0x2CEBF),
        (0x447, 0x3EF4A), (0x464, 0x3D832), (0x477, 0x3D6D1),
        (0x4AC, 0x2558B), (0x4BD, 0x29D2D), (0x4DB, 0x2CEBF),
        (0x4ED, 0x3EF4A), (0x50A, 0x3D832), (0x51C, 0x3EF4A),
        (0x539, 0x3D832), (0x54C, 0x3D6D1), (0x55B, 0x20C32),
        (0x579, 0x22090), (0x619, 0x2558B), (0x62A, 0x29D2D),
        (0x648, 0x2CEBF), (0x66B, 0x2BD8B), (0x6D1, 0x2558B),
        (0x6E2, 0x29D2D), (0x700, 0x2CEBF), (0x712, 0x3EF4A),
        (0x72F, 0x3D832), (0x741, 0x3EF4A), (0x75E, 0x3D832),
        (0x771, 0x3D6D1), (0x780, 0x20C32), (0x89A, 0x2558B),
        (0x8AB, 0x29D2D), (0x8C9, 0x2CEBF), (0x8DB, 0x3EF4A),
        (0x90D, 0x22090), (0x93B, 0x2B483), (0x963, 0x2B227),
        (0x96B, 0x20C32), (0x98C, 0x2B483),
    )
    for offset, target in calls:
        assert raw[offset] == 0xE8
        (displacement,) = struct.unpack_from("<i", raw, offset + 1)
        assert loaded_start + offset + 5 + displacement == target

    raw_last_key = struct.pack("<I", 0x31B6B)
    write_pattern = b"\xC6\x05" + raw_last_key + b"\x00"
    selector_read_pattern = b"\xA0" + raw_last_key
    yes_read_pattern = b"\x80\x3D" + raw_last_key + b"\x59"
    last_key_writes = [
        loaded_start + offset
        for offset in range(len(raw))
        if raw.startswith(write_pattern, offset)
    ]
    selector_reads = [
        loaded_start + offset
        for offset in range(len(raw))
        if raw.startswith(selector_read_pattern, offset)
    ]
    yes_reads = [
        loaded_start + offset
        for offset in range(len(raw))
        if raw.startswith(yes_read_pattern, offset)
    ]
    assert last_key_writes == [
        0x2A88B, 0x2A8C6, 0x2A90F, 0x2A924, 0x2A93B, 0x2A950,
        0x2A968, 0x2A980, 0x2A9D3, 0x2A9E1, 0x2ADC0, 0x2AFE5,
        0x2B1DC, 0x2B218,
    ]
    assert selector_reads == [0x2A8EA]
    assert yes_reads == [0x2ADCC, 0x2AFF1]

    def transition(
        page: int,
        row: int,
        column: int,
        key: int,
        show_introduction: bool = True,
        mapped_slot: int = 0,
    ) -> dict[str, object]:
        result: dict[str, object] = {
            "before": [page, row, column],
            "key": f"0x{key:02x}",
            "show_introduction": show_introduction,
        }
        if key == 0x9A:
            column = 4 if column == 0 else column - 1
            outcome = "redraw"
        elif key == 0x9C:
            column = 0 if column == 4 else column + 1
            outcome = "redraw"
        elif key == 0x98:
            if row < 2:
                row += 1
            elif page < 37:
                page += 1
            outcome = "redraw"
        elif key == 0x9E:
            if row > 0:
                row -= 1
            elif page > 0:
                page -= 1
            outcome = "redraw"
        elif key == 0x99:
            if page < 35:
                page += 3
            outcome = "redraw"
        elif key == 0x9F:
            if page > 2:
                page -= 3
            outcome = "redraw"
        elif key == 0x1B:
            outcome = "return_0"
        elif key in (0x0D, 0x20):
            if show_introduction:
                outcome = "return_1"
                result["grid_index"] = 5 * (page + row) + column
                result["mapped_inventory_slot"] = mapped_slot
            else:
                outcome = "wait"
        else:
            outcome = "wait"
        result["after"] = [page, row, column]
        result["outcome"] = outcome
        return result

    vector_inputs = (
        (0, 0, 0, 0x9A, True, 0), (0, 0, 3, 0x9A, True, 0),
        (0, 0, 4, 0x9C, True, 0), (0, 0, 2, 0x9C, True, 0),
        (4, 0, 2, 0x98, True, 0), (4, 2, 2, 0x98, True, 0),
        (37, 2, 2, 0x98, True, 0), (4, 2, 2, 0x9E, True, 0),
        (4, 0, 2, 0x9E, True, 0), (0, 0, 2, 0x9E, True, 0),
        (34, 1, 1, 0x99, True, 0), (35, 1, 1, 0x99, True, 0),
        (3, 1, 1, 0x9F, True, 0), (2, 1, 1, 0x9F, True, 0),
        (2, 1, 3, 0x0D, True, 37), (2, 1, 3, 0x20, True, 37),
        (2, 1, 3, 0x0D, False, 37), (2, 1, 3, 0x96, True, 37),
        (2, 1, 3, 0x1B, True, 37), (2, 1, 3, 0x41, True, 37),
    )
    vectors = [transition(*values) for values in vector_inputs]
    vectors_sha256 = sha256(
        json.dumps(vectors, sort_keys=True, separators=(",", ":")).encode("utf-8")
    )
    assert vectors_sha256 == "f80f863c607a8d9aea5b9a678e7a9bb644608cc6c57d838d014479df2f49f09a"

    shared_tail = z_dat[0x23AD1:0x23AD9]
    assert shared_tail == bytes.fromhex("83c4045d5f5e5bc3")
    entry_caller = z_dat[0x23AD9:0x23B0F]
    battle_caller = z_dat[0x33C9C:0x33D0B]
    assert sha256(entry_caller) == "1fd36ede5cb17507e24e83bc883074f6205a911ee34359aad6514b6280a2f383"
    assert sha256(battle_caller) == "eb358abcceab4d2b8c735da754e73ff00bc611237a8cf23087e2475bfffad5c7"

    contract = {
        "accepted_keys": {
            "confirm": ["0x0d", "0x20"],
            "escape": "0x1b",
            "movement": ["0x98", "0x99", "0x9a", "0x9c", "0x9e", "0x9f"],
            "keypad_insert_0x96": "ignored_not_confirm",
        },
        "strict_visible_item_gate": "show_introduction == 1",
        "last_key": {
            "write_zero_count": len(last_key_writes),
            "selector_read_count": len(selector_reads),
            "uppercase_y_read_count": len(yes_reads),
        },
        "callers": {
            "0x2a0d9:0x2a106": "arguments_0_minus_1_return_ignored",
            "0x3a29c:0x3a2d4": "arguments_4_actor_return_4_targets_return_1_action_done",
        },
        "exit": "0x2b222_jumps_shared_tail_0x2a0d1",
        "owner_boundary": "input_only_ui_draw_effect_and_shared_tail_remain_separate",
    }
    contract_sha256 = sha256(
        json.dumps(contract, sort_keys=True, separators=(",", ":")).encode("utf-8")
    )
    assert contract_sha256 == "32629e0bed2b0284822887666be436db69ac8e37f75dcb3770533ca7e40ab4c1"
    return {
        "raw_range": "Z.DAT[0x2426c:0x24c27]",
        "loaded_range": "0x2a86c..0x2b227",
        "size_bytes": len(raw),
        "instruction_count": 548,
        "basic_block_count": 137,
        "conditional_branch_count": 77,
        "unconditional_jump_count": 35,
        "call_count": len(calls),
        "call_targets": [f"0x{target:x}" for _, target in calls],
        "fixup_count": len(fixup_offsets),
        "relocation_delta": "0x20000",
        "raw_sha256": sha256(raw),
        "loaded_sha256": sha256(loaded),
        "normalized_loaded_equals_raw": True,
        "entry_xrefs": ["sub_2a0d9:0x2a106", "sub_3a29c:0x3a2d4"],
        "external_internal_entries": [],
        "last_key_write_addresses": [f"0x{address:x}" for address in last_key_writes],
        "last_key_selector_reads": [f"0x{address:x}" for address in selector_reads],
        "last_key_uppercase_y_reads": [f"0x{address:x}" for address in yes_reads],
        "shared_tail": {
            "range": "0x2a0d1..0x2a0d9",
            "bytes_sha256": sha256(shared_tail),
            "other_owner": "sub_29d2d",
            "other_entries": ["0x2a0ce", "0x2a055", "0x2a0bc"],
        },
        "caller_raw_sha256": {
            "sub_2a0d9": sha256(entry_caller),
            "sub_3a29c": sha256(battle_caller),
        },
        "contract": contract,
        "contract_sha256": contract_sha256,
        "vectors": vectors,
        "vectors_sha256": vectors_sha256,
    }


def game_menu_item_use_machine(z_dat: bytes) -> dict[str, object]:
    raw = z_dat[0x2426C:0x24C27]
    assert len(raw) == 2491
    assert sha256(raw) == "7494f3fde6bf4003b02830c6fb99e3899e2332bb3ccf94367f4eaf254347ae9f"

    loaded_start = 0x2A86C
    critical_calls = {
        0x186: 0x2558B,
        0x197: 0x29D2D,
        0x1A8: 0x3AA85,
        0x1B8: 0x3D6D1,
        0x217: 0x2B288,
        0x231: 0x22090,
        0x254: 0x2BD8B,
        0x55B: 0x20C32,
        0x579: 0x22090,
        0x66B: 0x2BD8B,
        0x780: 0x20C32,
        0x90D: 0x22090,
        0x93B: 0x2B483,
        0x963: 0x2B227,
        0x96B: 0x20C32,
        0x98C: 0x2B483,
    }
    for offset, target in critical_calls.items():
        assert raw[offset] == 0xE8
        (displacement,) = struct.unpack_from("<i", raw, offset + 1)
        assert loaded_start + offset + 5 + displacement == target

    shared_tail = z_dat[0x23AD1:0x23AD9]
    assert shared_tail == bytes.fromhex("83c4045d5f5e5bc3")
    assert sha256(shared_tail) == "b389950002ba37dbf8ee72e175c575064b3a60d86d656fbc1a0aebbdf240f405"

    contract = {
        "owned_range": "0x2a86c..0x2b227",
        "selection_exit": {
            "accepted_result": "write_real_inventory_slot_and_return_1",
            "escape_result": 0,
            "all_exits_before_dispatch": [
                "clear_last_key",
                "draw_context_world_scene_or_battle",
                "present",
            ],
            "recognized_navigation": "redraw_and_present_even_at_boundary",
        },
        "mode_dispatch": {
            "0": {"type_0": "event", "type_1": "equip", "type_2": "practice", "type_3": "consume"},
            "4": {"type_3": "consume_second_argument_role", "type_4": "return_4"},
            "other_unsigned_values": "return_without_item_business",
        },
        "equipment": {
            "target_selector_argument": 3,
            "requirement_result": "strict_equal_1",
            "slot": "equipment_type_0_uses_slot_0_other_values_use_slot_1",
            "writes": [
                "old_user_same_slot_minus_1",
                "target_old_item_user_minus_1",
                "target_slot_current_record_id",
                "current_item_user_target_record_id",
            ],
            "inventory_consumed": False,
        },
        "practice": {
            "existing_user_prompt": "present_then_wait_uppercase_Y_only",
            "target_selector_argument": 4,
            "magic_full": "magic_id_not_minus_1_and_last_slot_gt_0_and_not_known",
            "requirement_result": "strict_equal_1",
            "castration": "record_id_78_or_93_and_sexual_0_present_then_uppercase_Y_sets_2",
            "same_user": "return_without_reset",
            "writes": [
                "old_user_practice_item_minus_1",
                "old_user_item_experience_0",
                "target_old_item_user_minus_1",
                "current_item_user_target_record_id",
                "target_practice_item_current_record_id",
                "target_item_experience_0",
                "target_make_item_experience_0",
            ],
        },
        "consumable": {
            "mode_0_target_selector_argument": 5,
            "mode_4_target": "second_argument_role",
            "effect_result_1_order": [
                "effect_panel_present_inside_callee",
                "signed_i16_count_decrement",
                "delete_and_compact_if_count_not_gt_0",
                "wait_any_new_key",
            ],
            "other_effect_result": "no_inventory_change_no_wait",
        },
        "returns": {"cancel": 0, "accepted_default": 1, "battle_type_4": 4},
        "owner_boundary": "ui_use_orchestration_only_callees_callers_and_shared_tail_separate",
    }

    vectors = [
        {
            "name": "escape_world_background_first",
            "input": {"mode": 0, "selection": "escape", "context": "world"},
            "trace": ["draw_world", "present", "return_0"],
            "return": 0,
        },
        {
            "name": "type0_scene_event_after_background",
            "input": {"mode": 0, "item_type": 0, "context": "scene"},
            "trace": ["latch_slot", "draw_scene", "present", "item_event"],
            "return": 1,
        },
        {
            "name": "equipment_slot0_reassigns_both_sides",
            "input": {
                "mode": 0,
                "item_type": 1,
                "equipment_type": 0,
                "item_record_id": 17,
                "item_user": 2,
                "target_role_id": 1,
                "target_old_item_id": 18,
                "requirements": 1,
            },
            "trace": ["present_background", "select_role_3", "requirements", "equip"],
            "final": {
                "role_2_equipment_0": -1,
                "item_18_user": -1,
                "role_1_equipment_0": 17,
                "item_17_user": 1,
                "inventory_consumed": False,
            },
            "return": 1,
        },
        {
            "name": "equipment_nonzero_uses_slot1",
            "input": {"mode": 0, "item_type": 1, "equipment_type": -1, "requirements": 1},
            "final": {"equipment_slot": 1, "inventory_consumed": False},
            "return": 1,
        },
        {
            "name": "equipment_requirement_notice_waits_after_present",
            "input": {"mode": 0, "item_type": 1, "requirements": 0},
            "trace": [
                "present_background",
                "select_role_3",
                "draw_context",
                "draw_notice",
                "present_notice",
                "wait_any_key",
            ],
            "final": {"equipment_changed": False, "inventory_consumed": False},
            "return": 1,
        },
        {
            "name": "practice_reassign_no",
            "input": {"mode": 0, "item_type": 2, "item_user": 2, "prompt_key": "N"},
            "trace": ["present_background", "present_reassign_prompt", "wait_key"],
            "final": {"item_user": 2, "target_selector_called": False},
            "return": 1,
        },
        {
            "name": "practice_magic_full",
            "input": {"magic_id": 77, "known": False, "last_magic_slot": 9},
            "trace": ["present_background", "select_role_4", "present_magic_full_notice", "wait_any_key"],
            "final": {"practice_changed": False},
            "return": 1,
        },
        {
            "name": "practice_known_magic_bypasses_full_and_reassigns",
            "input": {
                "item_record_id": 20,
                "item_user": 2,
                "reassign_key": "Y",
                "target_role_id": 1,
                "magic_id": 77,
                "known": True,
                "last_magic_slot": 9,
                "requirements": 1,
            },
            "final": {
                "old_role_practice_item": -1,
                "old_role_item_experience": 0,
                "target_old_item_user": -1,
                "item_user": 1,
                "target_practice_item": 20,
                "target_item_experience": 0,
                "target_make_item_experience": 0,
            },
            "return": 1,
        },
        {
            "name": "practice_same_user_preserves_experience",
            "input": {"item_user": 1, "target_role_id": 1, "item_experience": 12, "make_item_experience": 13},
            "final": {"item_experience": 12, "make_item_experience": 13},
            "return": 1,
        },
        {
            "name": "castration_yes_uses_record_id",
            "input": {"item_array_index": 196, "item_record_id": 93, "sexual": 0, "prompt_key": "Y"},
            "trace": ["present_castration_prompt", "wait_key", "set_sexual_2", "assign_practice"],
            "final": {"sexual": 2, "practice_item": 93},
            "return": 1,
        },
        {
            "name": "consumable_zero_effect",
            "input": {"mode": 0, "item_type": 3, "effect_result": 0, "count": 1},
            "trace": ["present_background", "select_role_5", "apply_effect"],
            "final": {"count": 1, "deleted": False, "wait_called": False},
            "return": 1,
        },
        {
            "name": "consumable_effect_count_two",
            "input": {"mode": 0, "item_type": 3, "effect_result": 1, "count": 2},
            "trace": ["present_background", "select_role_5", "effect_panel_present", "decrement", "wait_any_key"],
            "final": {"count": 1, "deleted": False},
            "return": 1,
        },
        {
            "name": "consumable_effect_count_one_deletes",
            "input": {"mode": 0, "item_type": 3, "effect_result": 1, "count": 1},
            "trace": ["present_background", "select_role_5", "effect_panel_present", "decrement", "delete_compact", "wait_any_key"],
            "final": {"count": 0, "deleted": True},
            "return": 1,
        },
        {
            "name": "battle_type3_uses_second_argument",
            "input": {"mode": 4, "second_argument_role": 9, "item_type": 3, "effect_result": 0},
            "trace": ["draw_battle", "present", "apply_effect_actor_9_target_9"],
            "return": 1,
        },
        {
            "name": "battle_type4_returns_four_after_background",
            "input": {"mode": 4, "second_argument_role": 9, "item_type": 4},
            "trace": ["draw_battle", "present", "return_4"],
            "return": 4,
        },
    ]
    contract_sha256 = sha256(
        json.dumps(contract, sort_keys=True, separators=(",", ":")).encode("utf-8")
    )
    vectors_sha256 = sha256(
        json.dumps(vectors, sort_keys=True, separators=(",", ":")).encode("utf-8")
    )
    assert contract_sha256 == "a322d31f3ee654f0425ef42899b4057396150339b2fa8ec674cc7dd6faffae97"
    assert vectors_sha256 == "5647f386cb37172ac2dd355f7e9a2df9520375b2114e6020dbeddb14b7beaeb4"
    return {
        "raw_range": "Z.DAT[0x2426c:0x24c27]",
        "loaded_range": "0x2a86c..0x2b227",
        "raw_sha256": sha256(raw),
        "shared_tail_sha256": sha256(shared_tail),
        "critical_call_targets": [f"0x{target:x}" for target in critical_calls.values()],
        "contract": contract,
        "contract_sha256": contract_sha256,
        "vectors": vectors,
        "vectors_sha256": vectors_sha256,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--data-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    z_dat = (args.data_root / "Z.DAT").read_bytes()
    title_index = (args.data_root / "title.idx").read_bytes()
    title_group = (args.data_root / "title.grp").read_bytes()
    title_big = (args.data_root / "title.big").read_bytes()
    palette = (args.data_root / "mmap.col").read_bytes()
    mmap_index = (args.data_root / "MMAP.IDX").read_bytes()
    mmap_group = (args.data_root / "MMAP.GRP").read_bytes()
    ascii_font = (args.data_root / "FONT3.E16").read_bytes()
    big5_font = (args.data_root / "FONT3.C16").read_bytes()
    cfont = (args.data_root / "CFONT").read_bytes()
    ranger = (args.data_root / "RANGER.GRP").read_bytes()
    assert len(title_big) == 320 * 200
    assert len(palette) == 256 * 3
    assert len(ascii_font) == 128 * 16
    assert len(big5_font) % 32 == 0
    assert len(cfont) == 29674
    assert len(ranger) == 114242
    frames = archive_entries(title_index, title_group)
    item_frames = archive_entries(mmap_index, mmap_group)
    assert len(frames) == 9
    assert len(item_frames) >= 200
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
            "Z.DAT": {"bytes": len(z_dat), "sha256": sha256(z_dat)},
            "title.idx": {"bytes": len(title_index), "sha256": sha256(title_index)},
            "title.grp": {"bytes": len(title_group), "sha256": sha256(title_group)},
            "title.big": {"bytes": len(title_big), "sha256": sha256(title_big)},
            "mmap.col": {"bytes": len(palette), "sha256": sha256(palette)},
            "FONT3.E16": {"bytes": len(ascii_font), "sha256": sha256(ascii_font)},
            "FONT3.C16": {"bytes": len(big5_font), "sha256": sha256(big5_font)},
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
            "source": "independent synthetic indexed background plus current mmap.col/FONT3.C16",
            "world_selection_0": fnv1a64(
                game_menu_screen(big5_font, parsed_palette, panel_lookup, 6, 0)
            ),
            "scene_selection_3": fnv1a64(
                game_menu_screen(big5_font, parsed_palette, panel_lookup, 4, 3)
            ),
        },
        "game_menu_item_draw_pixels_fnv1a64": {
            "source": "independent synthetic indexed background plus original RANGER.GRP/MMAP/FONT3 assets",
            "assets": {
                "MMAP.IDX": {"bytes": len(mmap_index), "sha256": sha256(mmap_index)},
                "MMAP.GRP": {"bytes": len(mmap_group), "sha256": sha256(mmap_group)},
                "RANGER.GRP": {"bytes": len(ranger), "sha256": sha256(ranger)},
            },
            **item_draw_pixel_vectors(
                ascii_font,
                big5_font,
                parsed_palette,
                panel_lookup,
                item_frames,
                ranger,
            ),
        },
        "name_entry_pixels_fnv1a64": {
            "source": "independent machine-coordinate FONT3.E16/FONT3.C16/CFONT renderer with bounded invalid-glyph platform adaptation",
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
        "game_menu_item_entry_machine": game_menu_item_entry_machine(z_dat),
        "game_menu_item_reset_machine": game_menu_item_reset_machine(z_dat, ranger),
        "game_menu_item_draw_machine": game_menu_item_draw_machine(z_dat, ranger),
        "game_menu_item_selector_machine": game_menu_item_selector_machine(z_dat),
        "game_menu_item_use_machine": game_menu_item_use_machine(z_dat),
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
