# IDAPython 9.x headless exporter for B6 world-map evidence.

import collections
import os
import traceback

import ida_auto
import ida_bytes
import ida_funcs
import ida_hexrays
import ida_lines
import ida_pro
import ida_ua
import idautils
import idc


RAW_RANGES = (
    ("alpha_component_table", 0x54294, 0x544D4),
    ("world_animation_and_collision_tables", 0x544DE, 0x54570),
)

TARGET_FUNCTIONS = (
    ("main", 0x20D35),
    ("weather_particle_draw", 0x206AD),
    ("weather_particle_blend_run", 0x20899),
    ("weather_particle_blend_pixel", 0x208A9),
    ("weather_particle_wrapper", 0x3D88A),
    ("world_idle_tick", 0x2399E),
    ("world_idle_animation", 0x23AA6),
    ("world_periodic_weather", 0x23B3E),
    ("world_move_x", 0x23C98),
    ("world_move_y", 0x23F28),
    ("ship_move_x", 0x241DC),
    ("ship_move_y", 0x2422A),
    ("world_commit_position", 0x24278),
    ("weather_scroll", 0x24417),
    ("world_collision", 0x24496),
    ("ship_collision", 0x24559),
    ("world_entrance_probe", 0x24667),
    ("ship_entrance_probe", 0x246F9),
    ("startup_resource_initialize", 0x247DD),
    ("world_session_initialize", 0x24A02),
    ("world_layer_open", 0x24C23),
    ("world_layer_close", 0x24C8D),
    ("world_cache_load", 0x24CE8),
    ("earth_cache_load", 0x24D43),
    ("building_cache_load", 0x24DB8),
    ("surface_cache_load", 0x24E2D),
    ("build_x_cache_load", 0x24EA2),
    ("build_y_cache_load", 0x24F17),
    ("world_depth_rebuild", 0x24F8C),
    ("world_render", 0x2558B),
    ("world_scene_entrance", 0x25911),
    ("world_sprite_tables", 0x26A92),
    ("bounded_random", 0x3D612),
    ("draw_legacy_sprite", 0x3D643),
)

CALL_TARGETS = TARGET_FUNCTIONS[1:-2]

DATA_TARGETS = (
    ("player_world_x", 0xC0B88),
    ("player_world_y", 0xC0B8C),
    ("player_cache_x", 0xC0824),
    ("player_cache_y", 0xC0828),
    ("cache_origin_x", 0xC082C),
    ("cache_origin_y", 0xC0830),
    ("ship_previous_x", 0xC0BA4),
    ("ship_previous_y", 0xC0BA0),
    ("ship_next_x", 0xC0BA6),
    ("ship_next_y", 0xC0BA2),
    ("earth_file", 0xC0BF0),
    ("building_file", 0xC0BF4),
    ("surface_file", 0xC0BEC),
    ("build_x_file", 0xC0BEE),
    ("build_y_file", 0xC0BE6),
    ("in_ship", 0xC0834),
    ("header_world_x", 0xC0838),
    ("header_world_y", 0xC083A),
    ("world_frame", 0xC0BF8),
    ("ship_frame", 0xC0BEA),
    ("weather_active", 0xC0BE8),
)

DATA_INTERVALS = (
    ("build_x_cache", 0x62FFC, 0x6AFFC),
    ("build_y_cache", 0x6AFFC, 0x72FFC),
    ("surface_cache", 0x7300C, 0x7B00C),
    ("earth_cache", 0x7FE2C, 0x87E2C),
    ("building_cache", 0x87E2C, 0x8FE2C),
    ("world_header", 0xC0834, 0xC0B78),
    ("world_runtime_globals", 0xC0B80, 0xC0C00),
    ("world_animation_tables", 0x544DE, 0x54520),
)


def line(ea):
    return ida_lines.tag_remove(idc.generate_disasm_line(ea, 0) or "")


def string_at(ea):
    value = idc.get_strlit_contents(ea)
    if value is None:
        return None
    return value.decode("ascii", errors="backslashreplace")


def dump_bytes(output, label, start, end):
    output.write("\n" + "=" * 100 + "\n")
    output.write(f"BYTES {label} start=0x{start:X} end=0x{end:X} size={end-start}\n")
    data = ida_bytes.get_bytes(start, end - start) or b""
    for offset in range(0, len(data), 16):
        chunk = data[offset:offset + 16]
        output.write(f"{start + offset:08X}  {' '.join(f'{value:02x}' for value in chunk)}\n")


