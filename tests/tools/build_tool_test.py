from __future__ import annotations

import argparse
import importlib.util
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock

PROJECT_ROOT = Path(__file__).resolve().parents[2]
MODULE_PATH = PROJECT_ROOT / "build.py"
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

    def test_accepts_game_data_directory_argument_and_environment(self) -> None:
        configured = r"E:\Game\OpenLegend\data"
        self.assertEqual(
            build.parse_args(["app", "--data-dir", configured]).data_dir, configured
        )
        with mock.patch.dict(
            os.environ, {"OPENLEGEND_GAME_DATA_ROOT": configured}, clear=True
        ):
            self.assertEqual(build.parse_args(["core"]).data_dir, configured)
            self.assertEqual(
                build.parse_args(["app", "--data-dir", "command-line-data"]).data_dir,
                "command-line-data",
            )

    def test_resolves_relative_game_data_directory_from_repository_root(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            project_root = Path(directory) / "OpenLegend"
            data_root = Path(directory) / "data"
            project_root.mkdir()
            data_root.mkdir()
            self.assertEqual(
                build.game_data_root_path(project_root, "../data"), data_root.resolve()
            )
            self.assertEqual(
                build.game_data_root_path(project_root, None), Path(directory).resolve()
            )

    def test_validates_minimum_game_data_identity(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            data_root = Path(directory)
            with self.assertRaisesRegex(RuntimeError, "Z.COM, Z.DAT"):
                build.validate_game_data_root(data_root)
            (data_root / "Z.COM").write_bytes(b"")
            (data_root / "Z.DAT").write_bytes(b"")
            build.validate_game_data_root(data_root)

    def test_resolves_project_root_from_root_build_script(self) -> None:
        with mock.patch.dict(os.environ, {}, clear=True):
            self.assertEqual(build.project_root_path(MODULE_PATH), PROJECT_ROOT)

    def test_defaults_to_clang_and_rejects_non_clang_compilers(self) -> None:
        with mock.patch.dict(os.environ, {}, clear=True):
            self.assertEqual(build.compiler_commands("linux"), ("clang-23", "clang++-23"))
            self.assertEqual(build.compiler_commands("windows"), ("clang.exe", "clang++.exe"))
        build.validate_compiler_environment("linux", "app", "clang-23", "clang++-23")
        with self.assertRaisesRegex(RuntimeError, "require Clang"):
            build.validate_compiler_environment("linux", "app", "gcc", "g++")

    def test_separates_ordinary_and_sanitizer_build_directories(self) -> None:
        self.assertEqual(
            build.build_directory(PROJECT_ROOT, "windows", "app", False),
            PROJECT_ROOT / "build" / "windows-app",
        )
        self.assertEqual(
            build.build_directory(PROJECT_ROOT, "windows", "app", True),
            PROJECT_ROOT / "build" / "windows-app-asan",
        )

    def test_accepts_sanitizer_gate(self) -> None:
        sanitized = build.parse_args(["app", "--sanitizers"])
        ordinary = build.parse_args(["app"])
        self.assertTrue(sanitized.sanitizers)
        self.assertEqual(sanitized.config, "Release")
        self.assertFalse(ordinary.sanitizers)
        self.assertEqual(ordinary.config, "Debug")

    def test_rejects_windows_debug_sanitizers(self) -> None:
        with self.assertRaisesRegex(RuntimeError, "does not support the Windows Debug CRT"):
            build.validate_configuration("windows", "Debug", True)
        build.validate_configuration("windows", "Release", True)
        build.validate_configuration("windows", "Debug", False)
        build.validate_configuration("linux", "Debug", True)

    def test_skips_tests_by_default_and_supports_explicit_test_mode(self) -> None:
        self.assertTrue(build.parse_args(["app"]).skip_tests)
        self.assertFalse(build.parse_args(["app", "--tests"]).skip_tests)
        self.assertTrue(build.parse_args(["app", "--skip-tests"]).skip_tests)

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
            game_data_root=Path(r"E:\Game\OpenLegend\data"),
        )
        self.assertIn("Ninja Multi-Config", command)
        self.assertIn("-DOPENLEGEND_BUILD_APP:BOOL=OFF", command)
        self.assertIn("-DOPENLEGEND_FETCH_TOMLPLUSPLUS:BOOL=ON", command)
        self.assertIn("-DOPENLEGEND_ENABLE_SANITIZERS:BOOL=OFF", command)
        self.assertIn(r"-DCMAKE_CXX_COMPILER:FILEPATH=D:\Dev\clang++.exe", command)
        self.assertNotIn(r"-DCMAKE_C_COMPILER:FILEPATH=D:\Dev\clang.exe", command)
        self.assertIn(r"-DPython3_EXECUTABLE:FILEPATH=D:\Dev\python.exe", command)
        self.assertIn(
            r"-DOPENLEGEND_GAME_DATA_ROOT:PATH=E:\Game\OpenLegend\data", command
        )

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
            True,
            Path("data"),
        )
        self.assertIn("-DOPENLEGEND_BUILD_APP:BOOL=ON", command)
        self.assertIn("-DCMAKE_C_COMPILER:FILEPATH=clang", command)
        self.assertIn("-DOPENLEGEND_ENABLE_SANITIZERS:BOOL=ON", command)
        self.assertIn("-DOPENLEGEND_GAME_DATA_ROOT:PATH=data", command)

    def test_locates_clang_windows_sanitizer_runtime(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            resource_directory = Path(directory) / "lib" / "clang" / "21"
            runtime_directory = resource_directory / "lib" / "windows"
            runtime_directory.mkdir(parents=True)
            (runtime_directory / "clang_rt.asan_dynamic-x86_64.dll").write_bytes(b"")
            completed = mock.Mock(stdout=str(resource_directory) + "\n")
            with mock.patch.object(build.subprocess, "run", return_value=completed) as runner:
                self.assertEqual(
                    build.sanitizer_runtime_directory("clang++"), runtime_directory
                )
            runner.assert_called_once_with(
                ["clang++", "-print-resource-dir"],
                check=True,
                capture_output=True,
                text=True,
            )

    def test_prepends_windows_sanitizer_runtime_to_test_path(self) -> None:
        runtime_directory = Path("runtime")
        with mock.patch.object(
            build, "sanitizer_runtime_directory", return_value=runtime_directory
        ):
            environment = build.sanitizer_test_environment(
                "clang++", {"PATH": "existing", "KEEP": "value"}
            )
        self.assertEqual(
            environment["PATH"], str(runtime_directory) + os.pathsep + "existing"
        )
        self.assertEqual(environment["KEEP"], "value")

    def test_stages_windows_sanitizer_runtime_beside_configuration_executables(
        self,
    ) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            runtime_directory = root / "runtime"
            runtime_directory.mkdir()
            runtime_file = runtime_directory / "clang_rt.asan_dynamic-x86_64.dll"
            runtime_file.write_bytes(b"asan-runtime")
            build_dir = root / "build"
            executable_paths = [
                build_dir / "src" / "platform" / "sdl3" / "Release" / "OpenLegend.exe",
                build_dir / "tests" / "Release" / "openlegend_core_tests.exe",
                build_dir / "tests" / "Debug" / "openlegend_core_tests.exe",
            ]
            for executable in executable_paths:
                executable.parent.mkdir(parents=True, exist_ok=True)
                executable.write_bytes(b"")

            deployed = build.stage_windows_sanitizer_runtime(
                runtime_directory, build_dir, "Release"
            )
            expected = {
                executable_paths[0].parent / runtime_file.name,
                executable_paths[1].parent / runtime_file.name,
            }
            self.assertEqual(set(deployed), expected)
            for destination in expected:
                self.assertEqual(destination.read_bytes(), b"asan-runtime")
            self.assertFalse((executable_paths[2].parent / runtime_file.name).exists())

            removed = build.remove_windows_sanitizer_runtimes(build_dir, "Release")
            self.assertEqual(set(removed), expected)
            self.assertTrue(all(not destination.exists() for destination in expected))

    def test_finds_configured_application_output(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            build_dir = Path(directory)
            executable = build.executable_name("openlegend")
            release = build_dir / "src" / "platform" / "sdl3" / "Release" / executable
            debug = build_dir / "src" / "platform" / "sdl3" / "Debug" / executable
            for output in (release, debug):
                output.parent.mkdir(parents=True, exist_ok=True)
                output.write_bytes(b"")
            self.assertEqual(build.application_outputs(build_dir, "Release"), [release])

    def test_rejects_missing_windows_sanitizer_runtime(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            completed = mock.Mock(stdout=directory + "\n")
            with mock.patch.object(build.subprocess, "run", return_value=completed):
                with self.assertRaisesRegex(RuntimeError, "runtime was not found"):
                    build.sanitizer_runtime_directory("clang++")

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

    def test_reads_cached_tool_value(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory) / "CMakeCache.txt"
            cache.write_text(
                "CMAKE_MAKE_PROGRAM:FILEPATH=CMAKE_MAKE_PROGRAM-NOTFOUND\n",
                encoding="utf-8",
            )
            self.assertEqual(
                build.cached_value(cache, "CMAKE_MAKE_PROGRAM"),
                "CMAKE_MAKE_PROGRAM-NOTFOUND",
            )

    def test_detects_changed_compiler_path(self) -> None:
        with mock.patch.object(
            build.shutil, "which", side_effect=lambda value: "/usr/bin/clang++-23"
        ):
            self.assertFalse(
                build.compiler_path_changed("/usr/bin/clang++-23", "clang++-23")
            )
        self.assertTrue(build.compiler_path_changed("/usr/bin/c++", "/usr/bin/clang++-23"))
        self.assertFalse(build.compiler_path_changed(None, "clang++-23"))

    def test_detects_changed_game_data_cache_path(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            data_root = Path(directory).resolve()
            self.assertTrue(build.cached_path_changed(None, data_root))
            self.assertFalse(build.cached_path_changed(str(data_root), data_root))
            self.assertTrue(
                build.cached_path_changed(str(data_root / "old"), data_root)
            )

    def test_reads_cached_bool(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory) / "CMakeCache.txt"
            cache.write_text(
                "OPENLEGEND_ENABLE_SANITIZERS:BOOL=ON\n",
                encoding="utf-8",
            )
            self.assertTrue(
                build.cached_bool(cache, "OPENLEGEND_ENABLE_SANITIZERS")
            )
            self.assertIsNone(build.cached_bool(cache, "MISSING"))

    def test_reset_preserves_runtime_configuration(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            build_dir = Path(directory) / "windows-app"
            configuration = (
                build_dir / "src" / "platform" / "sdl3" / "Debug" / "openlegend.toml"
            )
            configuration.parent.mkdir(parents=True)
            configuration.write_text("[window]\nwidth = 960\n", encoding="utf-8")
            (build_dir / "CMakeCache.txt").write_text("stale", encoding="utf-8")

            build.reset_build_directory(build_dir)

            self.assertEqual(
                configuration.read_text(encoding="utf-8"), "[window]\nwidth = 960\n"
            )
            self.assertFalse((build_dir / "CMakeCache.txt").exists())

    def test_linux_shell_only_prepares_environment_and_forwards_arguments(self) -> None:
        shell = (PROJECT_ROOT / "build.sh").read_text(encoding="utf-8")
        self.assertIn('export CC="${CC:-clang-23}"', shell)
        self.assertIn('export CXX="${CXX:-clang++-23}"', shell)
        self.assertIn('exec python3 "$ROOT/build.py" "$@"', shell)
        self.assertNotIn("tools/build.py", shell)
        self.assertNotIn("case ", shell)

    def test_game_data_path_is_runtime_test_configuration(self) -> None:
        cmake = (PROJECT_ROOT / "tests" / "CMakeLists.txt").read_text(
            encoding="utf-8"
        )
        support = (PROJECT_ROOT / "tests" / "support" / "test_support.hpp").read_text(
            encoding="utf-8"
        )
        self.assertNotIn(
            'OPENLEGEND_GAME_DATA_ROOT="${PROJECT_SOURCE_DIR}/.."', cmake
        )
        self.assertIn(
            'ENVIRONMENT "OPENLEGEND_GAME_DATA_ROOT=${OPENLEGEND_GAME_DATA_ROOT}"',
            cmake,
        )
        self.assertIn('"--data-dir=${OPENLEGEND_GAME_DATA_ROOT}"', cmake)
        self.assertIn("game_data_root()", support)
        self.assertIn("_wdupenv_s(", support)
        self.assertIn('L"OPENLEGEND_GAME_DATA_ROOT"', support)

    def test_large_unit_tests_are_ctest_sharded(self) -> None:
        cmake = (PROJECT_ROOT / "tests" / "CMakeLists.txt").read_text(
            encoding="utf-8"
        )
        support = (PROJECT_ROOT / "tests" / "support" / "test_support.hpp").read_text(
            encoding="utf-8"
        )
        self.assertIn(
            "add_openlegend_test_shards(openlegend.ui openlegend_ui_tests 17)", cmake
        )
        self.assertIn(
            "add_openlegend_test_shards(openlegend.scene openlegend_scene_tests 47)",
            cmake,
        )
        self.assertIn(
            "add_openlegend_test_shards(openlegend.battle openlegend_battle_tests 45)",
            cmake,
        )
        self.assertIn("TMP=${TEST_TEMP_ROOT}", cmake)
        self.assertIn("TEMP=${TEST_TEMP_ROOT}", cmake)
        self.assertIn("TMPDIR=${TEST_TEMP_ROOT}", cmake)
        self.assertIn("TestShard", support)
        self.assertIn("test_shard(const int argc, char* argv[])", support)

        for relative_path in (
            "unit/ui/title_menu_test.cpp",
            "unit/scene/scene_test.cpp",
            "unit/battle/battle_data_test.cpp",
        ):
            source = (PROJECT_ROOT / "tests" / relative_path).read_text(
                encoding="utf-8"
            )
            self.assertIn("test_shard(argc, argv)", source)
            self.assertIn("shard.includes(index)", source)

    def test_tests_do_not_mask_large_stack_frames(self) -> None:
        cmake = (PROJECT_ROOT / "tests" / "CMakeLists.txt").read_text(
            encoding="utf-8"
        )
        self.assertNotIn("/STACK:", cmake)
        self.assertNotIn("prlimit", cmake)
        self.assertNotIn("--stack=", cmake)

    def test_windows_build_uses_static_msvc_runtime(self) -> None:
        cmake = (PROJECT_ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn("if(WIN32)", cmake)
        self.assertIn(
            'set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")',
            cmake,
        )

    def test_windows_batch_uses_locked_tools_and_long_path(self) -> None:
        batch = (PROJECT_ROOT / "build.bat").read_text(encoding="utf-8")
        self.assertNotIn(r'D:\Dev\lldb\tools\cmake\bin\cmake.exe', batch)
        self.assertNotIn(r'D:\Dev\lldb\tools\ninja\ninja.exe', batch)
        self.assertIn(r'D:\Dev\Compiler\LLVM\x64\bin', batch)
        self.assertIn(r'D:\Dev\Python\python.exe', batch)
        self.assertIn('set "PROJECT_ROOT=%%~fI"', batch)
        self.assertNotIn('set "PROJECT_ROOT=%%~fsI"', batch)
        self.assertIn('set "OPENLEGEND_PROJECT_ROOT=%PROJECT_ROOT%"', batch)
        self.assertIn('set "OPENLEGEND_CMAKE="', batch)
        self.assertIn('set "OPENLEGEND_CTEST="', batch)
        self.assertIn('set "OPENLEGEND_NINJA="', batch)
        self.assertIn(r'"%PYTHON%" "%PROJECT_ROOT%\build.py" %*', batch)
        self.assertNotIn(r"tools\build.py", batch)
        self.assertNotIn('set "TARGET=', batch)
        self.assertNotIn(":usage", batch)
        self.assertNotIn(":missing_tools", batch)


if __name__ == "__main__":
    unittest.main()
