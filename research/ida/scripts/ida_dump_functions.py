# IDAPython 9.x headless full-function dump for small research targets.

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


def main():
    ida_auto.auto_wait()
    idb_path = idc.get_idb_path()
    ida_root = os.path.dirname(os.path.dirname(idb_path))
    artifact_name = os.path.splitext(os.path.basename(idb_path))[0]
    output_path = os.path.join(
        ida_root, "reports", artifact_name + ".functions.txt"
    )
    decompiler_available = ida_hexrays.init_hexrays_plugin()

    with open(output_path, "w", encoding="utf-8") as output:
        output.write(f"IDB={idb_path}\n")
        output.write(f"DECOMPILER_AVAILABLE={decompiler_available}\n")
        for start_ea in idautils.Functions():
            function = ida_funcs.get_func(start_ea)
            if function is None:
                continue
            output.write("\n\n" + "=" * 100 + "\n")
            output.write(
                f"FUNCTION {ida_funcs.get_func_name(function.start_ea)} "
                f"start=0x{function.start_ea:X} end=0x{function.end_ea:X} "
                f"size={function.end_ea - function.start_ea}\n"
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

    print(f"OPENLEGEND_IDA_FUNCTION_DUMP={output_path}")
    ida_pro.qexit(0)


try:
    main()
except Exception:
    traceback.print_exc()
    ida_pro.qexit(1)
