#!/usr/bin/env python3

from __future__ import annotations

import argparse
import os
from pathlib import Path
import platform
import subprocess
import sys

CMAKE_VERSION = "3.31.10"
NINJA_VERSION = "1.13.0"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Configure, build, and test OpenLegend")
    parser.add_argument("target", choices=("core", "sdl"), nargs="?", default="sdl")
    parser.add_argument("--config", choices=("Debug", "Release"), default="Debug")
    parser.add_argument("--jobs", type=int, default=max(1, min(4, os.cpu_count() or 1)))
    parser.add_argument("--configure-only", action="store_true")
    parser.add_argument("--skip-tests", action="store_true")
    return parser.parse_args()


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


def main() -> int:
    args = parse_args()
    project_root = Path(__file__).resolve().parents[1]
    cmake, ninja, ctest = ensure_tools(project_root)

    platform_name = "windows" if os.name == "nt" else "linux"
    build_dir = project_root / "build" / f"{platform_name}-{args.target}-{args.config.lower()}"
    build_app = "ON" if args.target == "sdl" else "OFF"

    configure = [
        str(cmake),
        "-S",
        str(project_root),
        "-B",
        str(build_dir),
        "-G",
        "Ninja",
        f"-DCMAKE_MAKE_PROGRAM={ninja}",
        f"-DCMAKE_BUILD_TYPE={args.config}",
        "-DBUILD_TESTING=ON",
        f"-DOPENLEGEND_BUILD_APP={build_app}",
        "-DOPENLEGEND_FETCH_SDL3=ON",
    ]
    if compiler := os.environ.get("CXX"):
        configure.append(f"-DCMAKE_CXX_COMPILER={compiler}")

    run(configure, project_root)
    if args.configure_only:
        return 0

    run([str(cmake), "--build", str(build_dir), "-j", str(args.jobs)], project_root)
    if not args.skip_tests:
        run([str(ctest), "--test-dir", str(build_dir), "--output-on-failure"], project_root)

    print(f"OpenLegend {args.target} {args.config} build completed: {build_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
