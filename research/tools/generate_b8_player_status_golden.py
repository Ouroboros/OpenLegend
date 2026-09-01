from __future__ import annotations

import importlib.util
import json
import struct
import sys
from pathlib import Path

if len(sys.argv) != 3:
    raise SystemExit(
        'usage: generate_b8_player_status_golden.py <original-data-root> <output-json>')

ROOT = Path(sys.argv[1])
OUTPUT = Path(sys.argv[2])
GEN_PATH = Path(__file__).resolve().with_name('generate_b8_battle_goldens.py')
spec = importlib.util.spec_from_file_location('b8gold', GEN_PATH)
assert spec is not None and spec.loader is not None
g = importlib.util.module_from_spec(spec)
spec.loader.exec_module(g)

war = (ROOT / 'WAR.STA').read_bytes()
record = war[4 * 186:5 * 186]
setup = g.battle_setup_record(4, record)
entries = g.cumulative_entries((ROOT / 'WARFLD.IDX').read_bytes(), (ROOT / 'WARFLD.GRP').read_bytes())
field_words = list(struct.unpack('<8192h', entries[int(setup['battlefield_id'])][:16384]))
view_x, view_y = 15, 13
occupancy = [-1] * 4096
combatants: list[tuple[int, int]] = []
for write in setup['static_occupancy_writes']:
    role_id = int(write['role_id'])
    initial_mode = 2 if write['side'] == 'party' else 1
    sprite = 8 * (role_id % 17) + 5106 + 2 * initial_mode
    combatants.append((role_id, sprite))
    occupancy[int(write['occupancy_index'])] = int(write['slot'])
commands: list[list[int]] = []
for local_x in range(32):
    for local_y in range(32):
        map_x, map_y = local_x + view_x, local_y + view_y
        commands.append([0, map_x, map_y, 18 * (local_x - local_y) + 145,
                         9 * (local_x + local_y) - 81,
                         field_words[map_y * 64 + map_x], 0, 0, 0])
for local_x in range(32):
    for local_y in range(32):
        map_x, map_y = local_x + view_x, local_y + view_y
        cell = map_y * 64 + map_x
        screen_x = 18 * (local_x - local_y) + 145
        screen_y = 9 * (local_x + local_y) - 81
        object_sprite = field_words[4096 + cell]
        if object_sprite not in (0, 15000):
            commands.append([0, map_x, map_y, screen_x, screen_y, object_sprite, 0, 0, 0])
        occupant = occupancy[cell]
        if occupant >= 0:
            commands.append([0, map_x, map_y, screen_x, screen_y,
                             combatants[occupant][1], 0, 0, 0])
_, _, base = g.battle_pixel_hashes(ROOT, int(setup['battlefield_id']), commands)

palette_bytes = (ROOT / 'MMAP.COL').read_bytes()
palette = [tuple(palette_bytes[i:i + 3]) for i in range(0, 768, 3)]
rgb4: list[int] = []
for red in range(16):
    for green in range(16):
        for blue in range(16):
            target = (red * 4 + 2, green * 4 + 2, blue * 4 + 2)
            rgb4.append(min(range(256), key=lambda index: sum(
                (target[channel] - palette[index][channel]) ** 2 for channel in range(3))))
ascii_font = (ROOT / 'FONT.X16').read_bytes()
big5_font = (ROOT / 'FONT.C16').read_bytes()
portraits = g.cumulative_entries((ROOT / 'HDGRP.IDX').read_bytes(), (ROOT / 'HDGRP.GRP').read_bytes())

