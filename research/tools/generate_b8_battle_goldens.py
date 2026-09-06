#!/usr/bin/env python3
"""Generate independent B8 battle asset goldens from original read-only files."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
from pathlib import Path


WAR_RECORD_SIZE = 186
Z_DAT_LOAD_BASE = 0x6600
BATTLE_ENTRY_ADDRESS = 0x31C75
BATTLE_ENTRY_END = 0x31DA0
BATTLE_ENTRY_CALL_OFFSETS = (
    0x05, 0x29, 0x3A, 0x3F, 0x6A, 0x72, 0x94,
    0x9C, 0xAC, 0xBC, 0xC4, 0xC9, 0xEE, 0x111,
)
BATTLE_ENTRY_RELOCATION_OFFSETS = (
    0x10, 0x17, 0x23, 0x34, 0x46, 0x4D, 0x55, 0x5D,
    0x65, 0x78, 0x85, 0x8B, 0x90, 0xA3, 0xA8, 0xB7,
    0xD0, 0xD9, 0xE1, 0xE9, 0xF9, 0x103, 0x11C, 0x125,
)
BATTLE_DATA_LOADER_ADDRESS = 0x31DA0
BATTLE_DATA_LOADER_END = 0x31EB9
BATTLE_DATA_LOADER_CALL_OFFSETS = (
    0x05, 0x11, 0x35, 0x48, 0x51, 0x85,
    0x97, 0x9F, 0xB2, 0xC9, 0xDC, 0xE5,
)
BATTLE_DATA_LOADER_RELOCATION_OFFSETS = (
    0x0D, 0x43, 0x5C, 0x63, 0x6A, 0x73, 0x7C,
    0x81, 0x93, 0xA7, 0xAE, 0xC4, 0xD7, 0x102,
)
BATTLE_PARTY_SETUP_ADDRESS = 0x31EB9
BATTLE_PARTY_SETUP_END = 0x3265C
BATTLE_PARTY_SETUP_CALL_OFFSETS = (
    0x005, 0x0C5, 0x178, 0x247, 0x2E9, 0x307, 0x319,
    0x336, 0x358, 0x38B, 0x457, 0x483, 0x49F, 0x4C4,
    0x4FD, 0x5D8, 0x5F4, 0x607, 0x705,
)
BATTLE_ENEMY_SETUP_ADDRESS = 0x3265C
BATTLE_ENEMY_SETUP_END = 0x3271E
BATTLE_ENEMY_SETUP_CALL_OFFSETS = (0x05, 0x6F)
BATTLE_REST_WRAPPER_ADDRESS = 0x34AD3
BATTLE_REST_WRAPPER_END = 0x34AEC
BATTLE_REST_WRAPPER_CALL_OFFSETS = (0x05, 0x10)
BATTLE_REST_WRAPPER_CALLER_SITES = (
    0x33BB1, 0x34C37, 0x3503C, 0x355F3, 0x363A0, 0x36504,
)
BATTLE_ESCAPE_PLAN_ADDRESS = 0x34AEC
BATTLE_ESCAPE_PLAN_END = 0x34C47
BATTLE_ESCAPE_PLAN_CALL_OFFSETS = (0x005, 0x03C, 0x0AB, 0x0C3, 0x136, 0x14B)
BATTLE_ESCAPE_PLAN_RELOCATION_OFFSETS = (
    0x024, 0x02B, 0x032, 0x038, 0x06B, 0x072, 0x090,
    0x097, 0x0A0, 0x0B8, 0x0D3, 0x11F, 0x128,
)
BATTLE_ESCAPE_PLAN_CALLER_SITES = (0x33C31, 0x35816)
BATTLE_AI_ATTACK_HANDLER_ADDRESS = 0x34C47
BATTLE_AI_ATTACK_HANDLER_END = 0x3505B
BATTLE_AI_ATTACK_HANDLER_CALL_OFFSETS = (
    0x005, 0x04F, 0x11A, 0x168, 0x20B, 0x239, 0x280, 0x312, 0x363, 0x3F5,
)
BATTLE_AI_ATTACK_HANDLER_RELOCATION_OFFSETS = (
    0x02B, 0x03C, 0x059, 0x060, 0x073, 0x080, 0x08E, 0x095,
    0x09E, 0x0A5, 0x0AE, 0x0B4, 0x0C8, 0x0D5, 0x0E0, 0x0F5,
    0x109, 0x115, 0x125, 0x12B, 0x136, 0x13D, 0x144, 0x14A,
    0x151, 0x157, 0x15E, 0x164, 0x170, 0x17A, 0x184, 0x18C,
    0x1D6, 0x1DD, 0x1E6, 0x1ED, 0x221, 0x244, 0x24E, 0x255,
    0x25C, 0x262, 0x269, 0x26F, 0x276, 0x27C, 0x288, 0x292,
    0x29C, 0x2A4, 0x2E2, 0x2E9, 0x2F2, 0x2F9, 0x320, 0x326,
    0x331, 0x338, 0x33F, 0x345, 0x34C, 0x352, 0x359, 0x35F,
    0x36B, 0x375, 0x37F, 0x387, 0x3C5, 0x3CC, 0x3D5, 0x3DC,
    0x406,
)
BATTLE_AI_ATTACK_HANDLER_CALLER_SITES = (
    0x33BD7, 0x35430, 0x35980, 0x36200, 0x36398, 0x364F9,
)
BATTLE_AI_ATTACK_TARGET_ADDRESS = 0x3505B
BATTLE_AI_ATTACK_TARGET_END = 0x3513A
BATTLE_AI_ATTACK_TARGET_CALL_OFFSETS = (
    0x005, 0x02F, 0x03D, 0x06F, 0x07D, 0x0AF, 0x0BD, 0x0D4,
)
BATTLE_AI_ATTACK_TARGET_RELOCATION_OFFSETS = (
    0x019, 0x026, 0x059, 0x066, 0x099, 0x0A6,
)
BATTLE_AI_ATTACK_TARGET_CALLER_SITES = (0x34D61, 0x3583E)
BATTLE_AI_STRONGEST_TARGET_ADDRESS = 0x3513A
BATTLE_AI_STRONGEST_TARGET_END = 0x351A7
BATTLE_AI_STRONGEST_TARGET_CALL_OFFSETS = (0x005,)
BATTLE_AI_STRONGEST_TARGET_RELOCATION_OFFSETS = (
    0x026, 0x02D, 0x036, 0x040, 0x04D, 0x05B, 0x063,
)
BATTLE_AI_STRONGEST_TARGET_CALLER_SITES = (0x35098,)
BATTLE_AI_WEAKEST_TARGET_ADDRESS = 0x351A7
BATTLE_AI_WEAKEST_TARGET_END = 0x35217
BATTLE_AI_WEAKEST_TARGET_CALL_OFFSETS = (0x005,)
BATTLE_AI_WEAKEST_TARGET_RELOCATION_OFFSETS = (
    0x029, 0x030, 0x039, 0x043, 0x050, 0x05E, 0x066,
)
BATTLE_AI_WEAKEST_TARGET_CALLER_SITES = (0x350D8, 0x35363)
BATTLE_AI_SPECIALIST_TARGET_ADDRESS = 0x35217
BATTLE_AI_SPECIALIST_TARGET_END = 0x35372
BATTLE_AI_SPECIALIST_TARGET_CALL_OFFSETS = (0x005, 0x14C)
BATTLE_AI_SPECIALIST_TARGET_RELOCATION_OFFSETS = (
    0x034, 0x03B, 0x044, 0x051, 0x064, 0x085, 0x08C, 0x095, 0x09F,
    0x0AC, 0x0BA, 0x0CF, 0x0EF, 0x0F6, 0x0FF, 0x109, 0x116, 0x124,
    0x13A,
)
BATTLE_AI_SPECIALIST_TARGET_CALLER_SITES = (0x35118,)
BATTLE_AI_NEAREST_TARGET_ADDRESS = 0x35372
BATTLE_AI_NEAREST_TARGET_END = 0x3540E
BATTLE_AI_NEAREST_TARGET_CALL_OFFSETS = (0x005, 0x05D)
BATTLE_AI_NEAREST_TARGET_RELOCATION_OFFSETS = (
    0x02C, 0x033, 0x03C, 0x046, 0x04C, 0x053,
    0x059, 0x065, 0x06F, 0x077, 0x084, 0x08F,
)
BATTLE_AI_NEAREST_TARGET_CALLER_SITES = (0x34F59, 0x3512F)
BATTLE_AI_POISON_HANDLER_ADDRESS = 0x3540E
BATTLE_AI_POISON_HANDLER_END = 0x355FF
BATTLE_AI_POISON_HANDLER_CALL_OFFSETS = (
    0x005, 0x013, 0x022, 0x090, 0x0B3,
    0x0D5, 0x10D, 0x130, 0x17E, 0x1E5,
)
BATTLE_AI_POISON_HANDLER_RELOCATION_OFFSETS = (
    0x032, 0x038, 0x045, 0x04B, 0x052, 0x058, 0x05F, 0x06C,
    0x082, 0x089, 0x09B, 0x0A5, 0x0AC, 0x0BE, 0x0C4, 0x0CB,
    0x0D1, 0x0DD, 0x0E7, 0x0F1, 0x0F9, 0x105, 0x122, 0x13B,
    0x145, 0x14B, 0x152, 0x158, 0x167, 0x16D, 0x174, 0x17A,
    0x186, 0x190, 0x19A, 0x1A2, 0x1B2, 0x1BF, 0x1C8, 0x1D1,
)
BATTLE_AI_POISON_HANDLER_CALLER_SITES = (0x33BE2,)
BATTLE_AI_POISON_TARGET_ADDRESS = 0x355FF
BATTLE_AI_POISON_TARGET_END = 0x35667
BATTLE_AI_POISON_TARGET_CALL_OFFSETS = (0x005, 0x032, 0x040, 0x058)
BATTLE_AI_POISON_TARGET_RELOCATION_OFFSETS = (0x01C, 0x029)
BATTLE_AI_POISON_TARGET_CALLER_SITES = (0x35421,)
BATTLE_AI_STRONGEST_POISON_TARGET_ADDRESS = 0x35667
BATTLE_AI_STRONGEST_POISON_TARGET_END = 0x3570F
BATTLE_AI_STRONGEST_POISON_TARGET_CALL_OFFSETS = (0x005,)
BATTLE_AI_STRONGEST_POISON_TARGET_RELOCATION_OFFSETS = (
    0x027, 0x02E, 0x037, 0x041, 0x04E, 0x058, 0x065, 0x06C, 0x075, 0x083, 0x08B,
)
BATTLE_AI_STRONGEST_POISON_TARGET_CALLER_SITES = (0x3563F,)
BATTLE_AI_FIRST_POISON_TARGET_ADDRESS = 0x3570F
BATTLE_AI_FIRST_POISON_TARGET_END = 0x35803
BATTLE_AI_FIRST_POISON_TARGET_CALL_OFFSETS = (0x005, 0x097)
BATTLE_AI_FIRST_POISON_TARGET_RELOCATION_OFFSETS = (
    0x02A, 0x031, 0x03E, 0x04C, 0x059, 0x063, 0x070, 0x077, 0x080,
    0x086, 0x08D, 0x093, 0x09F, 0x0A9, 0x0B3, 0x0BB, 0x0C7, 0x0D1,
)
BATTLE_AI_FIRST_POISON_TARGET_CALLER_SITES = (0x35657,)
BATTLE_AI_ITEM_HANDLER_ADDRESS = 0x35803
BATTLE_AI_ITEM_HANDLER_END = 0x3582B
BATTLE_AI_ITEM_HANDLER_CALL_OFFSETS = (0x005, 0x013, 0x01E)
BATTLE_AI_ITEM_HANDLER_CALLER_SITES = (0x33C03,)
BATTLE_AI_THROWING_HANDLER_ADDRESS = 0x3582B
BATTLE_AI_THROWING_HANDLER_END = 0x3598C
BATTLE_AI_THROWING_HANDLER_CALL_OFFSETS = (
    0x005, 0x013, 0x087, 0x0B7, 0x0D4, 0x123, 0x155,
)
BATTLE_AI_THROWING_HANDLER_RELOCATION_OFFSETS = (
    0x021, 0x027, 0x032, 0x039, 0x040, 0x046, 0x04D, 0x05A,
    0x070, 0x076, 0x07D, 0x083, 0x08F, 0x099, 0x0A3, 0x0AB,
    0x0C6, 0x0DF, 0x0E9, 0x0F0, 0x0F7, 0x0FD, 0x10C, 0x112,
    0x119, 0x11F, 0x12B, 0x135, 0x13F, 0x147,
)
BATTLE_AI_THROWING_HANDLER_CALLER_SITES = (0x33C24,)
BATTLE_AI_ITEM_EXECUTOR_ADDRESS = 0x3598C
BATTLE_AI_ITEM_EXECUTOR_END = 0x36133
BATTLE_AI_ITEM_EXECUTOR_CALL_OFFSETS = (
    0x005, 0x09B, 0x0A8, 0x0CF, 0x102, 0x10F, 0x199, 0x21C, 0x2A9,
    0x39B, 0x44F, 0x49E, 0x528, 0x5B5, 0x6A5, 0x75D, 0x798,
)
BATTLE_AI_ITEM_EXECUTOR_RELOCATION_OFFSETS = (
    0x025, 0x03C, 0x046, 0x050, 0x058, 0x074, 0x07E, 0x086, 0x08E,
    0x096, 0x0B3, 0x0BB, 0x0C3, 0x0DF, 0x0EC, 0x0F4, 0x0FC, 0x11A,
    0x127, 0x131, 0x139, 0x156, 0x164, 0x16E, 0x176, 0x17F, 0x187,
    0x194, 0x1A4, 0x1AE, 0x1B6, 0x1C0, 0x1C7, 0x1D2, 0x1DA, 0x1EA,
    0x1F4, 0x207, 0x22E, 0x241, 0x25F, 0x269, 0x27C, 0x297, 0x2B4,
    0x2C8, 0x302, 0x30B, 0x318, 0x32A, 0x334, 0x346, 0x34D, 0x354,
    0x35D, 0x364, 0x374, 0x37E, 0x390, 0x3A8, 0x3B2, 0x3BA, 0x3C4,
    0x3D0, 0x3E2, 0x3F9, 0x41A, 0x421, 0x42B, 0x43D, 0x447, 0x45C,
    0x466, 0x46E, 0x477, 0x484, 0x48C, 0x499, 0x4A9, 0x4B3, 0x4BB,
    0x4C5, 0x4CC, 0x4DE, 0x4E6, 0x4F6, 0x500, 0x513, 0x53A, 0x54D,
    0x56B, 0x575, 0x588, 0x5A3, 0x5C0, 0x5D4, 0x60C, 0x615, 0x622,
    0x634, 0x63E, 0x650, 0x657, 0x65E, 0x667, 0x66E, 0x67E, 0x688,
    0x69A, 0x6B2, 0x6BC, 0x6C4, 0x6CE, 0x6DA, 0x6EC, 0x703, 0x728,
    0x72F, 0x739, 0x74B, 0x755, 0x770, 0x77D, 0x787, 0x78F,
)
BATTLE_AI_ITEM_EXECUTOR_CALLER_SITES = (0x35821, 0x358E2)
BATTLE_AI_ITEM_EXECUTOR_SHARED_EPILOGUE = 0x3612C
BATTLE_AI_ITEM_EXECUTOR_EPILOGUE_JUMP_SITES = (0x370EA, 0x37161, 0x395E7)
BATTLE_CARRIED_ITEM_REMOVE_ADDRESS = 0x36133
BATTLE_CARRIED_ITEM_REMOVE_END = 0x361AC
BATTLE_CARRIED_ITEM_REMOVE_CALL_OFFSETS = (0x005,)
BATTLE_CARRIED_ITEM_REMOVE_RELOCATION_OFFSETS = (
    0x01E, 0x032, 0x039, 0x040, 0x047, 0x05B, 0x068, 0x071,
)
BATTLE_CARRIED_ITEM_REMOVE_CALLER_SITES = (0x36124,)
BATTLE_AI_REQUEST_MEDICINE_ADDRESS = 0x361AC
BATTLE_AI_REQUEST_MEDICINE_END = 0x36209
BATTLE_AI_REQUEST_MEDICINE_CALL_OFFSETS = (0x005, 0x021, 0x054)
BATTLE_AI_REQUEST_MEDICINE_RELOCATION_OFFSETS = (
    0x015, 0x02C, 0x036, 0x03D, 0x044, 0x04A,
)
BATTLE_AI_REQUEST_MEDICINE_CALLER_SITES = (0x33C0E,)
BATTLE_AI_REQUEST_SHARED_ENTRY = 0x361B1
BATTLE_AI_REQUEST_DETOX_JUMP_SITES = (0x3620E,)
BATTLE_AI_REQUEST_DETOX_ADDRESS = 0x36209
BATTLE_AI_REQUEST_DETOX_END = 0x36210
BATTLE_AI_REQUEST_DETOX_CALLER_SITES = (0x33C19,)
BATTLE_AI_SUPPORT_MEDICINE_ADDRESS = 0x36210
BATTLE_AI_SUPPORT_MEDICINE_END = 0x363AC
BATTLE_AI_SUPPORT_MEDICINE_CALL_OFFSETS = (
    0x05,
    0x52,
    0x80,
    0x9B,
    0xE0,
    0x103,
    0x125,
    0x188,
    0x190,
)
BATTLE_AI_SUPPORT_MEDICINE_CALL_TARGETS = (
    0x3ED1E,
    0x36E7F,
    0x39EF7,
    0x3650E,
    0x3F50B,
    0x3F50B,
    0x36E7F,
    0x34C47,
    0x34AD3,
)
BATTLE_AI_SUPPORT_MEDICINE_RELOCATION_OFFSETS = (
    0x18,
    0x25,
    0x3B,
    0x41,
    0x48,
    0x4E,
    0x5A,
    0x64,
    0x6E,
    0x76,
    0x8D,
    0xA6,
    0xB0,
    0xB6,
    0xBD,
    0xC3,
    0xCA,
    0xD9,
    0xEB,
    0xF5,
    0xFC,
    0x10E,
    0x114,
    0x11B,
    0x121,
    0x12D,
    0x137,
    0x141,
    0x149,
    0x159,
    0x166,
    0x16F,
    0x178,
)
BATTLE_AI_SUPPORT_MEDICINE_CALLER_SITES = (0x33BF8,)
BATTLE_AI_SUPPORT_DETOX_ADDRESS = 0x363AC
BATTLE_AI_SUPPORT_DETOX_END = 0x3650E
BATTLE_AI_SUPPORT_DETOX_CALL_OFFSETS = (
    0x05,
    0x52,
    0x80,
    0x9B,
    0xEA,
    0x14D,
    0x158,
)
BATTLE_AI_SUPPORT_DETOX_CALL_TARGETS = (
    0x3ED1E,
    0x36E7F,
    0x39B8E,
    0x3650E,
    0x36E7F,
    0x34C47,
    0x34AD3,
)
BATTLE_AI_SUPPORT_DETOX_RELOCATION_OFFSETS = (
    0x18,
    0x25,
    0x3B,
    0x41,
    0x48,
    0x4E,
    0x5A,
    0x64,
    0x6E,
    0x76,
    0x8D,
    0xA6,
    0xB0,
    0xB7,
    0xBE,
    0xC4,
    0xD3,
    0xD9,
    0xE0,
    0xE6,
    0xF2,
    0xFC,
    0x106,
    0x10E,
    0x11E,
    0x12B,
    0x134,
    0x13D,
)
BATTLE_AI_SUPPORT_DETOX_CALLER_SITES = (0x33BED,)
BATTLE_AI_MOVEMENT_ADDRESS = 0x3650E
BATTLE_AI_MOVEMENT_END = 0x36A98
BATTLE_AI_MOVEMENT_CALL_OFFSETS = (
    0x005,
    0x059,
    0x0EA,
    0x15F,
    0x176,
    0x229,
    0x24B,
    0x264,
    0x2B6,
    0x31D,
    0x331,
    0x3CD,
    0x413,
    0x48E,
)
BATTLE_AI_MOVEMENT_CALL_TARGETS = (
    0x3ED1E,
    0x36E7F,
    0x36E06,
    0x3F50B,
    0x3F50B,
    0x36E06,
    0x37245,
    0x37355,
    0x36E06,
    0x3F50B,
    0x3F50B,
    0x36E06,
    0x36E06,
    0x36E06,
)
BATTLE_AI_MOVEMENT_RELOCATION_OFFSETS = (
    0x02E,
    0x039,
    0x043,
    0x049,
    0x04F,
    0x055,
    0x06F,
    0x076,
    0x090,
    0x09A,
    0x0BF,
    0x0CA,
    0x0D4,
    0x0DA,
    0x0E0,
    0x0E6,
    0x127,
    0x135,
    0x13E,
    0x1CC,
    0x1D6,
    0x1E0,
    0x1EA,
    0x1F0,
    0x1F7,
    0x1FF,
    0x206,
    0x20F,
    0x219,
    0x221,
    0x231,
    0x23B,
    0x243,
    0x28B,
    0x296,
    0x2A0,
    0x2A6,
    0x2AC,
    0x2B2,
    0x2FA,
    0x383,
    0x38D,
    0x397,
    0x3A1,
    0x3B5,
    0x3BC,
    0x3C3,
    0x3C9,
    0x3F8,
    0x400,
    0x407,
    0x40F,
    0x425,
    0x430,
    0x439,
    0x442,
    0x449,
    0x473,
    0x47B,
    0x482,
    0x48A,
    0x4A0,
    0x4AB,
    0x4B2,
    0x4D8,
    0x4DF,
    0x4ED,
    0x4F8,
    0x4FF,
    0x50E,
    0x516,
    0x51D,
    0x52B,
    0x533,
    0x53A,
    0x549,
    0x559,
    0x560,
    0x56C,
    0x573,
)
BATTLE_AI_MOVEMENT_CALLER_SITES = (
    0x33BC6,
    0x34C22,
    0x34E80,
    0x3553E,
    0x358FF,
    0x361CD,
    0x362AB,
    0x36447,
)
BATTLE_PLAYER_MOVEMENT_ADDRESS = 0x36A98
BATTLE_PLAYER_MOVEMENT_END = 0x36AF7
BATTLE_PLAYER_MOVEMENT_CALL_OFFSETS = (0x05, 0x2B, 0x44, 0x50)
BATTLE_PLAYER_MOVEMENT_CALL_TARGETS = (0x3ED1E, 0x36AF7, 0x37245, 0x37355)
BATTLE_PLAYER_MOVEMENT_RELOCATION_OFFSETS = (0x25,)
BATTLE_PLAYER_MOVEMENT_CALLER_SITES = (0x33323,)
BATTLE_CURSOR_HANDLER_ADDRESS = 0x36AF7
BATTLE_CURSOR_HANDLER_END = 0x36E06
BATTLE_CURSOR_HANDLER_CALL_OFFSETS = (0x05, 0x6E, 0x7B, 0x8F, 0x9F, 0xB6, 0xC6)
BATTLE_CURSOR_HANDLER_CALL_TARGETS = (
    0x3ED1E, 0x36E06, 0x36E7F, 0x3AA85, 0x3D6D1, 0x3AA85, 0x3D6D1,
)
BATTLE_CURSOR_HANDLER_RELOCATION_OFFSETS = (
    0x17, 0x1E, 0x25, 0x35, 0x3C, 0x43, 0x4A, 0x51, 0x58, 0x5F, 0x65,
    0x8B, 0x96, 0x9B, 0xBD, 0xC2, 0xD0, 0xD9, 0xE2, 0xE9, 0xF1, 0xFB,
    0x106, 0x10D, 0x116, 0x122, 0x12A, 0x133, 0x13C, 0x143, 0x14B, 0x155,
    0x160, 0x167, 0x170, 0x180, 0x18B, 0x194, 0x19D, 0x1A4, 0x1AC, 0x1B6,
    0x1C1, 0x1C8, 0x1D1, 0x1E1, 0x1EC, 0x1F5, 0x1FE, 0x205, 0x20D, 0x217,
    0x222, 0x229, 0x232, 0x242, 0x24D, 0x256, 0x25E, 0x272, 0x27B, 0x284,
    0x291, 0x298, 0x29F, 0x2A7, 0x2B1, 0x2BC, 0x2E2, 0x2EC, 0x2F4, 0x302,
)
BATTLE_CURSOR_HANDLER_CALLER_SITES = (
    0x36AC3, 0x37951, 0x397BC, 0x39B65, 0x39ECE, 0x3A35A,
)
BATTLE_MOVEMENT_PATH_ADDRESS = 0x36E06
BATTLE_MOVEMENT_PATH_END = 0x36E7F
BATTLE_MOVEMENT_PATH_CALL_OFFSETS = (0x05, 0x0A, 0x6F)
BATTLE_MOVEMENT_PATH_CALL_TARGETS = (0x3ED1E, 0x36EF8, 0x37070)
BATTLE_MOVEMENT_PATH_RELOCATION_OFFSETS = (
    0x12, 0x1C, 0x24, 0x2D, 0x36, 0x3F,
    0x48, 0x51, 0x59, 0x5F, 0x65, 0x6B,
)
BATTLE_MOVEMENT_PATH_CALLER_SITES = (
    0x34B28, 0x365F8, 0x36737, 0x367C4,
    0x368DB, 0x36921, 0x3699C, 0x36B65,
)
BATTLE_TARGETING_PATH_ADDRESS = 0x36E7F
BATTLE_TARGETING_PATH_END = 0x36EF8
BATTLE_TARGETING_PATH_CALL_OFFSETS = (0x05, 0x0A, 0x6F)
BATTLE_TARGETING_PATH_CALL_TARGETS = (0x3ED1E, 0x36FF9, 0x37070)
BATTLE_TARGETING_PATH_RELOCATION_OFFSETS = (
    0x12, 0x1C, 0x24, 0x2D, 0x36, 0x3F,
    0x48, 0x51, 0x59, 0x5F, 0x65, 0x6B,
)
BATTLE_TARGETING_PATH_CALLER_SITES = (
    0x34DAF, 0x34EC7, 0x34FAA, 0x353CF, 0x354E3,
    0x3558C, 0x357A6, 0x358B2, 0x3594E, 0x36262,
    0x36335, 0x363FE, 0x36496, 0x36567, 0x36B72,
)
BATTLE_MOVEMENT_INITIALIZER_ADDRESS = 0x36EF8
BATTLE_MOVEMENT_INITIALIZER_END = 0x36FF9
BATTLE_MOVEMENT_INITIALIZER_CALL_OFFSETS = (0x05,)
BATTLE_MOVEMENT_INITIALIZER_RELOCATION_OFFSETS = (
    0x25, 0x57, 0x65, 0x8E, 0x95, 0x9E, 0xCB, 0xE0,
)
BATTLE_MOVEMENT_INITIALIZER_CALLER_SITES = (0x36E10,)
BATTLE_PATH_TILE_BEGIN_ADDRESS = 0x5456A
BATTLE_PATH_TILE_END_ADDRESS = 0x5457C
BATTLE_MOVEMENT_INITIALIZER_SHARED_TAIL_ADDRESS = 0x39A3E
BATTLE_MOVEMENT_INITIALIZER_SHARED_TAIL_END = 0x39A45
BATTLE_TARGETING_INITIALIZER_ADDRESS = 0x36FF9
BATTLE_TARGETING_INITIALIZER_END = 0x37070
BATTLE_TARGETING_INITIALIZER_CALL_OFFSETS = (0x05,)
BATTLE_TARGETING_INITIALIZER_RELOCATION_OFFSETS = (0x20, 0x4C, 0x56, 0x61)
BATTLE_TARGETING_INITIALIZER_CALLER_SITES = (0x36E89,)
BATTLE_FLOOD_STEP_ADDRESS = 0x37070
BATTLE_FLOOD_STEP_END = 0x37166
BATTLE_FLOOD_STEP_CALL_OFFSETS = (0x05, 0x1F, 0x4E, 0x65, 0xBD, 0xCE, 0xE0)
BATTLE_FLOOD_STEP_CALL_TARGETS = (
    0x3ED1E, 0x37166, 0x371AE, 0x37166, 0x3721E, 0x371AE, 0x371F7,
)
BATTLE_FLOOD_STEP_RELOCATION_OFFSETS = (0x11, 0x32, 0x46, 0x57, 0x8D, 0x99, 0xD9)
BATTLE_FLOOD_STEP_CALLER_SITES = (0x36E75, 0x36EEE)
BATTLE_FLOOD_DIRECTION_X_ADDRESS = 0x556DE
BATTLE_FLOOD_DIRECTION_Y_ADDRESS = 0x556E6
BATTLE_FLOOD_SHARED_TAIL_ADDRESS = 0x3612C
BATTLE_FLOOD_SHARED_TAIL_END = 0x36133
BATTLE_DEQUEUE_ADDRESS = 0x37166
BATTLE_DEQUEUE_END = 0x371AE
BATTLE_DEQUEUE_CALL_OFFSETS = (0x05,)
BATTLE_DEQUEUE_CALL_TARGETS = (0x3ED1E,)
BATTLE_DEQUEUE_RELOCATION_OFFSETS = (0x16, 0x28)
BATTLE_DEQUEUE_CALLER_SITES = (0x3708F, 0x370D5)
BATTLE_DEQUEUE_X_WORDS_ADDRESS = 0xE6ABE
BATTLE_DEQUEUE_Y_WORDS_ADDRESS = 0xE6CBC
BATTLE_DEQUEUE_READ_INDEX_ADDRESS = 0xE6ED4
BATTLE_ENQUEUE_ADDRESS = 0x371AE
BATTLE_ENQUEUE_END = 0x371F7
BATTLE_ENQUEUE_CALL_OFFSETS = (0x05,)
BATTLE_ENQUEUE_CALL_TARGETS = (0x3ED1E,)
BATTLE_ENQUEUE_RELOCATION_OFFSETS = (0x11, 0x1D, 0x29, 0x40)
BATTLE_ENQUEUE_CALLER_SITES = (0x370BE, 0x3713E)
BATTLE_ENQUEUE_X_WORDS_ADDRESS = 0xE6ABE
BATTLE_ENQUEUE_Y_WORDS_ADDRESS = 0xE6CBC
BATTLE_ENQUEUE_WRITE_INDEX_ADDRESS = 0xE6ED0
BATTLE_ROUND_LOOP_ADDRESS = 0x3271E
BATTLE_ROUND_LOOP_END = 0x32A51
BATTLE_ROUND_LOOP_CALL_OFFSETS = (
    0x005, 0x020, 0x09D, 0x0AD, 0x0B5, 0x0D1, 0x28B,
    0x29B, 0x2D0, 0x2DB, 0x2EA, 0x2EF, 0x31A,
)
BATTLE_ROUND_LOOP_RELOCATION_OFFSETS = (
    0x01A, 0x027, 0x02D, 0x033, 0x039, 0x03F, 0x048, 0x054, 0x05D,
    0x067, 0x06F, 0x078, 0x084, 0x08D, 0x097, 0x0A4, 0x0A9, 0x0C6,
    0x0CD, 0x0D9, 0x0E2, 0x0EB, 0x101, 0x10E, 0x115, 0x11F, 0x12C,
    0x139, 0x146, 0x150, 0x15D, 0x16A, 0x177, 0x19D, 0x1A9, 0x1B3,
    0x1C6, 0x1CF, 0x1D8, 0x1E1, 0x1E8, 0x1EF, 0x1F7, 0x206, 0x214,
    0x21A, 0x221, 0x227, 0x22D, 0x236, 0x242, 0x24B, 0x255, 0x25D,
    0x266, 0x272, 0x27B, 0x285, 0x292, 0x297, 0x2AC, 0x2B5, 0x2BE,
    0x2C8, 0x2F7, 0x308, 0x310, 0x321, 0x326,
)
BATTLE_SPEED_SORT_ADDRESS = 0x32A51
BATTLE_SPEED_SORT_END = 0x32B78
BATTLE_SPEED_SORT_CALL_OFFSETS = (0x005, 0x101)
BATTLE_SPEED_SORT_RELOCATION_OFFSETS = (
    0x010, 0x02B, 0x038, 0x03F, 0x049, 0x056, 0x063, 0x070, 0x07A,
    0x087, 0x094, 0x0A1, 0x0A8, 0x0B2, 0x0BF, 0x0CC, 0x0D9, 0x0E3,
    0x0F0,
)
BATTLE_COMBATANT_SWAP_ADDRESS = 0x32B78
BATTLE_COMBATANT_SWAP_END = 0x32E2F
BATTLE_COMBATANT_SWAP_CALL_OFFSETS = (0x005, 0x284, 0x29E)
BATTLE_COMBATANT_SWAP_RELOCATION_OFFSETS = (
    0x019, 0x024, 0x02F, 0x03A, 0x044, 0x04F, 0x05A, 0x065, 0x070,
    0x07B, 0x086, 0x091, 0x09C, 0x0AB, 0x0B2, 0x0B9, 0x0C0, 0x0C7,
    0x0CE, 0x0D5, 0x0DC, 0x0E3, 0x0EA, 0x0F1, 0x0F8, 0x0FF, 0x106,
    0x10D, 0x114, 0x11B, 0x122, 0x129, 0x130, 0x137, 0x13E, 0x145,
    0x14C, 0x153, 0x15A, 0x161, 0x16B, 0x175, 0x184, 0x18D, 0x197,
    0x19F, 0x1B4, 0x1BF, 0x1CA, 0x1D4, 0x1DF, 0x1EA, 0x1F5, 0x200,
    0x20B, 0x216, 0x221, 0x22C, 0x233, 0x23A, 0x244, 0x24E, 0x25D,
    0x266, 0x270, 0x278, 0x294, 0x2AE,
)
Z_DAT_EFFECT_FRAME_COUNTS_OFFSET = 324_814
Z_DAT_AI_SPECIAL_ATTACK_OFFSET = 324_920
EFFECT_FRAME_COUNT = 53
AI_SPECIAL_ATTACK_COUNT = 7
FIGHT_PATTERN = re.compile(r"^FIGHT(?P<id>\d{3})\.IDX$", re.IGNORECASE)
PATH_DIRECTIONS = ((0, -1), (1, 0), (-1, 0), (0, 1))
BLOCKED_TILE_RANGES = (
    (0x0166, 0x016A),
    (0x0176, 0x017C),
    (0x01CA, 0x01D0),
    (0x01FA, 0x0262),
    (0x0332, 0x0338),
    (0x0346, 0x0346),
    (0x03A6, 0x03A8),
    (0x03F8, 0x03FE),
    (0x052C, 0x0544),
)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def relative_call_target(code: bytes, offset: int, address: int) -> int:
    if code[offset] != 0xE8:
        raise ValueError(f"expected near call opcode at {address + offset:#x}")
    displacement = struct.unpack_from("<i", code, offset + 1)[0]
    return address + offset + 5 + displacement


def battle_entry_contract(z_dat_bytes: bytes) -> dict[str, object]:
    raw_offset = BATTLE_ENTRY_ADDRESS - Z_DAT_LOAD_BASE
    raw = z_dat_bytes[raw_offset:raw_offset + BATTLE_ENTRY_END - BATTLE_ENTRY_ADDRESS]
    if len(raw) != 299:
        raise ValueError("Z.DAT does not contain the complete battle entry function")
    call_targets = [
        relative_call_target(raw, offset, BATTLE_ENTRY_ADDRESS)
        for offset in BATTLE_ENTRY_CALL_OFFSETS
    ]
    expected_targets = [
        0x3ED1E, 0x31DA0, 0x31EB9, 0x3265C, 0x3D6E0, 0x3CC97, 0x3D6E0,
        0x3AA85, 0x3D6D1, 0x3E1B2, 0x3271E, 0x3CC97, 0x3D6E0, 0x3E1B2,
    ]
    if call_targets != expected_targets:
        raise ValueError("Z.DAT battle entry call sequence changed")
    relocated = bytearray(raw)
    for offset in BATTLE_ENTRY_RELOCATION_OFFSETS:
        raw_value = struct.unpack_from("<I", raw, offset)[0]
        struct.pack_into("<I", relocated, offset, raw_value + 0x20000)
    caller_sites = (0x2DE15, 0x304CA)
    caller_targets = []
    caller_post_call_bytes = []
    for site in caller_sites:
        site_offset = site - Z_DAT_LOAD_BASE
        caller_targets.append(
            relative_call_target(z_dat_bytes, site_offset, Z_DAT_LOAD_BASE)
        )
        caller_post_call_bytes.append(
            z_dat_bytes[site_offset + 5:site_offset + 11].hex()
        )
    if caller_targets != [BATTLE_ENTRY_ADDRESS, BATTLE_ENTRY_ADDRESS]:
        raise ValueError("Z.DAT battle entry caller target changed")
    if caller_post_call_bytes != ["83c40883f801", "83c40883f801"]:
        raise ValueError("Z.DAT battle entry caller result comparison changed")
    return {
        "address": hex(BATTLE_ENTRY_ADDRESS),
        "end": hex(BATTLE_ENTRY_END),
        "raw_offset": hex(raw_offset),
        "size": len(raw),
        "instruction_count": 72,
        "raw_sha256": sha256(raw),
        "loaded_sha256": sha256(bytes(relocated)),
        "relocation_count": len(BATTLE_ENTRY_RELOCATION_OFFSETS),
        "relocation_delta": 0x20000,
        "call_sites": [hex(BATTLE_ENTRY_ADDRESS + offset) for offset in BATTLE_ENTRY_CALL_OFFSETS],
        "call_targets": [hex(target) for target in call_targets],
        "call_contract": [
            "stack_probe",
            "load_war_and_battlefield",
            "initialize_party",
            "append_enemies",
            "load_wdx_wmp",
            "fade_scene_to_black",
            "load_eft",
            "draw_battlefield",
            "present_black_battlefield",
            "start_battle_music",
            "run_battle_loop",
            "fade_battle_to_black",
            "load_sdx_smp",
            "restore_scene_music",
        ],
        "callers": [hex(site) for site in caller_sites],
        "caller_result_compare": "eax == 1",
        "result_word": {
            "storage": "signed_int16",
            "transient": 0,
            "defeat": 1,
            "victory": 2,
            "return_expression": "result_word - 1",
            "returned_defeat": 0,
            "returned_victory": 1,
        },
        "mode_word": {"during_battle": 2, "after_battle": 1},
        "argument_storage": {"battle_id": "signed_int16", "grant_experience": "signed_int16"},
        "transition_frames": {
            "scene_fade_to_black": 64,
            "black_battlefield_present": 1,
            "battle_initial_fade_after_present": 66,
            "battle_exit_fade_to_black": 64,
        },
    }


def battle_data_loader_contract(z_dat_bytes: bytes) -> dict[str, object]:
    raw_offset = BATTLE_DATA_LOADER_ADDRESS - Z_DAT_LOAD_BASE
    raw = z_dat_bytes[
        raw_offset:raw_offset + BATTLE_DATA_LOADER_END - BATTLE_DATA_LOADER_ADDRESS
    ]
    if len(raw) != 281:
        raise ValueError("Z.DAT does not contain the complete battle data loader")
    call_targets = [
        relative_call_target(raw, offset, BATTLE_DATA_LOADER_ADDRESS)
        for offset in BATTLE_DATA_LOADER_CALL_OFFSETS
    ]
    expected_targets = [
        0x3ED1E, 0x3DA66, 0x3DB07, 0x3DABF, 0x3DB2B, 0x3CF45,
        0x3EF96, 0x20C32, 0x3DA66, 0x3DB07, 0x3DABF, 0x3DB2B,
    ]
    if call_targets != expected_targets:
        raise ValueError("Z.DAT battle data loader call sequence changed")
    relocated = bytearray(raw)
    for offset in BATTLE_DATA_LOADER_RELOCATION_OFFSETS:
        raw_value = struct.unpack_from("<I", raw, offset)[0]
        struct.pack_into("<I", relocated, offset, raw_value + 0x20000)
    caller_site = 0x31C9E
    caller_offset = caller_site - Z_DAT_LOAD_BASE
    if (
        relative_call_target(z_dat_bytes, caller_offset, Z_DAT_LOAD_BASE)
        != BATTLE_DATA_LOADER_ADDRESS
    ):
        raise ValueError("Z.DAT battle data loader caller target changed")
    return {
        "address": hex(BATTLE_DATA_LOADER_ADDRESS),
        "end": hex(BATTLE_DATA_LOADER_END),
        "raw_offset": hex(raw_offset),
        "size": len(raw),
        "instruction_count": 80,
        "raw_sha256": sha256(raw),
        "loaded_sha256": sha256(bytes(relocated)),
        "relocation_count": len(BATTLE_DATA_LOADER_RELOCATION_OFFSETS),
        "relocation_delta": 0x20000,
        "call_sites": [
            hex(BATTLE_DATA_LOADER_ADDRESS + offset)
            for offset in BATTLE_DATA_LOADER_CALL_OFFSETS
        ],
        "call_targets": [hex(target) for target in call_targets],
        "call_contract": [
            "stack_probe",
            "open_war_sta",
            "seek_war_record",
            "read_war_record",
            "close_war_sta",
            "load_warfld_idx_if_cache_miss",
            "print_warfld_idx_error",
            "terminate_after_warfld_idx_error",
            "open_warfld_grp",
            "seek_warfld_entry",
            "read_battlefield_prefix",
            "close_warfld_grp",
        ],
        "callers": [hex(caller_site)],
        "battle_id_storage": "signed_int16",
        "war_record": {
            "size": WAR_RECORD_SIZE,
            "offset_expression": "sign_extend_i16(battle_id) * 186",
            "battlefield_id_word": 6,
        },
        "warfld": {
            "cache_tag": 6,
            "index_layout": "synthetic_zero_then_cumulative_ends",
            "seek_expression": "offsets[sign_extend_i16(battlefield_id)]",
            "battlefield_prefix_size": 0x4000,
        },
        "occupancy": {
            "width": 64,
            "height": 64,
            "loop_order": "y_then_x",
            "fill_value": -1,
        },
    }


def battle_setup_machine_contract(z_dat_bytes: bytes) -> dict[str, object]:
    party_raw_offset = BATTLE_PARTY_SETUP_ADDRESS - Z_DAT_LOAD_BASE
    party_raw = z_dat_bytes[
        party_raw_offset:party_raw_offset + BATTLE_PARTY_SETUP_END - BATTLE_PARTY_SETUP_ADDRESS
    ]
    if len(party_raw) != 1955:
        raise ValueError("Z.DAT does not contain the complete battle party setup")
    party_calls = [
        relative_call_target(party_raw, offset, BATTLE_PARTY_SETUP_ADDRESS)
        for offset in BATTLE_PARTY_SETUP_CALL_OFFSETS
    ]
    expected_party_calls = [
        0x3ED1E, 0x3B1E6, 0x3B1E6, 0x3B1E6, 0x29D2D, 0x2CEBF,
        0x3EF4A, 0x3D832, 0x2CEBF, 0x3EF4A, 0x3D832, 0x3D832,
        0x3EF4A, 0x3D832, 0x3EF4A, 0x3EF4A, 0x3D832, 0x3D6D1, 0x3B1E6,
    ]
    if party_calls != expected_party_calls:
        raise ValueError("Z.DAT battle party setup call sequence changed")
    party_caller = 0x31CAF
    if (
        relative_call_target(
            z_dat_bytes, party_caller - Z_DAT_LOAD_BASE, Z_DAT_LOAD_BASE
        )
        != BATTLE_PARTY_SETUP_ADDRESS
    ):
        raise ValueError("Z.DAT battle party setup caller target changed")

    enemy_raw_offset = BATTLE_ENEMY_SETUP_ADDRESS - Z_DAT_LOAD_BASE
    enemy_raw = z_dat_bytes[
        enemy_raw_offset:enemy_raw_offset + BATTLE_ENEMY_SETUP_END - BATTLE_ENEMY_SETUP_ADDRESS
    ]
    if len(enemy_raw) != 194:
        raise ValueError("Z.DAT does not contain the complete battle enemy setup")
    enemy_calls = [
        relative_call_target(enemy_raw, offset, BATTLE_ENEMY_SETUP_ADDRESS)
        for offset in BATTLE_ENEMY_SETUP_CALL_OFFSETS
    ]
    if enemy_calls != [0x3ED1E, 0x3B1E6]:
        raise ValueError("Z.DAT battle enemy setup call sequence changed")
    enemy_caller = 0x31CB4
    if (
        relative_call_target(
            z_dat_bytes, enemy_caller - Z_DAT_LOAD_BASE, Z_DAT_LOAD_BASE
        )
        != BATTLE_ENEMY_SETUP_ADDRESS
    ):
        raise ValueError("Z.DAT battle enemy setup caller target changed")

    return {
        "party": {
            "address": hex(BATTLE_PARTY_SETUP_ADDRESS),
            "end": hex(BATTLE_PARTY_SETUP_END),
            "raw_offset": hex(party_raw_offset),
            "size": len(party_raw),
            "instruction_count": 453,
            "raw_sha256": sha256(party_raw),
            "ida_loaded_sha256": "6743a9d962317bde209fd1a6c36b54a60678d374fa9ca5a90dfa6b9a934feb0f",
            "ida_relocation_count": 144,
            "call_sites": [
                hex(BATTLE_PARTY_SETUP_ADDRESS + offset)
                for offset in BATTLE_PARTY_SETUP_CALL_OFFSETS
            ],
            "call_targets": [hex(target) for target in party_calls],
            "callers": [hex(party_caller)],
            "combatant_slots": 26,
            "combatant_words": 14,
            "party_slots": 6,
            "party_prefix_rule": "slot0 unconditional; first slot 1..5 with signed id <= 0 ends prefix; otherwise 6",
            "fixed_party_words": [15, 20],
            "preset_party_words": [9, 14],
            "preset_rule": "append every non--1 preset before scanning the party prefix only to mark matching mandatory states",
            "party_coordinate_words": [[21, 26], [27, 32]],
            "selection_states": {"unselected": 0, "selected": 1, "mandatory": 2},
            "confirm_index": "party_prefix_length",
            "confirm_requires_nonempty_combatants": True,
        },
        "enemy": {
            "address": hex(BATTLE_ENEMY_SETUP_ADDRESS),
            "end": hex(BATTLE_ENEMY_SETUP_END),
            "raw_offset": hex(enemy_raw_offset),
            "size": len(enemy_raw),
            "instruction_count": 42,
            "raw_sha256": sha256(enemy_raw),
            "ida_loaded_sha256": "dd7d372cd20ca5f93ab279c9f30d6ec874aa1e695bb2b367c4112c11a6c8de3a",
            "ida_relocation_count": 17,
            "call_sites": [
                hex(BATTLE_ENEMY_SETUP_ADDRESS + offset)
                for offset in BATTLE_ENEMY_SETUP_CALL_OFFSETS
            ],
            "call_targets": [hex(target) for target in enemy_calls],
            "callers": [hex(enemy_caller)],
            "enemy_slots": 20,
            "empty_sentinel": -1,
            "enemy_words": [33, 52],
            "enemy_coordinate_words": [[53, 72], [73, 92]],
            "write_order": [
                "role_id", "side", "x", "y", "initial_mode", "sprite", "occupancy", "count",
            ],
            "duplicate_occupancy": "later_write_wins",
        },
    }


def relocated_machine_function_contract(
    z_dat_bytes: bytes,
    *,
    address: int,
    end: int,
    call_offsets: tuple[int, ...],
    expected_call_targets: tuple[int, ...],
    relocation_offsets: tuple[int, ...],
    caller_sites: tuple[int, ...],
    instruction_count: int,
    branch_count: int,
) -> dict[str, object]:
    raw_offset = address - Z_DAT_LOAD_BASE
    raw = z_dat_bytes[raw_offset:raw_offset + end - address]
    if len(raw) != end - address:
        raise ValueError(f"Z.DAT does not contain the complete function at {address:#x}")
    call_targets = tuple(
        relative_call_target(raw, offset, address) for offset in call_offsets
    )
    if call_targets != expected_call_targets:
        raise ValueError(f"Z.DAT call sequence changed at {address:#x}")
    caller_targets = tuple(
        relative_call_target(
            z_dat_bytes, caller_site - Z_DAT_LOAD_BASE, Z_DAT_LOAD_BASE
        )
        for caller_site in caller_sites
    )
    if caller_targets != (address,) * len(caller_sites):
        raise ValueError(f"Z.DAT caller set changed at {address:#x}")
    relocated = bytearray(raw)
    for offset in relocation_offsets:
        raw_value = struct.unpack_from("<I", raw, offset)[0]
        struct.pack_into("<I", relocated, offset, raw_value + 0x20000)
    return {
        "address": hex(address),
        "end": hex(end),
        "raw_offset": hex(raw_offset),
        "size": len(raw),
        "instruction_count": instruction_count,
        "branch_count": branch_count,
        "raw_sha256": sha256(raw),
        "loaded_sha256": sha256(bytes(relocated)),
        "relocation_count": len(relocation_offsets),
        "relocation_delta": 0x20000,
        "call_sites": [hex(address + offset) for offset in call_offsets],
        "call_targets": [hex(target) for target in call_targets],
        "callers": [hex(site) for site in caller_sites],
    }


def battle_rest_wrapper_contract(z_dat_bytes: bytes) -> dict[str, object]:
    raw_offset = BATTLE_REST_WRAPPER_ADDRESS - Z_DAT_LOAD_BASE
    raw = z_dat_bytes[
        raw_offset:raw_offset + BATTLE_REST_WRAPPER_END - BATTLE_REST_WRAPPER_ADDRESS
    ]
    if raw.hex() != "6808000000e841a200000fbf44240450e8bc5d000083c404c3":
        raise ValueError("Z.DAT battle rest wrapper changed")
    contract = relocated_machine_function_contract(
        z_dat_bytes,
        address=BATTLE_REST_WRAPPER_ADDRESS,
        end=BATTLE_REST_WRAPPER_END,
        call_offsets=BATTLE_REST_WRAPPER_CALL_OFFSETS,
        expected_call_targets=(0x3ED1E, 0x3A8A4),
        relocation_offsets=(),
        caller_sites=BATTLE_REST_WRAPPER_CALLER_SITES,
        instruction_count=7,
        branch_count=0,
    )
    del contract["relocation_delta"]
    return {
        **contract,
        "stack_probe_bytes": 8,
        "actor_argument": "signed int16",
        "delegated_core": "0x3a8a4",
        "wrapper_state_writes": 0,
        "return_value": "delegated core eax unchanged",
        "caller_roles": [
            "ai none-or-wait dispatch",
            "escape or item-reposition tail when parameter is zero",
            "attack fallback",
            "poison fallback",
            "medicine fallback",
            "detox fallback",
        ],
    }


def battle_escape_plan_contract(z_dat_bytes: bytes) -> dict[str, object]:
    contract = relocated_machine_function_contract(
        z_dat_bytes,
        address=BATTLE_ESCAPE_PLAN_ADDRESS,
        end=BATTLE_ESCAPE_PLAN_END,
        call_offsets=BATTLE_ESCAPE_PLAN_CALL_OFFSETS,
        expected_call_targets=(0x3ED1E, 0x36E06, 0x3F50B, 0x3F50B, 0x3650E, 0x34AD3),
        relocation_offsets=BATTLE_ESCAPE_PLAN_RELOCATION_OFFSETS,
        caller_sites=BATTLE_ESCAPE_PLAN_CALLER_SITES,
        instruction_count=92,
        branch_count=9,
    )
    if contract["raw_sha256"] != (
        "6e0f72a5051e1073414fade410106d3632865629b3acebc78382eb027e960831"
    ):
        raise ValueError("Z.DAT battle escape-plan raw bytes changed")
    if contract["loaded_sha256"] != (
        "93623d56fd7d837faae5f2326a6bae3f9cd643f168f1ebfb83283810c59669c0"
    ):
        raise ValueError("Z.DAT battle escape-plan relocation image changed")
    return contract


def battle_ai_attack_handler_contract(z_dat_bytes: bytes) -> dict[str, object]:
    contract = relocated_machine_function_contract(
        z_dat_bytes,
        address=BATTLE_AI_ATTACK_HANDLER_ADDRESS,
        end=BATTLE_AI_ATTACK_HANDLER_END,
        call_offsets=BATTLE_AI_ATTACK_HANDLER_CALL_OFFSETS,
        expected_call_targets=(
            0x3ED1E, 0x3D612, 0x3505B, 0x36E7F, 0x37734,
            0x3650E, 0x36E7F, 0x35372, 0x36E7F, 0x34AD3,
        ),
        relocation_offsets=BATTLE_AI_ATTACK_HANDLER_RELOCATION_OFFSETS,
        caller_sites=BATTLE_AI_ATTACK_HANDLER_CALLER_SITES,
        instruction_count=248,
        branch_count=42,
    )
    if contract["raw_sha256"] != (
        "16f17deba817156bdfe4744fa5f49e8bdcabb30d7724f734b953d6698b593af0"
    ):
        raise ValueError("Z.DAT battle AI attack-handler raw bytes changed")
    if contract["loaded_sha256"] != (
        "3cd7c4306cfd7df22a3569f3a658f7c91aedf3efd876eefcaec69cd8f23cb213"
    ):
        raise ValueError("Z.DAT battle AI attack-handler relocation image changed")
    return {
        **contract,
        "magic_slot_scan_count": 10,
        "learned_magic_predicate": "magic_id > 0",
        "selected_slot": "bounded(learned_count) used as direct packed slot index",
        "special_bonus_entry_count": AI_SPECIAL_ATTACK_COUNT,
        "special_bonus_match_policy": "last matching entry wins",
        "magic_level_word": "unsigned int16",
        "magic_level_divisor": 100,
        "magic_record_bytes": 136,
        "target_range_rechecks": 3,
        "attack_automatic_flag": 1,
        "action_done_shared_tail": True,
    }


def battle_ai_attack_target_contract(z_dat_bytes: bytes) -> dict[str, object]:
    contract = relocated_machine_function_contract(
        z_dat_bytes,
        address=BATTLE_AI_ATTACK_TARGET_ADDRESS,
        end=BATTLE_AI_ATTACK_TARGET_END,
        call_offsets=BATTLE_AI_ATTACK_TARGET_CALL_OFFSETS,
        expected_call_targets=(
            0x3ED1E, 0x3D612, 0x3513A, 0x3D612,
            0x351A7, 0x3D612, 0x35217, 0x35372,
        ),
        relocation_offsets=BATTLE_AI_ATTACK_TARGET_RELOCATION_OFFSETS,
        caller_sites=BATTLE_AI_ATTACK_TARGET_CALLER_SITES,
        instruction_count=63,
        branch_count=9,
    )
    if contract["raw_sha256"] != (
        "10afcc5cd8f924d80d1749c1fe5f2e906fa60b9a191767fb15436d50e6bae992"
    ):
        raise ValueError("Z.DAT battle AI attack-target raw bytes changed")
    if contract["loaded_sha256"] != (
        "bbbf59bbdc65946d1c1e0d2e093cb5ccb4d46d831002dd285ae13720f8f9bbdf"
    ):
        raise ValueError("Z.DAT battle AI attack-target relocation image changed")
    return {
        **contract,
        "strategy_order": [
            "signed morality >= 75",
            "signed morality <= 25",
            "signed IQ >= 70",
            "nearest fallback",
        ],
        "random_bound": 10,
        "random_success_predicate": "result < 7",
        "reachable_random_draw_counts": [0, 1, 2],
        "selected_strategy_falls_back_when_target_not_written": False,
        "nearest_fallback_consumes_random": False,
    }


def battle_ai_strongest_target_contract(z_dat_bytes: bytes) -> dict[str, object]:
    contract = relocated_machine_function_contract(
        z_dat_bytes,
        address=BATTLE_AI_STRONGEST_TARGET_ADDRESS,
        end=BATTLE_AI_STRONGEST_TARGET_END,
        call_offsets=BATTLE_AI_STRONGEST_TARGET_CALL_OFFSETS,
        expected_call_targets=(0x3ED1E,),
        relocation_offsets=BATTLE_AI_STRONGEST_TARGET_RELOCATION_OFFSETS,
        caller_sites=BATTLE_AI_STRONGEST_TARGET_CALLER_SITES,
        instruction_count=32,
        branch_count=5,
    )
    if contract["raw_sha256"] != (
        "ab91c4c972a5a17b4db00ceea5a803d7d0e5f6d949c4bccd0b37c97fe7dbcb0c"
    ):
        raise ValueError("Z.DAT battle AI strongest-target raw bytes changed")
    if contract["loaded_sha256"] != (
        "786b1e5e58c1c76dae493c53978e0f2568c230ec67c48fd9a9a55a4c140188ea"
    ):
        raise ValueError("Z.DAT battle AI strongest-target relocation image changed")
    return {
        **contract,
        "slot_loop_comparison": "signed int16 slot < combatant_count",
        "candidate_filters": ["side != actor side", "hidden == 0"],
        "best_initial": 0,
        "attack_comparison": "signed candidate > best",
        "ties_preserve_first_slot": True,
        "nonpositive_attacks_write_target": False,
        "reads_hp": False,
        "consumes_random": False,
    }


def battle_ai_weakest_target_contract(z_dat_bytes: bytes) -> dict[str, object]:
    contract = relocated_machine_function_contract(
        z_dat_bytes,
        address=BATTLE_AI_WEAKEST_TARGET_ADDRESS,
        end=BATTLE_AI_WEAKEST_TARGET_END,
        call_offsets=BATTLE_AI_WEAKEST_TARGET_CALL_OFFSETS,
        expected_call_targets=(0x3ED1E,),
        relocation_offsets=BATTLE_AI_WEAKEST_TARGET_RELOCATION_OFFSETS,
        caller_sites=BATTLE_AI_WEAKEST_TARGET_CALLER_SITES,
        instruction_count=32,
        branch_count=5,
    )
    if contract["raw_sha256"] != (
        "56611048416b38e1eca2c1cf738e488175de001df5cc2b851ef947fa85ec398f"
    ):
        raise ValueError("Z.DAT battle AI weakest-target raw bytes changed")
    if contract["loaded_sha256"] != (
        "76408936111ba57a30049c0dd11814af62191e1b81797757cee29e8f48488b46"
    ):
        raise ValueError("Z.DAT battle AI weakest-target relocation image changed")
    return {
        **contract,
        "slot_loop_comparison": "signed int16 slot < combatant_count",
        "candidate_filters": ["side != actor side", "hidden == 0"],
        "best_initial": 1000,
        "attack_comparison": "signed candidate < best",
        "ties_preserve_first_slot": True,
        "attack_at_or_above_1000_writes_target": False,
        "negative_attack_can_write_target": True,
        "reads_hp": False,
        "consumes_random": False,
    }


def battle_ai_poison_handler_contract(z_dat_bytes: bytes) -> dict[str, object]:
    contract = relocated_machine_function_contract(
        z_dat_bytes,
        address=BATTLE_AI_POISON_HANDLER_ADDRESS,
        end=BATTLE_AI_POISON_HANDLER_END,
        call_offsets=BATTLE_AI_POISON_HANDLER_CALL_OFFSETS,
        expected_call_targets=(
            0x3ED1E, 0x355FF, 0x34C47, 0x3F50B, 0x3F50B,
            0x36E7F, 0x397E5, 0x3650E, 0x36E7F, 0x34AD3,
        ),
        relocation_offsets=BATTLE_AI_POISON_HANDLER_RELOCATION_OFFSETS,
        caller_sites=BATTLE_AI_POISON_HANDLER_CALLER_SITES,
        instruction_count=115,
        branch_count=8,
    )
    if contract["raw_sha256"] != (
        "d153ed4a0997bc2735ea31b5a00ecc62ad634b9f574ce57dcf06c867c9546e1a"
    ):
        raise ValueError("Z.DAT battle AI poison-handler raw bytes changed")
    if contract["loaded_sha256"] != (
        "8be7484923c0296d3c6060e77fe945fe29440cc56df79a3c8b8c545ae2c4c335"
    ):
        raise ValueError("Z.DAT battle AI poison-handler relocation image changed")
    return {
        **contract,
        "selector_minus_one_fallback": "automatic attack",
        "selected_target_source": "actor word12 copied to shared target slot",
        "range": "wrapping int16(signed actor use_poison / 15 + 1)",
        "discarded_absolute_value_calls": 2,
        "targeting_map_calls": 2,
        "range_comparison": "signed target distance <= range",
        "round_zero_in_range": "poison after first range check",
        "round_positive": "movement mode3 then same-target recheck",
        "round_nonpositive_without_first_hit": "same-target second range check without movement",
        "fallback_attack_value": "2 * signed actor attack",
        "fallback_average": "2 * frozen outer allied_total_i16 / allied_count_i16",
        "fallback_comparison": "strictly greater attacks; otherwise rests",
        "direct_random_calls": 0,
        "caller_uses_return": False,
        "outer_marks_action_done_after_handler": True,
    }


def battle_ai_poison_target_contract(z_dat_bytes: bytes) -> dict[str, object]:
    contract = relocated_machine_function_contract(
        z_dat_bytes,
        address=BATTLE_AI_POISON_TARGET_ADDRESS,
        end=BATTLE_AI_POISON_TARGET_END,
        call_offsets=BATTLE_AI_POISON_TARGET_CALL_OFFSETS,
        expected_call_targets=(0x3ED1E, 0x3D612, 0x35667, 0x3570F),
        relocation_offsets=BATTLE_AI_POISON_TARGET_RELOCATION_OFFSETS,
        caller_sites=BATTLE_AI_POISON_TARGET_CALLER_SITES,
        instruction_count=32,
        branch_count=3,
    )
    if contract["raw_sha256"] != (
        "7821ab41dc1724bec3b199715ed04e2cd819aa105b5b69e59810f583e088afe7"
    ):
        raise ValueError("Z.DAT battle AI poison-target raw bytes changed")
    if contract["loaded_sha256"] != (
        "940a59f0582883b27b20b1f91cdc27c3326670fbe0053281a1cfae35e2d038cc"
    ):
        raise ValueError("Z.DAT battle AI poison-target relocation image changed")
    return {
        **contract,
        "local_result_initial": -1,
        "writes_actor_word12_directly": False,
        "iq_gate": "signed actor iq > 60",
        "rng_call": "bounded(10) only after IQ gate",
        "rng_comparison": "signed result < 7",
        "strongest_selector": "called only when IQ gate and RNG gate both pass",
        "strongest_minus_one_fallback": "first-eligible stale-distance selector",
        "rng_gate_failure_fallback": "first-eligible stale-distance selector",
        "final_return": "callee EAX truncated to int16 then sign-extended",
        "direct_rng_draws": "zero or one",
        "caller": "branches on return -1; otherwise reads actor word12",
        "delegated_word12_writes_only": True,
    }


def battle_ai_strongest_poison_target_contract(z_dat_bytes: bytes) -> dict[str, object]:
    contract = relocated_machine_function_contract(
        z_dat_bytes,
        address=BATTLE_AI_STRONGEST_POISON_TARGET_ADDRESS,
        end=BATTLE_AI_STRONGEST_POISON_TARGET_END,
        call_offsets=BATTLE_AI_STRONGEST_POISON_TARGET_CALL_OFFSETS,
        expected_call_targets=(0x3ED1E,),
        relocation_offsets=BATTLE_AI_STRONGEST_POISON_TARGET_RELOCATION_OFFSETS,
        caller_sites=BATTLE_AI_STRONGEST_POISON_TARGET_CALLER_SITES,
        instruction_count=46,
        branch_count=8,
    )
    if contract["raw_sha256"] != (
        "a8d1da0042ee44038243a0bb88e57f997936834ebfdc210d6bce7efe90996b93"
    ):
        raise ValueError("Z.DAT strongest poison-target raw bytes changed")
    if contract["loaded_sha256"] != (
        "12572b96cfa2b9999aee3f02ba635490c26c9cf6268d83fe2a2ce04a34d9912d"
    ):
        raise ValueError("Z.DAT strongest poison-target relocation image changed")
    return {
        **contract,
        "best_attack_initial": 0,
        "slot_loop": "signed int16 from zero while slot < combatant_count",
        "candidate_side": "must differ from actor side",
        "candidate_hidden": "must equal zero",
        "candidate_poison": "signed value < 95",
        "candidate_anti_poison": "signed value < actor signed use_poison",
        "candidate_attack": "signed value strictly > current signed best",
        "attack_zero_or_negative": "never selected while best starts at zero",
        "equal_attack": "first selected slot remains",
        "candidate_hp_read": False,
        "writes_actor_word12": "candidate slot on every strict improvement",
        "no_selection_word12": "preserved",
        "success_return": 1,
        "failure_return": -1,
        "direct_rng_draws": 0,
        "caller": "sub_355FF uses return only to decide fallback",
    }


def battle_ai_first_poison_target_contract(z_dat_bytes: bytes) -> dict[str, object]:
    contract = relocated_machine_function_contract(
        z_dat_bytes,
        address=BATTLE_AI_FIRST_POISON_TARGET_ADDRESS,
        end=BATTLE_AI_FIRST_POISON_TARGET_END,
        call_offsets=BATTLE_AI_FIRST_POISON_TARGET_CALL_OFFSETS,
        expected_call_targets=(0x3ED1E, 0x36E7F),
        relocation_offsets=BATTLE_AI_FIRST_POISON_TARGET_RELOCATION_OFFSETS,
        caller_sites=BATTLE_AI_FIRST_POISON_TARGET_CALLER_SITES,
        instruction_count=56,
        branch_count=8,
    )
    if contract["raw_sha256"] != (
        "3bf9b8e1b03074465262d0a5b08257cde7d2ce0243a56f51cef7892cee30562e"
    ):
        raise ValueError("Z.DAT first poison-target raw bytes changed")
    if contract["loaded_sha256"] != (
        "b06d503b356f1c7587888e743a9dfcda7851d38e86999acf2d7098156322d2b9"
    ):
        raise ValueError("Z.DAT first poison-target relocation image changed")
    return {
        **contract,
        "best_distance_initial": 1000,
        "slot_loop": "signed int16 from zero while slot < combatant_count",
        "candidate_side": "must differ from actor side",
        "candidate_hidden": "must equal zero",
        "candidate_poison": "signed value < 95",
        "candidate_anti_poison": "signed value < actor signed use_poison",
        "candidate_hp_read": False,
        "path_source": "actor x/y copied before every eligible-candidate path build",
        "path_builds": "once per eligible candidate",
        "distance_target": "global signed word_E6EE0 stale target slot, not candidate slot",
        "distance_comparison": "signed distance strictly < current signed best",
        "first_eligible_bug":
            "all eligible candidates query the same stale target distance, so the first writes and later ties do not",
        "distance_below_1000":
            "first eligible candidate writes actor word12 and result is success",
        "distance_at_or_above_1000":
            "no candidate writes actor word12 and result is failure",
        "writes_actor_word12": "candidate slot on strict improvement",
        "no_selection_word12": "preserved",
        "success_return": 1,
        "failure_return": -1,
        "direct_rng_draws": 0,
        "caller": "sub_355FF invokes this for roll >= 7 or strongest-selector failure",
        "stale_target_storage": "word_E6EE0 is independent from actor word12",
        "handler_write_order":
            "sub_3540E copies selected actor word12 to word_E6EE0 only after sub_355FF returns",
    }


def battle_ai_item_handler_contract(z_dat_bytes: bytes) -> dict[str, object]:
    contract = relocated_machine_function_contract(
        z_dat_bytes,
        address=BATTLE_AI_ITEM_HANDLER_ADDRESS,
        end=BATTLE_AI_ITEM_HANDLER_END,
        call_offsets=BATTLE_AI_ITEM_HANDLER_CALL_OFFSETS,
        expected_call_targets=(0x3ED1E, 0x34AEC, 0x3598C),
        relocation_offsets=(),
        caller_sites=BATTLE_AI_ITEM_HANDLER_CALLER_SITES,
        instruction_count=14,
        branch_count=0,
    )
    if contract["raw_sha256"] != (
        "59b666e9af54bafb5ba99c95859d8e3a9bcda425348fa15267ef3bfdfb9a2bc5"
    ):
        raise ValueError("Z.DAT AI item-handler raw bytes changed")
    if contract["loaded_sha256"] != contract["raw_sha256"]:
        raise ValueError("Z.DAT AI item-handler unexpectedly changed while loading")
    return {
        **contract,
        "actor_argument": "entry int16 sign-extended once into EBX",
        "first_call": "sub_34AEC(actor, 1)",
        "first_call_result": "ignored",
        "second_call": "sub_3598C(actor, 0)",
        "second_call_execution": "unconditional after sub_34AEC returns",
        "function_result": "EAX from sub_3598C passes through RET",
        "caller_result_use": "sole caller sub_33599 ignores EAX",
        "direct_rng_draws": 0,
        "direct_state_writes": 0,
        "branches": 0,
        "outer_action_done": "written by sole caller after handler returns",
        "delegated_boundaries": "sub_34AEC and sub_3598C retain independent owners",
    }


def battle_ai_throwing_handler_contract(z_dat_bytes: bytes) -> dict[str, object]:
    contract = relocated_machine_function_contract(
        z_dat_bytes,
        address=BATTLE_AI_THROWING_HANDLER_ADDRESS,
        end=BATTLE_AI_THROWING_HANDLER_END,
        call_offsets=BATTLE_AI_THROWING_HANDLER_CALL_OFFSETS,
        expected_call_targets=(
            0x3ED1E, 0x3505B, 0x36E7F, 0x3598C, 0x3650E, 0x36E7F, 0x34C47,
        ),
        relocation_offsets=BATTLE_AI_THROWING_HANDLER_RELOCATION_OFFSETS,
        caller_sites=BATTLE_AI_THROWING_HANDLER_CALLER_SITES,
        instruction_count=84,
        branch_count=3,
    )
    if contract["raw_sha256"] != (
        "4dbe948d5cbd3b588e9638016ce6a44c3b24f99d014071eeabb9200b7e471ded"
    ):
        raise ValueError("Z.DAT AI throwing-handler raw bytes changed")
    if contract["loaded_sha256"] != (
        "f0d4f1e000b8ae30590805156b76ceda27c2e93672876ca5fce4989a796e60b2"
    ):
        raise ValueError("Z.DAT AI throwing-handler relocation image changed")
    return {
        **contract,
        "actor_argument": "entry int16 sign-extended into ESI",
        "target_selection": "sub_3505B(actor) runs once before all range checks",
        "target_scratch_write":
            "actor word11 copied to word_E6EE0 even when selector retained stale word11",
        "target_reselection_after_move": False,
        "range_formula":
            "signed actor hidden_weapon / 15 truncating toward zero, then +1",
        "range_storage_and_compare":
            "range retained in EDI; DI and path word compared signed",
        "first_check":
            "range >= signed path distance calls sub_3598C(actor,1) and returns",
        "movement_gate":
            "only first distance miss with signed round_value > 0 calls sub_3650E(actor,1,range)",
        "movement_result": "ignored",
        "second_check":
            "always rebuilds actor-source targeting map against same word_E6EE0 target, even when movement skipped",
        "second_hit":
            "range >= signed path distance reuses shared sub_3598C(actor,1) exit",
        "second_miss":
            "calls sub_34C47(actor) automatic attack and returns without direct item consumption",
        "direct_rng_draws": 0,
        "delegated_rng_draws":
            "sub_3505B owns 0..2 selection draws; action callees retain their own RNG",
        "function_result":
            "EAX from sub_3598C or sub_34C47 passes through RET; movement EAX never controls flow",
        "caller_result_use":
            "sole caller sub_33599 ignores EAX and writes outer action_done after handler returns",
        "direct_state_writes":
            "word_E6EE0 target scratch plus path source/target coordinate globals",
        "delegated_boundaries":
            "sub_3505B, sub_36E7F, sub_3598C, sub_3650E and sub_34C47 retain independent owners",
    }


def battle_ai_item_executor_contract(z_dat_bytes: bytes) -> dict[str, object]:
    contract = relocated_machine_function_contract(
        z_dat_bytes,
        address=BATTLE_AI_ITEM_EXECUTOR_ADDRESS,
        end=BATTLE_AI_ITEM_EXECUTOR_END,
        call_offsets=BATTLE_AI_ITEM_EXECUTOR_CALL_OFFSETS,
        expected_call_targets=(
            0x3ED1E, 0x2B483, 0x3DB83, 0x2B227, 0x2B483, 0x3DB83,
            0x3884A, 0x3D612, 0x3D612, 0x3F50B, 0x38910, 0x3884A,
            0x3D612, 0x3D612, 0x3F50B, 0x38910, 0x36133,
        ),
        relocation_offsets=BATTLE_AI_ITEM_EXECUTOR_RELOCATION_OFFSETS,
        caller_sites=BATTLE_AI_ITEM_EXECUTOR_CALLER_SITES,
        instruction_count=440,
        branch_count=47,
    )
    if contract["raw_sha256"] != (
        "039adfbd48a1e5a282cbddce9a80b7499745277228c1c432cbf4a590263ed2a5"
    ):
        raise ValueError("Z.DAT AI item executor raw bytes changed")
    if contract["loaded_sha256"] != (
        "a1318ae160d72c1a5093248aaa7c60dc601be62adc83b6114a30b3dad8895e47"
    ):
        raise ValueError("Z.DAT AI item executor relocation image changed")
    epilogue_jump_targets = []
    for site in BATTLE_AI_ITEM_EXECUTOR_EPILOGUE_JUMP_SITES:
        offset = site - Z_DAT_LOAD_BASE
        if z_dat_bytes[offset] != 0xE9:
            raise ValueError(f"expected AI item shared-epilogue jump at {site:#x}")
        displacement = struct.unpack_from("<i", z_dat_bytes, offset + 1)[0]
        epilogue_jump_targets.append(site + 5 + displacement)
    if epilogue_jump_targets != [BATTLE_AI_ITEM_EXECUTOR_SHARED_EPILOGUE] * 3:
        raise ValueError("Z.DAT AI item shared-epilogue targets changed")
    stale_slot_initial = struct.unpack_from(
        "<h", z_dat_bytes, 0x54B74 - Z_DAT_LOAD_BASE
    )[0]
    if stale_slot_initial != -1:
        raise ValueError("Z.DAT stale player item slot initial value changed")
    return {
        **contract,
        "external_internal_entries": [
            {
                "site": hex(site),
                "target": hex(BATTLE_AI_ITEM_EXECUTOR_SHARED_EPILOGUE),
            }
            for site in BATTLE_AI_ITEM_EXECUTOR_EPILOGUE_JUMP_SITES
        ],
        "effect_map_entry":
            "clear all 64x64 words then mark the word_E6EE0 combatant coordinate",
        "mode0": {
            "party_source": "word_E6EDE inventory item/count pair",
            "enemy_source": "word_E6EDE actor carried item/count pair",
            "effect_call": "sub_2B483(actor role, target role, source item)",
            "delay": "sub_3DB83(340) = 9 BIOS tick changes",
            "reads_input": False,
            "count_update": "signed-word decrement after the delay",
            "party_exhaustion": "sub_2B227(word_E6EDE)",
            "enemy_exhaustion": "sub_36133(actor, word_E6EDE)",
        },
        "mode1": {
            "source_effect_item": "word_E6EDE source item passed to sub_3884A",
            "party_payload_item": "inventory item at stale word_54B74",
            "enemy_payload_item": "actor carried item at word_E6EDE",
            "stale_party_slot_initial": stale_slot_initial,
            "stale_party_slot_writer":
                "only sub_2A86C:0x2A9BF after valid show-introduction confirmation",
            "order": [
                "mark target", "complete sub_3884A", "apply HP/hurt/poison",
                "complete sub_38910(0)", "decrement source",
            ],
            "hurt_tiers": "signed 0:/4, 1..33:/3, 34..66:/2, >66:/1",
            "normal_domain_rng": "one bounded(5) after hurt-tier division",
            "randomized_base_storage":
                "tiered add_hp minus bounded(5) truncates to signed word before hidden-weapon scaling",
            "hp_delta": "(signed-word randomized base - 2*actor.hidden_weapon) / 3",
            "poison":
                "nonnegative add_poison direct; negative (add_poison-hidden_weapon)/2; no anti-poison or extra RNG",
        },
        "changes_facing": False,
        "writes_action_done": False,
        "shared_epilogue_owner_propagation": False,
        "delegated_boundaries": (
            "sub_2B483/sub_3DB83/sub_2B227/sub_3884A/sub_3D612/"
            "sub_3F50B/sub_38910/sub_36133 retain independent owners"
        ),
    }


def battle_carried_item_remove_contract(z_dat_bytes: bytes) -> dict[str, object]:
    contract = relocated_machine_function_contract(
        z_dat_bytes,
        address=BATTLE_CARRIED_ITEM_REMOVE_ADDRESS,
        end=BATTLE_CARRIED_ITEM_REMOVE_END,
        call_offsets=BATTLE_CARRIED_ITEM_REMOVE_CALL_OFFSETS,
        expected_call_targets=(0x3ED1E,),
        relocation_offsets=BATTLE_CARRIED_ITEM_REMOVE_RELOCATION_OFFSETS,
        caller_sites=BATTLE_CARRIED_ITEM_REMOVE_CALLER_SITES,
        instruction_count=28,
        branch_count=2,
    )
    if contract["raw_sha256"] != (
        "589cedb4fed28fa00c4fbe852d4111d772f44dc3454354d697161d47f4cd493c"
    ):
        raise ValueError("Z.DAT carried-item removal raw bytes changed")
    if contract["loaded_sha256"] != (
        "761bb81a8e1063e29e693892d293c56d63cb9995aedaa7ee645e15f505f2b331"
    ):
        raise ValueError("Z.DAT carried-item removal relocation image changed")
    return {
        **contract,
        "arguments":
            "actor slot and deletion slot are consumed through signed low words CX and DX",
        "caller":
            "sole caller sub_3598C invokes only for exhausted enemy carried source and ignores EAX",
        "loop":
            "initial jump then signed int16 DX < 3; legal slot0..2 shifts successors and increments full EDX",
        "role_lookup":
            "each iteration and terminal clear re-read signed combatant role_id",
        "item_shift": "taking-item ID at slot+1 copies to slot",
        "count_shift": "matching taking-item quantity at slot+1 copies to slot",
        "terminal_clear":
            "always writes item ID -1 and quantity 0 to slot3, including initial slot3 or >=3",
        "return": "EAX equals signed role_id * 182; sole caller does not read it",
        "direct_rng_draws": 0,
        "invalid_domain":
            "negative slot/invalid actor or role performs legacy linear access; modern safety rejection allowed",
        "delegated_boundaries": "sub_3ED1E stack probe remains independent owner",
    }


def battle_ai_request_medicine_contract(z_dat_bytes: bytes) -> dict[str, object]:
    contract = relocated_machine_function_contract(
        z_dat_bytes,
        address=BATTLE_AI_REQUEST_MEDICINE_ADDRESS,
        end=BATTLE_AI_REQUEST_MEDICINE_END,
        call_offsets=BATTLE_AI_REQUEST_MEDICINE_CALL_OFFSETS,
        expected_call_targets=(0x3ED1E, 0x3650E, 0x34C47),
        relocation_offsets=BATTLE_AI_REQUEST_MEDICINE_RELOCATION_OFFSETS,
        caller_sites=BATTLE_AI_REQUEST_MEDICINE_CALLER_SITES,
        instruction_count=22,
        branch_count=1,
    )
    if contract["raw_sha256"] != (
        "717b2ff358eec5fc59796f2eff00fa93a0662ef80c4a4ed99e0dbc021c4255cb"
    ):
        raise ValueError("Z.DAT request-medicine handler raw bytes changed")
    if contract["loaded_sha256"] != (
        "a92c3af77c368fcb219065d9a1e1cef84d0a4388fffdccfb50d4eaa2d2d41aa9"
    ):
        raise ValueError("Z.DAT request-medicine handler relocation image changed")
    return {
        **contract,
        "branch_sites": ["0x361c6"],
        "argument": "actor slot is consumed as signed low word through MOVSX",
        "entry_owner": "sub_33599 action case8 request_medicine",
        "caller":
            "sub_33599 ignores EAX, cleans actor argument and later writes actor action_done word13=1",
        "shared_entry": {
            "address": hex(BATTLE_AI_REQUEST_SHARED_ENTRY),
            "owner": "sub_36209 request_detox",
            "jump_sites": [hex(site) for site in BATTLE_AI_REQUEST_DETOX_JUMP_SITES],
            "closure_propagated": False,
        },
        "movement_gate":
            "signed combatant round/action word6 > 0 calls sub_3650E(actor, mode=0, value=0); zero or negative skips",
        "movement_result": "sub_3650E EAX is ignored and both paths reconverge at 0x361D5",
        "request_target":
            "signed word_E6EE0 is re-read after movement/skip; target words2/3 overwrite word_556DA/word_556DC",
        "attack": "sub_34C47(actor) is always called after request target coordinates are restored",
        "return": "sub_34C47 EAX passes through RET but sub_33599 ignores it",
        "direct_writes":
            "only global target x/y coordinates; no direct action, action_done, facing or RNG-state write",
        "direct_rng_draws": 0,
        "invalid_domain":
            "signed actor and request-target indices are unchecked legacy linear accesses; modern safety rejection allowed",
        "delegated_boundaries":
            "sub_3ED1E, sub_3650E, sub_34C47 and shared-entry owner sub_36209 remain independent closures",
    }


def battle_ai_request_detox_contract(z_dat_bytes: bytes) -> dict[str, object]:
    contract = relocated_machine_function_contract(
        z_dat_bytes,
        address=BATTLE_AI_REQUEST_DETOX_ADDRESS,
        end=BATTLE_AI_REQUEST_DETOX_END,
        call_offsets=(),
        expected_call_targets=(),
        relocation_offsets=(),
        caller_sites=BATTLE_AI_REQUEST_DETOX_CALLER_SITES,
        instruction_count=2,
        branch_count=1,
    )
    if contract["raw_sha256"] != (
        "39c017dd2b811b83553060d2eafb40e83c07e19d3e6685c6382075fffa133f09"
    ):
        raise ValueError("Z.DAT request-detox wrapper raw bytes changed")
    if contract["loaded_sha256"] != contract["raw_sha256"]:
        raise ValueError("Z.DAT request-detox wrapper unexpectedly relocates")
    return {
        **contract,
        "branch_sites": ["0x3620e"],
        "branch_targets": [hex(BATTLE_AI_REQUEST_SHARED_ENTRY)],
        "entry_owner": "sub_33599 action case9 request_detox",
        "caller":
            "sub_33599 pushes signed actor, ignores delegated EAX, cleans one actor argument and later writes actor action_done word13=1",
        "stack":
            "push immediate 16 duplicates sub_361AC stack-probe setup before entering 0x361B1",
        "transfer":
            "short jump from 0x3620E to order32 internal address 0x361B1; no local call or RET",
        "delegated_body":
            "0x361B1..0x36209 performs stack probe, signed round>0 optional sub_3650E(actor,0,0), request-target coordinate restore, unconditional sub_34C47(actor), then RET",
        "return": "delegated RET returns directly to sub_33599 and its EAX is ignored",
        "direct_writes": "none in the 7-byte owner",
        "direct_rng_draws": 0,
        "closure_boundary":
            "close only sub_36209 wrapper owner; do not duplicate or alter the independent sub_361AC body owner",
    }


def battle_ai_support_medicine_contract(z_dat_bytes: bytes) -> dict[str, object]:
    contract = relocated_machine_function_contract(
        z_dat_bytes,
        address=BATTLE_AI_SUPPORT_MEDICINE_ADDRESS,
        end=BATTLE_AI_SUPPORT_MEDICINE_END,
        call_offsets=BATTLE_AI_SUPPORT_MEDICINE_CALL_OFFSETS,
        expected_call_targets=BATTLE_AI_SUPPORT_MEDICINE_CALL_TARGETS,
        relocation_offsets=BATTLE_AI_SUPPORT_MEDICINE_RELOCATION_OFFSETS,
        caller_sites=BATTLE_AI_SUPPORT_MEDICINE_CALLER_SITES,
        instruction_count=97,
        branch_count=6,
    )
    if contract["raw_sha256"] != (
        "170152b7fbff2346acf70a92693b0aabe6e58b50b5198769553c9e25bdf92bd3"
    ):
        raise ValueError("Z.DAT AI medicine handler raw bytes changed")
    if contract["loaded_sha256"] != (
        "b37845bf6761f6a50de0f11a59e0ca389b2b836d51c50dcb059aee01da735197"
    ):
        raise ValueError("Z.DAT AI medicine handler relocation image changed")
    return {
        **contract,
        "branches": [
            {"site": "0x3628d", "target": "0x3629a", "condition": "signed range < distance"},
            {"site": "0x36295", "target": "0x363a5", "condition": "medicine exit"},
            {"site": "0x362a2", "target": "0x362b3", "condition": "signed round <= 0"},
            {"site": "0x36360", "target": "0x3628f", "condition": "signed range >= rebuilt distance"},
            {"site": "0x36395", "target": "0x3639f", "condition": "actor doubled attack <= doubled allied average"},
            {"site": "0x3639d", "target": "0x363a5", "condition": "automatic-attack exit"},
        ],
        "entry_owner": "sub_33599 action case5 medicine",
        "caller":
            "sub_33599 pushes signed actor, ignores EAX, cleans one actor argument and later writes actor action_done word13=1",
        "range":
            "signed actor-role medicine is IDIV 15 toward zero then incremented; signed DI retains the range and EDX remainder is discarded",
        "initial_range_check":
            "actor x/y become targeting source, sub_36E7F builds values and signed range >= signed target distance enters sub_39EF7 medicine",
        "movement":
            "only signed actor round word6 > 0 calls sub_3650E(actor,mode1,range); zero or negative skips and EAX is ignored",
        "second_range_check":
            "target slot/current x/y are re-read, two pure sub_3F50B results are discarded, actor source and targeting values are rebuilt and signed range >= distance re-enters medicine",
        "fallback":
            "2*signed live actor attack is strict-greater compared with signed IDIV of 2*signed AI-entry-frozen allied total by its frozen allied count; attack else rest",
        "return": "delegated medicine, attack or rest EAX reaches RET but sub_33599 ignores it",
        "direct_rng_draws": 0,
        "closure_boundary":
            "sub_3ED1E, sub_36E7F, sub_39EF7, sub_3650E, sub_3F50B, sub_34C47 and sub_34AD3 remain independent owners",
    }


def battle_ai_support_detox_contract(z_dat_bytes: bytes) -> dict[str, object]:
    contract = relocated_machine_function_contract(
        z_dat_bytes,
        address=BATTLE_AI_SUPPORT_DETOX_ADDRESS,
        end=BATTLE_AI_SUPPORT_DETOX_END,
        call_offsets=BATTLE_AI_SUPPORT_DETOX_CALL_OFFSETS,
        expected_call_targets=BATTLE_AI_SUPPORT_DETOX_CALL_TARGETS,
        relocation_offsets=BATTLE_AI_SUPPORT_DETOX_RELOCATION_OFFSETS,
        caller_sites=BATTLE_AI_SUPPORT_DETOX_CALLER_SITES,
        instruction_count=79,
        branch_count=7,
    )
    if contract["raw_sha256"] != (
        "31cc8176adbb3df2976334035fa8a1d10adf44ecdb639d389f1b541c659c0acd"
    ):
        raise ValueError("Z.DAT AI detox handler raw bytes changed")
    if contract["loaded_sha256"] != (
        "892ef6cd06154bd14bb479526d94cccfab365ec97a2061d23ca5e3dfbdcc08f4"
    ):
        raise ValueError("Z.DAT AI detox handler relocation image changed")
    return {
        **contract,
        "branches": [
            {"site": "0x36429", "target": "0x36436", "condition": "signed range < distance"},
            {"site": "0x36431", "target": "0x39a3e", "condition": "detox shared-epilogue exit"},
            {"site": "0x3643e", "target": "0x3644f", "condition": "signed round <= 0"},
            {"site": "0x364c1", "target": "0x3642b", "condition": "signed range >= rebuilt distance"},
            {"site": "0x364f6", "target": "0x36503", "condition": "actor doubled attack <= doubled allied average"},
            {"site": "0x364fe", "target": "0x39a3e", "condition": "automatic-attack shared-epilogue exit"},
            {"site": "0x36509", "target": "0x39a3e", "condition": "rest shared-epilogue exit"},
        ],
        "entry_owner": "sub_33599 action case4 detox",
        "caller":
            "sub_33599 pushes signed actor, ignores EAX, cleans one actor argument and later writes actor action_done word13=1",
        "range":
            "signed actor-role detoxification is IDIV 15 toward zero then incremented; signed DI retains the range and EDX remainder is discarded",
        "initial_range_check":
            "actor x/y become targeting source, sub_36E7F builds values and signed range >= signed target distance enters sub_39B8E detox",
        "movement":
            "only signed actor round word6 > 0 calls sub_3650E(actor,mode1,range); zero or negative skips and EAX is ignored",
        "second_range_check":
            "target slot/current x/y are re-read, actor source and targeting values are rebuilt and signed range >= distance re-enters detox",
        "fallback":
            "2*signed live actor attack is strict-greater compared with signed IDIV of 2*signed AI-entry-frozen allied total by its frozen allied count; attack else rest",
        "return":
            "detox, attack and rest all tail-jump to loc_39A3E, which drops the delegated actor argument, restores EDI/ESI/EBX and returns; sub_33599 ignores EAX",
        "absolute_value_calls": 0,
        "direct_rng_draws": 0,
        "closure_boundary":
            "sub_3ED1E, sub_36E7F, sub_39B8E, sub_3650E, sub_34C47, sub_34AD3 and loc_39A3E remain independent owners",
    }


def battle_ai_movement_contract(z_dat_bytes: bytes) -> dict[str, object]:
    contract = relocated_machine_function_contract(
        z_dat_bytes,
        address=BATTLE_AI_MOVEMENT_ADDRESS,
        end=BATTLE_AI_MOVEMENT_END,
        call_offsets=BATTLE_AI_MOVEMENT_CALL_OFFSETS,
        expected_call_targets=BATTLE_AI_MOVEMENT_CALL_TARGETS,
        relocation_offsets=BATTLE_AI_MOVEMENT_RELOCATION_OFFSETS,
        caller_sites=BATTLE_AI_MOVEMENT_CALLER_SITES,
        instruction_count=311,
        branch_count=57,
    )
    if contract["raw_sha256"] != (
        "31785d54867d026266133cad9cd3ae56f0d9efe11b6627ef1bb62781029a97f7"
    ):
        raise ValueError("Z.DAT AI movement raw bytes changed")
    if contract["loaded_sha256"] != (
        "cf5d93c2bda18388199c1a72e5c0cf20207281276bf2a360cb0f304ac2d65e63"
    ):
        raise ValueError("Z.DAT AI movement relocation image changed")
    direction_words = struct.unpack_from("<8h", z_dat_bytes, 0x556DE - Z_DAT_LOAD_BASE)
    if direction_words != (0, 1, -1, 0, -1, 0, 0, 1):
        raise ValueError("Z.DAT AI movement direction deltas changed")
    return {
        **contract,
        "direction_deltas": [[0, -1], [1, 0], [-1, 0], [0, 1]],
        "normal_callers": [
            {"site": "0x33bc6", "role": "direct AI move", "mode": 0},
            {"site": "0x34c22", "role": "AI item relocation", "mode": 0},
            {"site": "0x34e80", "role": "automatic attack", "mode": "computed 0/1/2"},
            {"site": "0x3553e", "role": "AI poison", "mode": 3},
            {"site": "0x358ff", "role": "AI throwing weapon", "mode": 1},
            {"site": "0x361cd", "role": "request medicine/detox", "mode": 0},
            {"site": "0x362ab", "role": "AI medicine", "mode": 1},
            {"site": "0x36447", "role": "AI detox", "mode": 1},
        ],
        "external_internal_entry": {
            "site": "0x36ba7",
            "target": "0x3677e",
            "role": "sub_36AF7 shared pop EDI/ESI/EBX/RET; preceding BX=1 makes JNZ untaken",
        },
        "arguments":
            "actor, mode and range are signed low words; requested target is word_556DA/word_556DC",
        "preliminary":
            "target-source targeting distance at actor minus signed actor round <= signed range sets within-turn flag",
        "mode2":
            "mode2 plus within-turn uses descending exact range layers aligned with target x or y; otherwise generic selection",
        "mode3": "mode3 always uses descending exact range layers without alignment",
        "layer_scan":
            "x outer/y inner scan, strict nearest Manhattan with first tie and signed layer decrement until zero",
        "generic":
            "actor-source movement map; up/right/left/down target neighbors first require actor-axis alignment, then accept any path<128, else x-priority/y target retreat repeats",
        "reachability":
            "source==destination exits; otherwise current-map path<128, rebuilt actor-map path<128, shortest-path mark, then movement delegation",
        "movement_delegation": "sub_37355(actor,ai_flag1,mode,range); EAX is not branched on",
        "direct_rng_draws": 0,
        "closure_boundary":
            "sub_3ED1E, sub_36E7F, sub_36E06, sub_3F50B, sub_37245, sub_37355 and sub_36AF7 shared tail remain independent owners",
    }


def battle_player_movement_contract(z_dat_bytes: bytes) -> dict[str, object]:
    contract = relocated_machine_function_contract(
        z_dat_bytes,
        address=BATTLE_PLAYER_MOVEMENT_ADDRESS,
        end=BATTLE_PLAYER_MOVEMENT_END,
        call_offsets=BATTLE_PLAYER_MOVEMENT_CALL_OFFSETS,
        expected_call_targets=BATTLE_PLAYER_MOVEMENT_CALL_TARGETS,
        relocation_offsets=BATTLE_PLAYER_MOVEMENT_RELOCATION_OFFSETS,
        caller_sites=BATTLE_PLAYER_MOVEMENT_CALLER_SITES,
        instruction_count=32,
        branch_count=1,
    )
    if contract["raw_sha256"] != (
        "b750d0562b8ff3c8f0a518ff963fa69f417a3fa55b51f55f16acebc125734b84"
    ):
        raise ValueError("Z.DAT player movement wrapper raw bytes changed")
    if contract["loaded_sha256"] != (
        "b3383e9b8b3d33025818f2bf512f47b48c4d938a59c55115aa5abf74e5c4a79e"
    ):
        raise ValueError("Z.DAT player movement wrapper relocation image changed")
    post_call_offset = 0x33328 - Z_DAT_LOAD_BASE
    if z_dat_bytes[post_call_offset:post_call_offset + 6].hex() != "83c4046bc31c":
        raise ValueError("Z.DAT player movement caller no longer discards EAX")
    return {
        **contract,
        "stack_probe_bytes": 28,
        "argument": "actor slot is consumed as a signed low word and retained in EBX",
        "selector_call": "sub_36AF7(actor, signed actor round word6, mode0, &cancel_word)",
        "cancel_local":
            "32-bit local starts at zero; only its low word is compared exactly with 1 after selector return",
        "cancel_exit":
            "cancel word 1 skips path marking and movement and returns EAX=-1",
        "selected_exit":
            "every other value calls sub_37245 then sub_37355(actor,player_flag0,mode0,range0), ignores their EAX values and returns zero",
        "sole_caller":
            "sub_32E59 action0 cleans the actor argument then IMUL overwrites EAX, so wrapper result does not control the menu tail",
        "direct_rng_draws": 0,
        "direct_state_writes": "local cancel only; selector, path marker and movement own external state",
        "closure_boundary":
            "sub_3ED1E, sub_36AF7, sub_37245 and sub_37355 remain independent owners",
    }


def battle_cursor_handler_contract(z_dat_bytes: bytes) -> dict[str, object]:
    contract = relocated_machine_function_contract(
        z_dat_bytes,
        address=BATTLE_CURSOR_HANDLER_ADDRESS,
        end=BATTLE_CURSOR_HANDLER_END,
        call_offsets=BATTLE_CURSOR_HANDLER_CALL_OFFSETS,
        expected_call_targets=BATTLE_CURSOR_HANDLER_CALL_TARGETS,
        relocation_offsets=BATTLE_CURSOR_HANDLER_RELOCATION_OFFSETS,
        caller_sites=BATTLE_CURSOR_HANDLER_CALLER_SITES,
        instruction_count=157,
        branch_count=35,
    )
    if contract["raw_sha256"] != (
        "880246a653a60137ff63b21d437f2722ffbfb14bb4a0dfa6f4b06d64d34215e9"
    ):
        raise ValueError("Z.DAT battle cursor handler raw bytes changed")
    if contract["loaded_sha256"] != (
        "cb0b440727ac96038276f0de9a9b139c820184b9dba7a5772b45c4b4800ed9de"
    ):
        raise ValueError("Z.DAT battle cursor handler relocation image changed")
    return {
        **contract,
        "arguments": "signed actor slot, signed path limit, signed mode, cancel-word pointer",
        "path_modes": {"0": "movement map via sub_36E06", "1": "targeting map via sub_36E7F"},
        "presentation": {
            "before_first_input": 2,
            "after_direction_or_rejected_confirmation": 1,
            "sequence": "draw battlefield then present before every input scan",
        },
        "input_priority": ["down", "right", "left", "up", "cancel", "activate"],
        "direction_rule":
            "signed 64-column linear index; move when path<=limit or aliased occupancy!=-1",
        "cancel": "clear path limit, write cancel word 1 and exit",
        "confirm":
            "require signed path<=limit and additionally path>0 in mode0 or path>=0 in mode1",
        "shared_exit": {
            "site": "0x36ba7",
            "target": "0x3677e",
            "role": "independently owned shared pop EDI/ESI/EBX/RET tail",
        },
        "direct_rng_draws": 0,
        "closure_boundary":
            "sub_36E06, sub_36E7F, sub_3AA85, sub_3D6D1 and shared tail 0x3677E remain independent owners",
    }


def battle_movement_path_contract(z_dat_bytes: bytes) -> dict[str, object]:
    contract = relocated_machine_function_contract(
        z_dat_bytes,
        address=BATTLE_MOVEMENT_PATH_ADDRESS,
        end=BATTLE_MOVEMENT_PATH_END,
        call_offsets=BATTLE_MOVEMENT_PATH_CALL_OFFSETS,
        expected_call_targets=BATTLE_MOVEMENT_PATH_CALL_TARGETS,
        relocation_offsets=BATTLE_MOVEMENT_PATH_RELOCATION_OFFSETS,
        caller_sites=BATTLE_MOVEMENT_PATH_CALLER_SITES,
        instruction_count=20,
        branch_count=1,
    )
    if contract["raw_sha256"] != (
        "776fc38943f45416200cbac30378f6acbfb2fe5d956570ed4d3a2ade0d23dce3"
    ):
        raise ValueError("Z.DAT movement path wrapper raw bytes changed")
    if contract["loaded_sha256"] != (
        "68e70ab2c5fca8aaed97f6d2bf40562a34dfd90f3e7fe3069e987d05fc5ec860"
    ):
        raise ValueError("Z.DAT movement path wrapper relocation image changed")
    expected_post_call_prefixes = (
        "c744240c", "895c2418", "0fbf05dc", "c7442418",
        "c7442418", "0fbfd3c1", "0fbfd3c1", "eb0b6683",
    )
    post_call_prefixes = tuple(
        z_dat_bytes[
            site - Z_DAT_LOAD_BASE + 5:site - Z_DAT_LOAD_BASE + 9
        ].hex()
        for site in BATTLE_MOVEMENT_PATH_CALLER_SITES
    )
    if post_call_prefixes != expected_post_call_prefixes:
        raise ValueError("Z.DAT movement path caller continuation changed")
    return {
        **contract,
        "stack_probe_bytes": 4,
        "arguments": "none; source x/y are read as signed int16 globals",
        "initializer": "sub_36EF8 movement blocking map",
        "source": {
            "x_global": "0x556d6",
            "y_global": "0x556d8",
            "index": "signed y*64 + signed x",
            "forced_value": 0,
        },
        "queue": {
            "slots": 255,
            "read_index": 0,
            "write_index": 2,
            "distance": 0,
            "slot0": [0, -1],
            "slot1": "source",
        },
        "loop": "call sub_37070 until EAX is nonzero",
        "return": "first nonzero EAX from sub_37070 passes through RET",
        "caller_post_call_prefixes": list(post_call_prefixes),
        "caller_uses_return": False,
        "direct_rng_draws": 0,
        "closure_boundary": "sub_36EF8 and sub_37070 remain independent owners",
    }


def battle_movement_initializer_contract(z_dat_bytes: bytes) -> dict[str, object]:
    contract = relocated_machine_function_contract(
        z_dat_bytes,
        address=BATTLE_MOVEMENT_INITIALIZER_ADDRESS,
        end=BATTLE_MOVEMENT_INITIALIZER_END,
        call_offsets=BATTLE_MOVEMENT_INITIALIZER_CALL_OFFSETS,
        expected_call_targets=(0x3ED1E,),
        relocation_offsets=BATTLE_MOVEMENT_INITIALIZER_RELOCATION_OFFSETS,
        caller_sites=BATTLE_MOVEMENT_INITIALIZER_CALLER_SITES,
        instruction_count=72,
        branch_count=17,
    )
    if contract["raw_sha256"] != (
        "aef811e34f5552d31e095f5560b3f72ef0a9eab77c0242f7e30e7820f8b32209"
    ):
        raise ValueError("Z.DAT movement initializer raw bytes changed")
    if contract["loaded_sha256"] != (
        "12da496f917b4551ba90a0d82da8a2a5676632e533e16993c625714a4a8ee97d"
    ):
        raise ValueError("Z.DAT movement initializer relocation image changed")

    caller_continuation = z_dat_bytes[
        0x36E15 - Z_DAT_LOAD_BASE:0x36E1C - Z_DAT_LOAD_BASE
    ].hex()
    if caller_continuation != "0fbf15d8560300":
        raise ValueError("Z.DAT movement initializer caller continuation changed")

    tile_begin = struct.unpack_from(
        "<9h", z_dat_bytes, BATTLE_PATH_TILE_BEGIN_ADDRESS - Z_DAT_LOAD_BASE
    )
    tile_end = struct.unpack_from(
        "<9h", z_dat_bytes, BATTLE_PATH_TILE_END_ADDRESS - Z_DAT_LOAD_BASE
    )
    tile_ranges = tuple(zip(tile_begin, tile_end))
    if tile_ranges != BLOCKED_TILE_RANGES:
        raise ValueError("Z.DAT movement blocked tile ranges changed")

    shared_tail = z_dat_bytes[
        BATTLE_MOVEMENT_INITIALIZER_SHARED_TAIL_ADDRESS - Z_DAT_LOAD_BASE:
        BATTLE_MOVEMENT_INITIALIZER_SHARED_TAIL_END - Z_DAT_LOAD_BASE
    ]
    if shared_tail.hex() != "83c4045f5e5bc3":
        raise ValueError("Z.DAT movement initializer shared tail changed")

    def classify(upper_layer: int, occupancy: int, ground_tile: int) -> int:
        blocked_ground = any(begin <= ground_tile <= end for begin, end in tile_ranges)
        return 555 if upper_layer != 0 or occupancy != -1 or blocked_ground else 254

    tile_boundary_vectors = []
    for begin, end in tile_ranges:
        tile_boundary_vectors.append(
            {
                "begin": begin,
                "end": end,
                "below_begin": classify(0, -1, begin - 1),
                "at_begin": classify(0, -1, begin),
                "at_end": classify(0, -1, end),
                "above_end": classify(0, -1, end + 1),
            }
        )

    return {
        **contract,
        "stack_probe_bytes": 20,
        "arguments": "none; battlefield ground/upper layers and occupancy are shared globals",
        "clear_pass": "64x64 path words set to zero with outer y and inner x",
        "scan_pass": "64x64 with outer x and inner y; each cell receives exactly one final value",
        "cell_index": "signed y*64+x",
        "predicate_order": ["upper_layer != 0", "occupancy != -1", "signed ground tile in range"],
        "unblocked_value": 254,
        "blocked_value": 555,
        "tile_ranges": [[begin, end] for begin, end in tile_ranges],
        "predicate_vectors": {
            "upper_layer": [
                {"value": value, "result": classify(value, -1, 0)}
                for value in (-32768, -1, 0, 1, 32767)
            ],
            "occupancy": [
                {"value": value, "result": classify(0, value, 0)}
                for value in (-32768, -2, -1, 0, 32767)
            ],
            "signed_ground_outside_ranges": [
                {"value": value, "result": classify(0, -1, value)}
                for value in (-32768, -1, 0, 32767)
            ],
            "tile_boundaries": tile_boundary_vectors,
        },
        "caller_continuation": caller_continuation,
        "caller_uses_return": False,
        "shared_tail": {
            "address": hex(BATTLE_MOVEMENT_INITIALIZER_SHARED_TAIL_ADDRESS),
            "end": hex(BATTLE_MOVEMENT_INITIALIZER_SHARED_TAIL_END),
            "bytes": shared_tail.hex(),
            "sha256": sha256(shared_tail),
            "semantics": "add esp,4; pop edi; pop esi; pop ebx; ret; EAX unchanged",
        },
        "return": "normal completion reaches the shared tail with EAX=63",
        "direct_rng_draws": 0,
        "closure_boundary":
            "shared tail 0x39A3E and its independent owner sub_397E5 are not closed here",
    }


def battle_targeting_initializer_contract(z_dat_bytes: bytes) -> dict[str, object]:
    contract = relocated_machine_function_contract(
        z_dat_bytes,
        address=BATTLE_TARGETING_INITIALIZER_ADDRESS,
        end=BATTLE_TARGETING_INITIALIZER_END,
        call_offsets=BATTLE_TARGETING_INITIALIZER_CALL_OFFSETS,
        expected_call_targets=(0x3ED1E,),
        relocation_offsets=BATTLE_TARGETING_INITIALIZER_RELOCATION_OFFSETS,
        caller_sites=BATTLE_TARGETING_INITIALIZER_CALLER_SITES,
        instruction_count=39,
        branch_count=10,
    )
    if contract["raw_sha256"] != (
        "bd2a44e6f71689d02b361c3ce4efdff3d14927ab43b509e9c2bfd4cb6ae67e0d"
    ):
        raise ValueError("Z.DAT targeting initializer raw bytes changed")
    if contract["loaded_sha256"] != (
        "c36d21be447066b7fd6ba9c2b113e67a63c965cac16c285363eb1316405c1b4b"
    ):
        raise ValueError("Z.DAT targeting initializer relocation image changed")
    caller_continuation = z_dat_bytes[
        0x36E8E - Z_DAT_LOAD_BASE:0x36E95 - Z_DAT_LOAD_BASE
    ].hex()
    if caller_continuation != "0fbf15d8560300":
        raise ValueError("Z.DAT targeting initializer caller continuation changed")

    def classify(upper_layer: int) -> int:
        return 555 if upper_layer != 0 else 254

    return {
        **contract,
        "stack_probe_bytes": 8,
        "arguments": "none; battlefield upper layer and path map are shared globals",
        "clear_pass": "64x64 path words set to zero with outer y and inner x",
        "scan_pass": "64x64 with outer x and inner y; each cell receives exactly one final value",
        "cell_index": "signed y*64+x",
        "predicate": "upper_layer != 0",
        "unblocked_value": 254,
        "blocked_value": 555,
        "upper_layer_vectors": [
            {"value": value, "result": classify(value)}
            for value in (-32768, -1, 0, 1, 32767)
        ],
        "occupancy_reads": 0,
        "ground_reads": 0,
        "caller_continuation": caller_continuation,
        "caller_uses_return": False,
        "return": "normal completion returns EAX=8190, the final cell byte offset",
        "direct_rng_draws": 0,
        "closure_boundary":
            "sub_36E7F caller and sub_37070 flood step remain independent owners",
    }


def battle_flood_step_contract(z_dat_bytes: bytes) -> dict[str, object]:
    contract = relocated_machine_function_contract(
        z_dat_bytes,
        address=BATTLE_FLOOD_STEP_ADDRESS,
        end=BATTLE_FLOOD_STEP_END,
        call_offsets=BATTLE_FLOOD_STEP_CALL_OFFSETS,
        expected_call_targets=BATTLE_FLOOD_STEP_CALL_TARGETS,
        relocation_offsets=BATTLE_FLOOD_STEP_RELOCATION_OFFSETS,
        caller_sites=BATTLE_FLOOD_STEP_CALLER_SITES,
        instruction_count=76,
        branch_count=11,
    )
    if contract["raw_sha256"] != (
        "0f08f89615cfeef431df99ad4136751a7dd8264ce0e724472b5926e51ba0116d"
    ):
        raise ValueError("Z.DAT flood step raw bytes changed")
    if contract["loaded_sha256"] != (
        "2230edd43668c7b5259fd41f3e68a03316edcb1adb6c67597f94fd0569e1473e"
    ):
        raise ValueError("Z.DAT flood step relocation image changed")

    direction_x = struct.unpack_from(
        "<4h", z_dat_bytes, BATTLE_FLOOD_DIRECTION_X_ADDRESS - Z_DAT_LOAD_BASE
    )
    direction_y = struct.unpack_from(
        "<4h", z_dat_bytes, BATTLE_FLOOD_DIRECTION_Y_ADDRESS - Z_DAT_LOAD_BASE
    )
    directions = tuple(zip(direction_x, direction_y))
    if directions != PATH_DIRECTIONS:
        raise ValueError("Z.DAT flood step direction order changed")
    caller_continuations = {
        "0x36e75": z_dat_bytes[0x36E7A - Z_DAT_LOAD_BASE:0x36E7F - Z_DAT_LOAD_BASE].hex(),
        "0x36eee": z_dat_bytes[0x36EF3 - Z_DAT_LOAD_BASE:0x36EF8 - Z_DAT_LOAD_BASE].hex(),
    }
    if set(caller_continuations.values()) != {"85c074f7c3"}:
        raise ValueError("Z.DAT flood step caller continuation changed")
    shared_tail = z_dat_bytes[
        BATTLE_FLOOD_SHARED_TAIL_ADDRESS - Z_DAT_LOAD_BASE:
        BATTLE_FLOOD_SHARED_TAIL_END - Z_DAT_LOAD_BASE
    ]
    if shared_tail.hex() != "83c4085f5e5bc3":
        raise ValueError("Z.DAT flood step shared tail changed")

    values = [555] * 4096
    source = (32, 20)
    values[source[1] * 64 + source[0]] = 0
    for dx, dy in directions:
        values[(source[1] + dy) * 64 + source[0] + dx] = 254
    queue = [(0, 0)] * 255
    queue[0] = (0, -1)
    queue[1] = source
    read_index = 0
    write_index = 2
    distance = 0

    def enqueue(coordinate: tuple[int, int]) -> None:
        nonlocal write_index
        queue[write_index] = coordinate
        write_index = (write_index + 1) % 255

    def dequeue() -> tuple[int, int]:
        nonlocal read_index
        coordinate = queue[read_index]
        read_index = (read_index + 1) % 255
        return coordinate

    def step() -> dict[str, object]:
        nonlocal distance
        current = dequeue()
        sentinel = current[1] < 0
        if sentinel:
            distance = (distance + 1) % 128
            enqueue((0, -1))
            current = dequeue()
            if current[1] < 0:
                return {
                    "result": -1,
                    "sentinel": True,
                    "processed": None,
                    "enqueued": [],
                    "distance": distance,
                    "read_index": read_index,
                    "write_index": write_index,
                }
        enqueued = []
        for dx, dy in directions:
            coordinate = (current[0] + dx, current[1] + dy)
            x, y = coordinate
            if not (0 <= x <= 64 and 0 <= y <= 64):
                continue
            index = y * 64 + x
            if 0 <= index < 4096 and values[index] == 254:
                enqueue(coordinate)
                values[index] = distance
                enqueued.append(list(coordinate))
        return {
            "result": 0,
            "sentinel": sentinel,
            "processed": list(current),
            "enqueued": enqueued,
            "distance": distance,
            "read_index": read_index,
            "write_index": write_index,
        }

    synthetic_trace = []
    while not synthetic_trace or synthetic_trace[-1]["result"] == 0:
        synthetic_trace.append(step())
    expected_trace = [
        {"result": 0, "sentinel": True, "processed": [32, 20],
         "enqueued": [[32, 19], [33, 20], [31, 20], [32, 21]],
         "distance": 1, "read_index": 2, "write_index": 7},
        {"result": 0, "sentinel": True, "processed": [32, 19], "enqueued": [],
         "distance": 2, "read_index": 4, "write_index": 8},
        {"result": 0, "sentinel": False, "processed": [33, 20], "enqueued": [],
         "distance": 2, "read_index": 5, "write_index": 8},
        {"result": 0, "sentinel": False, "processed": [31, 20], "enqueued": [],
         "distance": 2, "read_index": 6, "write_index": 8},
        {"result": 0, "sentinel": False, "processed": [32, 21], "enqueued": [],
         "distance": 2, "read_index": 7, "write_index": 8},
        {"result": -1, "sentinel": True, "processed": None, "enqueued": [],
         "distance": 3, "read_index": 9, "write_index": 9},
    ]
    if synthetic_trace != expected_trace:
        raise ValueError("flood step synthetic trace changed")

    return {
        **contract,
        "stack_probe_bytes": 36,
        "arguments": "none; queue, read/write indexes, distance and path map are shared globals",
        "dequeue": "dequeue x/y; any signed y<0 is a layer sentinel",
        "sentinel":
            "distance=(signed int16(distance)+1)%128; enqueue (0,-1); dequeue again; return -1 if second y<0",
        "directions": [list(value) for value in directions],
        "coordinate_math":
            "int16 x/y additions; accept each coordinate in signed inclusive range 0..64",
        "expansion":
            "when delegated path read equals exactly 254, enqueue coordinate then write current signed distance",
        "synthetic_trace": synthetic_trace,
        "normal_return": 0,
        "finished_return": -1,
        "caller_continuations": caller_continuations,
        "caller_control": "both callers loop only while EAX is zero",
        "shared_tail": {
            "address": hex(BATTLE_FLOOD_SHARED_TAIL_ADDRESS),
            "end": hex(BATTLE_FLOOD_SHARED_TAIL_END),
            "bytes": shared_tail.hex(),
            "sha256": sha256(shared_tail),
            "semantics": "add esp,8; pop edi; pop esi; pop ebx; ret; EAX unchanged",
        },
        "direct_rng_draws": 0,
        "closure_boundary":
            "sub_37166, sub_371AE, sub_371F7, sub_3721E and shared tail owner sub_3598C remain independent owners",
    }


def battle_dequeue_contract(z_dat_bytes: bytes) -> dict[str, object]:
    contract = relocated_machine_function_contract(
        z_dat_bytes,
        address=BATTLE_DEQUEUE_ADDRESS,
        end=BATTLE_DEQUEUE_END,
        call_offsets=BATTLE_DEQUEUE_CALL_OFFSETS,
        expected_call_targets=BATTLE_DEQUEUE_CALL_TARGETS,
        relocation_offsets=BATTLE_DEQUEUE_RELOCATION_OFFSETS,
        caller_sites=BATTLE_DEQUEUE_CALLER_SITES,
        instruction_count=21,
        branch_count=0,
    )
    if contract["raw_sha256"] != (
        "1fab3fba7ceae1e002dce08eebdcf2e7b48ce91dc7ccc61ef494e40a27ccdf9b"
    ):
        raise ValueError("Z.DAT dequeue raw bytes changed")
    if contract["loaded_sha256"] != (
        "3bd60c28edc86229bb6609c2dfac3809e6b1a6d4077a2e5af4295a0ddc7d2e6a"
    ):
        raise ValueError("Z.DAT dequeue relocation image changed")
    caller_continuations = {
        "0x3708f": z_dat_bytes[0x37094 - Z_DAT_LOAD_BASE:0x3709F - Z_DAT_LOAD_BASE].hex(),
        "0x370d5": z_dat_bytes[0x370DA - Z_DAT_LOAD_BASE:0x370E5 - Z_DAT_LOAD_BASE].hex(),
    }
    if caller_continuations != {
        "0x3708f": "83c40c66837c2404007d50",
        "0x370d5": "83c40c66837c2404007d0a",
    }:
        raise ValueError("Z.DAT dequeue caller continuation changed")

    x_queue = [0] * 255
    y_queue = [0] * 255
    x_queue[0], y_queue[0] = -32768, 32767
    x_queue[253], y_queue[253] = 1234, -1234
    x_queue[254], y_queue[254] = 32767, -32768
    read_index = 253
    synthetic_wrap_trace = []
    for _ in range(3):
        old_index = read_index
        x = x_queue[read_index]
        y = y_queue[read_index]
        dividend = read_index + 1
        quotient = int(dividend / 255)
        read_index = dividend - quotient * 255
        synthetic_wrap_trace.append({
            "old_index": old_index,
            "x": x,
            "y": y,
            "quotient_return": quotient,
            "new_index": read_index,
        })
    expected_trace = [
        {"old_index": 253, "x": 1234, "y": -1234,
         "quotient_return": 0, "new_index": 254},
        {"old_index": 254, "x": 32767, "y": -32768,
         "quotient_return": 1, "new_index": 0},
        {"old_index": 0, "x": -32768, "y": 32767,
         "quotient_return": 0, "new_index": 1},
    ]
    if synthetic_wrap_trace != expected_trace:
        raise ValueError("dequeue synthetic wrap trace changed")

    return {
        **contract,
        "stack_probe_bytes": 8,
        "queue_layout": {
            "slots": 255,
            "x_words": {
                "address": hex(BATTLE_DEQUEUE_X_WORDS_ADDRESS),
                "end": hex(BATTLE_DEQUEUE_Y_WORDS_ADDRESS),
            },
            "y_words": {
                "address": hex(BATTLE_DEQUEUE_Y_WORDS_ADDRESS),
                "end": hex(BATTLE_DEQUEUE_Y_WORDS_ADDRESS + 255 * 2),
            },
            "read_index": hex(BATTLE_DEQUEUE_READ_INDEX_ADDRESS),
        },
        "caller_arguments":
            "both callers pass distinct x local, y local and shared read-index pointer",
        "write_order": "x output, then y output, then read-index remainder",
        "index_math":
            "signed int16 index reloaded for x, y and update; (index+1) signed idiv 255",
        "legal_index_domain": "wrappers initialize 0 and modulo update preserves 0..254",
        "synthetic_wrap_trace": synthetic_wrap_trace,
        "return": "signed division quotient: 1 only for legal index 254, otherwise 0",
        "caller_continuations": caller_continuations,
        "caller_uses_return": False,
        "direct_rng_draws": 0,
        "closure_boundary":
            "sub_37070 caller remains independently closed; sub_371AE enqueue remains independent",
    }


def battle_enqueue_contract(z_dat_bytes: bytes) -> dict[str, object]:
    contract = relocated_machine_function_contract(
        z_dat_bytes,
        address=BATTLE_ENQUEUE_ADDRESS,
        end=BATTLE_ENQUEUE_END,
        call_offsets=BATTLE_ENQUEUE_CALL_OFFSETS,
        expected_call_targets=BATTLE_ENQUEUE_CALL_TARGETS,
        relocation_offsets=BATTLE_ENQUEUE_RELOCATION_OFFSETS,
        caller_sites=BATTLE_ENQUEUE_CALLER_SITES,
        instruction_count=18,
        branch_count=0,
    )
    if contract["raw_sha256"] != (
        "b06189eb16e1f2aac51e7242d7f18221427a0f26de02589ef40c835a688c1976"
    ):
        raise ValueError("Z.DAT enqueue raw bytes changed")
    if contract["loaded_sha256"] != (
        "76a887a959bf8e3c83699ae298131217201ff6f4e59fb84f2e7e3e4ee0d0df92"
    ):
        raise ValueError("Z.DAT enqueue relocation image changed")
    caller_continuations = {
        "0x370be": z_dat_bytes[0x370C3 - Z_DAT_LOAD_BASE:0x370CF - Z_DAT_LOAD_BASE].hex(),
        "0x3713e": z_dat_bytes[0x37143 - Z_DAT_LOAD_BASE:0x3714D - Z_DAT_LOAD_BASE].hex(),
    }
    if caller_continuations != {
        "0x370be": "83c40868d46e0c008d442408",
        "0x3713e": "83c4080fbf05d86e0c00",
    }:
        raise ValueError("Z.DAT enqueue caller continuation changed")

    x_queue = [0] * 255
    y_queue = [0] * 255
    write_index = 253
    coordinates = ((1234, -1234), (32767, -32768), (-32768, 32767))
    synthetic_wrap_trace = []
    for x, y in coordinates:
        old_index = write_index
        x_queue[old_index] = x
        y_queue[old_index] = y
        dividend = old_index + 1
        quotient = int(dividend / 255)
        write_index = dividend - quotient * 255
        synthetic_wrap_trace.append({
            "old_index": old_index,
            "x": x,
            "y": y,
            "quotient_return": quotient,
            "new_index": write_index,
        })
    expected_trace = [
        {"old_index": 253, "x": 1234, "y": -1234,
         "quotient_return": 0, "new_index": 254},
        {"old_index": 254, "x": 32767, "y": -32768,
         "quotient_return": 1, "new_index": 0},
        {"old_index": 0, "x": -32768, "y": 32767,
         "quotient_return": 0, "new_index": 1},
    ]
    if synthetic_wrap_trace != expected_trace:
        raise ValueError("enqueue synthetic wrap trace changed")
    if [(x_queue[index], y_queue[index]) for index in (253, 254, 0)] != list(coordinates):
        raise ValueError("enqueue synthetic queue writes changed")

    return {
        **contract,
        "stack_probe_bytes": 12,
        "queue_layout": {
            "slots": 255,
            "x_words": {
                "address": hex(BATTLE_ENQUEUE_X_WORDS_ADDRESS),
                "end": hex(BATTLE_ENQUEUE_Y_WORDS_ADDRESS),
            },
            "y_words": {
                "address": hex(BATTLE_ENQUEUE_Y_WORDS_ADDRESS),
                "end": hex(BATTLE_ENQUEUE_Y_WORDS_ADDRESS + 255 * 2),
            },
            "write_index": hex(BATTLE_ENQUEUE_WRITE_INDEX_ADDRESS),
        },
        "arguments":
            "first argument x and second argument y are stored as signed 16-bit words",
        "write_order":
            "read one signed write index; write x then y to that same slot; update index last",
        "index_math":
            "signed int16 index plus one uses signed idiv 255; remainder stored globally",
        "legal_index_domain":
            "wrappers initialize 2 and modulo update preserves 0..254; no full guard",
        "synthetic_wrap_trace": synthetic_wrap_trace,
        "return": "signed division quotient: 1 only for legal index 254, otherwise 0",
        "caller_roles": {
            "0x370be": "enqueue (0,-1) sentinel",
            "0x3713e": "enqueue candidate (x,y)",
        },
        "caller_continuations": caller_continuations,
        "caller_uses_return": False,
        "direct_rng_draws": 0,
        "closure_boundary":
            "sub_37070 caller remains independently closed; sub_371F7 path write remains independent",
    }


def battle_targeting_path_contract(z_dat_bytes: bytes) -> dict[str, object]:
    contract = relocated_machine_function_contract(
        z_dat_bytes,
        address=BATTLE_TARGETING_PATH_ADDRESS,
        end=BATTLE_TARGETING_PATH_END,
        call_offsets=BATTLE_TARGETING_PATH_CALL_OFFSETS,
        expected_call_targets=BATTLE_TARGETING_PATH_CALL_TARGETS,
        relocation_offsets=BATTLE_TARGETING_PATH_RELOCATION_OFFSETS,
        caller_sites=BATTLE_TARGETING_PATH_CALLER_SITES,
        instruction_count=20,
        branch_count=1,
    )
    if contract["raw_sha256"] != (
        "7de2240639b89bd98d88ad212cdce7dadf53b0d35255af2524d7155fcda890a6"
    ):
        raise ValueError("Z.DAT targeting path wrapper raw bytes changed")
    if contract["loaded_sha256"] != (
        "7de0062638c1709694717aef2703770addcd76495c6ee171a327d8cfd69d5610"
    ):
        raise ValueError("Z.DAT targeting path wrapper relocation image changed")
    expected_post_call_until_eax_overwrite = {
        0x34DAF: "0fbf05e06e0c00",
        0x34EC7: "0fbf05e06e0c00",
        0x34FAA: "0fbf05e06e0c00",
        0x353CF: "0fbf8332c70b00",
        0x354E3: "0fbf0de06e0c006bc91c0fbf9132c70b00c1e2070fbf8130c70b00",
        0x3558C: "0fbf0de06e0c006bc91c0fbf9132c70b00c1e2070fbf8130c70b00",
        0x357A6: "0fbf05e06e0c00",
        0x358B2: "0fbf05e06e0c00",
        0x3594E: "0fbf05e06e0c00",
        0x36262: "0fbf05e06e0c00",
        0x36335: "0fbf05e06e0c00",
        0x363FE: "0fbf05e06e0c00",
        0x36496: "0fbf05e06e0c00",
        0x36567: "0fbf54240cc1e2070fbf442408",
        0x36B72: "66c70600008b442414",
    }
    actual_post_call_until_eax_overwrite = {}
    for site, expected in expected_post_call_until_eax_overwrite.items():
        offset = site - Z_DAT_LOAD_BASE + 5
        actual = z_dat_bytes[offset:offset + len(bytes.fromhex(expected))].hex()
        if actual != expected:
            raise ValueError(f"Z.DAT targeting path caller continuation changed at {site:#x}")
        actual_post_call_until_eax_overwrite[hex(site)] = actual
    return {
        **contract,
        "stack_probe_bytes": 4,
        "arguments": "none; source x/y are read as signed int16 globals",
        "initializer": "sub_36FF9 targeting blocking map",
        "source": {
            "x_global": "0x556d6",
            "y_global": "0x556d8",
            "index": "signed y*64 + signed x",
            "forced_value": 0,
        },
        "queue": {
            "slots": 255,
            "read_index": 0,
            "write_index": 2,
            "distance": 0,
            "slot0": [0, -1],
            "slot1": "source",
        },
        "loop": "call sub_37070 until EAX is nonzero",
        "return": "first nonzero EAX from sub_37070 passes through RET",
        "caller_post_call_until_eax_overwrite": actual_post_call_until_eax_overwrite,
        "caller_uses_return": False,
        "direct_rng_draws": 0,
        "closure_boundary": "sub_36FF9 and sub_37070 remain independent owners",
    }


def battle_ai_specialist_target_contract(z_dat_bytes: bytes) -> dict[str, object]:
    contract = relocated_machine_function_contract(
        z_dat_bytes,
        address=BATTLE_AI_SPECIALIST_TARGET_ADDRESS,
        end=BATTLE_AI_SPECIALIST_TARGET_END,
        call_offsets=BATTLE_AI_SPECIALIST_TARGET_CALL_OFFSETS,
        expected_call_targets=(0x3ED1E, 0x351A7),
        relocation_offsets=BATTLE_AI_SPECIALIST_TARGET_RELOCATION_OFFSETS,
        caller_sites=BATTLE_AI_SPECIALIST_TARGET_CALLER_SITES,
        instruction_count=91,
        branch_count=19,
    )
    if contract["raw_sha256"] != (
        "461f8b3d53f24104b1ca7305af927bd28945715d425f93bd48e793ab423f6916"
    ):
        raise ValueError("Z.DAT battle AI specialist-target raw bytes changed")
    if contract["loaded_sha256"] != (
        "cdb3563325b0382c5bf05d0f4744cc041519d55d94120fc35b6fcc3ad5f4216c"
    ):
        raise ValueError("Z.DAT battle AI specialist-target relocation image changed")
    return {
        **contract,
        "slot_loop_comparison": "signed int16 slot < combatant_count",
        "trigger_scan": {
            "candidate_filter": "side == actor side only",
            "checks_hidden_or_hp": False,
            "role_field": "use_poison",
            "comparison": "signed value > 20",
            "early_exit": False,
        },
        "detoxification_scan": {
            "candidate_filters": ["side != actor side", "hidden == 0"],
            "best_initial": 0,
            "comparison": "signed detoxification > best",
            "target_write_before_threshold": True,
            "threshold": "selected value >= 20",
        },
        "medicine_scan": {
            "candidate_filters": ["side != actor side", "hidden == 0"],
            "best_initial": "inherited detoxification best or 0",
            "comparison": "signed medicine > inherited best",
            "target_write_before_threshold": True,
            "threshold": "selected value >= 20",
        },
        "independent_flag_bug": (
            "detoxification >= 20 skips medicine but final check still tests only "
            "medicine flag and therefore calls weakest-attack fallback"
        ),
        "ties_preserve_first_slot": True,
        "reads_hp": False,
        "consumes_random": False,
        "caller_uses_return": False,
    }


def battle_ai_nearest_target_contract(z_dat_bytes: bytes) -> dict[str, object]:
    contract = relocated_machine_function_contract(
        z_dat_bytes,
        address=BATTLE_AI_NEAREST_TARGET_ADDRESS,
        end=BATTLE_AI_NEAREST_TARGET_END,
        call_offsets=BATTLE_AI_NEAREST_TARGET_CALL_OFFSETS,
        expected_call_targets=(0x3ED1E, 0x36E7F),
        relocation_offsets=BATTLE_AI_NEAREST_TARGET_RELOCATION_OFFSETS,
        caller_sites=BATTLE_AI_NEAREST_TARGET_CALLER_SITES,
        instruction_count=39,
        branch_count=5,
    )
    if contract["raw_sha256"] != (
        "c20a0bad9f0b84b7d6c757dd0f2338f9bf6427e022c3c7644c8a378d06ff17f1"
    ):
        raise ValueError("Z.DAT battle AI nearest-target raw bytes changed")
    if contract["loaded_sha256"] != (
        "d5a95b43e79f688fad6798458fd71c41bc91e1e00aaa225b380e22a24e8a457c"
    ):
        raise ValueError("Z.DAT battle AI nearest-target relocation image changed")
    return {
        **contract,
        "slot_loop_comparison": "signed int16 slot < combatant_count",
        "candidate_filters": ["side != actor side", "hidden == 0"],
        "best_initial": 1000,
        "path_mode": "targeting",
        "path_rebuild": "once per qualifying candidate from actor coordinates",
        "distance_lookup": "signed int16 path[candidate_y * 64 + candidate_x]",
        "distance_comparison": "signed candidate < best",
        "ties_preserve_first_slot": True,
        "blocked_distance_555_can_write_target": True,
        "no_candidate_preserves_target": True,
        "reads_hp": False,
        "consumes_random": False,
        "callers_use_return": False,
    }


def battle_round_machine_contract(
    z_dat_bytes: bytes, ranger_group_bytes: bytes
) -> dict[str, object]:
    round_values = []
    for label, base_speed, equipment_speeds, hurt in (
        ("ordinary", 100, [7, -10], 79),
        ("positive_wrap", 32760, [20], 0),
        ("negative_wrap", -32760, [-20], -79),
    ):
        effective_speed = base_speed
        for item_speed in equipment_speeds:
            effective_speed = wrapping_i16(effective_speed + item_speed)
        value = trunc_div(effective_speed, 15) - trunc_div(hurt, 40)
        round_values.append({
            "label": label,
            "base_speed": base_speed,
            "equipment_add_speed": equipment_speeds,
            "hurt": hurt,
            "effective_speed": effective_speed,
            "round_value": max(value, 0),
        })

    speed_entries = [
        {"label": "equal_a", "speed": 10},
        {"label": "equal_b", "speed": 10},
        {"label": "fast", "speed": 20},
        {"label": "slow", "speed": -3},
    ]
    sorted_entries = [dict(entry) for entry in speed_entries]
    swap_trace = []
    for first in range(len(sorted_entries) - 1):
        for second in range(first + 1, len(sorted_entries)):
            if sorted_entries[first]["speed"] < sorted_entries[second]["speed"]:
                swap_trace.append([first, second])
                sorted_entries[first], sorted_entries[second] = (
                    sorted_entries[second], sorted_entries[first]
                )

    first = [10, 0, 12, 13, 2, 0, 7, 8, 6000, 9, 10, 11, 12, 13]
    second = [20, 1, 12, 13, 3, 0, 17, 18, 6100, 19, 20, 21, 22, 23]
    copied_words = [*range(0, 8), *range(9, 14)]
    exchanged_first = list(first)
    exchanged_second = list(second)
    for word in copied_words:
        exchanged_first[word] = second[word]
    occupancy_writes = [{"slot": 0, "x": second[2], "y": second[3], "value": 0}]
    for word in copied_words:
        exchanged_second[word] = first[word]
    occupancy_writes.append({"slot": 1, "x": first[2], "y": first[3], "value": 1})
    sprite_globals = [
        struct.unpack_from("<h", z_dat_bytes, address - Z_DAT_LOAD_BASE)[0]
        for address in (0x556CC, 0x556D4)
    ]
    sprite_results = []
    for words in (exchanged_first, exchanged_second):
        head = struct.unpack_from("<h", ranger_group_bytes, 836 + words[0] * 182 + 2)[0]
        words[8] = wrapping_i16(head * 8 + sprite_globals[0] * 2 + sprite_globals[1] + words[4] * 2)
        sprite_results.append(words[8])

    outcome_calls = {
        site: relative_call_target(z_dat_bytes, site - Z_DAT_LOAD_BASE, Z_DAT_LOAD_BASE)
        for site in (0x3B300, 0x3B371, 0x3B379, 0x3B37E)
    }
    if list(outcome_calls.values()) != [0x3AA85, 0x3D6D1, 0x20C32, 0x3B387]:
        raise ValueError("Z.DAT result display/wait/settlement boundary changed")
    shared_exit = z_dat_bytes[0x35407 - Z_DAT_LOAD_BASE:0x3540E - Z_DAT_LOAD_BASE]
    if shared_exit.hex() != "83c4045f5e5bc3":
        raise ValueError("Z.DAT battle loop shared return changed")

    return {
        "round_loop": {
            **relocated_machine_function_contract(
                z_dat_bytes,
                address=BATTLE_ROUND_LOOP_ADDRESS,
                end=BATTLE_ROUND_LOOP_END,
                call_offsets=BATTLE_ROUND_LOOP_CALL_OFFSETS,
                expected_call_targets=(
                    0x3ED1E, 0x32A51, 0x3AA85, 0x3D6D1, 0x3CD17,
                    0x32A51, 0x3AA85, 0x3D6D1, 0x32E59, 0x33599,
                    0x3B238, 0x3C672, 0x3C563,
                ),
                relocation_offsets=BATTLE_ROUND_LOOP_RELOCATION_OFFSETS,
                caller_sites=(0x31D39,),
                instruction_count=166,
                branch_count=27,
            ),
            "round_value_vectors": round_values,
            "sequence": [
                "initial_sort", "initial_actor_present", "initial_fade",
                "capture_tick", "round_sort_once", "clear_render_globals",
                "compute_round_values", "actor_loop", "round_status",
                "wait_until_tick_differs", "repeat_or_return",
            ],
            "confirm_state_offsets": [0x0D, 0x20, 0x96],
            "confirm_keys": ["enter", "space", "keypad_insert"],
            "confirm_effect": "sample current key states before each slot hidden check; clear all three and disable automatic mode before actor rendering",
            "shared_exit": {"address": "0x35407", "end": "0x3540e", "bytes": shared_exit.hex()},
            "released_confirmation": "a key released before the slot boundary does not cancel automatic mode",
            "hidden_actor": "skip dispatch, then evaluate outcome and clear hidden AI targets",
            "action_result_six": "decrement actor index before common post-action calls",
            "outcome_boundary": [
                "evaluate_result", "result_screen_present", "wait_for_key",
                "complete_post_battle_settlement_and_messages",
                "clear_hidden_ai_targets", "apply_round_status_once",
                "wait_until_tick_differs_without_redraw", "return_to_battle_entry",
            ],
            "outcome_callee_boundary_calls": {
                hex(site): hex(target) for site, target in outcome_calls.items()
            },
        },
        "speed_sort": {
            **relocated_machine_function_contract(
                z_dat_bytes,
                address=BATTLE_SPEED_SORT_ADDRESS,
                end=BATTLE_SPEED_SORT_END,
                call_offsets=BATTLE_SPEED_SORT_CALL_OFFSETS,
                expected_call_targets=(0x3ED1E, 0x32B78),
                relocation_offsets=BATTLE_SPEED_SORT_RELOCATION_OFFSETS,
                caller_sites=(0x3273E, 0x327EF),
                instruction_count=69,
                branch_count=9,
            ),
            "algorithm": "pairwise exchange sort, fixed first slot against every later slot, descending signed effective speed",
            "swap_condition": "first_speed < second_speed",
            "equal_speed_order": "no direct swap on equality; indirect exchanges may reverse equal-speed actors",
            "input": speed_entries,
            "swap_trace": swap_trace,
            "output": sorted_entries,
        },
        "combatant_swap": {
            **relocated_machine_function_contract(
                z_dat_bytes,
                address=BATTLE_COMBATANT_SWAP_ADDRESS,
                end=BATTLE_COMBATANT_SWAP_END,
                call_offsets=BATTLE_COMBATANT_SWAP_CALL_OFFSETS,
                expected_call_targets=(0x3ED1E, 0x3B1E6, 0x3B1E6),
                relocation_offsets=BATTLE_COMBATANT_SWAP_RELOCATION_OFFSETS,
                caller_sites=(0x32B52, 0x3AA30),
                instruction_count=131,
                branch_count=4,
            ),
            "copied_word_indexes": copied_words,
            "excluded_word_index": 8,
            "occupancy_write_order": "first_slot_then_second_slot",
            "sprite_recompute_order": "first_slot_then_second_slot",
            "vector": {
                "input": [first, second],
                "asset_initial_sprite_globals": sprite_globals,
                "asset_head_role_ids": [20, 10],
                "delegated_sprite_results": sprite_results,
                "occupancy_writes": occupancy_writes,
                "final": [exchanged_first, exchanged_second],
                "same_coordinate_final_occupancy": 1,
            },
        },
    }


def cumulative_entries(index_bytes: bytes, group_bytes: bytes) -> list[bytes]:
    if len(index_bytes) % 4 != 0:
        raise ValueError("IDX size is not divisible by four")
    offsets = struct.unpack(f"<{len(index_bytes) // 4}I", index_bytes)
    entries: list[bytes] = []
    start = 0
    for end in offsets:
        if end < start or end > len(group_bytes):
            raise ValueError(f"invalid cumulative offset {end} after {start}")
        entries.append(group_bytes[start:end])
        start = end
    if start != len(group_bytes):
        raise ValueError(f"final cumulative offset {start} != group size {len(group_bytes)}")
    return entries


def archive_record(index_path: Path, group_path: Path) -> dict[str, object]:
    index_bytes = index_path.read_bytes()
    group_bytes = group_path.read_bytes()
    entries = cumulative_entries(index_bytes, group_bytes)
    return {
        "index_size": len(index_bytes),
        "index_sha256": sha256(index_bytes),
        "group_size": len(group_bytes),
        "group_sha256": sha256(group_bytes),
        "entry_count": len(entries),
        "entry_sizes": [len(entry) for entry in entries],
        "entry_sha256": [sha256(entry) for entry in entries],
    }


def fnv1a_bytes(data: bytes | bytearray) -> str:
    value = 0xCBF29CE484222325
    for byte in data:
        value ^= byte
        value = value * 0x100000001B3 & 0xFFFFFFFFFFFFFFFF
    return f"0x{value:016x}"


def fnv1a_words(words: list[int]) -> str:
    value = 0xCBF29CE484222325
    for word in words:
        raw = word & 0xFFFF
        for byte in (raw & 0xFF, raw >> 8):
            value ^= byte
            value = value * 0x100000001B3 & 0xFFFFFFFFFFFFFFFF
    return f"0x{value:016x}"


def wrapping_i16(value: int) -> int:
    value &= 0xFFFF
    return value - 0x10000 if value >= 0x8000 else value


def trunc_div(value: int, divisor: int) -> int:
    quotient = abs(value) // abs(divisor)
    return -quotient if (value < 0) != (divisor < 0) else quotient


def legacy_bounded(state: int, upper_bound: int) -> tuple[int, int]:
    if upper_bound <= 1 or upper_bound > 30_000:
        return 0, state
    state = (state * 0x41C64E6D + 0x3039) & 0xFFFF_FFFF
    return ((state >> 16) & 0x7FFF) % upper_bound, state


def throwing_weapon_vector(
    item: bytes,
    *,
    seed: int,
    hidden_weapon: int,
    hp: int,
    maximum_hp: int,
    hurt: int,
    poison: int,
    anti_poison: int,
) -> dict[str, object]:
    word = lambda index: struct.unpack_from("<h", item, index * 2)[0]
    divisor = 4 if hurt == 0 else 3 if hurt <= 33 else 2 if hurt <= 66 else 1
    damage_random, state = legacy_bounded(seed, 5)
    base_delta = trunc_div(word(45), divisor) - damage_random
    hp_delta = wrapping_i16(trunc_div(base_delta - 2 * hidden_weapon, 3))
    hurt_after = min(99, max(0, wrapping_i16(hurt - trunc_div(hp_delta, 4))))
    hp_after = wrapping_i16(hp + hp_delta)
    if hp_after >= maximum_hp:
        hp_after = maximum_hp
    if hp_after <= 0:
        hp_after = 0
    damage = wrapping_i16(abs(hp_after - hp))

    item_poison = word(47)
    poison_rng: list[int] = []
    if item_poison > 0:
        poison_delta = wrapping_i16(trunc_div(item_poison - hidden_weapon, 2) - anti_poison)
        if anti_poison >= 100 or poison_delta < 0:
            poison_delta = 0
        poison_delta = wrapping_i16(trunc_div(poison_delta, 2))
    else:
        first, state = legacy_bounded(state, 5)
        second, state = legacy_bounded(state, 5)
        poison_rng = [first, second]
        poison_delta = wrapping_i16(trunc_div(item_poison, 2) + first - second)
    poison_after = wrapping_i16(poison + poison_delta)
    if poison_after >= 99:
        poison_after = 99
    if poison_after <= 0:
        poison_after = 0
    return {
        "item_id": word(0),
        "item_type": word(41),
        "effect_id": word(37),
        "add_hp": word(45),
        "add_poison": item_poison,
        "rng_seed": seed,
        "rng_outputs": [damage_random, *poison_rng],
        "rng_state_after": state,
        "hidden_weapon": hidden_weapon,
        "targeting_range": trunc_div(hidden_weapon, 15) + 1,
        "hp_before": hp,
        "maximum_hp": maximum_hp,
        "hurt_before": hurt,
        "poison_before": poison,
        "anti_poison": anti_poison,
        "hp_delta": hp_delta,
        "damage": damage,
        "hp_after": hp_after,
        "hurt_after": hurt_after,
        "poison_after": poison_after,
    }


def ai_throwing_weapon_vector(
    item: bytes,
    *,
    source_item: bytes | None = None,
    seed: int,
    hidden_weapon: int,
    hp: int,
    maximum_hp: int,
    hurt: int,
    poison: int,
) -> dict[str, object]:
    word = lambda index: struct.unpack_from("<h", item, index * 2)[0]
    source_word = lambda index: struct.unpack_from(
        "<h", source_item if source_item is not None else item, index * 2
    )[0]
    divisor = 4 if hurt == 0 else 3 if hurt <= 33 else 2 if hurt <= 66 else 1
    damage_random, state = legacy_bounded(seed, 5)
    randomized_base = wrapping_i16(trunc_div(word(45), divisor) - damage_random)
    hp_delta = wrapping_i16(trunc_div(randomized_base - 2 * hidden_weapon, 3))
    hurt_after = min(99, max(0, wrapping_i16(hurt - trunc_div(hp_delta, 4))))
    hp_after = wrapping_i16(hp + hp_delta)
    if hp_after >= maximum_hp:
        hp_after = maximum_hp
    if hp_after <= 0:
        hp_after = 0
    damage = wrapping_i16(abs(hp_after - hp))

    item_poison = word(47)
    poison_delta = (
        wrapping_i16(trunc_div(item_poison - hidden_weapon, 2))
        if item_poison < 0
        else item_poison
    )
    poison_after = wrapping_i16(poison + poison_delta)
    if poison_after >= 99:
        poison_after = 99
    if poison_after <= 0:
        poison_after = 0
    return {
        "item_id": source_word(0),
        "payload_item_id": word(0),
        "item_type": source_word(41),
        "effect_id": source_word(37),
        "add_hp": word(45),
        "add_poison": item_poison,
        "rng_seed": seed,
        "rng_outputs": [damage_random],
        "rng_state_after": state,
        "hidden_weapon": hidden_weapon,
        "hp_before": hp,
        "maximum_hp": maximum_hp,
        "hurt_before": hurt,
        "poison_before": poison,
        "randomized_base_word": randomized_base,
        "hp_delta": hp_delta,
        "damage": damage,
        "hp_after": hp_after,
        "hurt_after": hurt_after,
        "poison_delta": poison_delta,
        "poison_after": poison_after,
    }


def carried_item_removal_vectors() -> dict[str, object]:
    source_ids = [5, 6, 7, 8]
    source_counts = [1, 2, 3, 4]
    vectors = []
    for deletion_slot in range(4):
        ids = list(source_ids)
        counts = list(source_counts)
        for slot in range(deletion_slot, 3):
            ids[slot] = ids[slot + 1]
            counts[slot] = counts[slot + 1]
        ids[3] = -1
        counts[3] = 0
        vectors.append(
            {
                "deletion_slot": deletion_slot,
                "item_ids_after": ids,
                "quantities_after": counts,
                "loop_iterations": 3 - deletion_slot,
            }
        )
    return {
        "item_ids_before": source_ids,
        "quantities_before": source_counts,
        "legal_slots": [0, 1, 2, 3],
        "vectors": vectors,
        "pairing_preserved": True,
        "slot3_always_cleared": True,
    }


def shared_item_effect_vector(
    item: bytes,
    *,
    seed: int,
    role_words: list[int],
) -> dict[str, object]:
    item_word = lambda index: struct.unpack_from("<h", item, index * 2)[0]
    role = list(role_words)
    before = list(role)
    deltas = [0] * 23
    outputs: list[int] = []
    state = seed
    hidden_weapon = role[54]

    item_hp = item_word(45)
    if item_hp:
        old_hp = role[17]
        if item_hp > 0:
            value, state = legacy_bounded(state, 10)
            outputs.append(value)
            hp_delta = item_hp - trunc_div(role[19], 2) + value
            if hp_delta < 0:
                value, state = legacy_bounded(state, 5)
                outputs.append(value)
                hp_delta = value + 5
            role[19] = wrapping_i16(role[19] - trunc_div(item_hp, 4))
            role[19] = min(99, max(0, role[19]))
        else:
            value, state = legacy_bounded(state, 10)
            outputs.append(value)
            damage_base = item_hp + 50 - trunc_div(role[19], 2) - value
            if damage_base > 0:
                value, state = legacy_bounded(state, 5)
                outputs.append(value)
                damage_base = -5 - value
            hp_delta = trunc_div(damage_base - 3 * hidden_weapon, 3)
            role[19] = wrapping_i16(role[19] - trunc_div(hp_delta, 10))
            role[19] = min(99, max(0, role[19]))
        role[17] = wrapping_i16(role[17] + hp_delta)
        role[17] = min(role[18], max(0, role[17]))
        deltas[0] = wrapping_i16(role[17] - old_hp)

    if item_word(46):
        role[18] = wrapping_i16(role[18] + item_word(46))
        role[18] = min(999, max(0, role[18]))
        if role[17] >= role[18]:
            role[17] = role[18]
        deltas[1] = item_word(46)

    item_poison = item_word(47)
    if item_poison:
        old_poison = role[20]
        if item_poison > 0:
            poison_delta = trunc_div(hidden_weapon + item_poison, 2) - role[49]
            if role[49] >= 100 or poison_delta < 0:
                poison_delta = 0
            poison_delta = trunc_div(poison_delta, 2)
        else:
            first, state = legacy_bounded(state, 5)
            second, state = legacy_bounded(state, 5)
            outputs.extend((first, second))
            poison_delta = trunc_div(item_poison, 2) + first - second
        role[20] = wrapping_i16(role[20] + poison_delta)
        role[20] = min(99, max(0, role[20]))
        deltas[2] = wrapping_i16(role[20] - old_poison)

    if item_word(48):
        old_value = role[21]
        role[21] = wrapping_i16(role[21] + item_word(48))
        role[21] = min(100, max(0, role[21]))
        deltas[3] = wrapping_i16(role[21] - old_value)

    if item_word(49) == 2:
        role[40] = 2
        deltas[4] = 2

    if item_word(50):
        old_value = role[41]
        role[41] = wrapping_i16(role[41] + item_word(50))
        role[41] = min(role[42], max(0, role[41]))
        deltas[5] = wrapping_i16(role[41] - old_value)

    if item_word(51):
        role[42] = wrapping_i16(role[42] + item_word(51))
        role[42] = min(999, max(0, role[42]))
        if role[41] >= role[42]:
            role[41] = role[42]
        deltas[6] = item_word(51)

    for index in range(13):
        delta = item_word(52 + index)
        role[43 + index] = wrapping_i16(role[43 + index] + delta)
        deltas[7 + index] = delta
    deltas[20] = item_word(65)
    deltas[21] = item_word(66)
    role[57] = wrapping_i16(role[57] + item_word(67))
    deltas[22] = item_word(67)

    selected_names = {
        17: "hp",
        18: "maximum_hp",
        19: "hurt",
        20: "poison",
        21: "physical_power",
        40: "mp_type",
        41: "mp",
        42: "maximum_mp",
        43: "attack",
        44: "speed",
        45: "defence",
        49: "anti_poison",
        54: "hidden_weapon",
        56: "morality",
        57: "attack_with_poison",
        58: "attack_twice",
    }
    return {
        "item_id": item_word(0),
        "item_type": item_word(41),
        "rng_seed": seed,
        "rng_outputs": outputs,
        "rng_state_after": state,
        "deltas": deltas,
        "effect_count": sum(value != 0 for value in deltas),
        "role_before": {name: before[index] for index, name in selected_names.items()},
        "role_after": {name: role[index] for index, name in selected_names.items()},
    }


def ai_request_vectors() -> dict[str, object]:
    return {
        "entry_aliases": ["request_medicine", "request_detox"],
        "entry_owners": {
            "request_medicine": {"address": "0x361ac", "action": 8},
            "request_detox": {"address": "0x36209", "action": 9},
        },
        "target_slot": 1,
        "target": [13, 23],
        "positive_round_value": {
            "round_value": 3,
            "next_step": "move",
            "movement_mode": 0,
            "movement_value": 0,
        },
        "zero_round_value": {
            "round_value": 0,
            "next_step": "automatic_attack",
            "movement_called": False,
        },
        "negative_round_value": {
            "round_value": -1,
            "next_step": "automatic_attack",
            "movement_called": False,
        },
        "restore_request_target_before_attack": True,
        "target_slot_reread_after_move": True,
        "movement_return_ignored": True,
        "automatic_attack_after_move_or_skip": True,
        "automatic_attack_return_ignored_by_outer": True,
        "outer_marks_action_done_after_handler": True,
        "rng_consumed_before_automatic_attack": False,
    }


def ai_support_vectors() -> dict[str, object]:
    medicine = 30
    detoxification = 45
    medicine_range = trunc_div(medicine, 15) + 1
    detox_range = trunc_div(detoxification, 15) + 1
    allied_total = wrapping_i16(30_000 + 30_000 + 10_000 + 10_000)
    allied_count = 2
    doubled_average = trunc_div(2 * allied_total, allied_count)
    return {
        "entry_owners": {
            "medicine": {"address": "0x36210", "action": 5},
            "detox": {"address": "0x363ac", "action": 4},
        },
        "medicine": {
            "ability": medicine,
            "targeting_range": medicine_range,
            "direct_distance": medicine_range,
            "direct_next_step": "apply_support",
        },
        "detox": {
            "ability": detoxification,
            "targeting_range": detox_range,
            "direct_distance": detox_range,
            "direct_next_step": "apply_support",
        },
        "out_of_range_positive_round": {
            "round_value": 3,
            "range_checks_before_move": 1,
            "movement_mode": 1,
            "movement_value_source": "targeting_range",
            "next_step": "move",
        },
        "out_of_range_zero_round": {
            "round_value": 0,
            "range_checks": 2,
            "movement_called": False,
        },
        "out_of_range_negative_round": {
            "round_value": -1,
            "range_checks": 2,
            "movement_called": False,
        },
        "signed_medicine_range": {
            "minus_30": trunc_div(-30, 15) + 1,
            "minus_14": trunc_div(-14, 15) + 1,
            "plus_30": medicine_range,
        },
        "signed_detoxification_range": {
            "minus_30": trunc_div(-30, 15) + 1,
            "minus_14": trunc_div(-14, 15) + 1,
            "plus_45": detox_range,
        },
        "fallback": {
            "allied_total_wrapped": allied_total,
            "allied_count": allied_count,
            "doubled_allied_average": doubled_average,
            "actor_attack_30000_doubled": 60_000,
            "actor_attack_30000_next_step": "automatic_attack",
            "actor_attack_7232_next_step": "rest",
            "strict_greater_for_attack": True,
            "allied_total_and_count_frozen_at_ai_entry": True,
            "live_actor_attack_reread_after_move": True,
            "frozen_mutation_vector": {
                "entry_allied_total": 300,
                "entry_allied_count": 2,
                "post_entry_ally_attack": 1_000,
                "post_entry_ally_hp": 1_000,
                "frozen_doubled_average": 300,
                "recomputed_doubled_average_would_be": 2_300,
                "live_actor_doubled_attack": 600,
                "next_step": "automatic_attack",
            },
        },
        "restore_same_target_after_move": True,
        "target_coordinates_reread_after_move": True,
        "medicine_discarded_absolute_value_calls": 2,
        "detox_discarded_absolute_value_calls": 0,
        "detox_all_exits_use_shared_epilogue": "0x39a3e",
        "rng_consumed_before_support_or_fallback": False,
        "outer_marks_action_done_after_handler": True,
    }


def ai_movement_plan_vector(
    field_words: list[int],
    occupied: set[int],
    *,
    source: tuple[int, int],
    target: tuple[int, int],
    round_value: int,
    mode: int,
    range_value: int,
) -> dict[str, object]:
    targeting = build_path_map(field_words, target, "targeting")
    target_distance = targeting[source[1] * 64 + source[0]]
    within_turn_range = target_distance - round_value <= range_value
    destination = target
    selected_layer = -1
    movement_builds = 0
    selection = "generic_reachable_neighbor"
    values = targeting

    if mode == 2 and within_turn_range or mode == 3:
        selection = "aligned_range_layer" if mode == 2 else "range_layer"
        values = build_path_map(field_words, target, "movement", occupied)
        movement_builds += 1
        layer = range_value
        while True:
            found = False
            best_distance = 1000
            best = (0, 0)
            for x in range(64):
                for y in range(64):
                    if values[y * 64 + x] != layer:
                        continue
                    if mode == 2 and x != target[0] and y != target[1]:
                        continue
                    found = True
                    distance = abs(x - source[0]) + abs(y - source[1])
                    if distance < best_distance:
                        best = (x, y)
                        best_distance = distance
            if found:
                destination = best
                selected_layer = layer
                break
            layer = wrapping_i16(layer - 1)
            if layer == 0:
                break
    else:
        values = build_path_map(field_words, source, "movement", occupied)
        movement_builds += 1
        cursor = target
        for _ in range(4096):
            found = False
            for dx, dy in PATH_DIRECTIONS:
                candidate = (cursor[0] + dx, cursor[1] + dy)
                values = build_path_map(field_words, source, "movement", occupied)
                movement_builds += 1
                index = legacy_path_index(*candidate)
                value = values[index] if index is not None else 555
                if value < 128 and (candidate[0] == source[0] or candidate[1] == source[1]):
                    cursor = candidate
                    found = True
                    break
            if not found:
                for dx, dy in PATH_DIRECTIONS:
                    candidate = (cursor[0] + dx, cursor[1] + dy)
                    values = build_path_map(field_words, source, "movement", occupied)
                    movement_builds += 1
                    index = legacy_path_index(*candidate)
                    value = values[index] if index is not None else 555
                    if value < 128:
                        cursor = candidate
                        found = True
                        break
            if found:
                break
            x, y = cursor
            if x > source[0] and x > 0:
                x -= 1
            elif x < source[0] and x < 63:
                x += 1
            elif y > source[1] and y > 0:
                y -= 1
            elif y < source[1] and y < 63:
                y += 1
            cursor = (x, y)
            if cursor == source:
                break
        destination = cursor

    first_index = legacy_path_index(*destination)
    first_reachable = destination != source and first_index is not None and values[first_index] < 128
    second_reachable = False
    marked = False
    first_step = None
    if first_reachable:
        values = build_path_map(field_words, source, "movement", occupied)
        movement_builds += 1
        second_reachable = values[destination[1] * 64 + destination[0]] < 128
        if second_reachable:
            marked = mark_path(values, source, destination)
            if marked:
                first_step = next(
                    (
                        [source[0] + dx, source[1] + dy]
                        for dx, dy in PATH_DIRECTIONS
                        if 0 <= source[0] + dx < 64
                        and 0 <= source[1] + dy < 64
                        and values[(source[1] + dy) * 64 + source[0] + dx] == 250
                    ),
                    None,
                )
    return {
        "mode": mode,
        "range": range_value,
        "round_value": round_value,
        "selection": selection,
        "source": list(source),
        "target": list(target),
        "target_distance": target_distance,
        "within_turn_range": within_turn_range,
        "destination": list(destination),
        "selected_distance_layer": selected_layer,
        "movement_map_build_count": movement_builds,
        "first_reachability_passed": first_reachable,
        "second_reachability_passed": second_reachable,
        "path_marked": marked,
        "marked_path_hash": fnv1a_words(values),
        "first_step": first_step,
    }


def ai_movement_vectors(
    field_words: list[int], occupied: set[int]
) -> dict[str, object]:
    source = (26, 24)
    target = (26, 26)
    fully_occupied = set(range(4096))
    return {
        "battle_id": 4,
        "battlefield_id": 2,
        "direction_order": ["up", "right", "left", "down"],
        "mode_0": ai_movement_plan_vector(
            field_words, occupied, source=source, target=target,
            round_value=1, mode=0, range_value=0,
        ),
        "mode_1": ai_movement_plan_vector(
            field_words, occupied, source=source, target=target,
            round_value=8, mode=1, range_value=1,
        ),
        "mode_2": ai_movement_plan_vector(
            field_words, occupied, source=source, target=target,
            round_value=20, mode=2, range_value=3,
        ),
        "mode_3": ai_movement_plan_vector(
            field_words, occupied, source=source, target=target,
            round_value=20, mode=3, range_value=3,
        ),
        "mode_2_outside_turn": ai_movement_plan_vector(
            field_words, occupied, source=source, target=target,
            round_value=1, mode=2, range_value=0,
        ),
        "generic_second_pass": ai_movement_plan_vector(
            field_words, occupied, source=source, target=(28, 26),
            round_value=8, mode=0, range_value=0,
        ),
        "generic_axis_retreat_to_source": ai_movement_plan_vector(
            field_words, fully_occupied, source=source, target=target,
            round_value=8, mode=0, range_value=0,
        ),
        "mode_3_no_layer_second_reachability_failure": ai_movement_plan_vector(
            field_words, fully_occupied, source=source, target=target,
            round_value=20, mode=3, range_value=2,
        ),
        "first_step_state": {
            "source_path_after_step": 255,
            "old_occupancy": -1,
            "new_occupancy": 0,
            "round_value": [8, 7],
            "speed_div_10": 8,
            "physical_power": [10, 9],
            "render_required": True,
            "present_required": True,
            "wait_ticks": 40,
        },
        "mode_0_round_exhaustion_stops_after_first_step": True,
    }


def player_cursor_vectors(
    field_words: list[int], occupied: set[int]
) -> dict[str, object]:
    source = (26, 24)
    destination = (26, 25)
    occupied_target = (26, 26)
    movement = build_path_map(field_words, source, "movement", occupied)
    targeting = build_path_map(field_words, source, "targeting", occupied)
    movement_target_value = movement[occupied_target[1] * 64 + occupied_target[0]]
    targeting_target_value = targeting[occupied_target[1] * 64 + occupied_target[0]]
    marked = movement.copy()
    path_marked = mark_path(marked, source, destination)
    return {
        "battle_id": 4,
        "battlefield_id": 2,
        "source": list(source),
        "path_limit": 2,
        "input_priority": ["down", "right", "left", "up", "cancel", "activate"],
        "presentation_gate": {
            "before_first_input": 2,
            "after_direction": 1,
            "after_rejected_confirmation": 1,
        },
        "movement": {
            "source_activation_selects": False,
            "first_down": list(destination),
            "occupied_hover": list(occupied_target),
            "occupied_path_value": movement_target_value,
            "occupied_value_over_limit_but_occupancy_allows_hover":
                movement_target_value > 2 and occupied_target[1] * 64 + occupied_target[0] in occupied,
            "occupied_activation_selects": False,
            "selected": list(destination),
            "path_marked": path_marked,
            "first_step": list(destination),
            "round_value": [2, 1],
            "speed_div_10": 2,
            "physical_power": [10, 9],
            "render_required": True,
            "present_required": True,
            "wait_ticks": 40,
        },
        "targeting": {
            "occupied_path_value": targeting_target_value,
            "path_limit": targeting_target_value,
            "occupied_activation_selects": targeting_target_value >= 0,
        },
        "cancel": {
            "path_limit_after": 0,
            "cancel_word_after": 1,
            "wrapper_return": -1,
        },
    }


def post_battle_progression_vectors(total_experience: int) -> dict[str, object]:
    thresholds = [
        0, 50, 150, 300, 500, 750, 1050, 1400, 1800, 2250,
        2750, 3850, 5050, 6350, 7750, 9250, 10850, 12550, 14350, 16750,
        18250, 21400, 24700, 28150, 31750, 35500, 39400, 43450, 47650, 52000,
    ]
    old_level = 1
    experience = 150
    new_level = old_level
    for level in range(old_level, 30):
        if experience >= thresholds[level]:
            new_level = level + 1
    level_count = new_level - old_level
    state = 1
    growth_zero_based, state = legacy_bounded(state, 6)
    hp_random, state = legacy_bounded(state, 3)
    skill_words = {
        "medicine": 21,
        "use_poison": 20,
        "detoxification": 22,
        "fist": 23,
        "sword": 24,
        "knife": 25,
    }
    skill_after: dict[str, int] = {}
    for name, value in skill_words.items():
        if value > 20:
            addition, state = legacy_bounded(state, 3)
            value = min(100, wrapping_i16(value + addition))
        skill_after[name] = value
    hidden_addition, state = legacy_bounded(state, 3)
    growth_roll = growth_zero_based + 1

    craft_state = 1
    craft_picks: list[int] = []
    while True:
        choice, craft_state = legacy_bounded(craft_state, 5)
        craft_picks.append(choice)
        if choice == 0:
            break
    product_zero_based, craft_state = legacy_bounded(craft_state, 3)
    return {
        "level_thresholds": thresholds,
        "level_up": {
            "seed": 1,
            "old_level": old_level,
            "experience": experience,
            "new_level": new_level,
            "levels_gained": level_count,
            "growth_roll": growth_roll,
            "maximum_hp": 100 + (hp_random + 2) * 3 * level_count,
            "maximum_mp": 80 + (9 - growth_roll) * 4 * level_count,
            "primary_stats": [36, 36, 36],
            "skills": skill_after,
            "hidden_weapon": min(100, 26 + hidden_addition),
            "rng_state_after": state,
        },
        "practice": {
            "iq": 60,
            "factor": 3,
            "need_experience": 10,
            "existing_magic_level": 199,
            "magic_rank": 1,
            "required_experience": 60,
            "maximum_hp": [100, 110],
            "maximum_mp": [80, 100],
            "attack": [30, 100],
            "morality": [50, 0],
            "attack_twice": [0, 1],
            "attack_with_poison": [0, 5],
            "magic_level": [199, 299],
            "item_experience_after": 0,
        },
        "craft": {
            "seed": 1,
            "iq": 60,
            "factor": 3,
            "required_experience": 30,
            "eligible_recipes": [0],
            "selection_rng": craft_picks,
            "selected_recipe": 0,
            "product_count_added": product_zero_based + 1,
            "material_count": [3, 1],
            "product_count": [4, 4 + product_zero_based + 1],
            "rng_state_after": craft_state,
        },
        "settlement": {
            "battle_id": 2,
            "total_experience": total_experience,
            "living_party_count": 1,
            "shared_experience": total_experience,
            "living_reward": [5, wrapping_i16(5 + total_experience)],
            "dead_reward": 7,
            "dead_hp_floor_divisor": 5,
            "dead_physical_power_floor": 10,
            "enemy_restore": {
                "hp_to_maximum": True,
                "mp_to_maximum": True,
                "hurt": 0,
                "poison": 0,
                "physical_power": 100,
            },
        },
        "round_status_damage": {
            "condition": "hurt>0 || (poison>0 && hp>0 && physical_power>0 && hidden==0)",
            "hurt_divisor": 20,
            "poison_divisor": 10,
            "negative_hp_floor": 1,
            "negative_physical_power_floor": 1,
            "zero_hp_is_not_floored": True,
            "hurt_priority_vector": {
                "hp": [0, 1],
                "hurt": 20,
                "poison": 0,
                "physical_power": [-1, 1],
                "hidden": 1,
            },
            "poison_vector": {
                "hp": [100, 98],
                "hurt": 0,
                "poison": 20,
                "physical_power": 100,
                "hidden": 0,
            },
        },
        "ai_target_cleanup": {
            "fields": ["ai_target", "ai_poison_target"],
            "clear_only_when_hidden_equals": 1,
            "hidden_one": [1, -1],
            "hidden_two": [1, 1],
            "inactive_slot_within_26": [2, -1],
            "negative_target_preserved": [-1, -1],
            "true_out_of_bounds_preserved": [26, 26],
        },
        "status_panel": {
            "party_offset": 0,
            "enemy_offset": 220,
            "panel_origin_party": [220, 19],
            "panel_origin_enemy": [0, 19],
            "portrait_origin_party": [242, 82],
            "portrait_origin_enemy": [22, 82],
            "name_null_byte_2_x": 262,
            "hurt_colors": {"0_to_33": 1797, "34_to_66": 3600, "67_plus": 5142},
            "poison_colors": {"zero": 8993, "1_to_49": 12338, "50_plus": 13623},
            "mp_type_colors": {"0": 20558, "1": 1797, "2": 26211},
            "invalid_mp_type_reuses_poison_color": True,
        },
    }


def rest_vector(
    *,
    seed: int,
    speed: int,
    round_value: int,
    physical_power: int,
    hp: int,
    maximum_hp: int,
    mp: int,
    maximum_mp: int,
) -> dict[str, object]:
    first, state = legacy_bounded(seed, 3)
    physical_power = wrapping_i16(
        physical_power + first + (3 if round_value == trunc_div(speed, 10) else 2)
    )
    if physical_power > 100:
        physical_power = 100
    outputs = [first]
    if physical_power >= 30:
        bound = trunc_div(physical_power, 10) - 2
        second, state = legacy_bounded(state, bound)
        third, state = legacy_bounded(state, bound)
        outputs.extend([second, third])
        hp = min(maximum_hp, wrapping_i16(hp + second + 3))
        mp = min(maximum_mp, wrapping_i16(mp + third + 3))
    return {
        "rng_seed": seed,
        "rng_outputs": outputs,
        "rng_state_after": state,
        "speed": speed,
        "round_value": round_value,
        "physical_power_after": physical_power,
        "hp_after": hp,
        "mp_after": mp,
        "action_done": 1,
    }


def ai_entry_vectors() -> dict[str, object]:
    def consume(seed: int, bounds: list[int]) -> tuple[list[int], int]:
        outputs: list[int] = []
        state = seed
        for bound in bounds:
            value, state = legacy_bounded(state, bound)
            outputs.append(value)
        return outputs, state

    poisoned_outputs, poisoned_state = consume(1, [10])
    low_mp_outputs, low_mp_state = consume(1, [10, 10, 10])
    medicine_outputs, medicine_state = consume(1, [10, 10, 10])
    detox_outputs, detox_state = consume(1, [10, 10, 10])
    cleared_wait_outputs, cleared_wait_state = consume(1, [10, 10, 50])
    attack_outputs, attack_state = consume(1, [10, 10, 50])
    escape_outputs, escape_state = consume(10, [10, 10])
    frozen_outputs, frozen_state = consume(1, [10, 10, 10, 50])
    return {
        "prelude": {
            "allied_total": 330,
            "opponent_total": 220,
            "allied_count": 3,
            "opponent_count": 2,
            "order": ["render", "present", "wait"],
            "wait_ticks": 300,
        },
        "priority": [
            "low_hp",
            "poisoned",
            "low_mp",
            "medicine_target",
            "detox_target",
            "escape",
            "offensive",
        ],
        "wait": {"physical_power": 9, "rng_consumed": False, "action": 7},
        "wait_cleared_by_low_hp_no_choice": {
            "physical_power": 9,
            "hp": 10,
            "rng_bounds_after_low_hp": [10, 10, 50],
            "rng_outputs": cleared_wait_outputs,
            "rng_state_after": cleared_wait_state,
            "action": 0,
        },
        "poisoned": {
            "poison": 100,
            "rng_bounds": [10],
            "rng_outputs": poisoned_outputs,
            "rng_state_after": poisoned_state,
            "action": 4,
        },
        "low_mp": {
            "mp": 0,
            "maximum_mp": 100,
            "rng_bounds": [10, 10, 10],
            "rng_outputs": low_mp_outputs,
            "rng_state_after": low_mp_state,
            "action": 6,
        },
        "medicine": {
            "medicine": 80,
            "requested_target": 1,
            "rng_bounds": [10, 10, 10],
            "rng_outputs": medicine_outputs,
            "rng_state_after": medicine_state,
            "action": 5,
        },
        "detox": {
            "detoxification": 80,
            "requested_target": 1,
            "rng_bounds": [10, 10, 10],
            "rng_outputs": detox_outputs,
            "rng_state_after": detox_state,
            "action": 4,
        },
        "escape": {
            "seed": 10,
            "hp": 19,
            "rng_bounds": [10, 10],
            "rng_outputs": escape_outputs,
            "rng_state_after": escape_state,
            "action": 11,
        },
        "attack": {
            "mp": 5,
            "maximum_mp": 5,
            "minimum_need_mp": 5,
            "rng_bounds": [10, 10, 50],
            "rng_outputs": attack_outputs,
            "rng_state_after": attack_state,
            "writes_action_code": False,
            "action": 2,
        },
        "frozen_prelude": {
            "totals_before_present": [330, 220, 3, 2],
            "mutated_totals_during_wait": [1530, 620, 3, 2],
            "rng_bounds": [10, 10, 10, 50],
            "rng_outputs": frozen_outputs,
            "rng_state_after": frozen_state,
            "action_with_frozen_totals": 2,
            "action_if_totals_were_recomputed": 5,
        },
        "handler_by_action": {
            "0": "rest",
            "1": "move",
            "2": "attack",
            "3": "use_poison",
            "4": "detox",
            "5": "medicine",
            "6": "item",
            "7": "rest",
            "8": "request_medicine",
            "9": "request_detox",
            "10": "throwing_weapon",
            "11": "escape",
        },
        "action_done_written_after_handler": True,
    }


def ai_selector_vectors() -> dict[str, object]:
    medicine_outputs: list[int] = []
    medicine_states: list[int] = []
    state = 1
    for _ in range(3):
        value, state = legacy_bounded(state, 10)
        medicine_outputs.append(value)
        medicine_states.append(state)
    medicine_half_accept, medicine_half_accept_state = legacy_bounded(5, 10)
    medicine_fifth_outputs: list[int] = []
    medicine_fifth_state = 331
    for _ in range(3):
        value, medicine_fifth_state = legacy_bounded(medicine_fifth_state, 10)
        medicine_fifth_outputs.append(value)

    detox_outputs: list[int] = []
    detox_states: list[int] = []
    detox_state = 1
    for _ in range(3):
        value, detox_state = legacy_bounded(detox_state, 10)
        detox_outputs.append(value)
        detox_states.append(detox_state)
    detox_first_accept, detox_first_accept_state = legacy_bounded(9, 10)
    detox_second_outputs: list[int] = []
    detox_second_state = 6
    for _ in range(2):
        value, detox_second_state = legacy_bounded(detox_second_state, 10)
        detox_second_outputs.append(value)
    detox_fallback_outputs: list[int] = []
    detox_fallback_state = 331
    for _ in range(3):
        value, detox_fallback_state = legacy_bounded(detox_fallback_state, 10)
        detox_fallback_outputs.append(value)

    poison_gate, poison_state = legacy_bounded(1, 50)
    poison_roll, poison_state = legacy_bounded(poison_state, 150)
    throwing_gate, throwing_state = legacy_bounded(1, 50)
    throwing_roll, throwing_state = legacy_bounded(throwing_state, 100)
    party_roll_boundary_outputs: list[int] = []
    party_roll_boundary_state = 51
    for bound in (50, 100):
        value, party_roll_boundary_state = legacy_bounded(party_roll_boundary_state, bound)
        party_roll_boundary_outputs.append(value)
    party_poison_fallback_outputs: list[int] = []
    party_poison_fallback_state = 6
    for bound in (50, 100, 10):
        value, party_poison_fallback_state = legacy_bounded(party_poison_fallback_state, bound)
        party_poison_fallback_outputs.append(value)
    carried_throwing_outputs: list[int] = []
    carried_throwing_state = 6
    for bound in (50, 10):
        value, carried_throwing_state = legacy_bounded(carried_throwing_state, bound)
        carried_throwing_outputs.append(value)
    carried_poison_outputs: list[int] = []
    carried_poison_state = 14
    for bound in (50, 10):
        value, carried_poison_state = legacy_bounded(carried_poison_state, bound)
        carried_poison_outputs.append(value)
    attack_gate, attack_state = legacy_bounded(1, 50)
    return {
        "action_codes": {
            "none": 0,
            "move": 1,
            "attack": 2,
            "use_poison": 3,
            "detox": 4,
            "medicine": 5,
            "item": 6,
            "wait": 7,
            "request_medicine": 8,
            "request_detox": 9,
            "throwing_weapon": 10,
            "escape": 11,
        },
        "low_hp": {
            "self": {"medicine": 21, "hurt": 50, "physical_power": 50, "action": 5},
            "self_minimum": {
                "medicine": 20,
                "hurt": 49,
                "physical_power": 50,
                "action": 5,
            },
            "self_strict_reject": {
                "medicine": 20,
                "hurt": 50,
                "physical_power": 50,
                "action": 0,
                "writes_action_code": False,
            },
            "self_power_reject": {
                "medicine": 100,
                "hurt": 50,
                "physical_power": 49,
                "action": 0,
            },
            "inventory": {
                "add_hp": 1,
                "inventory_slot": 2,
                "quantity": 0,
                "action": 6,
            },
            "enemy_carried": {
                "add_hp": 1,
                "carried_slot": 2,
                "quantity": 0,
                "action": 6,
            },
            "request": {"hurt": 80, "ally_medicine": 51, "target_slot": 1, "action": 8},
            "hidden_request_skip": {
                "hurt": 80,
                "hidden_slot": 1,
                "ally_medicine": 51,
                "target_slot": 2,
                "action": 8,
            },
        },
        "poisoned": {
            "self": {"detoxification": 22, "poison": 51, "physical_power": 51, "action": 4},
            "self_minimum": {
                "detoxification": 21,
                "poison": 50,
                "physical_power": 51,
                "action": 4,
            },
            "self_skill_reject": {
                "detoxification": 20,
                "poison": 49,
                "physical_power": 51,
                "action": 0,
                "writes_action_code": False,
            },
            "self_relative_reject": {
                "detoxification": 21,
                "poison": 51,
                "physical_power": 51,
                "action": 0,
            },
            "self_power_reject": {
                "detoxification": 100,
                "poison": 50,
                "physical_power": 50,
                "action": 0,
            },
            "party_item_property_word": 56,
            "party_ignores_negative_add_poison_word": 47,
            "enemy_carried_item_property_word": 47,
            "party_item": {"inventory_slot": 1, "quantity": 0, "action": 6},
            "enemy_carried": {"side": -1, "carried_slot": 2, "quantity": 0, "action": 6},
            "party_item_slot": 1,
            "enemy_carried_slot": 2,
            "item_action": 6,
            "request": {
                "poison": 80,
                "ally_detoxification": 51,
                "target_slot": 1,
                "action": 9,
            },
            "hidden_request_skip": {
                "poison": 80,
                "hidden_slot": 1,
                "ally_detoxification": 51,
                "target_slot": 2,
                "action": 9,
            },
            "different_side_skip": {
                "poison": 80,
                "different_side_slot": 1,
                "ally_detoxification": 51,
                "target_slot": 2,
                "action": 9,
            },
            "request_skill_reject": {
                "poison": 49,
                "ally_detoxification": 20,
                "action": 0,
                "writes_action_code": False,
            },
            "request_relative_reject": {
                "poison": 51,
                "ally_detoxification": 21,
                "action": 0,
            },
        },
        "low_mp": {
            "add_mp_word": 50,
            "strict_zero_reject": {
                "add_mp": 0,
                "action": 0,
                "writes_action_code": False,
            },
            "inventory": {
                "current_mp": 100,
                "maximum_mp": 100,
                "negative_add_mp_slot": 1,
                "negative_add_mp": -1,
                "inventory_slot": 3,
                "quantity": 0,
                "add_mp": 1,
                "action": 6,
            },
            "inventory_first_match": {
                "positive_slots": [1, 3],
                "selected_slot": 1,
                "action": 6,
            },
            "enemy_carried": {
                "side": -1,
                "carried_slot": 2,
                "quantity": 0,
                "add_mp": 1,
                "action": 6,
            },
            "inventory_slot": 3,
            "action": 6,
        },
        "medicine_target": {
            "filter_skip": {
                "actor_hp": 1,
                "hidden_slot": 1,
                "different_side_slot": 2,
                "action": 0,
                "rng_state_after": 1,
                "writes_action_code": False,
            },
            "capability_strict_reject": {
                "medicine": 20,
                "target_hurt": 50,
                "target_hp": 1,
                "action": 0,
                "rng_state_after": 1,
                "writes_action_code": False,
            },
            "request_priority": {
                "request_action": 8,
                "target_slot": 1,
                "action": 5,
                "rng_state_after": 1,
            },
            "hp_direct": {"hp": 19, "action": 5, "rng_state_after": 1},
            "direct_boundaries": {
                "medicine": 11,
                "hp": 20,
                "maximum_hp": 20,
                "hurt": 40,
                "action": 0,
                "rng_state_after": 1,
            },
            "hurt_direct": {
                "medicine": 12,
                "hurt": 41,
                "action": 5,
                "rng_state_after": 1,
            },
            "half_accept": {
                "seed": 5,
                "hp": 49,
                "maximum_hp": 100,
                "rng_output": medicine_half_accept,
                "rng_state_after": medicine_half_accept_state,
                "action": 5,
            },
            "half_reject": {
                "seed": 1,
                "hp": 49,
                "maximum_hp": 100,
                "rng_outputs": medicine_outputs[:1],
                "rng_state_after": medicine_states[0],
                "action": 0,
            },
            "third_reject": {
                "seed": 1,
                "hp": 32,
                "maximum_hp": 100,
                "rng_outputs": medicine_outputs[:2],
                "rng_state_after": medicine_states[1],
                "action": 0,
            },
            "hp": 24,
            "maximum_hp": 100,
            "rng_bounds": [10, 10, 10],
            "rng_outputs": medicine_outputs,
            "rng_state_after": state,
            "target_slot": 1,
            "action": 5,
            "fifth_fallback": {
                "seed": 331,
                "hp": 20,
                "maximum_hp": 105,
                "rng_outputs": medicine_fifth_outputs,
                "rng_state_after": medicine_fifth_state,
                "target_slot": 1,
                "action": 5,
            },
        },
        "detox_target": {
            "filter_skip": {
                "actor_poison": 50,
                "hidden_slot": 1,
                "different_side_slot": 2,
                "action": 0,
                "rng_state_after": 1,
                "writes_action_code": False,
            },
            "capability_strict_reject": {
                "detoxification": 20,
                "target_poison": 50,
                "action": 0,
                "rng_state_after": 1,
                "writes_action_code": False,
            },
            "request_priority": {
                "request_action": 9,
                "target_slot": 1,
                "action": 4,
                "rng_state_after": 1,
            },
            "first_boundary": {
                "poison": 10,
                "action": 0,
                "rng_state_after": 1,
            },
            "first_accept": {
                "seed": 9,
                "poison": 11,
                "rng_output": detox_first_accept,
                "rng_state_after": detox_first_accept_state,
                "action": 4,
            },
            "second_boundary": {
                "seed": 1,
                "poison": 20,
                "rng_outputs": detox_outputs[:1],
                "rng_state_after": detox_states[0],
                "action": 0,
            },
            "second_accept": {
                "seed": 6,
                "poison": 21,
                "rng_outputs": detox_second_outputs,
                "rng_state_after": detox_second_state,
                "action": 4,
            },
            "third_boundary": {
                "seed": 1,
                "detoxification": 1,
                "poison": 30,
                "rng_outputs": detox_outputs[:2],
                "rng_state_after": detox_states[1],
                "action": 0,
            },
            "poison": 35,
            "rng_bounds": [10, 10, 10],
            "rng_outputs": detox_outputs,
            "rng_state_after": detox_state,
            "target_slot": 1,
            "action": 4,
            "fallback_boundary": {
                "seed": 331,
                "poison": 40,
                "rng_outputs": detox_fallback_outputs,
                "rng_state_after": detox_fallback_state,
                "action": 0,
            },
            "fallback": {
                "seed": 331,
                "poison": 41,
                "rng_outputs": detox_fallback_outputs,
                "rng_state_after": detox_fallback_state,
                "target_slot": 1,
                "action": 4,
            },
        },
        "offensive": {
            "aid": {
                "allied_total": 1002,
                "opponent_total": 400,
                "opponent_count": 2,
                "actor_hp_plus_attack": 2,
                "missing_hp": [100, 300],
                "target_slot": 2,
                "action": 5,
                "rng_consumed": False,
                "missing_wrap": {
                    "allied_total": wrapping_i16(2 - 1 + 1000),
                    "opponent_total": 400,
                    "opponent_count": 2,
                    "actor_hp_plus_attack": 2,
                    "candidate_missing_hp_i32": 32_767 - (-1),
                    "stored_best_i16": wrapping_i16(32_767 - (-1)),
                    "hidden_counted_slot": 2,
                    "target_slot": 1,
                    "action": 5,
                    "rng_consumed": False,
                    "next_candidate_replaces": {
                        "allied_total": wrapping_i16(2 - 1 + 999),
                        "first_missing_hp_i32": 32_767 - (-1),
                        "first_stored_best_i16": wrapping_i16(32_767 - (-1)),
                        "second_missing_hp_i32": 1_000 - 999,
                        "target_slot": 2,
                        "action": 5,
                        "rng_consumed": False,
                    },
                },
                "detox_tie": {
                    "medicine": 19,
                    "detoxification": 20,
                    "physical_power": 50,
                    "poison_values": [40, 40],
                    "target_slot": 1,
                    "action": 4,
                    "rng_consumed": False,
                },
                "medicine_precedence": {
                    "medicine": 20,
                    "detoxification": 100,
                    "full_hp": True,
                    "poison_values": [100, 100],
                    "rng_outputs_after_no_aid": [attack_gate],
                    "rng_state_after": attack_state,
                    "action": 0,
                },
                "actor_power_boundary": {
                    "allied_total": 902,
                    "opponent_total": 400,
                    "opponent_count": 2,
                    "opponent_average_half": trunc_div(trunc_div(400, 2), 2),
                    "actor_hp_plus_attack": 100,
                    "rng_outputs_after_no_aid": [attack_gate],
                    "rng_state_after": attack_state,
                    "action": 0,
                },
                "allied_total_boundary": {
                    "allied_total": 400,
                    "opponent_total": 200,
                    "opponent_count": 2,
                    "actor_hp_plus_attack": 2,
                    "doubled_opponent_total": 2 * 200,
                    "rng_outputs_after_no_aid": [attack_gate],
                    "rng_state_after": attack_state,
                    "action": 0,
                },
            },
            "poison": {
                "use_poison": 100,
                "attack": 10,
                "rng_bounds": [50, 150],
                "rng_outputs": [poison_gate, poison_roll],
                "rng_state_after": poison_state,
                "action": 3,
                "advantage_boundary": {
                    "use_poison": 48,
                    "attack": 10,
                    "advantage": 38,
                    "rng_outputs": [poison_gate],
                    "rng_state_after": attack_state,
                    "action": 0,
                },
                "roll_boundary": {
                    "use_poison": 58,
                    "attack": 0,
                    "rng_outputs": [poison_gate, poison_roll],
                    "rng_state_after": poison_state,
                    "action": 0,
                },
            },
            "party_throwing": {
                "add_hp": -100,
                "attack": 10,
                "hidden_weapon": 100,
                "rng_bounds": [50, 100],
                "rng_outputs": [throwing_gate, throwing_roll],
                "rng_state_after": throwing_state,
                "inventory_slot": 4,
                "quantity": 0,
                "action": 10,
                "threshold_boundary": {
                    "add_hp": -15,
                    "attack": 10,
                    "threshold": trunc_div(3 * 10, 2),
                    "rng_outputs": [attack_gate],
                    "rng_state_after": attack_state,
                    "action": 0,
                },
                "roll_boundary": {
                    "seed": 51,
                    "add_hp": -100,
                    "hidden_weapon": 100,
                    "rng_outputs": party_roll_boundary_outputs,
                    "rng_state_after": party_roll_boundary_state,
                    "action": 0,
                },
                "poison_fallback": {
                    "seed": 6,
                    "add_hp": -100,
                    "add_poison": 100,
                    "rng_outputs": party_poison_fallback_outputs,
                    "rng_state_after": party_poison_fallback_state,
                    "inventory_slot": 0,
                    "action": 10,
                },
            },
            "carried_throwing": {
                "seed": 6,
                "side": -1,
                "item_id": 0,
                "add_hp": -11,
                "attack": 10,
                "quantity": 0,
                "rng_outputs": carried_throwing_outputs,
                "rng_state_after": carried_throwing_state,
                "carried_slot": 2,
                "action": 10,
                "threshold_boundary": {
                    "side": -1,
                    "item_id": 0,
                    "add_hp": -10,
                    "attack": 10,
                    "rng_outputs": [attack_gate],
                    "rng_state_after": attack_state,
                    "action": 0,
                },
                "poison": {
                    "seed": 14,
                    "side": 1,
                    "item_id": 0,
                    "add_poison": 11,
                    "attack": 10,
                    "quantity": 0,
                    "rng_outputs": carried_poison_outputs,
                    "rng_state_after": carried_poison_state,
                    "carried_slot": 1,
                    "action": 10,
                },
            },
            "attack": {
                "physical_power": 100,
                "current_mp": 5,
                "minimum_need_mp": 5,
                "rng_bounds": [50],
                "rng_outputs": [attack_gate],
                "rng_state_after": attack_state,
                "writes_action_code": False,
                "action": 2,
                "physical_power_boundary": {
                    "physical_power": 10,
                    "current_mp": 1000,
                    "action": 0,
                    "writes_action_code": False,
                },
                "no_magic_reject": {
                    "physical_power": 11,
                    "current_mp": 999,
                    "initial_minimum_need_mp": 1000,
                    "action": 0,
                    "writes_action_code": False,
                },
                "no_magic_accept": {
                    "physical_power": 11,
                    "current_mp": 1000,
                    "initial_minimum_need_mp": 1000,
                    "action": 2,
                    "writes_action_code": False,
                },
            },
        },
    }


def sentinel_entries(index_bytes: bytes, group_bytes: bytes) -> list[bytes]:
    offsets = struct.unpack(f"<{len(index_bytes) // 4}I", index_bytes)
    if not offsets or offsets[-1] != 0:
        raise ValueError("sentinel archive is missing trailing zero")
    entries: list[bytes] = []
    begin = 0
    for end in offsets[:-1]:
        if end < begin or end > len(group_bytes):
            raise ValueError("sentinel archive has invalid offset")
        entries.append(group_bytes[begin:end])
        begin = end
    entries.append(group_bytes[begin:])
    return entries


def draw_battle_sprite(
    pixels: bytearray,
    frame: bytes,
    anchor_x: int,
    anchor_y: int,
    mode: str = "normal",
    value: int = 0,
    palette: list[tuple[int, int, int]] | None = None,
    rgb4_lookup: list[int] | None = None,
) -> None:
    if len(frame) < 8:
        return
    width, height, x_offset, y_offset = struct.unpack_from("<HHhh", frame)
    cursor = 8
    left = anchor_x - x_offset
    top = anchor_y - y_offset
    for row in range(height):
        row_size = frame[cursor]
        cursor += 1
        row_end = cursor + row_size
        destination_x = left
        while cursor < row_end:
            skip = frame[cursor]
            count = frame[cursor + 1]
            cursor += 2
            destination_x += skip
            for source in frame[cursor:cursor + count]:
                destination_y = top + row
                if 0 <= destination_x < 320 and 0 <= destination_y < 200:
                    offset = destination_y * 320 + destination_x
                    if mode == "tint":
                        pixels[offset] = value
                    elif mode == "blend":
                        assert palette is not None and rgb4_lookup is not None
                        destination = pixels[offset]
                        source_rgb = palette[source]
                        destination_rgb = palette[destination]
                        components = tuple(
                            value * source_rgb[index] // 32
                            + (8 - value) * destination_rgb[index] // 32
                            for index in range(3)
                        )
                        pixels[offset] = rgb4_lookup[
                            components[0] * 256 + components[1] * 16 + components[2]
                        ]
                    else:
                        pixels[offset] = source
                destination_x += 1
            cursor += count
        if cursor != row_end or destination_x > left + width:
            raise ValueError("malformed battle RLE row")
    if cursor != len(frame):
        raise ValueError("battle RLE frame has trailing bytes")


def draw_battle_text(
    pixels: bytearray,
    x: int,
    y: int,
    text: bytes,
    ascii_font: bytes,
    big5_font: bytes,
    packed_colors: int,
) -> None:
    shadow = packed_colors & 0xFF
    foreground = packed_colors >> 8 & 0xFF
    cursor = 0
    while cursor < len(text):
        first = text[cursor]
        cursor += 1
        if first == 0:
            return
        if first > 0x7F:
            second = text[cursor]
            cursor += 1
            trail = second - 0x40 if 0x40 <= second <= 0x7E else second - 0x62
            glyph_index = (first - 0xA1) * 157 + trail
            glyph = big5_font[glyph_index * 32:(glyph_index + 1) * 32]
            glyph_width = 16
            stride = 2
        else:
            glyph_index = 32 if first == ord("_") else first
            glyph = ascii_font[glyph_index * 16:(glyph_index + 1) * 16]
            glyph_width = 8
            stride = 1
        for row in range(16):
            for byte_index in range(stride):
                bits = glyph[row * stride + byte_index]
                for bit in range(8):
                    if bits & (0x80 >> bit):
                        target_x = x + byte_index * 8 + bit
                        if 0 <= target_x < 319 and 0 <= y + row < 200:
                            offset = (y + row) * 320 + target_x
                            pixels[offset] = foreground
                            pixels[offset + 1] = shadow
        x += 4 if first == ord("_") else glyph_width


def battle_pixel_hashes(
    root: Path,
    battlefield_id: int,
    commands: list[list[int]],
) -> tuple[str, str, bytes]:
    battlefield = sentinel_entries(
        (root / f"WDX{battlefield_id:03d}").read_bytes(),
        (root / f"WMP{battlefield_id:03d}").read_bytes(),
    )
    cloud = cumulative_entries(
        (root / "CLOUD.IDX").read_bytes(), (root / "CLOUD.GRP").read_bytes()
    )
    portraits = cumulative_entries(
        (root / "HDGRP.IDX").read_bytes(), (root / "HDGRP.GRP").read_bytes()
    )
    palette_bytes = (root / "MMAP.COL").read_bytes()
    palette = [tuple(palette_bytes[index:index + 3]) for index in range(0, 768, 3)]
    rgb4_lookup: list[int] = []
    for red in range(16):
        for green in range(16):
            for blue in range(16):
                target = (red * 4 + 2, green * 4 + 2, blue * 4 + 2)
                rgb4_lookup.append(min(
                    range(256),
                    key=lambda index: sum(
                        (target[channel] - palette[index][channel]) ** 2
                        for channel in range(3)
                    ),
                ))
    ascii_font = (root / "FONT.X16").read_bytes()
    big5_font = (root / "FONT.C16").read_bytes()
    pixels = bytearray(320 * 200)
    for command in commands:
        kind, _, _, screen_x, screen_y, sprite_id, variant, style, value = command
        if kind in (0, 2):
            if sprite_id < 0 or sprite_id > 0x7FFE or sprite_id & 1:
                continue
            index = sprite_id // 2
            if index >= len(battlefield):
                continue
            draw_battle_sprite(
                pixels,
                battlefield[index],
                screen_x,
                screen_y,
                "tint" if kind == 2 else "normal",
                style & 0xFF,
            )
        elif kind == 1:
            draw_battle_sprite(
                pixels,
                cloud[4 + variant],
                screen_x,
                screen_y,
                "blend",
                style,
                palette,
                rgb4_lookup,
            )
        elif kind == 3:
            sign = b"-" if variant < 0 else b"+"
            draw_battle_text(
                pixels,
                screen_x,
                screen_y,
                sign + str(value).encode("ascii") + b"\0",
                ascii_font,
                big5_font,
                style & 0xFFFF,
            )
    battle_hash = fnv1a_bytes(pixels)
    battle_pixels = bytes(pixels)

    panel_x, panel_y, panel_width, panel_height = 220, 19, 100, 140

    def blend_rectangle(x: int, y: int, width: int, height: int) -> None:
        for destination_y in range(max(y, 0), min(y + height, 200)):
            for destination_x in range(max(x, 0), min(x + width, 320)):
                offset = destination_y * 320 + destination_x
                destination = pixels[offset]
                source_rgb = palette[0]
                destination_rgb = palette[destination]
                components = tuple(
                    source_rgb[index] // 8 + destination_rgb[index] // 8
                    for index in range(3)
                )
                pixels[offset] = rgb4_lookup[
                    components[0] * 256 + components[1] * 16 + components[2]
                ]

    for rectangle in (
        (panel_x + 5, panel_y, panel_width - 10, 1),
        (panel_x + 4, panel_y + 1, panel_width - 8, 1),
        (panel_x + 3, panel_y + 2, panel_width - 6, 1),
        (panel_x + 2, panel_y + 3, panel_width - 4, 1),
        (panel_x + 1, panel_y + 4, panel_width - 2, 1),
        (panel_x, panel_y + 5, panel_width, panel_height - 10),
        (panel_x + 1, panel_y + panel_height - 5, panel_width - 2, 1),
        (panel_x + 2, panel_y + panel_height - 4, panel_width - 4, 1),
        (panel_x + 3, panel_y + panel_height - 3, panel_width - 6, 1),
        (panel_x + 4, panel_y + panel_height - 2, panel_width - 8, 1),
        (panel_x + 5, panel_y + panel_height - 1, panel_width - 10, 1),
    ):
        blend_rectangle(*rectangle)
    for left, top, width, height in (
        (panel_x + 5, panel_y + 1, panel_width - 10, 1),
        (panel_x + 4, panel_y + 2, 1, 2),
        (panel_x + panel_width - 5, panel_y + 2, 1, 2),
        (panel_x + 2, panel_y + 4, 2, 1),
        (panel_x + panel_width - 4, panel_y + 4, 2, 1),
        (panel_x + 1, panel_y + 5, 1, panel_height - 10),
        (panel_x + panel_width - 2, panel_y + 5, 1, panel_height - 10),
        (panel_x + 2, panel_y + panel_height - 5, 2, 1),
        (panel_x + panel_width - 4, panel_y + panel_height - 5, 2, 1),
        (panel_x + 4, panel_y + panel_height - 4, 1, 2),
        (panel_x + panel_width - 5, panel_y + panel_height - 4, 1, 2),
        (panel_x + 5, panel_y + panel_height - 2, panel_width - 10, 1),
    ):
        for row in range(top, top + height):
            pixels[row * 320 + left:row * 320 + left + width] = bytes([0xFF]) * width
    draw_battle_sprite(pixels, portraits[1], 242, 82)
    labels = (
        (225, 101, bytes.fromhex("ca5ea44f20"), 0x2321),
        (262, 101, b"  0", 0x0705),
        (285, 101, b"/", 0x6663),
        (292, 101, b"100", 0x2321),
        (225, 118, bytes.fromhex("a5cda95220"), 0x2321),
        (262, 118, b"  0", 0x0705),
        (285, 118, b"/", 0x6663),
        (292, 118, b"  0", 0x2321),
        (225, 135, bytes.fromhex("a4baa44f20"), 0x2321),
        (262, 135, b"  0", 0x504E),
        (285, 135, b"/", 0x504E),
        (292, 135, b"  0", 0x504E),
    )
    for x, y, text, colors in labels:
        draw_battle_text(
            pixels, x, y, text + b"\0", ascii_font, big5_font, colors
        )
    return battle_hash, fnv1a_bytes(pixels), battle_pixels


def battle_session_vector(root: Path, field_words: list[int]) -> dict[str, object]:
    palette_bytes = (root / "MMAP.COL").read_bytes()
    palette = [tuple(palette_bytes[index:index + 3]) for index in range(0, 768, 3)]
    rgb4_lookup: list[int] = []
    for red in range(16):
        for green in range(16):
            for blue in range(16):
                target = (red * 4 + 2, green * 4 + 2, blue * 4 + 2)
                rgb4_lookup.append(min(
                    range(256),
                    key=lambda index: sum(
                        (target[channel] - palette[index][channel]) ** 2
                        for channel in range(3)
                    ),
                ))
    ascii_font = (root / "FONT.X16").read_bytes()
    big5_font = (root / "FONT.C16").read_bytes()
    pixels = bytearray(index % 251 for index in range(320 * 200))

    def blend_rectangle(x: int, y: int, width: int, height: int) -> None:
        for destination_y in range(y, y + height):
            for destination_x in range(x, x + width):
                offset = destination_y * 320 + destination_x
                destination_rgb = palette[pixels[offset]]
                source_rgb = palette[0]
                components = tuple(
                    source_rgb[index] // 8 + destination_rgb[index] // 8
                    for index in range(3)
                )
                pixels[offset] = rgb4_lookup[
                    components[0] * 256 + components[1] * 16 + components[2]
                ]

    def draw_panel(x: int, y: int, width: int, height: int) -> None:
        for rectangle in (
            (x + 5, y, width - 10, 1),
            (x + 4, y + 1, width - 8, 1),
            (x + 3, y + 2, width - 6, 1),
            (x + 2, y + 3, width - 4, 1),
            (x + 1, y + 4, width - 2, 1),
            (x, y + 5, width, height - 10),
            (x + 1, y + height - 5, width - 2, 1),
            (x + 2, y + height - 4, width - 4, 1),
            (x + 3, y + height - 3, width - 6, 1),
            (x + 4, y + height - 2, width - 8, 1),
            (x + 5, y + height - 1, width - 10, 1),
        ):
            blend_rectangle(*rectangle)
        for left, top, rectangle_width, rectangle_height in (
            (x + 5, y + 1, width - 10, 1),
            (x + 4, y + 2, 1, 2),
            (x + width - 5, y + 2, 1, 2),
            (x + 2, y + 4, 2, 1),
            (x + width - 4, y + 4, 2, 1),
            (x + 1, y + 5, 1, height - 10),
            (x + width - 2, y + 5, 1, height - 10),
            (x + 2, y + height - 5, 2, 1),
            (x + width - 4, y + height - 5, 2, 1),
            (x + 4, y + height - 4, 1, 2),
            (x + width - 5, y + height - 4, 1, 2),
            (x + 5, y + height - 2, width - 10, 1),
        ):
            for row in range(top, top + rectangle_height):
                begin = row * 320 + left
                pixels[begin:begin + rectangle_width] = bytes([0xFF]) * rectangle_width

    draw_panel(64, 17, 180, 30)
    draw_battle_text(
        pixels,
        69,
        25,
        bytes.fromhex("bdd0bfefbedcb0d1bb50bed4b0aba4a7a448aaab00"),
        ascii_font,
        big5_font,
        0x0705,
    )
    draw_panel(64, 48, 66, 70)
    draw_battle_text(pixels, 95, 55, b"A\0", ascii_font, big5_font, 0x6663)
    draw_battle_text(pixels, 67, 55, b"*\0", ascii_font, big5_font, 0x0705)
    draw_battle_text(pixels, 95, 75, b"C\0", ascii_font, big5_font, 0x2321)
    draw_battle_text(
        pixels,
        83,
        95,
        bytes.fromhex("b5b2a7f400"),
        ascii_font,
        big5_font,
        0x2321,
    )
    selection_hash = fnv1a_bytes(pixels)

    occupancy = [-1] * 4096
    combatants = (
        (0, 30, 24, 5110),
        (1, 30, 22, 5118),
        (2, 30, 26, 5126),
        (4, 24, 24, 5140),
    )
    for slot, (_, x, y, _) in enumerate(combatants):
        occupancy[y * 64 + x] = slot

    def render_commands(
        view_x: int, view_y: int, actor_cursor_visible: bool = False
    ) -> list[list[int]]:
        commands: list[list[int]] = []
        for local_x in range(32):
            for local_y in range(32):
                map_x = local_x + view_x
                map_y = local_y + view_y
                screen_x = 18 * local_x - 18 * local_y + 145
                screen_y = 9 * local_x + 9 * local_y - 81
                commands.append([
                    0, map_x, map_y, screen_x, screen_y,
                    field_words[map_y * 64 + map_x], 0, 0, 0,
                ])
        for local_x in range(32):
            for local_y in range(32):
                map_x = local_x + view_x
                map_y = local_y + view_y
                cell = map_y * 64 + map_x
                screen_x = 18 * local_x - 18 * local_y + 145
                screen_y = 9 * local_x + 9 * local_y - 81
                # 3AC0C..3AC7B: 556F0==1 draws CLOUD frame5/style3 at
                # E6EE4/E6EE2 before upper terrain and the occupant.
                if actor_cursor_visible and (map_x, map_y) == combatants[0][1:3]:
                    commands.append([
                        1, map_x, map_y, screen_x - 18, screen_y, 0, 1, 3, 0,
                    ])
                object_sprite = field_words[4096 + cell]
                if object_sprite not in (0, 15000):
                    commands.append([
                        0, map_x, map_y, screen_x, screen_y,
                        object_sprite, 0, 0, 0,
                    ])
                occupant = occupancy[cell]
                if occupant >= 0:
                    commands.append([
                        0, map_x, map_y, screen_x, screen_y,
                        combatants[occupant][3], 0, 0, 0,
                    ])
        return commands

    initial_view_x, initial_view_y = 0, 0
    initial_commands = render_commands(initial_view_x, initial_view_y)
    initial_hash, _, _ = battle_pixel_hashes(root, 1, initial_commands)
    action_commands = render_commands(19, 13, actor_cursor_visible=True)
    _, _, action_pixels = battle_pixel_hashes(root, 1, action_commands)
    pixels = bytearray(action_pixels)
    draw_panel(20, 19, 42, 180)
    action_labels = (
        bytes.fromhex("b2beb0ca"),
        bytes.fromhex("a7f0c0bb"),
        bytes.fromhex("a5ceac72"),
        bytes.fromhex("b8d1ac72"),
        bytes.fromhex("c2e5c0f8"),
        bytes.fromhex("aaabab7e"),
        bytes.fromhex("b5a5abdd"),
        bytes.fromhex("aaacba41"),
        bytes.fromhex("a5f0aea7"),
        bytes.fromhex("a6dbb0ca"),
    )
    for ordinal, label in enumerate(action_labels):
        draw_battle_text(
            pixels,
            25,
            24 + 17 * ordinal,
            label + b"\0",
            ascii_font,
            big5_font,
            0x2321,
        )
    draw_battle_text(
        pixels, 25, 24, action_labels[0] + b"\0", ascii_font, big5_font, 0x6663
    )
    draw_panel(220, 19, 100, 140)
    portraits = cumulative_entries(
        (root / "HDGRP.IDX").read_bytes(), (root / "HDGRP.GRP").read_bytes()
    )
    draw_battle_sprite(pixels, portraits[0], 242, 82)
    for x, y, text, colors in (
        (266, 84, b"A", 0x0705),
        (225, 101, bytes.fromhex("ca5ea44f20"), 0x2321),
        (262, 101, b" 60", 0x0705),
        (285, 101, b"/", 0x6663),
        (292, 101, b"100", 0x2321),
        (225, 118, bytes.fromhex("a5cda95220"), 0x2321),
        (262, 118, b"  0", 0x0705),
        (285, 118, b"/", 0x6663),
        (292, 118, b"  0", 0x2321),
        (225, 135, bytes.fromhex("a4baa44f20"), 0x2321),
        (262, 135, b" 25", 0x504E),
        (285, 135, b"/", 0x504E),
        (292, 135, b"  0", 0x504E),
    ):
        draw_battle_text(
            pixels, x, y, text + b"\0", ascii_font, big5_font, colors
        )
    action_menu_hash = fnv1a_bytes(pixels)
    return {
        "battle_id": 2,
        "party_prefix_length": 2,
        "selection_states": [2, 0],
        "selection_cursor": 0,
        "selection_pixel_hash": selection_hash,
        "selected_role_ids": [0, 1, 2],
        "combatant_role_ids": [0, 1, 2, 4],
        "initial_view": [initial_view_x, initial_view_y],
        "initial_command_count": len(initial_commands),
        "initial_pixel_hash": initial_hash,
        "action_menu": {
            "availability": [1] * 10,
            "available_count": 10,
            "cursor": 0,
            "selected_action": -1,
            "actor_cursor": list(combatants[0][1:3]),
            "actor_cursor_visible": True,
            "pixel_hash": action_menu_hash,
        },
    }


def battle_render_plan_vector(
    root: Path, field_words: list[int], setup: dict[str, object]
) -> dict[str, object]:
    view_x = 15
    view_y = 17
    path_limit = 5
    primary_cursor = (27, 26)
    secondary_cursor = (25, 27)
    effect_cell = 26 * 64 + 26
    path_values = [0] * 4096
    path_values[25 * 64 + 25] = 10
    path_values[24 * 64 + 24] = 555
    effects = [0] * 4096
    effects[effect_cell] = 1
    occupancy = [-1] * 4096
    combatants: list[dict[str, int]] = []
    for write in setup["static_occupancy_writes"]:
        role_id = int(write["role_id"])
        initial_mode = 2 if write["side"] == "party" else 1
        combatants.append(
            {
                "role_id": role_id,
                "sprite": 8 * (role_id % 17) + 5106 + 2 * initial_mode,
                "damage": 17 if int(write["occupancy_index"]) == effect_cell else 0,
            }
        )
        occupancy[int(write["occupancy_index"])] = int(write["slot"])

    commands: list[list[int]] = []

    def append(
        kind: int,
        map_x: int,
        map_y: int,
        screen_x: int,
        screen_y: int,
        sprite_id: int = 0,
        overlay_variant: int = 0,
        style: int = 0,
        value: int = 0,
    ) -> None:
        commands.append(
            [
                kind,
                map_x,
                map_y,
                screen_x,
                screen_y,
                sprite_id,
                overlay_variant,
                wrapping_i16(style),
                value,
            ]
        )

    for local_x in range(32):
        for local_y in range(32):
            map_x = local_x + view_x
            map_y = local_y + view_y
            cell = map_y * 64 + map_x
            append(
                0,
                map_x,
                map_y,
                18 * local_x - 18 * local_y + 145,
                9 * local_x + 9 * local_y - 81,
                field_words[cell],
            )

    for local_x in range(32):
        for local_y in range(32):
            map_x = local_x + view_x
            map_y = local_y + view_y
            cell = map_y * 64 + map_x
            sprite_x = 18 * local_x - 18 * local_y + 145
            overlay_x = sprite_x - 18
            screen_y = 9 * local_x + 9 * local_y - 81
            if path_values[cell] > path_limit and path_values[cell] != 555:
                append(1, map_x, map_y, overlay_x, screen_y, overlay_variant=0, style=3)
            if (map_x, map_y) == primary_cursor:
                append(1, map_x, map_y, overlay_x, screen_y, overlay_variant=0, style=2)
            if (map_x, map_y) == secondary_cursor:
                append(1, map_x, map_y, overlay_x, screen_y, overlay_variant=1, style=3)
            object_sprite = field_words[4096 + cell]
            if object_sprite not in (0, 15000):
                append(0, map_x, map_y, sprite_x, screen_y, object_sprite)
            occupant = occupancy[cell]
            if occupant >= 0:
                combatant = combatants[occupant]
                if effects[cell] == 1:
                    append(2, map_x, map_y, sprite_x, screen_y, combatant["sprite"], style=47)
                else:
                    append(0, map_x, map_y, sprite_x, screen_y, combatant["sprite"])
            if effects[cell] == 1:
                append(0, map_x, map_y, sprite_x, screen_y, 8)
            if occupant >= 0 and effects[cell] == 1:
                append(
                    3,
                    map_x,
                    map_y,
                    overlay_x,
                    9 * local_x + 9 * local_y - 145,
                    overlay_variant=1,
                    style=0x9193,
                    value=combatants[occupant]["damage"],
                )

    words = [wrapping_i16(value) for command in commands for value in command]
    counts = {str(kind): sum(command[0] == kind for command in commands) for kind in range(4)}
    target_commands = [command for command in commands if command[1:3] == [26, 26]]
    battle_hash, status_hash, _ = battle_pixel_hashes(
        root, int(setup["battlefield_id"]), commands
    )
    return {
        "battle_id": 4,
        "view": [view_x, view_y],
        "path_limit": path_limit,
        "primary_cursor": list(primary_cursor),
        "secondary_cursor": list(secondary_cursor),
        "effect_cell": [26, 26],
        "command_count": len(commands),
        "command_kind_counts": counts,
        "command_hash": fnv1a_words(words),
        "pixel_hash": battle_hash,
        "status_panel_pixel_hash": status_hash,
        "first_command": commands[0],
        "target_commands": target_commands,
        "zero_path_limit_vector": {
            "secondary_cursor_visible": False,
            "command_count": len(commands) - counts["1"],
            "cursor_overlay_count": 0,
        },
    }


def animation_vectors(effect_counts: list[int]) -> dict[str, object]:
    magic_type = 2
    actor_frame_count = 4
    effect_start = 2
    magic_dispatch_frame = 4
    total_frames = 19
    sprite_base = 100 + 4 * (2 + 3)
    direction = 1
    sprite = 0
    effect_frame = -2 + 2 * sum(effect_counts[:2])
    effect_visible = False
    effect_dispatched = False
    magic_dispatched = False
    magic_words: list[int] = []
    for frame in range(total_frames):
        sprite_updated = frame < actor_frame_count
        if sprite_updated:
            sprite = wrapping_i16(
                2 * actor_frame_count * direction + 2 * sprite_base + 2 * frame
            )
        dispatch_effect = False
        if frame >= effect_start:
            effect_visible = True
            effect_frame = wrapping_i16(effect_frame + 2)
            if not effect_dispatched:
                effect_dispatched = True
                dispatch_effect = True
        dispatch_magic = False
        if frame >= magic_dispatch_frame and not magic_dispatched:
            magic_dispatched = True
            dispatch_magic = True
        magic_words.extend(
            [
                sprite,
                effect_frame,
                17,
                int(sprite_updated),
                int(effect_visible),
                int(dispatch_magic),
                int(dispatch_effect),
            ]
        )

    standalone_words: list[int] = []
    standalone_frame = 2 * sum(effect_counts[:2])
    for _ in range(effect_counts[2]):
        standalone_words.extend([standalone_frame, 17, 1])
        standalone_frame = wrapping_i16(standalone_frame + 2)

    damage_words: list[int] = []
    suppressed_words: list[int] = []
    for frame in range(10):
        damage_words.extend([frame, 1, int(frame < 4)])
        suppressed_words.extend([frame, 1, 0])
    return {
        "magic": {
            "magic_type": magic_type,
            "effect_id": 2,
            "fight_frame_count": 100,
            "direction": direction,
            "actor_frame_count": actor_frame_count,
            "effect_start_frame": effect_start,
            "magic_dispatch_frame": magic_dispatch_frame,
            "frame_count": total_frames,
            "first_sprite": 248,
            "last_sprite": 254,
            "first_effect_frame": 48,
            "last_effect_frame": 80,
            "frame_hash": fnv1a_words(magic_words),
        },
        "standalone_effect": {
            "effect_id": 2,
            "magic_sample_id": 13,
            "prelude_wait_ticks": 100,
            "frame_count": effect_counts[2],
            "first_effect_frame": 48,
            "last_effect_frame": 80,
            "frame_hash": fnv1a_words(standalone_words),
        },
        "damage_display": {
            "frame_count": 10,
            "wait_ticks": 1,
            "flash_frames": [0, 1, 2, 3],
            "frame_hash": fnv1a_words(damage_words),
            "suppressed_hash": fnv1a_words(suppressed_words),
        },
    }


def legacy_path_index(x: int, y: int) -> int | None:
    if x < 0 or y < 0 or x > 64 or y > 64:
        return None
    index = y * 64 + x
    return index if 0 <= index < 4096 else None


def build_path_map(
    field_words: list[int],
    source: tuple[int, int],
    mode: str,
    occupied: set[int] | None = None,
) -> list[int]:
    occupied = occupied or set()
    values: list[int] = []
    for index in range(4096):
        upper_layer = field_words[4096 + index]
        if mode == "targeting":
            values.append(555 if upper_layer != 0 else 254)
        else:
            blocked_ground = any(
                begin <= field_words[index] <= end for begin, end in BLOCKED_TILE_RANGES
            )
            values.append(
                555 if upper_layer != 0 or index in occupied or blocked_ground else 254
            )
    source_index = legacy_path_index(*source)
    if source_index is None:
        raise ValueError(f"invalid path source {source}")
    values[source_index] = 0

    queue = [(0, 0)] * 255
    queue[0] = (0, -1)
    queue[1] = source
    read_index = 0
    write_index = 2
    distance = 0

    def enqueue(coordinate: tuple[int, int]) -> None:
        nonlocal write_index
        queue[write_index] = coordinate
        write_index = (write_index + 1) % 255

    def dequeue() -> tuple[int, int]:
        nonlocal read_index
        coordinate = queue[read_index]
        read_index = (read_index + 1) % 255
        return coordinate

    while True:
        current = dequeue()
        if current[1] < 0:
            distance = (distance + 1) % 128
            enqueue((0, -1))
            current = dequeue()
            if current[1] < 0:
                break
        for dx, dy in PATH_DIRECTIONS:
            next_coord = (current[0] + dx, current[1] + dy)
            next_index = legacy_path_index(*next_coord)
            if next_index is not None and values[next_index] == 254:
                enqueue(next_coord)
                values[next_index] = distance
    return values


def ai_escape_vector(field_words: list[int]) -> dict[str, object]:
    combatants = [
        {"side": 0, "x": 10, "y": 20},
        {"side": 0, "x": 11, "y": 21},
        {"side": 0, "x": 12, "y": 22},
        {"side": 1, "x": 13, "y": 23},
        {"side": 1, "x": 14, "y": 24},
    ]
    source = (10, 20)
    occupied = {c["y"] * 64 + c["x"] for c in combatants}
    values = build_path_map(field_words, source, "movement", occupied)

    def select(
        path_values: list[int], round_value: int, opponents: list[tuple[int, int]]
    ) -> tuple[list[int] | None, int, list[list[int]]]:
        best_score = 0
        destination: list[int] | None = None
        tied_best: list[list[int]] = []
        for x in range(64):
            for y in range(64):
                if path_values[y * 64 + x] != round_value:
                    continue
                score = sum(abs(x - enemy_x) + abs(y - enemy_y) for enemy_x, enemy_y in opponents)
                if score > best_score:
                    best_score = score
                    destination = [x, y]
                    tied_best = [[x, y]]
                elif score == best_score and score > 0:
                    tied_best.append([x, y])
        return destination, best_score, tied_best

    round_value = 3
    opponents = [(c["x"], c["y"]) for c in combatants if c["side"] != 0]
    destination, best_score, _ = select(values, round_value, opponents)
    sparse_values = build_path_map(
        field_words, source, "movement", {source[1] * 64 + source[0]}
    )
    tie_destination, tie_score, tied_best = select(sparse_values, 1, [source])
    no_opponent_destination, no_opponent_score, _ = select(sparse_values, 1, [])
    zero_destination, zero_score, _ = select(sparse_values, 0, [(13, 23), (14, 24)])
    return {
        "battle_id": 3,
        "battlefield_id": 2,
        "source": list(source),
        "round_value": round_value,
        "combatants": combatants,
        "scan_order": ["x", "y"],
        "strictly_greater_replaces": True,
        "destination": destination,
        "maximum_enemy_distance_sum": best_score,
        "rest_after_move_for_escape_action": True,
        "rest_after_move_for_item_reposition": False,
        "hidden_dead_tie": {
            "round_value": 1,
            "enemy": {"x": 10, "y": 20, "hidden": 1, "hp": 0},
            "tied_best": tied_best,
            "destination": tie_destination,
            "maximum_enemy_distance_sum": tie_score,
        },
        "no_opponents": {
            "round_value": 1,
            "destination": no_opponent_destination,
            "maximum_enemy_distance_sum": no_opponent_score,
        },
        "zero_round": {
            "round_value": 0,
            "destination": zero_destination,
            "maximum_enemy_distance_sum": zero_score,
        },
    }


def ai_attack_handler_vectors(
    z_dat_bytes: bytes, field_words: list[int]
) -> dict[str, object]:
    table_end = Z_DAT_AI_SPECIAL_ATTACK_OFFSET + AI_SPECIAL_ATTACK_COUNT * 6
    if table_end > len(z_dat_bytes):
        raise ValueError("Z.DAT does not contain the AI special-attack table")
    table_bytes = z_dat_bytes[Z_DAT_AI_SPECIAL_ATTACK_OFFSET:table_end]
    table_words = struct.unpack(f"<{AI_SPECIAL_ATTACK_COUNT * 3}h", table_bytes)
    table = [
        {
            "weapon_id": table_words[index * 3],
            "magic_id": table_words[index * 3 + 1],
            "bonus": table_words[index * 3 + 2],
        }
        for index in range(AI_SPECIAL_ATTACK_COUNT)
    ]
    zero_magic_roll, zero_magic_state = legacy_bounded(1, 0)
    magic_roll, state = legacy_bounded(9, 2)
    target_roll, state = legacy_bounded(state, 10)
    targeting = build_path_map(field_words, (10, 20), "targeting")
    distance_3 = targeting[23 * 64 + 13]
    distance_4 = targeting[24 * 64 + 14]
    adjacent_targeting = build_path_map(field_words, (10, 20), "targeting")
    return {
        "special_attack_table": {
            "z_dat_file_offset": Z_DAT_AI_SPECIAL_ATTACK_OFFSET,
            "entry_count": AI_SPECIAL_ATTACK_COUNT,
            "bytes_sha256": sha256(table_bytes),
            "entries": table,
        },
        "magic_selection_edges": {
            "zero_learned_count_slot": zero_magic_roll,
            "zero_learned_count_state_after": zero_magic_state,
            "packed_slots_required": True,
            "selected_count_rank_used_as_direct_slot": True,
            "magic_level_word_unsigned": True,
            "magic_level_divisor": 100,
            "magic_record_bytes": 136,
            "invalid_linear_reads": "modern safety rejection",
        },
        "magic_then_target_rng": {
            "seed": 9,
            "learned_magic_count": 2,
            "magic_slot": magic_roll,
            "target_bound": 10,
            "target_roll": target_roll,
            "state_after": state,
            "call_order": ["automatic_magic_slot", "attack_target_strategy"],
        },
        "range_rules": {
            "target_distance": distance_3,
            "area_0": {"movement_mode": 1, "range": 6, "in_range": True},
            "area_1_diagonal": {"movement_mode": 2, "range": 6, "in_range": False},
            "area_1_aligned": {"movement_mode": 2, "distance": 1, "in_range": True},
            "area_2_diagonal": {"movement_mode": 2, "range": 6, "in_range": False},
            "area_3": {"movement_mode": 1, "range": 6, "in_range": True},
            "unsupported_area": {"movement_mode": 0, "in_range": False},
        },
        "branch_order": {
            "initial_target_distances": [distance_3, distance_4],
            "already_in_range": "attack",
            "out_of_range_with_no_round_value": "finish_without_rest",
            "out_of_range_with_round_value": "move",
            "after_move_original_target_first": True,
            "after_move_reselect_strategy": "nearest",
            "attack_call_automatic_flag": 1,
            "handler_writes_action_done_after_step": True,
            "reselected_adjacent_distance": adjacent_targeting[20 * 64 + 11],
            "reselected_adjacent_result": "attack",
            "reselected_distance_6_result_for_range_1": "rest",
            "selector_no_write_valid_stale_target": "reuse stale target",
            "selector_no_write_invalid_target": "modern safety rejection",
            "nearest_no_write_after_move": "retain stale target then rest",
        },
    }


def ai_item_handler_vectors(field_words: list[int]) -> dict[str, object]:
    targeting = build_path_map(field_words, (10, 20), "targeting")
    strongest_roll, strongest_state = legacy_bounded(9, 10)
    return {
        "consumable": {
            "calls_relocation_before_use": True,
            "relocation_rest_after_move": False,
            "source": [10, 20],
            "round_value": 3,
            "destination": [7, 20],
            "maximum_enemy_distance_sum": 20,
            "use_mode": 0,
            "next_step_with_destination": "move",
            "next_step_after_relocation": "use_item",
            "use_after_relocation_even_without_destination": True,
            "zero_enemy_score": {
                "maximum_enemy_distance_sum": 0,
                "destination": None,
                "next_step": "use_item",
            },
        },
        "throwing_weapon": {
            "target_selector_runs_before_range": True,
            "actor_hidden_weapon": 80,
            "targeting_range": trunc_div(80, 15) + 1,
            "movement_mode": 1,
            "use_mode": 1,
            "nearest": {
                "target_slot": 3,
                "distance": targeting[23 * 64 + 13],
                "rng_consumed": False,
                "result": "use_item",
                "range_checks": 1,
            },
            "strongest": {
                "rng_seed": 9,
                "rng_output": strongest_roll,
                "rng_state_after": strongest_state,
                "attacks": [30, 50],
                "target_slot": 4,
                "distance": targeting[24 * 64 + 14],
                "round_positive_result": "move",
                "range_checks_before_move": 1,
                "no_position_change_result": "attack_fallback",
                "range_checks_after_resume": 2,
                "moved_source": [13, 23],
                "moved_distance": 2,
                "moved_result": "use_item",
            },
            "round_zero_out_of_range": {
                "result": "attack_fallback",
                "range_checks": 2,
            },
            "signed_negative_range_and_round": {
                "hidden_weapon": -16,
                "targeting_range": trunc_div(-16, 15) + 1,
                "round_value": -1,
                "movement_skipped": True,
                "target_slot": 3,
                "distance": targeting[23 * 64 + 13],
                "result": "attack_fallback",
                "range_checks": 2,
                "rng_consumed": False,
            },
            "stale_target_bug": {
                "strategy_gate_hit": True,
                "eligible_attacks": [0, 0],
                "target_written": False,
                "preexisting_target_slot": 4,
                "target_slot_used": 4,
            },
            "post_move_target_reselected": False,
        },
        "item_source_by_side": {
            "party_side_0": "inventory_slot",
            "enemy_nonzero_side": "role_carried_slot",
            "inventory_count_checked_before_handler": False,
        },
        "carried_item_remove": {
            "removed_slot": 1,
            "item_ids_before": [5, 6, 7, 8],
            "counts_before": [1, 2, 3, 4],
            "item_ids_after": [5, 7, 8, -1],
            "counts_after": [1, 3, 4, 0],
        },
        "outer_ai_marks_action_done_after_handler": True,
    }


def ai_poison_handler_vectors(field_words: list[int]) -> dict[str, object]:
    targeting = build_path_map(field_words, (10, 20), "targeting")
    high_iq_roll, high_iq_state = legacy_bounded(9, 10)
    boundary_roll, boundary_state = legacy_bounded(3, 10)
    allied_total = wrapping_i16(0 + 10)
    allied_total = wrapping_i16(allied_total + 100)
    allied_total = wrapping_i16(allied_total + 10)
    allied_total = wrapping_i16(allied_total + 100)
    allied_total = wrapping_i16(allied_total + 10)
    allied_total = wrapping_i16(allied_total + 100)
    allied_count = 3
    doubled_average = trunc_div(2 * allied_total, allied_count)
    return {
        "eligibility": {
            "different_side": True,
            "hidden_equals_zero": True,
            "target_poison_strictly_below": 95,
            "target_anti_poison_strictly_below_actor_use_poison": True,
        },
        "high_iq_strongest": {
            "iq_strictly_above": 60,
            "rng_seed": 9,
            "rng_output": high_iq_roll,
            "rng_state_after": high_iq_state,
            "rng_threshold": 7,
            "attacks": [30, 50],
            "target_slot": 4,
        },
        "high_iq_roll_7_fallback": {
            "iq": 61,
            "rng_seed": 3,
            "rng_output": boundary_roll,
            "rng_state_after": boundary_state,
            "rng_threshold": 7,
            "strategy": "first_eligible_stale_distance",
            "target_slot": 3,
        },
        "strongest_dead_first_tie": {
            "hit_points": [0, 100],
            "attacks": [50, 50],
            "reads_hit_points": False,
            "comparison": "strictly_greater",
            "target_slot": 3,
        },
        "strongest_negative_hidden": {
            "hidden": [-1, 0],
            "attacks": [100, 50],
            "hidden_rule": "equals_zero",
            "target_slot": 4,
        },
        "strongest_signed_eligibility": {
            "actor_use_poison": -10,
            "candidate_poison": [-1, -1],
            "candidate_anti_poison": [-20, -10],
            "attacks": [30, 50],
            "anti_poison_rule": "strictly_less_than_actor_use_poison",
            "target_slot": 3,
        },
        "stale_distance_first_eligible_bug": {
            "strongest_attacks": [0, 0],
            "stale_target_slot": 4,
            "stale_target_distance": targeting[24 * 64 + 14],
            "candidate_distances_ignored": [
                targeting[23 * 64 + 13],
                targeting[24 * 64 + 14],
            ],
            "target_slot": 3,
            "strict_tie_keeps_first": True,
            "rebuilds_targeting_map_for_each_eligible_candidate": True,
            "no_eligible_target_does_not_read_stale_slot": True,
        },
        "first_negative_hidden_dead_target": {
            "hidden": [-1, 0],
            "hit_points": [100, 0],
            "reads_hit_points": False,
            "stale_target_slot": 4,
            "stale_target_distance": targeting[24 * 64 + 14],
            "target_slot": 4,
        },
        "first_signed_eligibility": {
            "actor_use_poison": -10,
            "candidate_poison": [-1, -1],
            "candidate_anti_poison": [-20, -10],
            "anti_poison_rule": "strictly_less_than_actor_use_poison",
            "stale_target_slot": 4,
            "stale_target_distance": targeting[24 * 64 + 14],
            "target_slot": 3,
        },
        "session_stale_target_storage": {
            "legacy_scratch_initial": 0,
            "actor_word12_before": 99,
            "selector_source": "independent_session_scratch",
            "target_slot": 1,
            "actor_word12_after": 1,
        },
        "range_and_round": {
            "actor_use_poison": 80,
            "targeting_range": trunc_div(80, 15) + 1,
            "target_distance": targeting[23 * 64 + 13],
            "round_zero_in_range": {"result": "poison", "range_checks": 1},
            "round_positive_in_range": {"result": "move_mode_3", "range_checks_before_move": 1},
            "round_zero_out_of_range": {
                "result": "average_attack_fallback",
                "range_checks": 2,
            },
            "round_negative_in_range": {
                "result": "poison_after_second_range_check",
                "range_checks": 2,
            },
        },
        "out_of_range_fallback": {
            "frozen_outer_totals": True,
            "actor_attack_10": {
                "allied_total_i16": allied_total,
                "allied_count_i16": allied_count,
                "doubled_allied_average": doubled_average,
                "doubled_actor_attack": 20,
                "result": "rest",
            },
            "actor_attack_200": {
                "allied_total_i16": 520,
                "allied_count_i16": 3,
                "doubled_allied_average": trunc_div(2 * 520, 3),
                "doubled_actor_attack": 400,
                "result": "attack",
            },
            "comparison": "strictly_greater",
        },
        "no_target_stale_word12": 99,
        "no_target_preserves_actor_word12": True,
        "no_target_result": "attack",
        "outer_ai_marks_action_done_after_handler": True,
    }


def strongest_attack_target_oracle(
    combatants: list[dict[str, int]],
    attacks: list[int],
    *,
    actor_slot: int = 0,
    initial_target: int = -1,
) -> dict[str, object]:
    actor_side = combatants[actor_slot]["side"]
    best = 0
    target = initial_target
    written = False
    for slot, combatant in enumerate(combatants):
        if combatant["side"] == actor_side or combatant["hidden"] != 0:
            continue
        attack = wrapping_i16(attacks[slot])
        if attack > best:
            best = attack
            target = slot
            written = True
    return {"best_attack": best, "target_slot": target, "target_written": written}


def weakest_attack_target_oracle(
    combatants: list[dict[str, int]],
    attacks: list[int],
    *,
    actor_slot: int = 0,
    initial_target: int = -1,
) -> dict[str, object]:
    actor_side = combatants[actor_slot]["side"]
    best = 1000
    target = initial_target
    written = False
    for slot, combatant in enumerate(combatants):
        if combatant["side"] == actor_side or combatant["hidden"] != 0:
            continue
        attack = wrapping_i16(attacks[slot])
        if attack < best:
            best = attack
            target = slot
            written = True
    return {"best_attack": best, "target_slot": target, "target_written": written}


def specialist_target_oracle(
    combatants: list[dict[str, int]],
    use_poison: list[int],
    detoxification: list[int],
    medicine: list[int],
    attacks: list[int],
    *,
    actor_slot: int = 0,
    initial_target: int = -1,
) -> dict[str, object]:
    actor_side = combatants[actor_slot]["side"]
    ally_can_poison = False
    for slot, combatant in enumerate(combatants):
        if combatant["side"] == actor_side and wrapping_i16(use_poison[slot]) > 20:
            ally_can_poison = True

    best = 0
    target = initial_target
    detox_target = None
    detox_target_at_least_20 = False
    if ally_can_poison:
        for slot, combatant in enumerate(combatants):
            if combatant["side"] == actor_side or combatant["hidden"] != 0:
                continue
            value = wrapping_i16(detoxification[slot])
            if value > best:
                best = value
                target = slot
                detox_target = slot
                if value >= 20:
                    detox_target_at_least_20 = True

    medicine_start_best = best
    medicine_target = None
    medicine_target_at_least_20 = False
    medicine_ran = not detox_target_at_least_20
    if medicine_ran:
        for slot, combatant in enumerate(combatants):
            if combatant["side"] == actor_side or combatant["hidden"] != 0:
                continue
            value = wrapping_i16(medicine[slot])
            if value > best:
                best = value
                target = slot
                medicine_target = slot
                if value >= 20:
                    medicine_target_at_least_20 = True

    fallback_called = not medicine_target_at_least_20
    fallback_written = False
    if fallback_called:
        fallback = weakest_attack_target_oracle(
            combatants, attacks, actor_slot=actor_slot, initial_target=target
        )
        target = fallback["target_slot"]
        fallback_written = fallback["target_written"]
    return {
        "ally_can_poison": ally_can_poison,
        "detoxification_ran": ally_can_poison,
        "detoxification_target": detox_target,
        "detoxification_best": medicine_start_best,
        "detoxification_at_least_20": detox_target_at_least_20,
        "medicine_ran": medicine_ran,
        "medicine_start_best": medicine_start_best,
        "medicine_target": medicine_target,
        "final_best": best,
        "medicine_at_least_20": medicine_target_at_least_20,
        "fallback_called": fallback_called,
        "fallback_written": fallback_written,
        "target_slot": target,
    }


def nearest_target_oracle(
    combatants: list[dict[str, int]],
    path_values: list[int],
    *,
    actor_slot: int = 0,
    initial_target: int = -1,
) -> dict[str, object]:
    actor_side = combatants[actor_slot]["side"]
    best = 1000
    target = initial_target
    written = False
    for slot, combatant in enumerate(combatants):
        if combatant["side"] == actor_side or combatant["hidden"] != 0:
            continue
        distance = wrapping_i16(path_values[combatant["y"] * 64 + combatant["x"]])
        if distance < best:
            best = distance
            target = slot
            written = True
    return {"best_distance": best, "target_slot": target, "target_written": written}


def ai_attack_target_vectors(field_words: list[int]) -> dict[str, object]:
    combatants = [
        {"side": 0, "hidden": 0, "x": 10, "y": 20},
        {"side": 0, "hidden": 0, "x": 11, "y": 21},
        {"side": 0, "hidden": 0, "x": 12, "y": 22},
        {"side": 1, "hidden": 0, "x": 13, "y": 23},
        {"side": 1, "hidden": 0, "x": 14, "y": 24},
    ]
    attacks = [10, 10, 10, 30, 50]
    visible_enemies = [
        i for i, c in enumerate(combatants) if c["side"] != 0 and c["hidden"] == 0
    ]
    strongest = strongest_attack_target_oracle(combatants, attacks)["target_slot"]
    weakest = weakest_attack_target_oracle(combatants, attacks)["target_slot"]
    targeting = build_path_map(field_words, (10, 20), "targeting")
    nearest = min(
        visible_enemies,
        key=lambda i: targeting[combatants[i]["y"] * 64 + combatants[i]["x"]],
    )
    seed9_roll, seed9_state = legacy_bounded(9, 10)
    cascade_first, cascade_state = legacy_bounded(1, 10)
    cascade_second, cascade_state = legacy_bounded(cascade_state, 10)
    cutoff_success, cutoff_success_state = legacy_bounded(6, 10)
    cutoff_failure, cutoff_failure_state = legacy_bounded(3, 10)
    if cutoff_success != 6 or cutoff_failure != 7:
        raise ValueError("AI attack-target cutoff seeds changed")
    tie_vector = strongest_attack_target_oracle(
        combatants, [10, 10, 10, 50, 50]
    )
    hidden_combatants = [dict(combatant) for combatant in combatants]
    hidden_combatants[4]["hidden"] = -1
    hidden_vector = strongest_attack_target_oracle(
        hidden_combatants, [10, 10, 10, 30, 100]
    )
    stale_vector = strongest_attack_target_oracle(
        combatants, [10, 10, 10, 0, -1], initial_target=4
    )
    weakest_tie_vector = weakest_attack_target_oracle(
        combatants, [10, 10, 10, 30, 30]
    )
    weakest_hidden_combatants = [dict(combatant) for combatant in combatants]
    weakest_hidden_combatants[4]["hidden"] = -1
    weakest_hidden_vector = weakest_attack_target_oracle(
        weakest_hidden_combatants, [10, 10, 10, 30, -100]
    )
    weakest_signed_vector = weakest_attack_target_oracle(
        combatants, [10, 10, 10, -1, -2]
    )
    weakest_stale_vector = weakest_attack_target_oracle(
        combatants, [10, 10, 10, 1000, 1001], initial_target=4
    )
    specialist_zeroes = [0, 0, 0, 0, 0]
    specialist_medicine_vector = specialist_target_oracle(
        combatants,
        specialist_zeroes,
        specialist_zeroes,
        [0, 0, 0, 30, 10],
        attacks,
    )
    specialist_hidden_ally_combatants = [dict(combatant) for combatant in combatants]
    specialist_hidden_ally_combatants[1]["hidden"] = -1
    specialist_hidden_ally_vector = specialist_target_oracle(
        specialist_hidden_ally_combatants,
        [0, 21, 0, 0, 0],
        specialist_zeroes,
        specialist_zeroes,
        attacks,
    )
    specialist_detox_bug_vector = specialist_target_oracle(
        combatants,
        [0, 21, 0, 0, 0],
        [0, 0, 0, 20, 0],
        [0, 0, 0, 0, 100],
        [10, 10, 10, 50, 10],
    )
    specialist_shared_best_vector = specialist_target_oracle(
        combatants,
        [0, 21, 0, 0, 0],
        [0, 0, 0, 15, 0],
        [0, 0, 0, 14, 12],
        [10, 10, 10, 50, 10],
    )
    specialist_medicine_after_detox_vector = specialist_target_oracle(
        combatants,
        [0, 21, 0, 0, 0],
        [0, 0, 0, 10, 0],
        [0, 0, 0, 0, 20],
        attacks,
    )
    specialist_tie_vector = specialist_target_oracle(
        combatants,
        specialist_zeroes,
        specialist_zeroes,
        [0, 0, 0, 20, 20],
        attacks,
    )
    specialist_hidden_enemy_combatants = [dict(combatant) for combatant in combatants]
    specialist_hidden_enemy_combatants[4]["hidden"] = -1
    specialist_hidden_enemy_vector = specialist_target_oracle(
        specialist_hidden_enemy_combatants,
        specialist_zeroes,
        specialist_zeroes,
        [0, 0, 0, 20, 100],
        attacks,
    )
    nearest_vector = nearest_target_oracle(combatants, targeting)
    nearest_tie_combatants = [dict(combatant) for combatant in combatants]
    nearest_tie_combatants[4]["x"] = nearest_tie_combatants[3]["x"]
    nearest_tie_combatants[4]["y"] = nearest_tie_combatants[3]["y"]
    nearest_tie_vector = nearest_target_oracle(nearest_tie_combatants, targeting)
    nearest_hidden_combatants = [dict(combatant) for combatant in combatants]
    nearest_hidden_combatants[3]["hidden"] = -1
    nearest_hidden_vector = nearest_target_oracle(nearest_hidden_combatants, targeting)
    nearest_stale_combatants = [dict(combatant) for combatant in combatants]
    nearest_stale_combatants[3]["hidden"] = 1
    nearest_stale_combatants[4]["hidden"] = -1
    nearest_stale_vector = nearest_target_oracle(
        nearest_stale_combatants, targeting, initial_target=4
    )
    nearest_blocked_combatants = [dict(combatant) for combatant in combatants]
    nearest_blocked_combatants[3]["x"] = 23
    nearest_blocked_combatants[3]["y"] = 9
    nearest_blocked_combatants[4]["hidden"] = -1
    nearest_blocked_vector = nearest_target_oracle(nearest_blocked_combatants, targeting)
    if nearest_blocked_vector["best_distance"] != 555:
        raise ValueError("battle 3 fixed blocked target no longer has distance 555")
    return {
        "strongest": {
            "morality": 75,
            "rng_output": seed9_roll,
            "rng_state_after": seed9_state,
            "attacks": attacks[3:],
            "target_slot": strongest,
        },
        "weakest": {
            "morality": 25,
            "rng_output": seed9_roll,
            "rng_state_after": seed9_state,
            "attacks": attacks[3:],
            "target_slot": weakest,
        },
        "specialist_medicine": {
            "iq": 70,
            "medicine": [30, 10],
            **specialist_medicine_vector,
        },
        "specialist_hidden_dead_ally_triggers": {
            "trigger_ally_slot": 1,
            "trigger_ally_hidden": -1,
            "trigger_ally_hp": 0,
            "ally_use_poison": 21,
            **specialist_hidden_ally_vector,
        },
        "specialist_detox_fallback_bug": {
            "ally_use_poison": 21,
            "detoxification": [20, 0],
            "medicine": [0, 100],
            "attacks": [50, 10],
            **specialist_detox_bug_vector,
        },
        "specialist_shared_best_below_threshold": {
            "detoxification": [15, 0],
            "medicine": [14, 12],
            "attacks": [50, 10],
            **specialist_shared_best_vector,
        },
        "specialist_medicine_after_detox": {
            "detoxification": [10, 0],
            "medicine": [0, 20],
            **specialist_medicine_after_detox_vector,
        },
        "specialist_medicine_first_tie_with_dead_first_candidate": {
            "medicine": [20, 20],
            "hp": [0, 100],
            **specialist_tie_vector,
        },
        "specialist_negative_hidden_enemy_skipped": {
            "medicine": [20, 100],
            "hidden": [0, -1],
            **specialist_hidden_enemy_vector,
        },
        "nearest": {
            "targeting_distances": [
                targeting[c["y"] * 64 + c["x"]] for c in combatants[3:]
            ],
            **nearest_vector,
            "rng_consumed": False,
        },
        "nearest_first_tie_with_dead_first_candidate": {
            "coordinates": [[13, 23], [13, 23]],
            "hp": [0, 100],
            **nearest_tie_vector,
        },
        "nearest_negative_hidden_candidate_skipped": {
            "hidden": [-1, 0],
            "distances": [6, 8],
            **nearest_hidden_vector,
        },
        "nearest_no_candidate_preserves_stale_target": {
            "hidden": [1, -1],
            "initial_target": 4,
            **nearest_stale_vector,
        },
        "nearest_blocked_distance_555_is_selectable": {
            "coordinate": [23, 9],
            "hidden_other_candidate": -1,
            **nearest_blocked_vector,
        },
        "failed_high_morality_then_failed_iq": {
            "rng_outputs": [cascade_first, cascade_second],
            "rng_state_after": cascade_state,
            "target_slot": nearest,
        },
        "selected_strongest_with_no_positive_attack": {
            "attacks": [0, 0],
            "target_slot": -1,
            "falls_back": False,
        },
        "roll_cutoff": {
            "success": {
                "seed": 6,
                "rng_output": cutoff_success,
                "rng_state_after": cutoff_success_state,
                "selected_strategy": "strongest_attack",
            },
            "failure": {
                "seed": 3,
                "rng_output": cutoff_failure,
                "rng_state_after": cutoff_failure_state,
                "fallback_strategy": "nearest",
            },
        },
        "signed_minimum_morality": {
            "morality": -32768,
            "seed": 6,
            "rng_output": cutoff_success,
            "rng_state_after": cutoff_success_state,
            "selected_strategy": "weakest_attack",
            "target_slot": weakest,
        },
        "strongest_selector": {
            "strict_first_tie_with_dead_first_candidate": {
                "attacks": [50, 50],
                "hp": [0, 100],
                **tie_vector,
            },
            "negative_hidden_flag_skips_stronger_candidate": {
                "attacks": [30, 100],
                "hidden": [0, -1],
                **hidden_vector,
            },
            "nonpositive_attacks_preserve_stale_target": {
                "attacks": [0, -1],
                "initial_target": 4,
                **stale_vector,
            },
            "same_side_candidates_skipped": True,
            "consumes_random": False,
        },
        "weakest_selector": {
            "strict_first_tie_with_dead_first_candidate": {
                "attacks": [30, 30],
                "hp": [0, 100],
                **weakest_tie_vector,
            },
            "negative_hidden_flag_skips_lower_candidate": {
                "attacks": [30, -100],
                "hidden": [0, -1],
                **weakest_hidden_vector,
            },
            "signed_negative_attacks": {
                "attacks": [-1, -2],
                **weakest_signed_vector,
            },
            "at_or_above_1000_preserves_stale_target": {
                "attacks": [1000, 1001],
                "initial_target": 4,
                **weakest_stale_vector,
            },
            "same_side_candidates_skipped": True,
            "consumes_random": False,
        },
        "strict_comparisons_preserve_first_tie": True,
        "hidden_opponents_skipped": True,
    }


def mark_path(values: list[int], source: tuple[int, int], target: tuple[int, int]) -> bool:
    target_index = legacy_path_index(*target)
    if target_index is None or not 0 <= values[target_index] < 254:
        return False
    current = target
    distance = values[target_index]
    values[target_index] = 250
    for _ in range(4096):
        if current == source:
            return True
        distance = (distance + 127) % 128
        for dx, dy in PATH_DIRECTIONS:
            previous = (current[0] + dx, current[1] + dy)
            if not (0 <= previous[0] < 64 and 0 <= previous[1] < 64):
                continue
            previous_index = legacy_path_index(*previous)
            if previous_index is not None and values[previous_index] == distance:
                values[previous_index] = 250
                current = previous
                break
        else:
            return False
    return False


def battle_setup_record(battle_id: int, record: bytes) -> dict[str, object]:
    words = list(struct.unpack("<93h", record))
    preset_party = words[9:15]
    fixed_party = words[15:21]
    party_x = words[21:27]
    party_y = words[27:33]
    enemies = words[33:53]
    enemy_x = words[53:73]
    enemy_y = words[73:93]
    use_fixed_party = any(role_id != -1 for role_id in fixed_party)
    active_party = fixed_party if use_fixed_party else preset_party
    writes: list[dict[str, int | str]] = []
    slot = 0
    for source_index, role_id in enumerate(active_party):
        if role_id == -1:
            continue
        writes.append(
            {
                "slot": slot,
                "side": "party",
                "source_index": source_index,
                "role_id": role_id,
                "x": party_x[source_index],
                "y": party_y[source_index],
                "occupancy_index": party_y[source_index] * 64 + party_x[source_index],
            }
        )
        slot += 1
    for source_index, role_id in enumerate(enemies):
        if role_id == -1:
            continue
        writes.append(
            {
                "slot": slot,
                "side": "enemy",
                "source_index": source_index,
                "role_id": role_id,
                "x": enemy_x[source_index],
                "y": enemy_y[source_index],
                "occupancy_index": enemy_y[source_index] * 64 + enemy_x[source_index],
            }
        )
        slot += 1
    duplicate_writes: list[dict[str, int]] = []
    previous_slots: dict[int, int] = {}
    for write in writes:
        occupancy_index = int(write["occupancy_index"])
        if occupancy_index in previous_slots:
            duplicate_writes.append(
                {
                    "occupancy_index": occupancy_index,
                    "previous_slot": previous_slots[occupancy_index],
                    "replacement_slot": int(write["slot"]),
                }
            )
        previous_slots[occupancy_index] = int(write["slot"])
    return {
        "battle_id": battle_id,
        "battlefield_id": words[6],
        "music_id": words[8],
        "preset_party_ids": preset_party,
        "fixed_party_ids": fixed_party,
        "party_x": party_x,
        "party_y": party_y,
        "enemy_ids": enemies,
        "enemy_x": enemy_x,
        "enemy_y": enemy_y,
        "active_party_source": "fixed" if use_fixed_party else "preset",
        "active_party_count": sum(role_id != -1 for role_id in active_party),
        "enemy_count": sum(role_id != -1 for role_id in enemies),
        "static_combatant_count": len(writes),
        "static_occupancy_writes": writes,
        "duplicate_occupancy_writes": duplicate_writes,
    }


def build(data_root: Path) -> dict[str, object]:
    z_dat_bytes = (data_root / "Z.DAT").read_bytes()
    effect_end = Z_DAT_EFFECT_FRAME_COUNTS_OFFSET + EFFECT_FRAME_COUNT * 2
    if effect_end > len(z_dat_bytes):
        raise ValueError("Z.DAT does not contain the battle effect frame-count table")
    effect_table_bytes = z_dat_bytes[Z_DAT_EFFECT_FRAME_COUNTS_OFFSET:effect_end]
    effect_counts = list(struct.unpack(f"<{EFFECT_FRAME_COUNT}h", effect_table_bytes))
    if any(value <= 0 for value in effect_counts):
        raise ValueError("battle effect frame-count table contains a non-positive value")

    ranger_group_bytes = (data_root / "RANGER.GRP").read_bytes()
    magic_bytes = ranger_group_bytes[101_444:114_092]
    if len(magic_bytes) != 93 * 136:
        raise ValueError("RANGER.GRP does not contain 93 complete magic records")
    magic_effect_ids = [
        struct.unpack_from("<h", magic_bytes, index * 136 + 13 * 2)[0]
        for index in range(93)
    ]
    item_bytes = ranger_group_bytes[59_076:97_076]
    if len(item_bytes) != 200 * 190:
        raise ValueError("RANGER.GRP does not contain 200 complete item records")
    item_records = [item_bytes[index * 190:(index + 1) * 190] for index in range(200)]
    poisoned_throw = throwing_weapon_vector(
        item_records[102],
        seed=1,
        hidden_weapon=20,
        hp=100,
        maximum_hp=200,
        hurt=40,
        poison=10,
        anti_poison=5,
    )
    plain_throw = throwing_weapon_vector(
        item_records[96],
        seed=2,
        hidden_weapon=20,
        hp=100,
        maximum_hp=200,
        hurt=0,
        poison=10,
        anti_poison=0,
    )
    ai_poisoned_throw = ai_throwing_weapon_vector(
        item_records[102],
        seed=1,
        hidden_weapon=20,
        hp=100,
        maximum_hp=200,
        hurt=40,
        poison=10,
    )
    ai_plain_throw = ai_throwing_weapon_vector(
        item_records[96],
        seed=2,
        hidden_weapon=20,
        hp=100,
        maximum_hp=200,
        hurt=0,
        poison=10,
    )
    ai_stale_payload_throw = ai_throwing_weapon_vector(
        item_records[96],
        source_item=item_records[102],
        seed=1,
        hidden_weapon=20,
        hp=100,
        maximum_hp=200,
        hurt=40,
        poison=10,
    )
    shared_role = [0] * 91
    shared_role[17] = 100
    shared_role[18] = 200
    shared_role[19] = 40
    shared_role[20] = 50
    shared_role[21] = 30
    shared_role[40] = 0
    shared_role[41] = 10
    shared_role[42] = 100
    shared_role[49] = 5
    shared_role[54] = 20
    shared_item_19 = shared_item_effect_vector(
        item_records[19], seed=1, role_words=shared_role
    )
    display_role = list(shared_role)
    display_role[58] = 4
    shared_item_91 = shared_item_effect_vector(
        item_records[91], seed=1, role_words=display_role
    )
    shared_item_40 = shared_item_effect_vector(
        item_records[40], seed=1, role_words=shared_role
    )
    if poisoned_throw["item_id"] != 102 or poisoned_throw["item_type"] != 4:
        raise ValueError("RANGER.GRP item 102 is not the expected throwing weapon")
    if plain_throw["item_id"] != 96 or plain_throw["item_type"] != 4:
        raise ValueError("RANGER.GRP item 96 is not the expected throwing weapon")
    if (ai_stale_payload_throw["item_id"], ai_stale_payload_throw["payload_item_id"]) != (102, 96):
        raise ValueError("RANGER.GRP stale AI throwing-weapon vector changed identity")
    if shared_item_19["item_id"] != 19 or shared_item_19["item_type"] != 3:
        raise ValueError("RANGER.GRP item 19 is not the expected shared-use item")
    if shared_item_91["item_id"] != 91 or shared_item_40["item_id"] != 40:
        raise ValueError("RANGER.GRP shared item bug vectors changed identity")

    war_bytes = (data_root / "WAR.STA").read_bytes()
    if len(war_bytes) % WAR_RECORD_SIZE != 0:
        raise ValueError("WAR.STA is not a whole number of 186-byte records")
    war_records = [
        war_bytes[offset:offset + WAR_RECORD_SIZE]
        for offset in range(0, len(war_bytes), WAR_RECORD_SIZE)
    ]
    setup_records = [
        battle_setup_record(battle_id, record)
        for battle_id, record in enumerate(war_records)
    ]
    for record in setup_records:
        for write in record["static_occupancy_writes"]:
            if not (0 <= int(write["x"]) < 64 and 0 <= int(write["y"]) < 64):
                raise ValueError(
                    f"battle {record['battle_id']} has out-of-range static coordinate {write}"
                )
        if int(record["static_combatant_count"]) > 26:
            raise ValueError(f"battle {record['battle_id']} exceeds 26 combatant slots")

    fight_ids: list[int] = []
    fight_packages: list[dict[str, object]] = []
    for index_path in sorted(data_root.glob("FIGHT*.IDX"), key=lambda path: path.name.upper()):
        match = FIGHT_PATTERN.match(index_path.name)
        if match is None:
            continue
        fight_id = int(match.group("id"))
        group_path = data_root / f"FIGHT{fight_id:03d}.GRP"
        if not group_path.is_file():
            raise ValueError(f"missing {group_path.name}")
        record = archive_record(index_path, group_path)
        fight_ids.append(fight_id)
        fight_packages.append({"id": fight_id, **record})
    if len(fight_packages) != 92:
        raise ValueError(f"expected 92 FIGHT packages, got {len(fight_packages)}")

    warfld_index_bytes = (data_root / "WARFLD.IDX").read_bytes()
    warfld_group_bytes = (data_root / "WARFLD.GRP").read_bytes()
    warfld_entries = cumulative_entries(warfld_index_bytes, warfld_group_bytes)
    warfld = archive_record(data_root / "WARFLD.IDX", data_root / "WARFLD.GRP")
    if warfld["entry_count"] != 26:
        raise ValueError(f"expected 26 WARFLD entries, got {warfld['entry_count']}")

    movement_setup = setup_records[4]
    movement_field_bytes = warfld_entries[int(movement_setup["battlefield_id"])][:16384]
    movement_field_words = list(struct.unpack("<8192h", movement_field_bytes))
    movement_occupied = {
        int(write["occupancy_index"])
        for write in movement_setup["static_occupancy_writes"]
    }
    movement_vectors = ai_movement_vectors(movement_field_words, movement_occupied)
    cursor_vectors = player_cursor_vectors(movement_field_words, movement_occupied)
    post_battle_vectors = post_battle_progression_vectors(
        struct.unpack_from("<h", war_records[2], 7 * 2)[0]
    )

    pathing_records: list[dict[str, object]] = []
    for battle_id in (0, 93):
        setup = setup_records[battle_id]
        party = next(
            write for write in setup["static_occupancy_writes"] if write["side"] == "party"
        )
        enemy = next(
            write for write in setup["static_occupancy_writes"] if write["side"] == "enemy"
        )
        source = (int(party["x"]), int(party["y"]))
        target = (int(enemy["x"]), int(enemy["y"]))
        field_bytes = warfld_entries[int(setup["battlefield_id"])][:16384]
        field_words = list(struct.unpack("<8192h", field_bytes))
        movement = build_path_map(field_words, source, "movement")
        movement_source_occupied = build_path_map(
            field_words,
            source,
            "movement",
            {source[1] * 64 + source[0]},
        )
        occupied_coordinate = (source[0] + 1, source[1])
        movement_occupied = build_path_map(
            field_words,
            source,
            "movement",
            {occupied_coordinate[1] * 64 + occupied_coordinate[0]},
        )
        targeting = build_path_map(field_words, source, "targeting")
        targeting_occupied = build_path_map(
            field_words,
            source,
            "targeting",
            {occupied_coordinate[1] * 64 + occupied_coordinate[0]},
        )
        targeting_before_mark = fnv1a_words(targeting)
        targeting_reachable_cells = sum(1 for value in targeting if 0 <= value < 128)
        targeting_blocked_source_index = next(
            index
            for index in range(4096)
            if field_words[4096 + index] != 0
        )
        targeting_blocked_source = (
            targeting_blocked_source_index % 64,
            targeting_blocked_source_index // 64,
        )
        targeting_from_blocked_source = build_path_map(
            field_words, targeting_blocked_source, "targeting"
        )
        path_marked = mark_path(targeting, source, target)
        first_marked_step = next(
            (
                [source[0] + dx, source[1] + dy]
                for dx, dy in PATH_DIRECTIONS
                if 0 <= source[0] + dx < 64
                and 0 <= source[1] + dy < 64
                and targeting[(source[1] + dy) * 64 + source[0] + dx] == 250
            ),
            None,
        )
        pathing_records.append(
            {
                "battle_id": battle_id,
                "source": list(source),
                "target": list(target),
                "movement_hash": fnv1a_words(movement),
                "source_occupied_hash": fnv1a_words(movement_source_occupied),
                "occupied_coordinate": list(occupied_coordinate),
                "movement_occupied_hash": fnv1a_words(movement_occupied),
                "targeting_hash": targeting_before_mark,
                "targeting_reachable_cells": targeting_reachable_cells,
                "targeting_occupied_hash": fnv1a_words(targeting_occupied),
                "targeting_occupied_value": targeting_occupied[
                    occupied_coordinate[1] * 64 + occupied_coordinate[0]
                ],
                "targeting_blocked_source": list(targeting_blocked_source),
                "targeting_blocked_source_hash": fnv1a_words(targeting_from_blocked_source),
                "targeting_marked": path_marked,
                "targeting_marked_hash": fnv1a_words(targeting),
                "first_marked_step": first_marked_step,
                "target_distance": build_path_map(field_words, source, "targeting")[
                    target[1] * 64 + target[0]
                ],
            }
        )

    targeting_ground_setup = setup_records[89]
    if int(targeting_ground_setup["battlefield_id"]) != 13:
        raise ValueError("battle89 targeting ground-only fixture battlefield changed")
    targeting_ground_words = list(struct.unpack("<8192h", warfld_entries[13][:16384]))
    targeting_ground_source = (25, 18)
    targeting_ground_coordinate = (26, 18)
    targeting_ground_index = targeting_ground_coordinate[1] * 64 + targeting_ground_coordinate[0]
    if targeting_ground_words[targeting_ground_index] != 0x0166:
        raise ValueError("battle89 targeting ground-only fixture tile changed")
    if targeting_ground_words[4096 + targeting_ground_index] != 0:
        raise ValueError("battle89 targeting ground-only fixture upper layer changed")
    targeting_ground_movement = build_path_map(
        targeting_ground_words, targeting_ground_source, "movement"
    )
    targeting_ground_targeting = build_path_map(
        targeting_ground_words, targeting_ground_source, "targeting"
    )
    targeting_ground_vector = {
        "battle_id": 89,
        "battlefield_id": 13,
        "source": list(targeting_ground_source),
        "coordinate": list(targeting_ground_coordinate),
        "ground_tile": targeting_ground_words[targeting_ground_index],
        "upper_layer": targeting_ground_words[4096 + targeting_ground_index],
        "movement_value": targeting_ground_movement[targeting_ground_index],
        "movement_hash": fnv1a_words(targeting_ground_movement),
        "targeting_value": targeting_ground_targeting[targeting_ground_index],
        "targeting_hash": fnv1a_words(targeting_ground_targeting),
    }

    x64_alias_words = list(struct.unpack("<8192h", warfld_entries[0][:16384]))
    x64_alias_source = (64, 0)
    x64_alias_targeting = build_path_map(x64_alias_words, x64_alias_source, "targeting")
    x64_alias_vector = {
        "battle_id": 0,
        "battlefield_id": 0,
        "source": list(x64_alias_source),
        "aliased_coordinate": [0, 1],
        "source_value": x64_alias_targeting[64],
        "aliased_value": x64_alias_targeting[64],
        "targeting_hash": fnv1a_words(x64_alias_targeting),
    }

    battle_session = battle_session_vector(
        data_root,
        list(struct.unpack("<8192h", warfld_entries[int(setup_records[2]["battlefield_id"])][:16384])),
    )
    render_plan = battle_render_plan_vector(
        data_root,
        list(struct.unpack("<8192h", warfld_entries[int(setup_records[4]["battlefield_id"])][:16384])),
        setup_records[4],
    )
    battle3_field_words = list(
        struct.unpack("<8192h", warfld_entries[int(setup_records[3]["battlefield_id"])][:16384])
    )
    escape_vector = ai_escape_vector(battle3_field_words)
    attack_target_vectors = ai_attack_target_vectors(battle3_field_words)
    attack_handler_vectors = ai_attack_handler_vectors(z_dat_bytes, battle3_field_words)
    poison_handler_vectors = ai_poison_handler_vectors(battle3_field_words)
    item_handler_vectors = ai_item_handler_vectors(battle3_field_words)

    return {
        "format": "openlegend-b8-battle-goldens-v1",
        "battle_entry": battle_entry_contract(z_dat_bytes),
        "battle_data_loader": battle_data_loader_contract(z_dat_bytes),
        "battle_setup_machine": battle_setup_machine_contract(z_dat_bytes),
        "battle_rest_wrapper_machine": battle_rest_wrapper_contract(z_dat_bytes),
        "battle_escape_plan_machine": battle_escape_plan_contract(z_dat_bytes),
        "battle_ai_attack_handler_machine": battle_ai_attack_handler_contract(z_dat_bytes),
        "battle_ai_attack_target_machine": battle_ai_attack_target_contract(z_dat_bytes),
        "battle_ai_strongest_target_machine": battle_ai_strongest_target_contract(z_dat_bytes),
        "battle_ai_weakest_target_machine": battle_ai_weakest_target_contract(z_dat_bytes),
        "battle_ai_specialist_target_machine": battle_ai_specialist_target_contract(z_dat_bytes),
        "battle_ai_nearest_target_machine": battle_ai_nearest_target_contract(z_dat_bytes),
        "battle_ai_poison_handler_machine": battle_ai_poison_handler_contract(z_dat_bytes),
        "battle_ai_poison_target_machine": battle_ai_poison_target_contract(z_dat_bytes),
        "battle_ai_strongest_poison_target_machine":
            battle_ai_strongest_poison_target_contract(z_dat_bytes),
        "battle_ai_first_poison_target_machine":
            battle_ai_first_poison_target_contract(z_dat_bytes),
        "battle_ai_item_handler_machine": battle_ai_item_handler_contract(z_dat_bytes),
        "battle_ai_throwing_handler_machine": battle_ai_throwing_handler_contract(z_dat_bytes),
        "battle_ai_item_executor_machine": battle_ai_item_executor_contract(z_dat_bytes),
        "battle_carried_item_remove_machine": battle_carried_item_remove_contract(z_dat_bytes),
        "battle_ai_request_medicine_machine": battle_ai_request_medicine_contract(z_dat_bytes),
        "battle_ai_request_detox_machine": battle_ai_request_detox_contract(z_dat_bytes),
        "battle_ai_support_medicine_machine": battle_ai_support_medicine_contract(z_dat_bytes),
        "battle_ai_support_detox_machine": battle_ai_support_detox_contract(z_dat_bytes),
        "battle_ai_movement_machine": battle_ai_movement_contract(z_dat_bytes),
        "battle_player_movement_machine": battle_player_movement_contract(z_dat_bytes),
        "battle_cursor_handler_machine": battle_cursor_handler_contract(z_dat_bytes),
        "battle_movement_path_machine": battle_movement_path_contract(z_dat_bytes),
        "battle_targeting_path_machine": battle_targeting_path_contract(z_dat_bytes),
        "battle_movement_initializer_machine": battle_movement_initializer_contract(z_dat_bytes),
        "battle_targeting_initializer_machine": battle_targeting_initializer_contract(z_dat_bytes),
        "battle_flood_step_machine": battle_flood_step_contract(z_dat_bytes),
        "battle_dequeue_machine": battle_dequeue_contract(z_dat_bytes),
        "battle_enqueue_machine": battle_enqueue_contract(z_dat_bytes),
        "battle_round_machine": battle_round_machine_contract(z_dat_bytes, ranger_group_bytes),
        "war_sta": {
            "record_size": WAR_RECORD_SIZE,
            "record_count": len(war_records),
            "file_size": len(war_bytes),
            "file_sha256": sha256(war_bytes),
            "record_sha256": [sha256(record) for record in war_records],
        },
        "warfld": warfld,
        "battle_setup": {
            "combatant_slot_count": 26,
            "combatant_words_per_slot": 14,
            "combatant_bytes_per_slot": 28,
            "initial_words": [-1, -1, 0, 0, 0, 0, 0, 0, 5098, 0, 0, -1, -1, 0],
            "sprite_word": {
                "role_head_word": 1,
                "archive_frame_offset": 0,
                "base": 5106,
                "empty_role_head_word": -1,
                "formula": "int16(8*role_head_word + 5106 + 2*initial_mode)",
            },
            "round_order": {
                "role_speed_word": 44,
                "role_equipment_words": [23, 24],
                "item_add_speed_word": 53,
                "effective_speed": "int16(role.speed + each equipped item.add_speed)",
                "sort": "signed descending; equal values do not swap",
                "swap_words": [0, 1, 2, 3, 4, 5, 6, 7, 9, 10, 11, 12, 13],
                "recomputed_word": 8,
                "occupancy_hidden_word": 5,
                "round_value_word": 6,
                "round_value": "max(0, effective_speed/15 - role.hurt/40), signed truncation toward zero",
                "vectors": {
                    "stable_equal": {
                        "input": [
                            {"role_id": 1, "speed": 10, "item_add_speed": 30},
                            {"role_id": 3, "speed": 40, "item_add_speed": 0},
                        ],
                        "expected_role_order": [1, 3],
                    },
                    "swap_hidden_round": {
                        "input_role_order": [3, 1],
                        "effective_speeds": [41, 50],
                        "hurt": [200, 40],
                        "hidden": [1, 0],
                        "expected_role_order": [1, 3],
                        "expected_round_values": [2, 0],
                        "expected_occupancy": {"party_26_24": 0, "enemy_26_26": -1},
                    },
                },
            },
            "result_codes": {
                "ongoing": 0,
                "no_party_alive": 1,
                "no_enemy_alive": 2,
                "both_empty": 2,
                "public_return": "raw_result - 1",
            },
            "attack_entry": {
                "learned_magic_rule": "count role magic ids > 0",
                "single_learned_slot_bug": {
                    "magic_ids": [0, 0, 5, 0, 0, 0, 0, 0, 0, 0],
                    "learned_count": 1,
                    "selected_slot": 0,
                    "selected_magic_id": 0,
                    "rng_consumed": False,
                },
                "magic_selection_vector": {
                    "magic_ids": [6, 0, 5, 0, 7, 0, 0, 0, 0, 0],
                    "current_mp": 6,
                    "need_mp": {"slot_0": 6, "slot_2": 4, "slot_4": 7},
                    "learned_count": 3,
                    "available_slots": [0, 2, -1, -1, -1, -1, -1, -1, -1, -1],
                    "available_count": 2,
                    "initial_cursor": 0,
                    "initial_state_hash": "0xc254d2cd83d7da76",
                    "cursor_after_next_next_previous": 1,
                    "selected_slot": 2,
                    "cancel_out_flag": 1,
                    "panel_width": "17*learned_count+10",
                },
                "profile_vector": {
                    "slot": 2,
                    "magic_id": 5,
                    "experience": 299,
                    "level_index": 2,
                    "select_distance": 7,
                    "attack_distance": 3,
                    "area_type": 2,
                    "hurt_type": 1,
                    "attack_count": 2,
                    "need_mp": 4,
                },
                "commit_vector": {
                    "rng_seed": 1,
                    "rng_bound": 2,
                    "rng_state_after": 1103527590,
                    "experience_after": 300,
                    "leveled": True,
                    "cost_scale": 3,
                    "mp_before": 3,
                    "mp_after": 0,
                    "action_done": 1,
                    "attack_counter_after": 2,
                    "physical_power_after_finish": 0,
                    "experience_cap": 999,
                },
                "hp_damage_vector": {
                    "attack": 30,
                    "magic_attack": 30,
                    "defence": 5,
                    "rng_bounds": [20, 20],
                    "rng_values": [18, 18],
                    "rng_state_after": 2524885223,
                    "distance": 1,
                    "damage": 30,
                    "cost_scale": 3,
                    "exact_hp_before": 30,
                    "exact_kill_bonus": 0,
                    "underkill_hp_before": 29,
                    "underkill_level": 4,
                    "underkill_attack_counter": 46,
                    "hurt_after": 3,
                    "poison_after": 4,
                    "fallback_vector": {
                        "allied_knowledge": 162,
                        "enemy_knowledge": 164,
                        "equipment_attack": 6,
                        "equipment_defence": 2,
                        "special_bonus": 3,
                        "rng_bounds": [20, 20, 4, 4],
                        "rng_values": [18, 18, 1, 3],
                        "rng_state_after": 3295386429,
                        "distance": 11,
                        "damage": 14,
                    },
                },
                "poison_vector": {
                    "use_poison": 80,
                    "targeting_range": 6,
                    "target_anti_poison": 20,
                    "target_poison_before": 90,
                    "raw_amount": 15,
                    "applied_amount": 9,
                    "target_poison_after": 99,
                    "direction_after": 3,
                    "effect_hash": "0xab559939923b4f74",
                    "effect_kind": 2,
                    "negative_amount_clamped": 0,
                    "maximum_amount": 99,
                    "action_done": 1,
                    "attack_counter_after": 1,
                    "physical_power_before": 1,
                    "physical_power_after": 0,
                    "empty_cell_marked": True,
                    "friendly_cell_marked": False,
                },
                "detox_vector": {
                    "detoxification": 80,
                    "targeting_range": 6,
                    "rng_seed": 1,
                    "rng_bounds": [10, 10],
                    "rng_outputs": [8, 8],
                    "rng_state_after": 2_524_885_223,
                    "target_poison_before": 90,
                    "raw_amount": 26,
                    "applied_amount": 26,
                    "target_poison_after": 64,
                    "direction_after": 3,
                    "effect_hash": "0xab559939923b4f74",
                    "effect_kind": 3,
                    "threshold_poison": 41,
                    "threshold_detoxification": 20,
                    "threshold_amount": 0,
                    "maximum_amount": 99,
                    "poison_100_after": 100,
                    "poison_101_after": 99,
                    "action_done": 1,
                    "attack_counter_after": 1,
                    "physical_power_before": 1,
                    "physical_power_after": 0,
                    "empty_cell_marked": True,
                    "enemy_cell_marked": False,
                },
                "medicine_vector": {
                    "medicine": 80,
                    "targeting_range": 6,
                    "rng_seed": 1,
                    "rng_bound": 5,
                    "rng_output": 3,
                    "rng_state_after": 1_103_527_590,
                    "target_hp_before": 100,
                    "target_maximum_hp": 200,
                    "target_hurt_before": 40,
                    "hurt_band_amounts": {"25": 67, "26": 63, "51": 56, "76": 43},
                    "healed_amount": 63,
                    "target_hp_after": 163,
                    "target_hurt_after": 0,
                    "threshold_medicine": 20,
                    "threshold_hurt": 41,
                    "threshold_healed_amount": 0,
                    "capped_healed_amount": 10,
                    "direction_after": 3,
                    "effect_hash": "0xab559939923b4f74",
                    "effect_kind": 4,
                    "action_done": 1,
                    "attack_counter_after": 1,
                    "physical_power_before": 51,
                    "physical_power_after_value": 49,
                    "physical_power_after_action": 47,
                    "low_physical_power": 49,
                    "low_physical_power_consumes_rng": False,
                    "empty_cell_marked": True,
                    "enemy_cell_marked": False,
                    "animation_effect_id": 0,
                    "damage_flash_suppressed": True,
                },
                "battle_item_vector": {
                    "menu_filter": {
                        "requested_type": 4,
                        "accepted_types": [3, 4],
                        "inventory_count_ignored": True,
                        "selected_type_4_opens_targeting": True,
                        "result_1_marks_action_done": True,
                    },
                    "poisoned_throw": poisoned_throw,
                    "plain_throw": plain_throw,
                    "targeting": {
                        "direction_after": 3,
                        "effect_hash": "0xab559939923b4f74",
                        "friendly_cell_marked": False,
                        "empty_cell_marked": True,
                        "friendly_consumes_item": False,
                        "empty_consumes_item": False,
                    },
                    "inventory": {
                        "before": [[102, 1], [97, 2], [10, 0], [-1, 0]],
                        "after_consuming_slot_0": [[97, 2], [10, 0], [-1, 0], [-1, 0]],
                        "decrement_wraps_to_int16": True,
                    },
                    "enemy_carried_removal": carried_item_removal_vectors(),
                    "ai_throwing": {
                        "poisoned_state_vector": ai_poisoned_throw,
                        "plain_state_vector": ai_plain_throw,
                        "party_stale_payload_vector": ai_stale_payload_throw,
                        "source_effect_and_payload_order": [
                            "source effect animation", "payload state commit",
                            "damage animation", "source consumption",
                        ],
                        "state_committed_after_effect_animation": True,
                        "effect_cell_marked": True,
                        "actor_direction_unchanged": True,
                        "action_done_before_outer_ai_finish": 0,
                        "party_inventory_before": [[102, 1], [97, 2]],
                        "party_inventory_after": [[97, 2], [-1, 0]],
                        "enemy_carried_before": [[102, 1], [97, 2], [-1, 0], [-1, 0]],
                        "enemy_carried_after": [[97, 2], [-1, 0], [-1, 0], [-1, 0]],
                    },
                    "ai_item_effect": {
                        "multi_effect_item_19": shared_item_19,
                        "display_only_item_91": shared_item_91,
                        "mp_type_item_40": shared_item_40,
                        "presentation": {
                            "panel_rect": [70, 18, 148],
                            "panel_height_formula": "20 * effect_count + 30",
                            "item_name_position": [75, 25],
                            "row_label_x": 75,
                            "row_sign_x": 155,
                            "row_value_x": 187,
                            "row_y_formula": "18 * visible_index + 45",
                            "battle_redraw_when_effect_count_positive": True,
                            "wait_for_input_when_effect_count_positive": False,
                            "post_effect_tick_changes": 340 // 40 + 1,
                        },
                        "effect_cell_marked": True,
                        "zero_effect_item_still_consumed": True,
                        "actor_direction_unchanged": True,
                        "action_done_before_outer_ai_finish": 0,
                        "party_inventory_before": [[19, 1], [2, 3]],
                        "party_inventory_after": [[2, 3], [-1, 0]],
                        "enemy_carried_before": [[19, 1], [2, 3], [-1, 0], [-1, 0]],
                        "enemy_carried_after": [[2, 3], [-1, 0], [-1, 0], [-1, 0]],
                    },
                },
                "ai_selector_vectors": ai_selector_vectors(),
                "ai_entry_vectors": ai_entry_vectors(),
                "ai_request_vectors": ai_request_vectors(),
                "ai_support_vectors": ai_support_vectors(),
                "ai_movement_vectors": movement_vectors,
                "player_cursor_vectors": cursor_vectors,
                "post_battle_progression_vectors": post_battle_vectors,
                "ai_escape_vector": escape_vector,
                "ai_attack_target_vectors": attack_target_vectors,
                "ai_attack_handler_vectors": attack_handler_vectors,
                "ai_poison_handler_vectors": poison_handler_vectors,
                "ai_item_handler_vectors": item_handler_vectors,
                "battle_session_vector": battle_session,
                "wait_auto_render_vector": {
                    "wait_order_before": [10, 20, 30, 40],
                    "wait_source_slot": 1,
                    "wait_order_after": [10, 30, 40, 20],
                    "wait_return_slot": 3,
                    "automatic_flag_after": 1,
                    "automatic_sequence": ["render", "present", "set_flag", "ai"],
                    "render_plan": render_plan,
                },
                "rest_vector": {
                    "ready": rest_vector(
                        seed=1,
                        speed=60,
                        round_value=6,
                        physical_power=50,
                        hp=95,
                        maximum_hp=100,
                        mp=48,
                        maximum_mp=50,
                    ),
                    "tired": rest_vector(
                        seed=1,
                        speed=60,
                        round_value=5,
                        physical_power=25,
                        hp=95,
                        maximum_hp=100,
                        mp=48,
                        maximum_mp=50,
                    ),
                },
                "mp_damage_vector": {
                    "rng_seed": 1,
                    "rng_bounds": [3, 3, 10, 3, 3],
                    "rng_values": [2, 1, 3, 1, 1],
                    "rng_state_after": 4182499122,
                    "add_mp": 20,
                    "hurt_mp": 15,
                    "actor_mp_after": 23,
                    "actor_max_mp_after": 23,
                    "target_mp_after": 35,
                    "drained": 15,
                },
                "area_vectors": {
                    "square": {
                        "scan_order": "x outer, y inner",
                        "center": [26, 26],
                        "radius": 1,
                        "friendly_skip": [25, 25],
                        "marked_cells": 8,
                        "effect_hash": "0xe5f47b0a810ce2bd",
                        "direction_after": 3,
                        "target_distance": 2,
                        "damage": 29,
                        "effect_kind": 1,
                    },
                    "cross": {
                        "scan_order": ["up", "down", "left", "right"],
                        "source": [26, 24],
                        "radius": 2,
                        "friendly_skip": [26, 25],
                        "marked_cells": 7,
                        "effect_hash": "0x3144c415023d9464",
                        "direction_unchanged": 2,
                        "hurt_type_ignored": 1,
                        "damage": 29,
                        "effect_kind": 1,
                    },
                    "mp_square": {
                        "center": [26, 26],
                        "radius": 0,
                        "marked_cells": 1,
                        "effect_hash": "0xab559939923b4f74",
                        "damage": 15,
                        "effect_kind": 3,
                        "last_hp_cost_scale_unchanged": 3,
                    },
                    "line": {
                        "direction_map": {
                            "0": [0, -1],
                            "1": [1, 0],
                            "2": [-1, 0],
                            "3": [0, 1],
                        },
                        "range": 2,
                        "effect_hash": "0xae7c1e4e161ac125",
                        "friendly_skip_hash": "0xab559939923b4f74",
                        "invalid_direction_hash": "0xb9d103fd6854a325",
                        "continues_after_friendly": True,
                        "hurt_type_ignored": 1,
                        "damage": 29,
                        "effect_kind": 1,
                    },
                },
                "animation": {
                    "effect_frame_table": {
                        "z_dat_file_offset": Z_DAT_EFFECT_FRAME_COUNTS_OFFSET,
                        "entry_count": len(effect_counts),
                        "bytes_sha256": sha256(effect_table_bytes),
                        "word_hash": fnv1a_words(effect_counts),
                        "values": effect_counts,
                    },
                    "ranger_magic_effect_ids": {
                        "record_count": len(magic_effect_ids),
                        "minimum": min(magic_effect_ids),
                        "maximum": max(magic_effect_ids),
                        "unique": sorted(set(magic_effect_ids)),
                    },
                    "vectors": animation_vectors(effect_counts),
                },
            },
            "pathing": {
                "directions": [[0, -1], [1, 0], [-1, 0], [0, 1]],
                "blocked_tile_ranges": [list(value) for value in BLOCKED_TILE_RANGES],
                "unvisited": 254,
                "blocked": 555,
                "marked": 250,
                "consumed": 255,
                "queue_slots": 255,
                "distance_modulus": 128,
                "movement_step": {
                    "source": [32, 20],
                    "marked_first_steps": [[31, 20], [30, 20]],
                    "slot": 0,
                    "role_head_word": 0,
                    "role_speed": 50,
                    "physical_power_before": 1,
                    "round_value_before": 5,
                    "first_direction": 2,
                    "first_sprite": 5110,
                    "physical_power_after_first": 0,
                    "round_value_after_first": 4,
                    "round_value_after_second": 3,
                    "stop_vectors": {
                        "destination_equal": True,
                        "destination_different": False,
                        "manhattan_13_in_range": True,
                        "manhattan_13_aligned": False,
                        "round_value_zero": True,
                    },
                    "consumed_value": 255,
                },
                "targeting_ground_only": targeting_ground_vector,
                "x64_alias": x64_alias_vector,
                "records": pathing_records,
            },
            "party_prefix_rule": "slot0 unconditional; first slot 1..5 with signed id <= 0 ends prefix; otherwise 6",
            "selection_states": {
                "unselected": 0,
                "selected": 1,
                "mandatory": 2,
                "confirm_index": "party_prefix_length",
            },
            "fixed_records": sum(
                record["active_party_source"] == "fixed" for record in setup_records
            ),
            "preset_records": sum(
                record["active_party_source"] == "preset" for record in setup_records
            ),
            "max_active_party_count": max(
                int(record["active_party_count"]) for record in setup_records
            ),
            "max_enemy_count": max(int(record["enemy_count"]) for record in setup_records),
            "max_static_combatant_count": max(
                int(record["static_combatant_count"]) for record in setup_records
            ),
            "records_with_duplicate_occupancy": [
                int(record["battle_id"])
                for record in setup_records
                if record["duplicate_occupancy_writes"]
            ],
            "battlefield_ids": sorted(
                {int(record["battlefield_id"]) for record in setup_records}
            ),
            "music_ids": sorted({int(record["music_id"]) for record in setup_records}),
            "records": setup_records,
        },
        "fight_packages": {
            "count": len(fight_packages),
            "ids": fight_ids,
            "missing_ids_between_min_max": sorted(
                set(range(min(fight_ids), max(fight_ids) + 1)) - set(fight_ids)
            ),
            "total_frames": sum(int(package["entry_count"]) for package in fight_packages),
            "packages": fight_packages,
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--data-root", type=Path, default=Path(".."))
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("research/evidence/battle-goldens.json"),
    )
    args = parser.parse_args()
    result = build(args.data_root)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print(f"wrote {args.output}")
    print(f"WAR records: {result['war_sta']['record_count']}")
    print(f"WARFLD entries: {result['warfld']['entry_count']}")
    print(f"FIGHT packages: {result['fight_packages']['count']}")
    print(f"FIGHT frames: {result['fight_packages']['total_frames']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
