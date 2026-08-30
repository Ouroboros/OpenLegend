#!/usr/bin/env python3
"""Generate B6 world-map goldens without using OpenLegend C++ code."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

EXTENT = 480
CACHE_EXTENT = 128
VIEW_EXTENT = 32
LAYER_FILES = ("EARTH.002", "SURFACE.002", "BUILDING.002", "BUILDX.002", "BUILDY.002")
PLAYER_BASE = (5002, 5016, 5030, 5044)
BLOCKED_WALK = ((358, 362), (374, 380), (458, 464), (506, 670),
                (818, 824), (838, 838), (934, 936), (1016, 1022))


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--data-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args()


def words(path: Path) -> tuple[int, ...]:
    data = path.read_bytes()
    return struct.unpack(f"<{len(data) // 2}h", data)


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def fnv1a64(data: bytes) -> str:
    value = 0xCBF29CE484222325
    for byte in data:
        value ^= byte
        value = (value * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return f"0x{value:016x}"


def layer_at(layer: tuple[int, ...], x: int, y: int) -> int:
    return layer[y * EXTENT + x]


def make_cache(layer: tuple[int, ...], origin_x: int, origin_y: int) -> tuple[int, ...]:
    return tuple(
        layer_at(layer, origin_x + x, origin_y + y)
        for y in range(CACHE_EXTENT)
        for x in range(CACHE_EXTENT)
    )


def cache_at(cache: tuple[int, ...], x: int, y: int) -> int:
    return cache[y * CACHE_EXTENT + x]


def parse_archive(index_path: Path, group_path: Path) -> list[bytes]:
    index = index_path.read_bytes()
    group = group_path.read_bytes()
    ends = struct.unpack(f"<{len(index) // 4}I", index)
    entries: list[bytes] = []
    begin = 0
    for end in ends:
        if end == 0 and begin != 0:
            break
        entries.append(group[begin:end])
        begin = end
    assert begin == len(group)
    return entries


def draw_sprite(pixels: bytearray, frame: bytes, anchor_x: int, anchor_y: int) -> None:
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


def depth_entries(
    build_x: tuple[int, ...],
    build_y: tuple[int, ...],
    building: tuple[int, ...],
    view_x: int,
    view_y: int,
    origin_x: int,
    origin_y: int,
    world_x: int,
    world_y: int,
) -> list[tuple[int, int, int]]:
    owners_x = list(build_x)
    owners_y = list(build_y)
    owners_x[view_y * CACHE_EXTENT + view_x] = world_x
    owners_y[view_y * CACHE_EXTENT + view_x] = world_y
    result: list[tuple[int, int, int]] = []
    for cache_x in range(view_x - 11, view_x + 21):
        scan_floor_y = view_y - 11
        for cache_y in range(view_y - 11, view_y + 21):
            index = cache_y * CACHE_EXTENT + cache_x
            occupied = owners_x[index] != 0 or owners_y[index] != 0
            primary_here = cache_x == view_x and cache_y == view_y
            if not occupied and not primary_here:
                continue
            if primary_here:
                result.append((world_x, world_y, 5000))
                continue
            owner = (owners_x[index], owners_y[index])
            found = next((i for i, entry in enumerate(result) if entry[:2] == owner), None)
            if found is None:
                local_x = owner[0] - origin_x
                local_y = owner[1] - origin_y
                assert 0 <= local_x < CACHE_EXTENT and 0 <= local_y < CACHE_EXTENT
                result.append((owner[0], owner[1], cache_at(building, local_x, local_y)))
                continue
            if found == len(result) - 1:
                continue
            saved_last = result[-1]
            for scan_y in range(cache_y - 1, scan_floor_y - 1, -1):
                scan_index = scan_y * CACHE_EXTENT + cache_x
                above = (owners_x[scan_index], owners_y[scan_index])
                if above == (0, 0):
                    continue
                above_is_current = above == owner
                above_is_found = above == result[found][:2]
                if not above_is_current and not above_is_found:
                    for move in range(len(result) - 1, found, -1):
                        result[move] = result[move - 1]
                    result[found] = saved_last
            scan_floor_y = cache_y + 1
    return result


def project(relative_x: int, relative_y: int, origin_x: int, origin_y: int) -> tuple[int, int]:
    return origin_x + (relative_x - relative_y) * 18, origin_y + (relative_x + relative_y) * 9


def initial_frame(
    caches: dict[str, tuple[int, ...]],
    archive: list[bytes],
    origin_x: int,
    origin_y: int,
    world_x: int,
    world_y: int,
    direction: int,
) -> bytes:
    pixels = bytearray(320 * 200)
    view_x = world_x - origin_x
    view_y = world_y - origin_y
    for cache_x in range(view_x - 11, view_x + 21):
        for cache_y in range(view_y - 11, view_y + 21):
            point = project(cache_x - (view_x - 11), cache_y - (view_y - 11), 145, -81)
            draw_sprite(pixels, archive[cache_at(caches["EARTH.002"], cache_x, cache_y) // 2], *point)
    for cache_x in range(view_x - 11, view_x + 21):
        for cache_y in range(view_y - 11, view_y + 21):
            sprite = cache_at(caches["SURFACE.002"], cache_x, cache_y)
            if sprite:
                point = project(cache_x - (view_x - 11), cache_y - (view_y - 11), 145, -81)
                draw_sprite(pixels, archive[sprite // 2], *point)
    entries = depth_entries(
        caches["BUILDX.002"], caches["BUILDY.002"], caches["BUILDING.002"],
        view_x, view_y, origin_x, origin_y, world_x, world_y)
    for entry_x, entry_y, sprite in entries:
        if sprite == 5000:
            draw_sprite(pixels, archive[PLAYER_BASE[direction] // 2], 145, 117)
        elif sprite:
            point = project(
                entry_x - origin_x - (view_x - 10),
                entry_y - origin_y - (view_y - 10),
                145,
                -63,
            )
            draw_sprite(pixels, archive[sprite // 2], *point)
    return bytes(pixels)


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


def trace_moves(
    layers: dict[str, tuple[int, ...]],
    scenes: list[tuple[int, ...]],
    roles: list[tuple[int, ...]],
    team: tuple[int, ...],
    start_x: int,
    start_y: int,
) -> list[dict[str, object]]:
    x, y = start_x, start_y
    output: list[dict[str, object]] = []
    for name, delta in (("right", (1, 0)), ("up", (0, -1)), ("left", (-1, 0)), ("down", (0, 1))):
        target_x, target_y = x + delta[0], y + delta[1]
        entrance = -1
        for index, scene in enumerate(scenes[:84]):
            if (target_x, target_y) not in ((scene[10], scene[11]), (scene[12], scene[13])):
                continue
            condition = scene[9]
            if condition == 0 or (condition == 2 and any(
                role_id >= 0 and roles[role_id][60] >= 70 for role_id in team)):
                entrance = index
                break
        cell = target_y * EXTENT + target_x
        blocked_terrain = any(low <= layers["EARTH.002"][cell] <= high for low, high in BLOCKED_WALK)
        walkable = (
            10 < target_x < 459 and 10 < target_y < 459
            and layers["BUILDING.002"][cell] == 0
            and layers["BUILDX.002"][cell] == 0
            and layers["BUILDY.002"][cell] == 0
            and not blocked_terrain
        )
        if entrance >= 0:
            kind = "enter_scene"
        elif walkable:
            x, y = target_x, target_y
            kind = "moved"
        else:
            kind = "stay"
        output.append({"direction": name, "kind": kind, "scene_id": entrance, "world": [x, y]})
    return output


def main() -> None:
    args = parse_args()
    root = args.data_root
    layer_bytes = {name: (root / name).read_bytes() for name in LAYER_FILES}
    layers = {name: struct.unpack(f"<{len(data) // 2}h", data) for name, data in layer_bytes.items()}
    ranger = (root / "RANGER.GRP").read_bytes()
    header = struct.unpack_from("<418h", ranger, 0)
    role_offset = 836
    roles = [struct.unpack_from("<91h", ranger, role_offset + index * 182) for index in range(320)]
    scene_offset = 59_076 + 38_000
    scenes = [struct.unpack_from("<26h", ranger, scene_offset + index * 52) for index in range(84)]
    world_x, world_y = header[2], header[3]
    origin_x = min(max(world_x - 64, 0), 352)
    origin_y = min(max(world_y - 64, 0), 352)
    caches = {name: make_cache(layer, origin_x, origin_y) for name, layer in layers.items()}
    archive = parse_archive(root / "MMAP.IDX", root / "MMAP.GRP")
    cloud_archive = parse_archive(root / "CLOUD.IDX", root / "CLOUD.GRP")
    palette_bytes = (root / "MMAP.COL").read_bytes()
    palette = [tuple(palette_bytes[index:index + 3]) for index in range(0, len(palette_bytes), 3)]
    frame = initial_frame(caches, archive, origin_x, origin_y, world_x, world_y, header[6])
    frame_300, particles_300, random_state_300 = weather_frame(
        frame, cloud_archive, palette, 300)
    cache_hashes = {
        name: sha256_bytes(struct.pack(f"<{len(cache)}h", *cache))
        for name, cache in caches.items()
    }
    left_target = (world_x - 1, world_y)
    left_scene = next(
        index for index, scene in enumerate(scenes)
        if left_target in ((scene[10], scene[11]), (scene[12], scene[13])) and scene[9] == 0
    )
    document = {
        "source": "independent Python int16le/cache/RLE/depth oracle over original assets",
        "world": {
            "extent": [480, 480],
            "layer_size": 460800,
            "layer_sha256": {name: sha256_bytes(data) for name, data in layer_bytes.items()},
        },
        "initial": {
            "world": [world_x, world_y],
            "cache_origin": [origin_x, origin_y],
            "cache_local": [world_x - origin_x, world_y - origin_y],
            "direction": header[6],
            "player_frame": PLAYER_BASE[header[6]],
            "cache_sha256_int16le": cache_hashes,
            "cache_fnv1a64_int16le": {
                name: fnv1a64(struct.pack(f"<{len(cache)}h", *cache))
                for name, cache in caches.items()
            },
            "framebuffer_sha256": sha256_bytes(frame),
            "framebuffer_fnv1a64": fnv1a64(frame),
        },
        "weather_after_300_ticks": {
            "particles": particles_300,
            "random_state": f"0x{random_state_300:08x}",
            "framebuffer_sha256": sha256_bytes(frame_300),
            "framebuffer_fnv1a64": fnv1a64(frame_300),
        },
        "initial_left_probe": {
            "target": list(left_target),
            "kind": "enter_scene",
            "scene_id": left_scene,
            "world_after": [world_x, world_y],
        },
        "fixed_trace": trace_moves(
            layers, scenes, roles, tuple(header[12:18]), world_x, world_y),
        "collision": {
            "blocked_walking_ranges": [list(pair) for pair in BLOCKED_WALK],
        },
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(document, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
