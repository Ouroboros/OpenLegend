#!/usr/bin/env python3

from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

CMAKE_VERSION = "3.31.10"
NINJA_VERSION = "1.13.0"
EXPECTED_GENERATOR = "Ninja Multi-Config"


def positive_integer(value: str) -> int:
    result = int(value)
    if result < 1:
        raise argparse.ArgumentTypeError("value must be a positive integer")
    return result


def environment_jobs(name: str, fallback: int) -> int:
    value = os.environ.get(name)
    if value is None:
        return fallback
    return positive_integer(value)


def normalize_target(target: str) -> str:
    return "app" if target == "sdl" else target


def validate_configuration(platform_name: str, config: str, sanitizers: bool) -> None:
    if platform_name == "windows" and sanitizers and config == "Debug":
        raise RuntimeError(
            "Upstream LLVM AddressSanitizer does not support the Windows Debug CRT; "
            "use --config Release --sanitizers"
        )


def project_root_path(script_file: Path) -> Path:
    if configured := os.environ.get("OPENLEGEND_PROJECT_ROOT"):
        return Path(configured)
    return script_file.resolve().parent


def game_data_root_path(project_root: Path, configured: str | None) -> Path:
    candidate = Path(configured) if configured else project_root.parent
    if not candidate.is_absolute():
        candidate = project_root / candidate
    return candidate.resolve()


def validate_game_data_root(game_data_root: Path) -> None:
    if not game_data_root.is_dir():
        raise RuntimeError(f"Game data directory was not found: {game_data_root}")
    missing = [name for name in ("Z.COM", "Z.DAT") if not (game_data_root / name).is_file()]
    if missing:
        raise RuntimeError(
            f"Game data directory is missing required file(s): {', '.join(missing)}"
        )


def compiler_commands(platform_name: str) -> tuple[str, str]:
    if platform_name == "windows":
        return os.environ.get("CC", "clang.exe"), os.environ.get("CXX", "clang++.exe")
    return os.environ.get("CC", "clang-23"), os.environ.get("CXX", "clang++-23")


def is_clang_command(command: str, cxx: bool = False) -> bool:
    name = Path(command).name.casefold()
    stem = "clang++" if cxx else "clang"
    executable_names = {stem, stem + ".exe"}
    return name in executable_names or name.startswith(stem + "-")


def validate_compiler_environment(
    platform_name: str, target: str, c_compiler: str, cxx_compiler: str
) -> None:
    if not is_clang_command(cxx_compiler, cxx=True):
        raise RuntimeError(
            f"OpenLegend {platform_name} builds require Clang; CXX={cxx_compiler}"
        )
    if target == "app" and not is_clang_command(c_compiler):
        raise RuntimeError(
            f"OpenLegend {platform_name} app builds require Clang; CC={c_compiler}"
        )


def build_directory(
    project_root: Path, platform_name: str, target: str, sanitizers: bool
) -> Path:
    sanitizer_suffix = "-asan" if sanitizers else ""
    return project_root / "build" / f"{platform_name}-{target}{sanitizer_suffix}"


def parse_args(arguments: list[str] | None = None) -> argparse.Namespace:
    processor_count = max(1, os.cpu_count() or 1)
    parser = argparse.ArgumentParser(description="Configure, build, and test OpenLegend with Ninja")
    parser.add_argument(
        "target", type=str.lower, choices=("core", "app", "sdl"), nargs="?", default="core"
    )
    parser.add_argument("--config", choices=("Debug", "Release"))
    parser.add_argument(
        "--data-dir",
        metavar="PATH",
        default=os.environ.get("OPENLEGEND_GAME_DATA_ROOT"),
        help=(
            "original game data directory; relative paths are resolved from the repository root "
            "(default: repository parent, environment: OPENLEGEND_GAME_DATA_ROOT)"
        ),
    )
    parser.add_argument(
        "--jobs",
        type=positive_integer,
        default=environment_jobs("OPENLEGEND_BUILD_JOBS", processor_count),
    )
    parser.add_argument(
        "--test-jobs",
        type=positive_integer,
        default=environment_jobs("OPENLEGEND_TEST_JOBS", processor_count),
    )
    parser.add_argument("--configure-only", action="store_true")
    parser.add_argument("--skip-tests", action="store_true")
    parser.add_argument("--sanitizers", action="store_true")
    result = parser.parse_args(arguments)
    if result.config is None:
        result.config = "Release" if result.sanitizers else "Debug"
    return result


