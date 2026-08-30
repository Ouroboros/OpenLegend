# IDAPython 9.x headless exporter for B4 input/time/RNG/audio evidence.

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
    ("keyboard_irq_source_block", 0x20BC0, 0x20C32),
    ("keyboard_irq_installed_trampoline", 0x3CDE3, 0x3CDFF),
    ("scan_code_translation_table", 0x51B16, 0x51B6A),
    ("keyboard_last_scan_last_key_and_state", 0x51B6A, 0x51C0D),
)

FORCED_CODE_RANGES = (
    ("keyboard_irq_source_block", 0x20BC0, 0x20C32),
    ("keyboard_irq_installed_trampoline", 0x3CDE3, 0x3CDFF),
)

TARGET_FUNCTIONS = (
    ("blocking_read_key", 0x20C32),
    ("main_loop", 0x20D35),
    ("keyboard_audio_font_initialize", 0x3CDFF),
    ("keyboard_restore", 0x3CF19),
    ("bios_tick_delay", 0x3DB83),
    ("audio_shutdown", 0x3DD57),
    ("audio_initialize", 0x3DD66),
    ("music_start_once_unused", 0x3DE4D),
    ("music_start_looping", 0x3DECB),
    ("music_end", 0x3DF59),
    ("sample_start_loaded", 0x3DF90),
    ("sample_start_raw_unused", 0x3E088),
    ("sample_end", 0x3E172),
    ("music_play_index", 0x3E1B2),
    ("music_fade_in", 0x3E23B),
    ("music_fade_out", 0x3E25B),
    ("sample_dispatch_loaded", 0x3E288),
    ("sample_load_index", 0x3E2E2),
    ("seed_from_dos_time", 0x3D5DE),
    ("rng_bounded", 0x3D612),
    ("rng_state_address", 0x3F987),
    ("rng_next", 0x3F98D),
    ("rng_seed", 0x3F9B0),
    ("rng_runtime_initialize", 0x3F9C0),
    ("miles_init_sample", 0x41232),
    ("miles_set_sample_data", 0x41383),
    ("miles_set_sample_type", 0x413F8),
    ("miles_start_sample", 0x4146D),
    ("miles_end_sample", 0x4159C),
    ("miles_set_sample_rate", 0x41601),
    ("miles_set_sample_volume", 0x4166E),
    ("miles_set_sample_loop_count", 0x41748),
    ("miles_sample_status", 0x417B5),
    ("miles_init_sequence", 0x42AF2),
    ("miles_start_sequence", 0x42BDE),
    ("miles_stop_sequence", 0x42D0D),
    ("miles_fade_sequence", 0x42DE7),
    ("miles_set_sequence_loop_count", 0x42E5C),
    ("miles_sequence_status", 0x42EC9),
    ("miles_set_music_master_volume", 0x43239),
    ("miles_apply_sample_volume", 0x47DB8),
    ("miles_internal_set_sample_volume", 0x49120),
)

DATA_TARGETS = (
    ("bios_tick_pointer", 0x544D8),
    ("last_raw_scan_code", 0x51B6A),
    ("last_translated_key", 0x51B6B),
    ("translated_key_state", 0x51B6D),
    ("audio_sample_handles", 0x118810),
    ("audio_sequence_handle", 0x118838),
    ("audio_digital_driver", 0x11883C),
    ("audio_music_driver", 0x118840),
    ("audio_disabled_sound", 0x118844),
    ("audio_disabled_music", 0x118846),
)

CALL_TARGETS = (
    ("music_start_once_unused", 0x3DE4D),
    ("music_start_looping", 0x3DECB),
    ("music_end", 0x3DF59),
    ("sample_start_loaded", 0x3DF90),
    ("sample_start_raw_unused", 0x3E088),
    ("sample_end", 0x3E172),
    ("music_play_index", 0x3E1B2),
    ("music_fade_in", 0x3E23B),
    ("music_fade_out", 0x3E25B),
    ("sample_dispatch_loaded", 0x3E288),
    ("sample_load_index", 0x3E2E2),
    ("rng_bounded", 0x3D612),
    ("rng_next", 0x3F98D),
    ("rng_seed", 0x3F9B0),
    ("bios_tick_delay", 0x3DB83),
)

