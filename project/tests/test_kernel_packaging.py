"""Exercise real SDK compression twice without flashing or running SCons setup."""
import ast
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "project/build"))
import artifacts


class KernelPackagingTests(unittest.TestCase):
    def setUp(self):
        (ROOT / "out").mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=ROOT / "out", prefix="kernel-pack-test-")
        self.addCleanup(self.temp.cleanup)
        self.image = Path(self.temp.name)
        self.original = self.image / "origin_bins"
        self.original.mkdir()
        self.output = self.image / "bin"
        self.output.mkdir()
        self.sources = [self.image / "ap.elf"]
        self.sources[0].write_bytes(b"partition checker is isolated in this test")
        self.raw = {}
        for index, name in enumerate(("ap_flash.bin", "ap_psram.bin", "ap_sysram.bin")):
            content = bytes(range(256)) * (32 + index)
            source = self.original / name
            source.write_bytes(content)
            self.sources.append(source)
            self.raw[name] = content
        self.target = self.output / "ap.img"
        self.env = {
            "IMAGE_COMPRESSOR": str(ROOT / "tools/scripts/compress_bin_ap.py"),
            "SCRIPTS_DIR": str(ROOT / "tools/scripts"),
            "LD_INI_SRC": str(ROOT / "kernel/prebuilts/open_mode/ld/apld.ini"),
            "PACKER": str(ROOT / "tools/scripts/xy_packer.py"),
            "IMAGE_INFO_JSON": str(self.image / "info.json"),
            "IMAGE_DIR": str(self.image),
            "CP_TARGET": "ap",
            "PARTITION_CHECKER": "isolated-partition-check",
            "PARTITION_CHECK_INI_SRC": "unused",
        }
        # Import only the actual action: importing KernelTools initializes SCons globals.
        source = (ROOT / "tools/scons/KernelTools.py").read_text(encoding="utf-8")
        function = next(node for node in ast.parse(source).body
                        if isinstance(node, ast.FunctionDef) and node.name == "generate_img")
        namespace = {"os": os, "shutil": shutil, "subprocess": subprocess, "sys": sys}
        exec(compile(ast.Module(body=[function], type_ignores=[]), "KernelTools.py", "exec"), namespace)
        self.generate = namespace["generate_img"]
        self.real_run = subprocess.run

    def run_tools(self, command, **kwargs):
        if command[1] == self.env["PARTITION_CHECKER"]:
            return subprocess.CompletedProcess(command, 0, b"")
        return self.real_run(command, **kwargs)

    def test_compression_does_not_mutate_objcopy_outputs(self):
        with patch.object(subprocess, "run", side_effect=self.run_tools):
            self.generate([self.target], self.sources, self.env)
        for name, expected in self.raw.items():
            self.assertEqual((self.original / name).read_bytes(), expected,
                             "Compression changed original machine code: " + name)
        self.assertTrue(self.target.is_file())

    def test_repeated_packaging_is_byte_identical(self):
        with patch.object(subprocess, "run", side_effect=self.run_tools):
            self.generate([self.target], self.sources, self.env)
            # Legacy action writes beside its sources; retain its first result to reproduce.
            first = self.target if self.target.exists() else self.original / "ap.img"
            expected = first.read_bytes()
            self.generate([self.target], self.sources, self.env)
            actual = self.target if self.target.exists() else self.original / "ap.img"
            self.assertEqual(actual.read_bytes(), expected, "Incremental build recompressed the base")

    def test_each_failed_tool_stops_packaging(self):
        for failed_step in range(3):
            with self.subTest(failed_step=failed_step):
                results = [subprocess.CompletedProcess([], 0, b"")] * failed_step
                results.append(subprocess.CompletedProcess([], 7, b"injected failure"))
                with patch.object(subprocess, "run", side_effect=results) as run:
                    self.assertEqual(self.generate([self.target], self.sources, self.env), 7)
                    self.assertEqual(run.call_count, failed_step + 1)

    def test_decoded_package_matches_machine_code(self):
        with patch.object(subprocess, "run", side_effect=self.run_tools):
            self.generate([self.target], self.sources, self.env)
        artifacts.verify_ap_image(self.target, self.raw)
        wrong = dict(self.raw)
        wrong["ap_sysram.bin"] = b"not the compiled program"
        with self.assertRaisesRegex(ValueError, "machine code"):
            artifacts.verify_ap_image(self.target, wrong)

    def test_valid_hash_cannot_hide_repeated_compression(self):
        with patch.object(subprocess, "run", side_effect=self.run_tools):
            self.generate([self.target], self.sources, self.env)
        # Reproduce the old second pass, including fresh, valid image HMACs.
        self.real_run([sys.executable, self.env["IMAGE_COMPRESSOR"], self.env["SCRIPTS_DIR"],
                       self.env["LD_INI_SRC"], "0x10000", str(self.output)], check=True)
        self.real_run([sys.executable, self.env["PACKER"], "-b", str(self.output),
                       "-i", self.env["LD_INI_SRC"]], check=True)
        with self.assertRaisesRegex(ValueError, "machine code"):
            artifacts.verify_ap_image(self.target, self.raw)

    def test_truncated_and_corrupt_packages_are_rejected(self):
        with patch.object(subprocess, "run", side_effect=self.run_tools):
            self.generate([self.target], self.sources, self.env)
        good = self.target.read_bytes()
        for damaged in (good[:16], good[:-1], good[:-1] + bytes([good[-1] ^ 1])):
            self.target.write_bytes(damaged)
            with self.assertRaises(ValueError):
                artifacts.verify_ap_image(self.target, self.raw)


if __name__ == "__main__":
    unittest.main()
