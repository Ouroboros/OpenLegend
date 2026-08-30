# IDAPython 9.x headless exporter for B5 title/menu/new/load evidence.

import collections
import os
import traceback

import ida_auto
import ida_bytes
import ida_funcs
import ida_lines
import ida_pro
import ida_ua
import idautils
import idc


RAW_RANGES = (
    ("inline_menu_tables", 0x20CE5, 0x20D35),
    ("game_menu_labels", 0x545CA, 0x545E8),
    ("name_input_tables", 0x545E8, 0x54B68),
    ("game_menu_text", 0x58228, 0x58359),
    ("menu_prompt_text", 0x58470, 0x584AF),
    ("new_game_text", 0x584FF, 0x585C5),
    ("third_menu_text", 0x58680, 0x58780),
)

TARGET_FUNCTIONS = (
    ("main", 0x20D35),
    ("title_new_load_flow", 0x20FAF),
    ("game_menu_dispatch", 0x212C0),
    ("game_menu_medicine", 0x21496),
    ("game_menu_detoxification", 0x21AC0),
    ("game_menu_status_entry", 0x22066),
    ("game_menu_party_select", 0x22090),
    ("game_menu_status_panel", 0x22A59),
    ("startup_resource_initialize", 0x247DD),
    ("import_loaded_runtime_state", 0x24A02),
    ("startup_state_initialize", 0x24C23),
    ("shutdown_state", 0x24C8D),
    ("world_ui_draw", 0x2558B),
    ("reset_working_scene_state", 0x25AB7),
    ("game_menu_leave_party", 0x25BBA),
    ("game_menu_system", 0x25D0E),
    ("system_slot_menu", 0x25F87),
    ("load_numbered_slot", 0x26208),
    ("game_menu_draw", 0x269AB),
    ("post_title_new_game_session", 0x26B5E),
    ("new_game_character_roll", 0x2711A),
    ("new_game_name_entry", 0x27A26),
    ("new_game_name_draw", 0x2841A),
    ("new_game_name_commit", 0x287CA),
    ("new_game_name_edit", 0x28975),
    ("game_menu_items", 0x2A0D9),
    ("game_menu_items_reset", 0x2A10F),
    ("game_menu_items_draw", 0x2A186),
    ("game_menu_items_select", 0x2A86C),
    ("palette_apply", 0x3CAE7),
    ("palette_fade_to_black", 0x3CC97),
    ("palette_fade_from_black", 0x3CD17),
    ("load_raw_file", 0x3CF45),
    ("build_rgb4_palette_lookup", 0x3D34A),
    ("draw_rle_sprite", 0x3D643),
    ("copy_framebuffer", 0x3D6D1),
    ("load_packed_archive", 0x3D6E0),
    ("fill_rectangle", 0x3D8D8),
    ("palette_from_vga_data", 0x3D939),
)

CALL_TARGETS = (
    ("title_new_load_flow", 0x20FAF),
    ("game_menu_dispatch", 0x212C0),
    ("game_menu_medicine", 0x21496),
    ("game_menu_detoxification", 0x21AC0),
    ("game_menu_status_entry", 0x22066),
    ("import_loaded_runtime_state", 0x24A02),
    ("reset_working_scene_state", 0x25AB7),
    ("game_menu_leave_party", 0x25BBA),
    ("game_menu_system", 0x25D0E),
    ("system_slot_menu", 0x25F87),
    ("load_numbered_slot", 0x26208),
    ("post_title_new_game_session", 0x26B5E),
    ("new_game_character_roll", 0x2711A),
    ("new_game_name_entry", 0x27A26),
    ("game_menu_items", 0x2A0D9),
    ("palette_fade_to_black", 0x3CC97),
    ("palette_fade_from_black", 0x3CD17),
    ("draw_rle_sprite", 0x3D643),
    ("load_packed_archive", 0x3D6E0),
)

DATA_TARGETS = (
    ("last_translated_key", 0x51B6B),
    ("main_loop_running", 0x544DE),
    ("pending_load_slot", 0xD2950),
    ("framebuffer_source", 0xC0B98),
)

DATA_INTERVALS = (
    ("keyboard_state", 0x51B6A, 0x51C0D),
    ("protagonist_record", 0x9014C, 0x90202),
    ("ranger_header", 0xC0834, 0xC0B78),
    ("title_strings", 0x58200, 0x58240),
    ("ui_text_data", 0x58240, 0x58900),
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
                        intervals[name].append((target, ea, owner_start, owner_name, disassembly))
    return exact, intervals


def main():
    ida_auto.auto_wait()
    idb_path = idc.get_idb_path()
    ida_root = os.path.dirname(os.path.dirname(idb_path))
    output_path = os.path.join(ida_root, "reports", "Z_DAT.b5_ui_xrefs.txt")
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
                output.write(f"from=0x{ea:X} owner=0x{owner_start:X} {owner_name} :: {disassembly}\n")

        output.write("\n" + "=" * 100 + "\n")
        output.write("INTERVAL DATA REFERENCES\n")
        for name, begin, end in DATA_INTERVALS:
            rows = sorted(intervals.get(name, ()), key=lambda row: (row[0], row[1]))
            output.write(f"\nRANGE {name} begin=0x{begin:X} end=0x{end:X} refs={len(rows)}\n")
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
                output.write(f"from=0x{ea:X} owner=0x{owner_start:X} {owner_name} :: {disassembly}\n")

    print(f"OPENLEGEND_IDA_B5_REPORT={output_path}")
    ida_pro.qexit(0)


try:
    main()
except Exception:
    traceback.print_exc()
    ida_pro.qexit(1)
