from __future__ import annotations

import argparse
import importlib.util
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock

PROJECT_ROOT = Path(__file__).resolve().parents[2]
MODULE_PATH = PROJECT_ROOT / "tools" / "build.py"
SPEC = importlib.util.spec_from_file_location("openlegend_build", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
build = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(build)


class BuildToolTest(unittest.TestCase):
    def test_normalizes_legacy_sdl_target(self) -> None:
        self.assertEqual(build.normalize_target("core"), "core")
        self.assertEqual(build.normalize_target("app"), "app")
        self.assertEqual(build.normalize_target("sdl"), "app")

    def test_accepts_case_insensitive_target_from_batch(self) -> None:
        self.assertEqual(build.parse_args(["APP", "--config", "Release"]).target, "app")

    def test_rejects_nonpositive_parallelism(self) -> None:
        with self.assertRaisesRegex(argparse.ArgumentTypeError, "positive integer"):
            build.positive_integer("0")

    def test_preserves_explicit_windows_short_project_root(self) -> None:
        short_path = r"E:\Game\金庸群~1\OPENLE~1"
        with mock.patch.dict(os.environ, {"OPENLEGEND_PROJECT_ROOT": short_path}):
            self.assertEqual(str(build.project_root_path(MODULE_PATH)), short_path)

    def test_builds_core_ninja_multi_config_command(self) -> None:
        command = build.configure_command(
            Path(r"D:\Dev\cmake.exe"),
            Path(r"D:\Dev\ninja.exe"),
            Path(r"E:\Game\OPENLE~1"),
            Path(r"E:\Game\OPENLE~1\build\windows-core"),
            "core",
            r"D:\Dev\clang++.exe",
            r"D:\Dev\clang.exe",
            r"D:\Dev\python.exe",
        )
        self.assertIn("Ninja Multi-Config", command)
        self.assertIn("-DOPENLEGEND_BUILD_APP:BOOL=OFF", command)
        self.assertIn("-DOPENLEGEND_FETCH_TOMLPLUSPLUS:BOOL=ON", command)
        self.assertIn(r"-DCMAKE_CXX_COMPILER:FILEPATH=D:\Dev\clang++.exe", command)
        self.assertNotIn(r"-DCMAKE_C_COMPILER:FILEPATH=D:\Dev\clang.exe", command)
        self.assertIn(r"-DPython3_EXECUTABLE:FILEPATH=D:\Dev\python.exe", command)

    def test_builds_app_command_with_c_compiler(self) -> None:
        command = build.configure_command(
            Path("cmake"),
            Path("ninja"),
            Path("source"),
            Path("build"),
            "app",
            "clang++",
            "clang",
            "python",
        )
        self.assertIn("-DOPENLEGEND_BUILD_APP:BOOL=ON", command)
        self.assertIn("-DCMAKE_C_COMPILER:FILEPATH=clang", command)

    def test_requires_complete_tool_override_set(self) -> None:
        with mock.patch.dict(
            os.environ,
            {
                "OPENLEGEND_CMAKE": "cmake",
                "OPENLEGEND_NINJA": "",
                "OPENLEGEND_CTEST": "",
            },
        ):
            with self.assertRaisesRegex(RuntimeError, "must be set together"):
                build.ensure_tools(PROJECT_ROOT)

    def test_accepts_existing_tool_overrides(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            tools = [root / name for name in ("cmake", "ninja", "ctest")]
            for tool in tools:
                tool.write_bytes(b"")
            with mock.patch.dict(
                os.environ,
                {
                    "OPENLEGEND_CMAKE": str(tools[0]),
                    "OPENLEGEND_NINJA": str(tools[1]),
                    "OPENLEGEND_CTEST": str(tools[2]),
                },
            ):
                self.assertEqual(build.ensure_tools(PROJECT_ROOT), tuple(tools))

    def test_reads_cached_generator(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory) / "CMakeCache.txt"
            cache.write_text(
                "OTHER:STRING=value\nCMAKE_GENERATOR:INTERNAL=Ninja Multi-Config\n",
                encoding="utf-8",
            )
            self.assertEqual(build.cached_generator(cache), "Ninja Multi-Config")

    def test_windows_batch_uses_fixed_tools_and_short_path(self) -> None:
        batch = (PROJECT_ROOT / "build.bat").read_text(encoding="utf-8")
        self.assertIn(r'D:\Dev\lldb\tools\cmake\bin\cmake.exe', batch)
        self.assertIn(r'D:\Dev\lldb\tools\ninja\ninja.exe', batch)
        self.assertIn(r'D:\Dev\Compiler\LLVM\x64\bin', batch)
        self.assertIn(r'D:\Dev\Python\python.exe', batch)
        self.assertIn('set "PROJECT_ROOT=%%~fsI"', batch)
        self.assertIn('set "OPENLEGEND_PROJECT_ROOT=%PROJECT_ROOT%"', batch)


if __name__ == "__main__":
    unittest.main()
