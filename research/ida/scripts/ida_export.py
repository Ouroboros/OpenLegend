# IDAPython 9.x headless research exporter for OpenLegend.
# This script only exports analysis metadata; it does not patch the input binary.

import hashlib
import json
import os
import traceback

import ida_auto
import ida_bytes
import ida_funcs
import ida_gdl
import ida_ida
import ida_idp
import ida_loader
import ida_name
import ida_nalt
import ida_pro
import ida_segment
import idautils
import idc


def hex_ea(value):
    return f"0x{int(value):X}"


def input_sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        while True:
            block = source.read(1024 * 1024)
            if not block:
                break
            digest.update(block)
    return digest.hexdigest()


def collect_segments():
    result = []
    for ea in idautils.Segments():
        segment = ida_segment.getseg(ea)
        if segment is None:
            continue
        result.append(
            {
                "name": ida_segment.get_segm_name(segment),
                "class": ida_segment.get_segm_class(segment),
                "start": hex_ea(segment.start_ea),
                "end": hex_ea(segment.end_ea),
                "size": int(segment.end_ea - segment.start_ea),
                "bitness": int(segment.bitness),
                "permissions": int(segment.perm),
            }
        )
    return result


def collect_entries():
    result = []
    for index, ordinal, ea, name in idautils.Entries():
        result.append(
            {
                "index": int(index),
                "ordinal": int(ordinal),
                "address": hex_ea(ea),
                "name": name,
            }
        )
    return result


def collect_strings():
    result = []
    strings = idautils.Strings()
    strings.setup(minlen=4)
    for item in strings:
        xrefs = []
        for xref in idautils.XrefsTo(item.ea):
            xrefs.append(
                {
                    "from": hex_ea(xref.frm),
                    "type": int(xref.type),
                    "is_code": bool(xref.iscode),
                }
            )
            if len(xrefs) >= 32:
                break
        result.append(
            {
                "address": hex_ea(item.ea),
                "length": int(item.length),
                "type": int(item.strtype),
                "text": str(item),
                "xrefs": xrefs,
            }
        )
    return result


def collect_function(ea, string_addresses):
    function = ida_funcs.get_func(ea)
    if function is None:
        return None

    callees = set()
    string_refs = set()
    instruction_count = 0
    for head in idautils.Heads(function.start_ea, function.end_ea):
        if not ida_bytes.is_code(ida_bytes.get_flags(head)):
            continue
        instruction_count += 1
        for target in idautils.CodeRefsFrom(head, False):
            target_function = ida_funcs.get_func(target)
            if target_function is not None:
                callees.add(target_function.start_ea)
        for target in idautils.DataRefsFrom(head):
            if target in string_addresses:
                string_refs.add(target)

    callers = set()
    for source in idautils.CodeRefsTo(function.start_ea, False):
        source_function = ida_funcs.get_func(source)
        callers.add(source_function.start_ea if source_function else source)

    try:
        basic_blocks = sum(1 for _ in ida_gdl.FlowChart(function))
    except Exception:
        basic_blocks = None

    chunks = [
        {"start": hex_ea(start), "end": hex_ea(end), "size": int(end - start)}
        for start, end in idautils.Chunks(function.start_ea)
    ]

    return {
        "address": hex_ea(function.start_ea),
        "end": hex_ea(function.end_ea),
        "size": int(function.end_ea - function.start_ea),
        "name": ida_funcs.get_func_name(function.start_ea),
        "flags": int(function.flags),
        "instruction_count": instruction_count,
        "basic_blocks": basic_blocks,
        "chunks": chunks,
        "callers": [hex_ea(value) for value in sorted(callers)],
        "callees": [hex_ea(value) for value in sorted(callees)],
        "string_refs": [hex_ea(value) for value in sorted(string_refs)],
    }


