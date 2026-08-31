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


def project_root_path(script_file: Path) -> Path:
    if configured := os.environ.get("OPENLEGEND_PROJECT_ROOT"):
        return Path(configured)
    return script_file.resolve().parents[1]


def parse_args(arguments: list[str] | None = None) -> argparse.Namespace:
    processor_count = max(1, os.cpu_count() or 1)
    parser = argparse.ArgumentParser(description="Configure, build, and test OpenLegend with Ninja")
    parser.add_argument(
        "target", type=str.lower, choices=("core", "app", "sdl"), nargs="?", default="core"
    )
    parser.add_argument("--config", choices=("Debug", "Release"), default="Debug")
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
    return parser.parse_args(arguments)


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


def run(command: list[str], cwd: Path) -> None:
    print("+", subprocess.list2cmdline(command), flush=True)
    subprocess.run(command, cwd=cwd, check=True)


def configure_command(
    cmake: Path,
    ninja: Path,
    project_root: Path,
    build_dir: Path,
    target: str,
    cxx_compiler: str | None = None,
    c_compiler: str | None = None,
    python_executable: str = sys.executable,
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
    ]
    if cxx_compiler:
        command.append(f"-DCMAKE_CXX_COMPILER:FILEPATH={cxx_compiler}")
    if target == "app" and c_compiler:
        command.append(f"-DCMAKE_C_COMPILER:FILEPATH={c_compiler}")
    return command


def cached_generator(cache_file: Path) -> str | None:
    if not cache_file.is_file():
        return None
    for line in cache_file.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith("CMAKE_GENERATOR:INTERNAL="):
            return line.split("=", 1)[1]
    return None


def cached_home_directory(cache_file: Path) -> str | None:
    if not cache_file.is_file():
        return None
    for line in cache_file.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith("CMAKE_HOME_DIRECTORY:INTERNAL="):
            return line.split("=", 1)[1]
    return None


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
    cmake, ninja, ctest = ensure_tools(project_root)

    target = normalize_target(args.target)
    if args.target == "sdl":
        print("[OpenLegend] 'sdl' is a compatibility alias; prefer 'app'.", flush=True)

    platform_name = "windows" if os.name == "nt" else "linux"
    build_dir = project_root / "build" / f"{platform_name}-{target}"
    cache_file = build_dir / "CMakeCache.txt"
    build_file = build_dir / "build.ninja"
    reconfigure = os.environ.get("OPENLEGEND_RECONFIGURE", "0") == "1"
    current_generator = cached_generator(cache_file)
    current_home = cached_home_directory(cache_file)
    if current_generator is not None and current_generator != EXPECTED_GENERATOR:
        print(
            f"[OpenLegend] Reset generator: {current_generator} -> {EXPECTED_GENERATOR}",
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

    configure = configure_command(
        cmake,
        ninja,
        project_root,
        build_dir,
        target,
        os.environ.get("CXX"),
        os.environ.get("CC"),
    )
    process_cwd = Path(sys.executable).parent if os.name == "nt" else project_root

    if reconfigure or not cache_file.is_file() or not build_file.is_file():
        print(f"[OpenLegend] Configure: {platform_name}-{target}", flush=True)
        run(configure, process_cwd)
    else:
        print(f"[OpenLegend] Configure: {platform_name}-{target} (reuse Ninja cache)", flush=True)

    if args.configure_only:
        return 0

    print(
        f"[OpenLegend] Build: {platform_name}-{target}-{args.config.lower()} "
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
        )

    print(f"[OpenLegend] Build and tests completed: {build_dir} ({args.config})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