DATA_INTERVALS = (
    ("keyboard_state_region", 0x51B6A, 0x51C0D),
    ("audio_runtime_region", 0x118810, 0x118848),
)


def line(ea):
    return ida_lines.tag_remove(idc.generate_disasm_line(ea, 0) or "")


def dump_bytes(output, label, start, end):
    output.write("\n" + "=" * 100 + "\n")
    output.write(f"BYTES {label} start=0x{start:X} end=0x{end:X} size={end-start}\n")
    data = ida_bytes.get_bytes(start, end - start) or b""
    for offset in range(0, len(data), 16):
        chunk = data[offset:offset + 16]
        output.write(f"{start + offset:08X}  {' '.join(f'{value:02x}' for value in chunk)}\n")


def decode_range(output, label, start, end):
    output.write("\n" + "=" * 100 + "\n")
    output.write(f"DECODED {label} start=0x{start:X} end=0x{end:X}\n")
    ea = start
    while ea < end:
        size = ida_ua.create_insn(ea)
        if size <= 0 or ea + size > end:
            raw = ida_bytes.get_byte(ea)
            output.write(f"{ea:08X}  {raw:02x}                                db      {raw:02X}h\n")
            ea += 1
            continue
        raw = ida_bytes.get_bytes(ea, size) or b""
        output.write(f"{ea:08X}  {' '.join(f'{value:02x}' for value in raw):<32}  {line(ea)}\n")
        ea += size


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
        f"start=0x{function.start_ea:X} end=0x{function.end_ea:X} size={function.end_ea-function.start_ea}\n"
    )
    for ea in idautils.Heads(function.start_ea, function.end_ea):
        if not ida_bytes.is_code(ida_bytes.get_flags(ea)):
            continue
        raw = ida_bytes.get_bytes(ea, ida_bytes.get_item_size(ea)) or b""
        output.write(f"{ea:08X}  {' '.join(f'{value:02x}' for value in raw):<32}  {line(ea)}\n")


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
    output_path = os.path.join(ida_root, "reports", "Z_DAT.b4_runtime_xrefs.txt")

    # These regions were emitted as embedded code but were not recognized as
    # functions by the original auto-analysis. Decode them transiently; the
    # caller restores the tracked database after this exporter exits.
    for _label, start, end in FORCED_CODE_RANGES:
        ida_bytes.del_items(start, ida_bytes.DELIT_SIMPLE, end - start)
        ea = start
        while ea < end:
            size = ida_ua.create_insn(ea)
            ea += size if size > 0 else 1

    exact, intervals = collect_data_references()

    with open(output_path, "w", encoding="utf-8") as output:
        output.write(f"IDB={idb_path}\n")
        output.write("SOURCE=current Z.DAT machine code decoded by IDA 9.x headless\n")
        output.write("NOTE=embedded IRQ regions are transiently decoded; tracked IDB is restored by the host command.\n")

        for label, start, end in RAW_RANGES:
            dump_bytes(output, label, start, end)
        for label, start, end in FORCED_CODE_RANGES:
            decode_range(output, label, start, end)
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

        output.write("\n" + "=" * 100 + "\n")
        output.write("FUNCTIONS AROUND AUDIO WRAPPERS start=0x3DD00 end=0x3E300\n")
        function = ida_funcs.get_next_func(0x3DCFF)
        while function and function.start_ea < 0x3E300:
            output.write(
                f"0x{function.start_ea:X}..0x{function.end_ea:X} "
                f"{ida_funcs.get_func_name(function.start_ea)}\n"
            )
            function = ida_funcs.get_next_func(function.start_ea)

    print(f"OPENLEGEND_IDA_B4_REPORT={output_path}")
    ida_pro.qexit(0)


try:
    main()
except Exception:
    traceback.print_exc()
    ida_pro.qexit(1)
