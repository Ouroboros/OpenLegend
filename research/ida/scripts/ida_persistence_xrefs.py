# IDAPython 9.x headless exporter for B3 persistence-state xrefs.

import collections
import os
import traceback

import ida_auto
import ida_bytes
import ida_funcs
import ida_lines
import ida_pro
import idautils
import idc


RANGES = (
    ("ranger_header", 0xC0834, 0xC0B78, 2),
    ("roles_320x182", 0x9014C, 0x9E4CC, 182),
    ("items_200x190", 0xA2744, 0xABBB4, 190),
    ("scenes_84x52", 0x9E4CC, 0x9F5DC, 52),
    ("magics_93x136", 0x9F5DC, 0xA2744, 136),
    ("shops_5x30", 0xDC694, 0xDC72A, 30),
)

# This immediate is the Mode 13h display destination used by sub_20039. Its
# numeric value happens to fall inside the DS-relative magic-record interval,
# but it is not a game-state reference.
EXCLUDED_REFERENCES = {(0x2004D, 0xA0000): "Mode 13h display destination"}


def classify(target):
    for name, begin, end, record_size in RANGES:
        if begin <= target < end:
            return name, begin, end, record_size
    return None


def main():
    ida_auto.auto_wait()
    idb_path = idc.get_idb_path()
    ida_root = os.path.dirname(os.path.dirname(idb_path))
    output_path = os.path.join(ida_root, "reports", "Z_DAT.persistence_xrefs.txt")

    references = collections.defaultdict(list)
    for segment_start in idautils.Segments():
        segment_end = idc.get_segm_end(segment_start)
        for ea in idautils.Heads(segment_start, segment_end):
            if not ida_bytes.is_code(ida_bytes.get_flags(ea)):
                continue
            owner = ida_funcs.get_func(ea)
            owner_start = owner.start_ea if owner else ea
            owner_name = ida_funcs.get_func_name(owner_start) if owner else "<no-function>"
            disassembly = ida_lines.tag_remove(idc.generate_disasm_line(ea, 0) or "")
            seen = set()
            for target in idautils.DataRefsFrom(ea):
                if target in seen:
                    continue
                seen.add(target)
                if (ea, target) in EXCLUDED_REFERENCES:
                    continue
                classification = classify(target)
                if classification is None:
                    continue
                name, begin, _end, record_size = classification
                byte_offset = target - begin
                references[name].append(
                    (target, byte_offset, byte_offset // record_size, byte_offset % record_size,
                     ea, owner_start, owner_name, disassembly)
                )

    with open(output_path, "w", encoding="utf-8") as output:
        output.write(f"IDB={idb_path}\n")
        output.write("SOURCE=machine-code data references in current Z.DAT database\n")
        output.write("NOTE=record/field values describe the referenced base displacement; indexed effective addresses may select later records or array elements at runtime.\n")
        output.write("EXCLUDED=0x2004D -> 0xA0000 (Mode 13h display destination, not DS-relative magic state)\n")
        for name, begin, end, record_size in RANGES:
            rows = sorted(references.get(name, ()), key=lambda row: (row[0], row[4]))
            functions = {row[5] for row in rows}
            output.write("\n" + "=" * 100 + "\n")
            output.write(
                f"RANGE {name} begin=0x{begin:X} end=0x{end:X} bytes={end-begin} "
                f"record_size={record_size} refs={len(rows)} functions={len(functions)}\n"
            )
            for row in rows:
                target, byte_offset, record, field_byte, ea, owner_start, owner_name, disassembly = row
                output.write(
                    f"target=0x{target:X} byte_offset={byte_offset} record={record} "
                    f"field_byte={field_byte} from=0x{ea:X} owner=0x{owner_start:X} "
                    f"{owner_name} :: {disassembly}\n"
                )

    print(f"OPENLEGEND_IDA_PERSISTENCE_REPORT={output_path}")
    for name, _begin, _end, _record_size in RANGES:
        rows = references.get(name, ())
        print(
            f"OPENLEGEND_IDA_PERSISTENCE_RANGE={name} refs={len(rows)} "
            f"functions={len({row[5] for row in rows})}"
        )
    ida_pro.qexit(0)


try:
    main()
except Exception:
    traceback.print_exc()
    ida_pro.qexit(1)
