"""Compile real buzzer configuration; never contacts or drives the board."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
COMPILER = ROOT / "tools/toolchain/riscv64-elf-tools-v3/bin/riscv64-unknown-elf-gcc.exe"


@unittest.skipUnless(COMPILER.is_file(), "Bundled RV64 compiler is required")
class BuzzerConfigTests(unittest.TestCase):
    def compile_config(self, frequency, duty):
        (ROOT / "out").mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ROOT / "out", prefix="buzzer-config-") as temporary:
            return subprocess.run([
                str(COMPILER), "-march=rv64imac", "-mabi=lp64", "-std=c11", "-Werror",
                "-Iproject/inc", "-DALARM_BUZZER_FREQUENCY_HZ=" + str(frequency),
                "-DALARM_BUZZER_DUTY_PERCENT=" + str(duty), "-x", "c", "-c", "-",
                "-o", str(Path(temporary) / "config.o")],
                input='#include "ml307y/alarm_buzzer.h"\n', text=True,
                capture_output=True, cwd=ROOT)

    def test_supported_boundaries_and_datasheet_setting_compile(self):
        for frequency, duty in ((200, 1), (4000, 50), (20000, 99)):
            with self.subTest(frequency=frequency, duty=duty):
                result = self.compile_config(frequency, duty)
                self.assertEqual(result.returncode, 0, result.stderr)

    def test_zero_or_unsupported_frequency_is_rejected(self):
        for frequency in (0, 199, 20001):
            with self.subTest(frequency=frequency):
                result = self.compile_config(frequency, 50)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Buzzer frequency must be between", result.stderr)

    def test_constant_level_or_invalid_duty_is_rejected(self):
        for duty in (-1, 0, 100, 101):
            with self.subTest(duty=duty):
                result = self.compile_config(4000, duty)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Buzzer duty must be between", result.stderr)


if __name__ == "__main__":
    unittest.main()