def executable_name(name: str) -> str:
    return name + (".exe" if os.name == "nt" else "")


def find_tool(root: Path, name: str) -> Path | None:
    candidates = [
        root / "cmake" / "data" / "bin" / executable_name(name),
        root / "bin" / executable_name(name),
        root / "Scripts" / executable_name(name),
        root / executable_name(name),
    ]
    return next((candidate for candidate in candidates if candidate.is_file()), None)


def ensure_tools(project_root: Path) -> tuple[Path, Path, Path]:
    overrides = {
        "cmake": os.environ.get("OPENLEGEND_CMAKE"),
        "ninja": os.environ.get("OPENLEGEND_NINJA"),
        "ctest": os.environ.get("OPENLEGEND_CTEST"),
    }
    if any(overrides.values()):
        if not all(overrides.values()):
            raise RuntimeError(
                "OPENLEGEND_CMAKE, OPENLEGEND_NINJA, and OPENLEGEND_CTEST must be set together"
            )
        configured = tuple(Path(overrides[name]) for name in ("cmake", "ninja", "ctest"))
        missing = [str(path) for path in configured if not path.is_file()]
        if missing:
            raise RuntimeError("Configured build tool was not found: " + ", ".join(missing))
        return configured

    suffix = "windows" if os.name == "nt" else "linux"
    tool_root = project_root / ".tools" / f"python-{suffix}"
    search_roots = [tool_root]
    if os.name != "nt":
        search_roots.append(project_root / ".tools" / "python")
    for candidate_root in search_roots:
        cmake = find_tool(candidate_root, "cmake")
        ninja = find_tool(candidate_root, "ninja")
        ctest = find_tool(candidate_root, "ctest")
        if cmake and ninja and ctest:
            return cmake, ninja, ctest

    tool_root.mkdir(parents=True, exist_ok=True)
    command = [
        sys.executable,
        "-m",
        "pip",
        "install",
        "--disable-pip-version-check",
        "--no-input",
        "--target",
        str(tool_root),
        f"cmake=={CMAKE_VERSION}",
        f"ninja=={NINJA_VERSION}",
    ]
    print("+", subprocess.list2cmdline(command), flush=True)
    subprocess.run(command, check=True)

    cmake = find_tool(tool_root, "cmake")
    ninja = find_tool(tool_root, "ninja")
    ctest = find_tool(tool_root, "ctest")
    if not cmake or not ninja or not ctest:
        raise RuntimeError(f"CMake/Ninja installation is incomplete under {tool_root}")
    return cmake, ninja, ctest


def run(
    command: list[str], cwd: Path, environment: dict[str, str] | None = None
) -> None:
    print("+", subprocess.list2cmdline(command), flush=True)
    subprocess.run(command, cwd=cwd, check=True, env=environment)


def sanitizer_runtime_directory(cxx_compiler: str | None) -> Path:
    if not cxx_compiler:
        raise RuntimeError("CXX is required to locate the Windows sanitizer runtime")
    result = subprocess.run(
        [cxx_compiler, "-print-resource-dir"],
        check=True,
        capture_output=True,
        text=True,
    )
    resource_directory = Path(result.stdout.strip())
    runtime_directory = resource_directory / "lib" / "windows"
    if not any(runtime_directory.glob("clang_rt.asan_dynamic-*.dll")):
        raise RuntimeError(
            f"Clang AddressSanitizer runtime was not found under {runtime_directory}"
        )
    return runtime_directory


def sanitizer_test_environment(
    cxx_compiler: str | None,
    environment: dict[str, str],
    runtime_directory: Path | None = None,
) -> dict[str, str]:
    runtime_directory = runtime_directory or sanitizer_runtime_directory(cxx_compiler)
    result = dict(environment)
    result["PATH"] = str(runtime_directory) + os.pathsep + result.get("PATH", "")
    return result


