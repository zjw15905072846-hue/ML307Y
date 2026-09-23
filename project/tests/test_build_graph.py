"""Build graph regressions; dry runs do not flash or contact a broker."""
from pathlib import Path
import subprocess
import sys
import unittest
import re

ROOT = Path(__file__).resolve().parents[2]

class BuildGraphTests(unittest.TestCase):
    def test_kernel_map_is_generated_before_install(self):
        result = subprocess.run([sys.executable, "-m", "SCons", "target=kernel",
                                 "product=alarm_button", "-n", "-j4"],
                                cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_product_objects_are_under_variant_directory(self):
        result = subprocess.run([sys.executable, "-m", "SCons", "target=userapp",
                                 "product=alarm_button", "-n", "--tree=derived,prune"],
                                cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        bad = sorted(set(re.findall(r"(?m)^[| +\-]*(project[\\/][^\n]*[.](?:psram|flash)[.]o)", result.stdout)))
        self.assertFalse(bad, "Product objects escaped the variant: " + repr(bad))

    def test_provision_header_is_tracked_in_build_graph(self):
        result = subprocess.run([sys.executable, "-m", "SCons", "target=userapp",
                                 "product=alarm_button", "provision=project/tests/mocks/provision.h",
                                 "-n", "--tree=all,prune"], cwd=ROOT,
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(re.search(r"(?m)^[| +\-]*project/tests/mocks/provision[.]h$",
                                  result.stdout.replace("\\", "/")),
                         "Changing a provision header must invalidate product object files")

if __name__ == "__main__":
    unittest.main()
