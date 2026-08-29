# IDAPython 9.x headless targeted decompiler for OpenLegend research.

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


TARGETS = {
    0x20D35: "game_main",
    0x20FAF: "title_loader",
    0x212C0: "main_action_bit0",
    0x2399E: "main_idle_update",
    0x23AA6: "main_optional_update",
    0x23B3E: "main_frame_update",
    0x23C98: "main_horizontal_input",
    0x23F28: "main_vertical_input",
    0x247DD: "base_resource_loader",
    0x24C23: "world_layer_loader",
    0x24F8C: "world_tile_logic",
    0x2558B: "main_render_or_present",
    0x25AB7: "allsin_copy_or_reset",
    0x26208: "ranger_slot_loader",
    0x265AB: "ranger_slot_writer_or_loader",
    0x26B5E: "scene_or_map_loader_a",
    0x27A26: "title_or_menu_flow",
    0x28E40: "scene_or_map_loader_b",
    0x2DBF4: "alldef_restore_a",
    0x2E337: "allsin_restore_a",
    0x2E659: "death_screen_or_status",
    0x2F721: "allsin_restore_b",
    0x30A5A: "alldef_restore_b",
    0x30C3D: "ending_flow",
    0x31C75: "battle_asset_loader",
    0x31DA0: "battlefield_loader",
    0x3859E: "fight_animation_loader",
    0x20039: "lowlevel_present_framebuffer",
    0x2005B: "lowlevel_clear_framebuffer",
    0x20087: "lowlevel_set_vga_palette",
    0x2010A: "lowlevel_blit_rectangle",
    0x20354: "lowlevel_decode_rle_sprite",
    0x20B22: "shadow_mask_effect",
    0x3CBE3: "periodic_tick_update",
    0x3CC97: "video_or_framebuffer_reset",
    0x3CD17: "video_transition_or_fade",
    0x3CDFF: "font_loader",
    0x3CF45: "generic_file_loader_a",
    0x3D1E5: "lowlevel_draw_text",
    0x3D612: "random_number",
    0x3D643: "draw_rle_sprite",
    0x3D6D1: "present_framebuffer",
    0x3D832: "draw_text",
    0x3D8D8: "blit_or_fill_rectangle",
    0x3D922: "clear_or_palette_transition",
    0x3D939: "set_palette",
    0x3DB83: "delay_ticks",
    0x3D6E0: "generic_file_loader_b",
    0x3DD66: "sound_mode_setup",
    0x3E1B2: "music_table_loader",
    0x3E2E2: "wave_name_builder",
    0x3F234: "watcom_startup",
    0x3F98D: "watcom_random_source",
    0x44A44: "miles_ini_parser",
    0x48814: "digital_audio_driver_install",
    0x48E94: "dig_ini_loader",
    0x4B584: "midi_driver_install",
    0x4BC7C: "mdi_ini_loader",
}


def cleaned_disassembly(function):
    lines = []
    for ea in idautils.Heads(function.start_ea, function.end_ea):
        if not ida_bytes.is_code(ida_bytes.get_flags(ea)):
            continue
        raw = ida_bytes.get_bytes(ea, max(1, idc.get_item_size(ea))) or b""
        text = ida_lines.tag_remove(idc.generate_disasm_line(ea, 0) or "")
        lines.append(f"{ea:08X}  {raw.hex(' '):<32}  {text}")
    return "\n".join(lines)


def main():
    ida_auto.auto_wait()
    idb_path = idc.get_idb_path()
    ida_root = os.path.dirname(os.path.dirname(idb_path))
    output_path = os.path.join(ida_root, "reports", "Z_DAT.targets.txt")

    decompiler_available = ida_hexrays.init_hexrays_plugin()
    with open(output_path, "w", encoding="utf-8") as output:
        output.write(f"IDB={idb_path}\n")
        output.write(f"DECOMPILER_AVAILABLE={decompiler_available}\n")
        for requested_ea, research_name in TARGETS.items():
            function = ida_funcs.get_func(requested_ea)
            output.write("\n\n" + "=" * 100 + "\n")
            output.write(f"TARGET {research_name} requested=0x{requested_ea:X}\n")
            if function is None:
                output.write("FUNCTION_NOT_FOUND\n")
                continue
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
            else:
                output.write("DECOMPILER_UNAVAILABLE\n")
            output.write("\n--- DISASSEMBLY ---\n")
            output.write(cleaned_disassembly(function))
            output.write("\n")

    print(f"OPENLEGEND_IDA_TARGET_REPORT={output_path}")
    print(f"OPENLEGEND_IDA_TARGET_COUNT={len(TARGETS)}")
    ida_pro.qexit(0)


try:
    main()
except Exception:
    traceback.print_exc()
    ida_pro.qexit(1)