def draw_panel(pixels: bytearray, x: int, y: int, width: int, height: int) -> None:
    def blend(left: int, top: int, w: int, h: int) -> None:
        for py in range(max(top, 0), min(top + h, 200)):
            for px in range(max(left, 0), min(left + w, 320)):
                offset = py * 320 + px
                source = palette[0]
                dest = palette[pixels[offset]]
                mixed = tuple(source[c] // 8 + dest[c] // 8 for c in range(3))
                pixels[offset] = rgb4[mixed[0] * 256 + mixed[1] * 16 + mixed[2]]
    for rectangle in (
        (x + 5, y, width - 10, 1), (x + 4, y + 1, width - 8, 1),
        (x + 3, y + 2, width - 6, 1), (x + 2, y + 3, width - 4, 1),
        (x + 1, y + 4, width - 2, 1), (x, y + 5, width, height - 10),
        (x + 1, y + height - 5, width - 2, 1),
        (x + 2, y + height - 4, width - 4, 1),
        (x + 3, y + height - 3, width - 6, 1),
        (x + 4, y + height - 2, width - 8, 1),
        (x + 5, y + height - 1, width - 10, 1),
    ):
        blend(*rectangle)
    for left, top, w, h in (
        (x + 5, y + 1, width - 10, 1),
        (x + 4, y + 2, 1, 2), (x + width - 5, y + 2, 1, 2),
        (x + 2, y + 4, 2, 1), (x + width - 4, y + 4, 2, 1),
        (x + 1, y + 5, 1, height - 10),
        (x + width - 2, y + 5, 1, height - 10),
        (x + 2, y + height - 5, 2, 1),
        (x + width - 4, y + height - 5, 2, 1),
        (x + 4, y + height - 4, 1, 2),
        (x + width - 5, y + height - 4, 1, 2),
        (x + 5, y + height - 2, width - 10, 1),
    ):
        for row in range(top, top + h):
            begin = row * 320 + left
            pixels[begin:begin + w] = bytes([0xFF]) * w

def text(pixels: bytearray, x: int, y: int, value: bytes, colors: int) -> None:
    g.draw_battle_text(pixels, x, y, value + b'\0', ascii_font, big5_font, colors)

def number(pixels: bytearray, x: int, y: int, value: int, width: int, colors: int = 0x0705) -> None:
    text(pixels, x, y, f'{value:{width}d}'.encode('ascii'), colors)

labels = {
    'level': bytes.fromhex('b5a5afc520'), 'life': bytes.fromhex('a5cda95220'),
    'mp': bytes.fromhex('a4baa44f20'), 'power': bytes.fromhex('ca5ea44f20'),
    'experience': bytes.fromhex('b867c5e720'), 'upgrade': bytes.fromhex('a4c9afc520'),
    'attack': bytes.fromhex('a7f0c0bba44f20'), 'defence': bytes.fromhex('a8bebf6da44f20'),
    'speed': bytes.fromhex('bbb4a55c20'), 'medicine': bytes.fromhex('c2e5c0f8afe0a44f20'),
    'poison_use': bytes.fromhex('a5ceac72afe0a44f20'), 'detox': bytes.fromhex('b8d1ac72afe0a44f20'),
    'fist': bytes.fromhex('aeb1b478a55ca4d220'), 'sword': bytes.fromhex('b173bc43afe0a44f20'),
    'knife': bytes.fromhex('ad41a44da7dea5a920'), 'unusual': bytes.fromhex('af53aeeda74cbeb920'),
    'hidden': bytes.fromhex('b774beb9a7dea5a920'), 'equipment': bytes.fromhex('b8cbb3c6aaabab7e20'),
    'practice': bytes.fromhex('add7bd6daaabab7e20'), 'magic': bytes.fromhex('a9d2b77ca55ca4d220'),
}

selection = bytearray(base)
draw_panel(selection, 70, 18, 124, 26)
text(selection, 75, 22, bytes.fromhex('ad6eac64be5cbd d6aaba aaacba41'.replace(' ', '')), 0x0705)
draw_panel(selection, 70, 45, 62, 50)
actor_name = bytes.fromhex('a440a442')
status_name = bytes.fromhex('a444a446a448')
text(selection, 83, 52, actor_name, 0x2321)
text(selection, 75, 72, status_name, 0x6663)
selection_hash = g.fnv1a_bytes(selection)

role = [0] * 91
for index, value in {
    1: 2, 15: 5, 16: 1234, 17: 87, 18: 123, 19: 67, 20: 50,
    21: 88, 23: 10, 24: 11, 40: 3, 41: 66, 42: 99, 43: 101,
    44: 103, 45: 102, 46: 104, 47: 105, 48: 106, 50: 107,
    51: 108, 52: 109, 53: 110, 54: 111, 60: 75, 61: 12, 62: 15,
    63: 5, 73: 800,
}.items():
    role[index] = value

page0 = bytearray(base)
draw_panel(page0, 55, 0, 210, 200)
g.draw_battle_sprite(page0, portraits[2], 78, 68)
text(page0, 80, 70, status_name, 0x6663)
text(page0, 60, 90, labels['level'], 0x2321); number(page0, 100, 90, role[15], 3)
text(page0, 60, 107, labels['life'], 0x2321); number(page0, 97, 107, role[17], 3, 0x1416)
text(page0, 120, 107, b'/', 0x6663); number(page0, 127, 107, role[18], 3, 0x3537)
text(page0, 60, 124, labels['mp'], 0x2321); number(page0, 97, 124, role[41], 3, 0x3537)
text(page0, 120, 124, b'/', 0x3537); number(page0, 127, 124, role[42], 3, 0x3537)
text(page0, 60, 141, labels['power'], 0x2321); number(page0, 97, 141, role[21], 3)
text(page0, 120, 141, b'/', 0x6663); text(page0, 127, 141, b'100', 0x2321)
text(page0, 60, 158, labels['experience'], 0x2321); number(page0, 97, 158, role[16], 6)
text(page0, 60, 175, labels['upgrade'], 0x2321); number(page0, 97, 175, 750, 6)
right = [
    (labels['attack'], 101 + 7 + 11), (labels['defence'], 102 + 5 + 13),
    (labels['speed'], 103 + 3 + 17), (labels['medicine'], 104),
    (labels['poison_use'], 105), (labels['detox'], 106), (labels['fist'], 107),
    (labels['sword'], 108), (labels['knife'], 109), (labels['unusual'], 110),
    (labels['hidden'], 111),
]
for index, (label, value) in enumerate(right):
    y = 5 + 17 * index
    text(page0, 160, y, label, 0x6663); number(page0, 230, y, value, 3)
page0_hash = g.fnv1a_bytes(page0)

page1 = bytearray(base)
draw_panel(page1, 55, 0, 210, 200)
g.draw_battle_sprite(page1, portraits[2], 78, 68)
text(page1, 80, 70, status_name, 0x6663)
text(page1, 60, 90, labels['equipment'], 0x2321)
text(page1, 60, 107, bytes.fromhex('a44aa44c'), 0x0705)
text(page1, 60, 124, bytes.fromhex('a44ea450'), 0x0705)
text(page1, 60, 141, labels['practice'], 0x2321)
text(page1, 60, 158, bytes.fromhex('a452a454'), 0x0705)
number(page1, 60, 175, 15, 5); text(page1, 100, 175, b'/', 0x6663); number(page1, 108, 175, 540, 5, 0x2321)
text(page1, 160, 5, labels['magic'], 0x2321)
text(page1, 160, 22, bytes.fromhex('a456a458'), 0x0705)
number(page1, 242, 22, 9, 2, 0x6663)
page1_hash = g.fnv1a_bytes(page1)

result = {
    'format': 'openlegend-b8-player-status-golden-v1',
    'battle_id': 4,
    'battlefield_id': int(setup['battlefield_id']),
    'view': [view_x, view_y],
    'party': [1, 2],
    'selected_party_slot': 1,
    'selected_role_id': 2,
    'fixture': {
        'role_name_hex': status_name.hex(),
        'equipment_secondary_name_hex': ['a44aa44c', 'a44ea450'],
        'practice_secondary_name_hex': 'a452a454',
        'magic_name_hex': 'a456a458',
        'level': 5,
        'experience': 1234,
        'item_experience': 15,
        'iq': 75,
        'magic_id': 5,
        'magic_proficiency': 800,
        'practice_experience_factor': 2,
        'practice_required_experience': 540,
    },
    'hashes': {
        'status_selection': selection_hash,
        'status_page_0': page0_hash,
        'status_page_1': page1_hash,
    },
    'asset_sha256': {
        name: g.sha256((ROOT / name).read_bytes())
        for name in (
            'WAR.STA',
            'WARFLD.IDX',
            'WARFLD.GRP',
            'WDX002',
            'WMP002',
            'HDGRP.IDX',
            'HDGRP.GRP',
            'FONT.X16',
            'FONT.C16',
            'MMAP.COL',
        )
    },
}
OUTPUT.parent.mkdir(parents=True, exist_ok=True)
OUTPUT.write_text(
    json.dumps(result, ensure_ascii=False, indent=2, sort_keys=True) + '\n',
    encoding='utf-8')
