# B9 persistence and compatibility integration evidence

## Status

- B9 functional implementation: `implemented_pending_review`.
- Finite closure: `research/inventory/persistence-closure.tsv`, 14 machine-code boundary functions.
- Final bidirectional assembly-to-C++ basic-block review: `not_started` for all 14 entries.
- This document records implementation evidence and identified integration differences. It does not claim `assembly_exact` or `platform_adapted` closure.

## Primary truth sources

- Original executables: current `Z.COM` and `Z.DAT` bytes in the game data root.
- Read-only formal database: `research/ida/databases/Z_DAT.i64`.
- Review work used a byte-identical disposable database under `tmp/`; the formal database was not opened for mutation.
- Machine-code packets: `tmp/b9-review/manifest.tsv` and `tmp/b9-review/0x*.txt`.
- Existing transport evidence: `research/evidence/model-persistence-1to1.md` and `research/ida/reports/Z_DAT.persistence_xrefs.txt`.

## Finite B9 boundary set

The closure is derived from the complete observable title/new/load/save/system-menu/scene-menu/ending call boundary, not from a raw xref owner set.

| Boundary | Machine responsibility | Modern owner |
|---|---|---|
| `main` | process initialization, title/world/scene dispatch, shared shutdown | app/platform |
| `sub_20FAF` | title new/load/exit flow | app + title UI |
| `sub_24A02` | imported snapshot to live runtime state | app + model |
| `sub_24C8D` | shared resource shutdown | app/platform |
| `sub_25AB7` | baseline scene/event working-state reset | persistence + model |
| `sub_25D0E` | in-game system menu | UI + app |
| `sub_25F87` | numbered-slot menu and wait frame | UI + app |
| `sub_26208` | numbered-slot load | persistence + model + app |
| `sub_265AB` | numbered-slot write | model + persistence + app |
| `sub_26B5E` | post-title new-game session and prologue entry | app + scene |
| `sub_2711A` | name and attribute roll/accept flow | UI + model |
| `sub_2EB49` | scene load/quit menu | scene + app |
| `sub_30C3D` | ending and credits | scene |
| `sub_31241` | ending shutdown and terminal messages | app/platform |

Detailed source-unit mappings and verification state are recorded row-by-row in `research/inventory/persistence-closure.tsv`.

## Snapshot transport and import contract

The numbered-slot contract remains the existing fixed-size `R*.GRP`, `S*.GRP`, and `D*.GRP` transport documented in `model-persistence-1to1.md`.

The reviewed machine order for a successful load is:

1. read the numbered scene-map record (`S*`);
2. read the numbered scene-event record (`D*`);
3. read the numbered ranger record (`R*`) using the shared ranger index;
4. import ranger/header/team/inventory/role/item state;
5. rebuild the active destination session from imported header state.

The modern path is split across `persistence::load_numbered_slot`, `model::GameState::import_snapshot`, and `app::LegacyGameRuntime::perform_pending_io`. A load failure is reported without committing a partial `GameSnapshot` to the active runtime.

The reviewed machine order for a save is:

1. write the active scene-map record (`S*`);
2. write the active scene-event record (`D*`);
3. copy live runtime/header/team/inventory values to the ranger save buffer;
4. write the numbered ranger record (`R*`).

The modern path exports one complete snapshot before `persistence::write_numbered_slot` performs the same `S -> D -> R` fixed-record writes.

## Confirmed integration differences repaired during B9

### New-game attribute maximum-HP formula

The exact instruction sequence after `increased_life = bounded(5) + 3` is `imul eax, 3`, then signed 16-bit multiplication by the role level, then `add eax, 29`. The observable formula is therefore `maximum_hp = increased_life * 3 * level + 29`. The earlier evidence and C++ implementation omitted the factor `3`; both the implementation and independent B5 golden generator now retain it. RNG consumption remains 17 calls.

### New-game prologue entry

The original does not return to the ordinary world session after attribute acceptance. It resets/imports the baseline and enters the prologue directly with these observable values:

- scene id `70`;
- scene position `(19, 20)`;
- direction `1` (`right`);
- initial player picture `6890`;
- view origin `(8, 9)`;
- first event `691` after fade-from-black;
- first synchronous event output: dialogue `2520`, head `0`, style `1`.

`scene::SceneEntryOverride` now carries coordinates, direction, initial player picture, and script id. `LegacyGameRuntime::begin_new_game` uses the full prologue entry contract. The ordinary scene-title path is skipped before event 691, matching the reviewed entry blocks.

### Numbered-slot wait-frame boundary

The original slot confirmation renders a centered wait box and performs an actual video present before disk I/O begins. The machine string bytes decode to the legacy text `请稍候`; geometry and color are:

- box `(154, 18, 68, 31)`;
- text origin `(158, 25)`;
- text color `0x0705`.