def collect_entry_disassembly(entries, maximum=1024):
    result = []
    starts = [int(entry["address"], 16) for entry in entries]
    if not starts:
        starts = [ida_ida.inf_get_start_ea()]

    seen = set()
    for start in starts:
        function = ida_funcs.get_func(start)
        end = function.end_ea if function else min(start + 0x800, ida_ida.inf_get_max_ea())
        ea = start
        while ea < end and len(result) < maximum:
            if ea in seen:
                break
            seen.add(ea)
            line = idc.generate_disasm_line(ea, 0) or ""
            result.append(
                {
                    "address": hex_ea(ea),
                    "bytes": ida_bytes.get_bytes(ea, min(16, max(1, idc.get_item_size(ea)))).hex(),
                    "text": ida_lines_tag_remove(line),
                }
            )
            next_ea = idc.next_head(ea, end)
            if next_ea == idc.BADADDR or next_ea <= ea:
                break
            ea = next_ea
    return result


def ida_lines_tag_remove(text):
    try:
        import ida_lines

        return ida_lines.tag_remove(text)
    except Exception:
        return text


def export_pseudocode(functions, output_path):
    exported = []
    try:
        import ida_hexrays

        if not ida_hexrays.init_hexrays_plugin():
            return {"available": False, "functions": []}

        ranked = sorted(
            functions,
            key=lambda item: (
                item["name"] not in {"start", "main", "WinMain"},
                -len(item["callers"]),
                -item["size"],
            ),
        )
        with open(output_path, "w", encoding="utf-8") as target:
            for item in ranked[:80]:
                ea = int(item["address"], 16)
                try:
                    pseudocode = str(ida_hexrays.decompile(ea))
                except Exception:
                    continue
                target.write(
                    f"\n\n===== {item['name']} @ {item['address']} size={item['size']} =====\n"
                )
                target.write(pseudocode)
                target.write("\n")
                exported.append(item["address"])
        return {"available": True, "functions": exported}
    except Exception as error:
        return {"available": False, "error": repr(error), "functions": exported}


def main():
    ida_auto.auto_wait()

    idb_path = idc.get_idb_path()
    artifact_name = os.path.splitext(os.path.basename(idb_path))[0]
    ida_research_root = os.path.dirname(os.path.dirname(idb_path))
    report_path = os.path.join(
        ida_research_root, "reports", artifact_name + ".report.json"
    )
    pseudocode_path = os.path.join(
        ida_research_root, "reports", artifact_name + ".pseudocode.txt"
    )
    input_path = idc.get_input_file_path()

    segments = collect_segments()
    entries = collect_entries()
    strings = collect_strings()
    string_addresses = {int(item["address"], 16) for item in strings}

    functions = []
    for ea in idautils.Functions():
        item = collect_function(ea, string_addresses)
        if item is not None:
            functions.append(item)

    report = {
        "input": {
            "path": input_path,
            "size": os.path.getsize(input_path),
            "sha256": input_sha256(input_path),
            "idb_path": idb_path,
        },
        "database": {
            "processor": ida_idp.get_idp_name(),
            "file_type": ida_loader.get_file_type_name(),
            "min_ea": hex_ea(ida_ida.inf_get_min_ea()),
            "max_ea": hex_ea(ida_ida.inf_get_max_ea()),
            "start_ea": hex_ea(ida_ida.inf_get_start_ea()),
            "start_ip": hex_ea(ida_ida.inf_get_start_ip()),
            "start_cs": hex_ea(ida_ida.inf_get_start_cs()),
            "is_64bit": bool(ida_ida.inf_is_64bit()),
            "is_32bit_exactly": bool(ida_ida.inf_is_32bit_exactly()),
        },
        "segments": segments,
        "entries": entries,
        "strings": strings,
        "functions": functions,
        "entry_disassembly": collect_entry_disassembly(entries),
    }
    report["decompiler"] = export_pseudocode(functions, pseudocode_path)

    with open(report_path, "w", encoding="utf-8") as target:
        json.dump(report, target, ensure_ascii=False, indent=2)

    print(f"OPENLEGEND_IDA_REPORT={report_path}")
    print(f"OPENLEGEND_IDA_FUNCTIONS={len(functions)}")
    print(f"OPENLEGEND_IDA_STRINGS={len(strings)}")
    print(f"OPENLEGEND_IDA_SEGMENTS={len(segments)}")
    print(f"OPENLEGEND_IDA_DECOMPILED={len(report['decompiler'].get('functions', []))}")
    ida_pro.qexit(0)


try:
    main()
except Exception:
    traceback.print_exc()
    ida_pro.qexit(1)