def configuration_executables(build_dir: Path, config: str) -> list[Path]:
    configuration = config.casefold()
    return sorted(
        executable
        for executable in build_dir.rglob("*.exe")
        if configuration
        in {part.casefold() for part in executable.relative_to(build_dir).parts[:-1]}
    )


def remove_windows_sanitizer_runtimes(build_dir: Path, config: str) -> list[Path]:
    executable_directories = {
        path.parent for path in configuration_executables(build_dir, config)
    }
    removed = []
    for directory in executable_directories:
        for runtime_file in directory.glob("clang_rt.asan_dynamic-*.dll"):
            runtime_file.unlink()
            removed.append(runtime_file)
    return sorted(removed)


def stage_windows_sanitizer_runtime(
    runtime_directory: Path, build_dir: Path, config: str
) -> list[Path]:
    executable_directories = sorted(
        {path.parent for path in configuration_executables(build_dir, config)}
    )
    if not executable_directories:
        raise RuntimeError(
            f"No Windows {config} executable outputs were found under {build_dir}"
        )

    remove_windows_sanitizer_runtimes(build_dir, config)
    runtime_files = sorted(runtime_directory.glob("clang_rt.asan_dynamic-*.dll"))
    if not runtime_files:
        raise RuntimeError(
            f"Clang AddressSanitizer runtime was not found under {runtime_directory}"
        )
    deployed = []
    for directory in executable_directories:
        for runtime_file in runtime_files:
            destination = directory / runtime_file.name
            shutil.copy2(runtime_file, destination)
            deployed.append(destination)
    return deployed


def application_outputs(build_dir: Path, config: str) -> list[Path]:
    expected_name = executable_name("openlegend").casefold()
    configuration = config.casefold()
    return sorted(
        path
        for path in build_dir.rglob("*")
        if path.is_file()
        and path.name.casefold() == expected_name
        and configuration
        in {part.casefold() for part in path.relative_to(build_dir).parts[:-1]}
    )


def configure_command(
    cmake: Path,
    ninja: Path,
    project_root: Path,
    build_dir: Path,
    target: str,
    cxx_compiler: str | None = None,
    c_compiler: str | None = None,
    python_executable: str = sys.executable,
    enable_sanitizers: bool = False,
    game_data_root: Path | None = None,
) -> list[str]:
    command = [
        str(cmake),
        "-S",
        str(project_root),
        "-B",
        str(build_dir),
        "-G",
        EXPECTED_GENERATOR,
        f"-DCMAKE_MAKE_PROGRAM:FILEPATH={ninja}",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=ON",
        "-DCMAKE_CXX_EXTENSIONS:BOOL=OFF",
        f"-DPython3_EXECUTABLE:FILEPATH={python_executable}",
        "-DBUILD_TESTING:BOOL=ON",
        f"-DOPENLEGEND_BUILD_APP:BOOL={'ON' if target == 'app' else 'OFF'}",
        "-DOPENLEGEND_FETCH_SDL3:BOOL=ON",
        "-DOPENLEGEND_FETCH_TOMLPLUSPLUS:BOOL=ON",
        f"-DOPENLEGEND_ENABLE_SANITIZERS:BOOL={'ON' if enable_sanitizers else 'OFF'}",
    ]
    if game_data_root is not None:
        command.append(f"-DOPENLEGEND_GAME_DATA_ROOT:PATH={game_data_root}")
    if cxx_compiler:
        command.append(f"-DCMAKE_CXX_COMPILER:FILEPATH={cxx_compiler}")
    if target == "app" and c_compiler:
        command.append(f"-DCMAKE_C_COMPILER:FILEPATH={c_compiler}")
    return command


def cached_value(cache_file: Path, name: str) -> str | None:
    if not cache_file.is_file():
        return None
    prefix = f"{name}:"
    for line in cache_file.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith(prefix) and "=" in line:
            return line.split("=", 1)[1]
    return None


