# IDAPython 9.x headless exporter for B8 battle evidence.

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


BATTLE_BEGIN = 0x31C75
BATTLE_END = 0x3CBE3


def line(ea):
    return ida_lines.tag_remove(idc.generate_disasm_line(ea, 0) or "")


def string_at(ea):
    value = idc.get_strlit_contents(ea)
    if value is None:
        return None
    return value.decode("ascii", errors="backslashreplace")


def battle_functions():
    result = []
    for address in idautils.Functions(BATTLE_BEGIN, BATTLE_END):
        function = ida_funcs.get_func(address)
        if function is not None and function.start_ea == address:
            result.append(function)
    return sorted(result, key=lambda function: function.start_ea)


def dump_function(output, function):
    research = f"battle_{function.start_ea:X}"
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


def collect_references(functions):
    starts = {function.start_ea for function in functions}
    calls = collections.defaultdict(list)
    data = collections.defaultdict(list)
    for function in functions:
        for ea in idautils.Heads(function.start_ea, function.end_ea):
            if not ida_bytes.is_code(ida_bytes.get_flags(ea)):
                continue
            for target in idautils.CodeRefsFrom(ea, False):
                target_function = ida_funcs.get_func(target)
                if target_function is None or target_function.start_ea == function.start_ea:
                    continue
                calls[target_function.start_ea].append(
                    (ea, function.start_ea, ida_funcs.get_func_name(function.start_ea), line(ea))
                )
            for target in idautils.DataRefsFrom(ea):
                if target in starts:
                    continue
                data[target].append(
                    (ea, function.start_ea, ida_funcs.get_func_name(function.start_ea), line(ea))
                )
    return calls, data


def main():
    ida_auto.auto_wait()
    idb_path = idc.get_idb_path()
    ida_root = os.path.dirname(os.path.dirname(idb_path))
    output_path = os.path.join(ida_root, "reports", "Z_DAT.b8_battle_xrefs.txt")
    functions = battle_functions()
    if not functions or functions[0].start_ea != BATTLE_BEGIN or functions[-1].end_ea != BATTLE_END:
        raise RuntimeError(
            f"unexpected battle range: count={len(functions)} "
            f"first={functions[0].start_ea if functions else -1:X} "
            f"last={functions[-1].end_ea if functions else -1:X}"
        )
    calls, data = collect_references(functions)
    with open(output_path, "w", encoding="utf-8", newline="\n") as output:
        output.write(f"IDB={idb_path}\n")
        output.write("SOURCE=current Z.DAT machine code decoded by IDA 9.x headless\n")
        output.write(
            f"BATTLE_RANGE start=0x{BATTLE_BEGIN:X} end=0x{BATTLE_END:X} "
            f"functions={len(functions)}\n"
        )
        for function in functions:
            dump_function(output, function)

        output.write("\n" + "=" * 100 + "\n")
        output.write("CALL REFERENCES\n")
        for target in sorted(calls):
            rows = sorted(calls[target])
            owner = ida_funcs.get_func(target)
            target_name = ida_funcs.get_func_name(owner.start_ea) if owner else "<no-function>"
            scope = "battle" if BATTLE_BEGIN <= target < BATTLE_END else "external"
            output.write(
                f"\nTARGET address=0x{target:X} name={target_name} scope={scope} refs={len(rows)}\n"
            )
            for ea, owner_start, owner_name, disassembly in rows:
                output.write(
                    f"from=0x{ea:X} owner=0x{owner_start:X} {owner_name} :: {disassembly}\n"
                )

        output.write("\n" + "=" * 100 + "\n")
        output.write("DATA REFERENCES\n")
        for target in sorted(data):
            rows = sorted(data[target])
            value = string_at(target)
            suffix = f" string={value!r}" if value is not None else ""
            output.write(f"\nTARGET address=0x{target:X} refs={len(rows)}{suffix}\n")
            for ea, owner_start, owner_name, disassembly in rows:
                output.write(
                    f"from=0x{ea:X} owner=0x{owner_start:X} {owner_name} :: {disassembly}\n"
                )

    print(f"OPENLEGEND_IDA_B8_REPORT={output_path}")
    print(f"OPENLEGEND_IDA_B8_FUNCTIONS={len(functions)}")
    ida_pro.qexit(0)


try:
    main()
except Exception:
    traceback.print_exc()
    ida_pro.qexit(1)