`BasicUiRenderer::render_io_wait` emits that frame. `GameMenuController` retains the slot page until `complete_slot_operation()`. `LegacyGameRuntime::finish_presented_tick` opens the pending-I/O gate only after the wait frame has been presented. Regression coverage mutates the live snapshot, confirms the numbered-slot bytes are unchanged before present, and confirms the complete snapshot is written after present.

### System-menu load return boundary

The reviewed call chain is `sub_25D0E -> sub_25F87 -> sub_26208`. `sub_26208` imports and redraws the loaded world, then returns through the slot-menu epilogue. The slot menu closes, but the outer system-menu loop remains active with selection `0`; the loaded world is therefore visible behind the system menu. It does not return directly to unrestricted world control.

`LegacyGameRuntime::perform_pending_io` now preserves that caller-specific continuation: title and scene loads enter the loaded world directly, while a system-menu load rebuilds the world session and returns to `LegacyGameView::game_menu` on the system page. Regression coverage mutates the live state after saving, loads from the system menu, verifies full snapshot restoration, and verifies that the system menu remains active.

## Menu, quit, ending, and cleanup boundaries

- The system menu retains separate load, save, and exit paths plus uppercase-`Y` exit confirmation.
- The scene menu retains its machine-code four-item layout and the original load-selection up-wrap from item `0` to item `2`.
- Title load, system load, and scene load converge on the same numbered-slot transport/import boundary while retaining their caller-specific destination handling.
- Scene quit, system quit, title exit, and ending shutdown converge on shared owned-resource release in the modern process lifetime.
- Ending completion keeps ending-specific terminal output before normal process exit.

The original shared shutdown routine explicitly releases five global resource handles. The modern port does not reproduce raw DOS handles; the equivalent resources are owned objects whose destructors run when their runtime/session owners are reset or when `main` exits. This remains `implemented_pending_review` until the final platform-adaptation review closes every release edge.

## Per-launch diagnostic log compatibility requirement

This is a modern operational requirement, not an original DOS behavior claim.

- `logging.path` supplies the directory, filename prefix, and extension template.
- Each SDL process resolves one independent `PREFIX-YYYY-MM-DD_HH-MM-SS-{PID}.log` path.
- SDL3's official `CategoryProcess` and `SDL_GetProcessProperties` documentation covers SDL-created subprocesses; `SDL_PROP_PROCESS_PID_NUMBER` is a property of an `SDL_Process`, and there is no current-process-ID accessor. The platform entry therefore uses `GetCurrentProcessId()` on Windows and `getpid()` on Linux/macOS.
- The actual resolved session path is written in the startup record; separate launches no longer append to one fixed `openlegend.log`.

Official SDL references:

- <https://wiki.libsdl.org/SDL3/CategoryProcess>
- <https://wiki.libsdl.org/SDL3/SDL_GetProcessProperties>

## Current verification

Completed after the readable PID log name, full prologue entry, system-menu load continuation, and maximum-HP formula corrections:

- Linux ASan+UBSan `./build.sh app --config Debug --sanitizers`: 14/14 tests passed.
- Latest ordinary `core/app × Debug/Release` matrix: Linux 4/4 and Windows 4/4 BUILD-script invocations passed.
- Two consecutive Linux SDL smoke launches produced distinct paths matching `openlegend-YYYY-MM-DD_HH-MM-SS-{PID}.log`; the Windows app smoke produced the same timestamp/PID shape.
- Independent B5 golden generation was byte-identical across two runs and matched the tracked golden.
- Original `Z.COM`, `Z.DAT`, `WAR.STA`, `WARFLD.IDX`, and `WARFLD.GRP` hashes remained unchanged.
- The B9 closure audit confirmed 14/14 implementation mappings and manually reviewed module owners; all 14 final reviews remain `not_started`.

The affected tests now cover:

- deterministic session-log filename construction;
- distinct PID suffixes for the same timestamp;
- 17-call attribute RNG and the `increased_life * 3 * level + 29` maximum-HP formula;
- scene 70 coordinates, direction, initial picture, view origin, fade/script order, and first dialogue;
- runtime snapshot header values after new-game entry;
- no numbered-slot disk mutation before the wait-frame present boundary;
- full numbered-slot snapshot write after that boundary;
- successful system-menu load restores the complete snapshot while retaining the system menu over the loaded world.

No runtime sanitizer finding or BUILD/test failure remains in this implementation slice.

## Closure rule

All 14 rows remain `implemented_pending_review`. After B9 implementation is stable, each function must be reviewed independently from machine instructions, then assembly-to-C++ and C++-to-assembly by basic block. Any difference restarts that function review from its entry. Only a complete zero-new-difference pass can advance a row beyond `implemented_pending_review`.