def cached_generator(cache_file: Path) -> str | None:
    return cached_value(cache_file, "CMAKE_GENERATOR")


def cached_home_directory(cache_file: Path) -> str | None:
    return cached_value(cache_file, "CMAKE_HOME_DIRECTORY")


def cached_bool(cache_file: Path, name: str) -> bool | None:
    if not cache_file.is_file():
        return None
    prefix = f"{name}:BOOL="
    for line in cache_file.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith(prefix):
            return line.removeprefix(prefix).upper() == "ON"
    return None


def resolved_tool_path(command: str) -> str:
    located = shutil.which(command)
    return os.path.normcase(str(Path(located or command).resolve()))


def compiler_path_changed(current: str | None, requested: str | None) -> bool:
    return bool(
        current
        and requested
        and resolved_tool_path(current) != resolved_tool_path(requested)
    )


def cached_path_changed(current: str | None, requested: Path) -> bool:
    return current is None or os.path.normcase(str(Path(current).resolve())) != os.path.normcase(
        str(requested.resolve())
    )


def reset_build_directory(build_dir: Path) -> None:
    configurations = [
        (path.relative_to(build_dir), path.read_bytes())
        for path in build_dir.glob("src/platform/sdl3/*/openlegend.toml")
        if path.is_file()
    ]
    shutil.rmtree(build_dir)
    for relative_path, contents in configurations:
        destination = build_dir / relative_path
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(contents)