def dump_function(output, research, requested):
    function = ida_funcs.get_func(requested)
    if function is None:
        ida_ua.create_insn(requested)
        ida_funcs.add_func(requested)
        function = ida_funcs.get_func(requested)
    if function is None:
        output.write("\n" + "=" * 100 + "\n")
        output.write(f"MISSING {research} requested=0x{requested:X}\n")
        return

    output.write("\n" + "=" * 100 + "\n")
    output.write(
        f"FUNCTION {ida_funcs.get_func_name(function.start_ea)} research={research} "
        f"start=0x{function.start_ea:X} end=0x{function.end_ea:X} "
        f"size={function.end_ea-function.start_ea}\n"
    )
    output.write("PSEUDOCODE\n")
    try:
        output.write(str(ida_hexrays.decompile(function.start_ea)))
        output.write("\nDISASSEMBLY\n")
    except Exception as error:
        output.write(f"DECOMPILE_FAILED {error!r}\nDISASSEMBLY\n")
    function_strings = collections.defaultdict(list)
    for ea in idautils.Heads(function.start_ea, function.end_ea):
        if not ida_bytes.is_code(ida_bytes.get_flags(ea)):
            continue
        raw = ida_bytes.get_bytes(ea, ida_bytes.get_item_size(ea)) or b""
        output.write(f"{ea:08X}  {' '.join(f'{value:02x}' for value in raw):<32}  {line(ea)}\n")
        for target in idautils.DataRefsFrom(ea):
            value = string_at(target)
            if value is not None:
                function_strings[(target, value)].append(ea)

    output.write("STRINGS\n")
    for (target, value), refs in sorted(function_strings.items()):
        output.write(
            f"address=0x{target:X} refs={','.join(f'0x{ea:X}' for ea in refs)} value={value!r}\n"
        )


def collect_data_references():
    exact = collections.defaultdict(list)
    intervals = collections.defaultdict(list)
    for segment_start in idautils.Segments():
        segment_end = idc.get_segm_end(segment_start)
        for ea in idautils.Heads(segment_start, segment_end):
            if not ida_bytes.is_code(ida_bytes.get_flags(ea)):
                continue
            owner = ida_funcs.get_func(ea)
            owner_start = owner.start_ea if owner else ea
            owner_name = ida_funcs.get_func_name(owner_start) if owner else "<no-function>"
            disassembly = line(ea)
            targets = set(idautils.DataRefsFrom(ea))
            for name, target in DATA_TARGETS:
                if target in targets:
                    exact[name].append((ea, owner_start, owner_name, disassembly))
            for target in targets:
                for name, begin, end in DATA_INTERVALS:
                    if begin <= target < end:
                        intervals[name].append(
                            (target, ea, owner_start, owner_name, disassembly)
                        )
    return exact, intervals


def main():
    ida_auto.auto_wait()
    idb_path = idc.get_idb_path()
    ida_root = os.path.dirname(os.path.dirname(idb_path))
    output_path = os.path.join(ida_root, "reports", "Z_DAT.b6_world_xrefs.txt")
    exact, intervals = collect_data_references()

    with open(output_path, "w", encoding="utf-8") as output:
        output.write(f"IDB={idb_path}\n")
        output.write("SOURCE=current Z.DAT machine code decoded by IDA 9.x headless\n")
        for label, start, end in RAW_RANGES:
            dump_bytes(output, label, start, end)
        for research, address in TARGET_FUNCTIONS:
            dump_function(output, research, address)

        output.write("\n" + "=" * 100 + "\n")
        output.write("EXACT DATA REFERENCES\n")
        for name, target in DATA_TARGETS:
            rows = sorted(exact.get(name, ()), key=lambda row: row[0])
            output.write(f"\nTARGET {name} address=0x{target:X} refs={len(rows)}\n")
            for ea, owner_start, owner_name, disassembly in rows:
                output.write(
                    f"from=0x{ea:X} owner=0x{owner_start:X} {owner_name} :: {disassembly}\n"
                )

        output.write("\n" + "=" * 100 + "\n")
        output.write("INTERVAL DATA REFERENCES\n")
        for name, begin, end in DATA_INTERVALS:
            rows = sorted(intervals.get(name, ()), key=lambda row: (row[0], row[1]))
            output.write(
                f"\nRANGE {name} begin=0x{begin:X} end=0x{end:X} refs={len(rows)}\n"
            )
            for target, ea, owner_start, owner_name, disassembly in rows:
                output.write(
                    f"target=0x{target:X} offset={target-begin} from=0x{ea:X} "
                    f"owner=0x{owner_start:X} {owner_name} :: {disassembly}\n"
                )

        output.write("\n" + "=" * 100 + "\n")
        output.write("CALL REFERENCES\n")
        for name, target in CALL_TARGETS:
            rows = []
            for ea in idautils.CodeRefsTo(target, False):
                owner = ida_funcs.get_func(ea)
                owner_start = owner.start_ea if owner else ea
                owner_name = ida_funcs.get_func_name(owner_start) if owner else "<no-function>"
                rows.append((ea, owner_start, owner_name, line(ea)))
            output.write(f"\nTARGET {name} address=0x{target:X} refs={len(rows)}\n")
            for ea, owner_start, owner_name, disassembly in sorted(rows):
                output.write(
                    f"from=0x{ea:X} owner=0x{owner_start:X} {owner_name} :: {disassembly}\n"
                )

    print(f"OPENLEGEND_IDA_B6_REPORT={output_path}")
    ida_pro.qexit(0)


try:
    main()
except Exception:
    traceback.print_exc()
    ida_pro.qexit(1)
