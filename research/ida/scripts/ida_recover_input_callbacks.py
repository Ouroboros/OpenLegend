# IDAPython 9.x headless recovery of two unrecognized input callback blocks.

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


FUNCTION_RANGES = {
    (0x20016, idc.BADADDR): "set_video_mode_13_and_framebuffer",
    (0x3CDE3, 0x3CDFF): "irq1_keyboard_isr_stub",
    (0x3CF19, idc.BADADDR): "restore_keyboard_interrupt",
    (0x3D5DE, idc.BADADDR): "initialize_keyboard_state",
    (0x3DD57, idc.BADADDR): "stop_music_before_shutdown",
    (0x3DD66, idc.BADADDR): "initialize_audio_system",
}

DATA_RANGES = {
    (0x20BC0, 0x20C32): "keyboard_translation_or_xref_table",
    (0x51B6B, 0x51C0D): "last_key_and_key_state_array_initial_bytes",
}


def dump(output, start_ea, end_ea, research_name, decompiler_available):
    function = ida_funcs.get_func(start_ea)
    if function is None and end_ea != idc.BADADDR:
        ida_funcs.add_func(start_ea, end_ea)
        function = ida_funcs.get_func(start_ea)
    if function is None:
        output.write(f"MISSING {research_name} 0x{start_ea:X}\n")
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
            output.write(str(ida_hexrays.decompile(function.start_ea)) + "\n")
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
    output_path = os.path.join(ida_root, "reports", "Z_DAT.input_callbacks.txt")
    decompiler_available = ida_hexrays.init_hexrays_plugin()
    with open(output_path, "w", encoding="utf-8") as output:
        output.write(f"IDB={idb_path}\n")
        output.write(f"tick_pointer_stored_value=0x{ida_bytes.get_dword(0x544D8):X}\n")
        for (start_ea, end_ea), research_name in DATA_RANGES.items():
            output.write("\n\n" + "=" * 100 + "\n")
            output.write(f"DATA {research_name} start=0x{start_ea:X} end=0x{end_ea:X}\n")
            data = ida_bytes.get_bytes(start_ea, end_ea - start_ea) or b""
            for offset in range(0, len(data), 16):
                chunk = data[offset:offset + 16]
                output.write(f"{start_ea + offset:08X}  {chunk.hex(' ')}\n")
        for (start_ea, end_ea), research_name in FUNCTION_RANGES.items():
            dump(output, start_ea, end_ea, research_name, decompiler_available)
    print(f"OPENLEGEND_IDA_INPUT_CALLBACK_REPORT={output_path}")
    ida_pro.qexit(0)


try:
    main()
except Exception:
    traceback.print_exc()
    ida_pro.qexit(1)