def main() -> int:
    args = parse_args()
    project_root = project_root_path(Path(__file__))
    game_data_root = game_data_root_path(project_root, args.data_dir)
    if not args.skip_tests and not args.configure_only:
        validate_game_data_root(game_data_root)
    print(f"[OpenLegend] Game data: {game_data_root}", flush=True)
    cmake, ninja, ctest = ensure_tools(project_root)

    target = normalize_target(args.target)
    if args.target == "sdl":
        print("[OpenLegend] 'sdl' is a compatibility alias; prefer 'app'.", flush=True)

    platform_name = "windows" if os.name == "nt" else "linux"
    validate_configuration(platform_name, args.config, args.sanitizers)
    requested_c_compiler, requested_cxx_compiler = compiler_commands(platform_name)
    validate_compiler_environment(
        platform_name, target, requested_c_compiler, requested_cxx_compiler
    )
    configured_c_compiler = requested_c_compiler if target == "app" else None
    build_dir = build_directory(project_root, platform_name, target, args.sanitizers)
    cache_file = build_dir / "CMakeCache.txt"
    build_file = build_dir / "build.ninja"
    reconfigure = os.environ.get("OPENLEGEND_RECONFIGURE", "0") == "1"
    current_generator = cached_generator(cache_file)
    current_home = cached_home_directory(cache_file)
    current_make_program = cached_value(cache_file, "CMAKE_MAKE_PROGRAM")
    current_cxx_compiler = cached_value(cache_file, "CMAKE_CXX_COMPILER")
    current_c_compiler = cached_value(cache_file, "CMAKE_C_COMPILER")
    current_sanitizers = cached_bool(cache_file, "OPENLEGEND_ENABLE_SANITIZERS")
    current_game_data_root = cached_value(cache_file, "OPENLEGEND_GAME_DATA_ROOT")
    if current_generator is not None and current_generator != EXPECTED_GENERATOR:
        print(
            f"[OpenLegend] Reset generator: {current_generator} -> {EXPECTED_GENERATOR}",
            flush=True,
        )
        reset_build_directory(build_dir)
        reconfigure = True
    elif current_make_program is not None and (
        current_make_program.endswith("-NOTFOUND") or not Path(current_make_program).is_file()
    ):
        print(
            f"[OpenLegend] Reset invalid Ninja cache: {current_make_program}",
            flush=True,
        )
        reset_build_directory(build_dir)
        reconfigure = True
    elif os.name == "nt" and current_home is not None and "~" in current_home:
        print(
            f"[OpenLegend] Reset legacy 8.3 source path: {current_home}",
            flush=True,
        )
        reset_build_directory(build_dir)
        reconfigure = True
    elif compiler_path_changed(current_cxx_compiler, requested_cxx_compiler) or (
        target == "app" and compiler_path_changed(current_c_compiler, configured_c_compiler)
    ):
        print(
            f"[OpenLegend] Reset compiler: "
            f"{current_cxx_compiler or '<unset>'} -> {requested_cxx_compiler}",
            flush=True,
        )
        reset_build_directory(build_dir)
        reconfigure = True
    elif cache_file.is_file() and cached_path_changed(current_game_data_root, game_data_root):
        print(
            f"[OpenLegend] Reconfigure game data: "
            f"{current_game_data_root or '<unset>'} -> {game_data_root}",
            flush=True,
        )
        reconfigure = True
    elif current_sanitizers is not None and current_sanitizers != args.sanitizers:
        print(
            f"[OpenLegend] Reconfigure sanitizers: "
            f"{'ON' if current_sanitizers else 'OFF'} -> {'ON' if args.sanitizers else 'OFF'}",
            flush=True,
        )
        reconfigure = True

    configure = configure_command(
        cmake,
        ninja,
        project_root,
        build_dir,
        target,
        requested_cxx_compiler,
        configured_c_compiler,
        enable_sanitizers=args.sanitizers,
        game_data_root=game_data_root,
    )
    process_cwd = Path(sys.executable).parent if os.name == "nt" else project_root
    sanitizer_runtime = None
    test_environment = None
    if args.sanitizers and os.name == "nt":
        sanitizer_runtime = sanitizer_runtime_directory(requested_cxx_compiler)
        test_environment = sanitizer_test_environment(
            requested_cxx_compiler, dict(os.environ), sanitizer_runtime
        )
        print(f"[OpenLegend] Sanitizer runtime: {sanitizer_runtime}", flush=True)

    build_label = build_dir.name
    if reconfigure or not cache_file.is_file() or not build_file.is_file():
        print(f"[OpenLegend] Configure: {build_label}", flush=True)
        run(configure, process_cwd)
    else:
        print(f"[OpenLegend] Configure: {build_label} (reuse Ninja cache)", flush=True)

    if args.configure_only:
        return 0

    print(
        f"[OpenLegend] Build: {build_label}-{args.config.lower()} "
        f"(parallel jobs: {args.jobs})",
        flush=True,
    )
    run(
        [
            str(cmake),
            "--build",
            str(build_dir),
            "--config",
            args.config,
            "--parallel",
            str(args.jobs),
        ],
        process_cwd,
    )
    if os.name == "nt" and args.sanitizers:
        if sanitizer_runtime is None:
            raise RuntimeError("Windows sanitizer runtime was not resolved")
        deployed_runtimes = stage_windows_sanitizer_runtime(
            sanitizer_runtime, build_dir, args.config
        )
        print(
            f"[OpenLegend] Deployed {len(deployed_runtimes)} sanitizer runtime file(s)",
            flush=True,
        )
    elif os.name == "nt":
        removed_runtimes = remove_windows_sanitizer_runtimes(build_dir, args.config)
        if removed_runtimes:
            print(
                f"[OpenLegend] Removed {len(removed_runtimes)} stale sanitizer runtime file(s)",
                flush=True,
            )
    if not args.skip_tests:
        print(
            f"[OpenLegend] Test: {args.config} (parallel jobs: {args.test_jobs})",
            flush=True,
        )
        run(
            [
                str(ctest),
                "--test-dir",
                str(build_dir),
                "-C",
                args.config,
                "--parallel",
                str(args.test_jobs),
                "--output-on-failure",
            ],
            process_cwd,
            test_environment,
        )

    if target == "app":
        outputs = application_outputs(build_dir, args.config)
        if not outputs:
            raise RuntimeError(
                f"OpenLegend {args.config} executable was not found under {build_dir}"
            )
        for output in outputs:
            print(f"[OpenLegend] Application: {output}", flush=True)

    print(f"[OpenLegend] Build and tests completed: {build_dir} ({args.config})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
