# IDAPython 9.x headless xref-driven input/font research exporter.

import os
import traceback

import ida_auto
import ida_bytes
import ida_funcs
import ida_hexrays
import ida_lines
import ida_pro
import idautils
import idc


DATA_TARGETS = {
    0x51B6B: "last_key_code",
    0x51B75: "input_flag_51B75",
    0x51B7A: "input_flag_51B7A",
    0x51B88: "input_flag_51B88",
    0x51B8D: "input_flag_51B8D",
    0x51B9B: "input_flag_51B9B",
    0x51BB9: "input_flag_51BB9",
    0x51BEF: "input_flag_51BEF",
    0x51BF0: "input_flag_51BF0",
    0x51BF1: "input_flag_51BF1",
    0x51C03: "input_flag_51C03",
    0x51C04: "move_down_a",
    0x51C05: "move_down_b",
    0x51C06: "move_left_a",
    0x51C07: "move_left_b",
    0x51C09: "move_right_a",
    0x51C0A: "move_right_b",
    0x51C0B: "move_up_a",
    0x51C0C: "move_up_b",
    0x544D8: "tick_counter_pointer",
}

EXPLICIT_FUNCTIONS = {
    0x3D27A: "big5_to_font_index",
    0x20615: "draw_big5_glyph_16x16",
    0x20663: "draw_ascii_glyph_8x16",
}


def dump_function(output, start_ea, research_name, decompiler_available):
    function = ida_funcs.get_func(start_ea)
    if function is None:
        output.write(f"\nMISSING_FUNCTION {research_name} 0x{start_ea:X}\n")
        return
    output.write("\n\n" + "=" * 100 + "\n")
    output.write(
        f"FUNCTION {ida_funcs.get_func_name(function.start_ea)} "
        f"research={research_name} start=0x{function.start_ea:X} "
        f"end=0x{function.end_ea:X} size={function.end_ea-function.start_ea}\n"
    )
    output.write("\n--- PSEUDOCODE ---\n")
    if decompiler_available:
        try:
            output.write(str(ida_hexrays.decompile(function.start_ea)))
            output.write("\n")
        except Exception as error:
            output.write(f"DECOMPILE_FAILED {error!r}\n")
    output.write("\n--- DISASSEMBLY ---\n")
    for ea in idautils.Heads(function.start_ea, function.end_ea):
        if not ida_bytes.is_code(ida_bytes.get_flags(ea)):
            continue
        raw = ida_bytes.get_bytes(ea, max(1, idc.get_item_size(ea))) or b""
        line = ida_lines.tag_remove(idc.generate_disasm_line(ea, 0) or "")
        output.write(f"{ea:08X}  {raw.hex(' '):<32}  {line}\n")


def main():
    ida_auto.auto_wait()
    idb_path = idc.get_idb_path()
    ida_root = os.path.dirname(os.path.dirname(idb_path))
    output_path = os.path.join(ida_root, "reports", "Z_DAT.input_font_xrefs.txt")
    decompiler_available = ida_hexrays.init_hexrays_plugin()

    functions = dict(EXPLICIT_FUNCTIONS)
    xref_summary = {}
    for target_ea, target_name in DATA_TARGETS.items():
        refs = []
        for xref in idautils.XrefsTo(target_ea):
            owner = ida_funcs.get_func(xref.frm)
            owner_ea = owner.start_ea if owner else xref.frm
            refs.append((xref.frm, owner_ea, int(xref.type), bool(xref.iscode)))
            if owner is not None:
                functions.setdefault(owner.start_ea, f"xref_{target_name}")
        xref_summary[target_ea] = (target_name, refs)

    with open(output_path, "w", encoding="utf-8") as output:
        output.write(f"IDB={idb_path}\n")
        output.write(f"DECOMPILER_AVAILABLE={decompiler_available}\n")
        output.write("\n--- XREF SUMMARY ---\n")
        for target_ea, (target_name, refs) in xref_summary.items():
            output.write(f"0x{target_ea:X} {target_name} refs={len(refs)}\n")
            for source_ea, owner_ea, ref_type, is_code in refs:
                output.write(
                    f"  from=0x{source_ea:X} owner=0x{owner_ea:X} "
                    f"type={ref_type} is_code={is_code}\n"
                )
        for start_ea, research_name in sorted(functions.items()):
            dump_function(output, start_ea, research_name, decompiler_available)

    print(f"OPENLEGEND_IDA_INPUT_FONT_REPORT={output_path}")
    print(f"OPENLEGEND_IDA_INPUT_FONT_FUNCTIONS={len(functions)}")
    ida_pro.qexit(0)


try:
    main()
except Exception:
    traceback.print_exc()
    ida_pro.qexit(1)
