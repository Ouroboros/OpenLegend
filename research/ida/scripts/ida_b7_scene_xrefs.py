# IDAPython 9.x headless exporter for B7 scene/event evidence.

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
    ("scene_direction_and_animation_tables", 0x544E2, 0x5450C),
    ("scene_blocked_earth_ranges", 0x5456A, 0x5458E),
    ("scene_weather_scene_ids", 0x555A4, 0x555BA),
    ("tournament_head_ids", 0x2C1CD, 0x2C209),
)

TARGET_FUNCTIONS = (
    ("main", 0x20D35),
    ("scene_session", 0x28E40),
    ("scene_archives_open", 0x29391),
    ("scene_archive_swap_open", 0x294AD),
    ("scene_archive_swap_close", 0x29537),
    ("scene_backup_archives_open", 0x295D0),
    ("scene_state_load", 0x296E6),
    ("scene_event_state_load", 0x29819),
    ("scene_depth_rebuild", 0x299A1),
    ("scene_collision_probe", 0x29B3C),
    ("scene_trigger_probe", 0x29C36),
    ("scene_render", 0x29D2D),
    ("scene_event_entry", 0x2B308),
    ("scene_auto_event_entry", 0x2B3B4),
    ("scene_exit_prompt", 0x2C0BB),
    ("event_interpreter", 0x2C319),
    ("event_helper_2CC21", 0x2CC21),
    ("event_helper_2CEBF", 0x2CEBF),
    ("event_helper_2D1CD", 0x2D1CD),
    ("event_helper_2D372", 0x2D372),
    ("event_helper_2D501", 0x2D501),
    ("event_helper_2D590", 0x2D590),
    ("event_helper_2D653", 0x2D653),
    ("event_helper_2D678", 0x2D678),
    ("event_helper_2D841", 0x2D841),
    ("event_helper_2DBF4", 0x2DBF4),
    ("event_helper_2DD45", 0x2DD45),
    ("event_helper_2DD77", 0x2DD77),
    ("event_helper_2DE03", 0x2DE03),
    ("event_helper_2DE39", 0x2DE39),
    ("event_helper_2DE7D", 0x2DE7D),
    ("event_helper_2DF0E", 0x2DF0E),
    ("event_helper_2E078", 0x2E078),
    ("event_helper_2E155", 0x2E155),
    ("event_helper_2E1E8", 0x2E1E8),
    ("event_helper_2E278", 0x2E278),
    ("event_helper_2E28A", 0x2E28A),
    ("event_helper_2E29C", 0x2E29C),
    ("event_helper_2E2D7", 0x2E2D7),
    ("event_helper_2E2F5", 0x2E2F5),
    ("event_helper_2E337", 0x2E337),
    ("event_helper_2E46B", 0x2E46B),
    ("event_helper_2E536", 0x2E536),
    ("event_helper_2E571", 0x2E571),
    ("event_helper_2E639", 0x2E639),
    ("event_helper_2E659", 0x2E659),
    ("event_helper_2EB49", 0x2EB49),
    ("event_helper_2ED8D", 0x2ED8D),
    ("event_helper_2F053", 0x2F053),
    ("event_helper_2F107", 0x2F107),
    ("event_helper_2F136", 0x2F136),
    ("event_helper_2F171", 0x2F171),
    ("event_helper_2F34C", 0x2F34C),
    ("event_helper_2F39C", 0x2F39C),
    ("event_helper_2F3F0", 0x2F3F0),
    ("event_helper_2F526", 0x2F526),
    ("event_helper_2F62F", 0x2F62F),
    ("event_helper_2F6C2", 0x2F6C2),
    ("event_helper_2F6E3", 0x2F6E3),
    ("event_helper_2F721", 0x2F721),
    ("event_helper_2F890", 0x2F890),
    ("event_helper_2F8AB", 0x2F8AB),
    ("event_helper_2F8D1", 0x2F8D1),
    ("event_helper_2F966", 0x2F966),
    ("event_helper_2F9B5", 0x2F9B5),
    ("event_helper_2F9F2", 0x2F9F2),
    ("event_helper_2FAB7", 0x2FAB7),
    ("event_helper_2FBC0", 0x2FBC0),
    ("event_helper_2FC9D", 0x2FC9D),
    ("event_helper_2FDA6", 0x2FDA6),
    ("event_helper_2FEBF", 0x2FEBF),
    ("event_helper_2FEDF", 0x2FEDF),
    ("event_helper_2FF87", 0x2FF87),
    ("event_helper_2FFB3", 0x2FFB3),
    ("event_helper_30035", 0x30035),
    ("event_helper_30094", 0x30094),
    ("event_helper_300D9", 0x300D9),
    ("event_helper_300FF", 0x300FF),
    ("event_helper_301D1", 0x301D1),
    ("event_helper_302E0", 0x302E0),
    ("event_helper_30480", 0x30480),
    ("event_helper_30510", 0x30510),
    ("event_helper_30559", 0x30559),
    ("event_helper_30A5A", 0x30A5A),
    ("event_helper_30B45", 0x30B45),
    ("event_helper_30B81", 0x30B81),
    ("event_helper_30C3D", 0x30C3D),
    ("event_helper_31241", 0x31241),
    ("event_helper_31284", 0x31284),
    ("event_helper_312A6", 0x312A6),
    ("event_helper_31945", 0x31945),
    ("event_helper_31C2F", 0x31C2F),
    ("event_helper_31C4A", 0x31C4A),
    ("battle_entry", 0x31C75),
    ("bounded_random", 0x3D612),
    ("draw_legacy_sprite", 0x3D643),
)

CALL_TARGETS = TARGET_FUNCTIONS[1:-2]

DATA_TARGETS = (
    ("current_scene", 0xC0B7C),
    ("scene_x", 0xC083C),
    ("scene_y", 0xC083E),
    ("scene_direction", 0xC0840),
)

DATA_INTERVALS = (
    ("scene_events", 0xC5690, 0xC67C0),
    ("scene_earth", 0xC67C0, 0xC87C0),
    ("scene_building", 0xC87C0, 0xCA7C0),
    ("scene_decoration", 0xCA7C0, 0xCC7C0),
    ("scene_event_index", 0xCC7C0, 0xCE7C0),
    ("scene_building_height", 0xCE7C0, 0xD07C0),
    ("scene_decoration_height", 0xD07C0, 0xD27C0),
    ("scene_metadata", 0x9E4CC, 0x9F5DC),
    ("role_records", 0x9014C, 0xA685C),
    ("inventory", 0xC0B84, 0xC11C4),
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
    output_path = os.path.join(ida_root, "reports", "Z_DAT.b7_scene_xrefs.txt")
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

    print(f"OPENLEGEND_IDA_B7_REPORT={output_path}")
    ida_pro.qexit(0)


try:
    main()
except Exception:
    traceback.print_exc()
    ida_pro.qexit(1)
