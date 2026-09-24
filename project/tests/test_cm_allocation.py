"""The CM platform adapter must allocate from the SDK-supported memory pool."""
from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[2]
ADAPTERS = (
    "project/src/ml307y/system_port.c",
    "project/src/ml307y/file_port.c",
    "project/src/ml307y/mqtt_port.c",
)


class CmAllocationTests(unittest.TestCase):
    def test_platform_adapters_use_cm_memory_api(self):
        for relative in ADAPTERS:
            with self.subTest(relative=relative):
                source = (ROOT / relative).read_text(encoding="utf-8")
                self.assertTrue('#include "cm_mem.h"' in source, relative)
                self.assertFalse(re.search(r"(?<!cm_)\b(?:malloc|calloc|free)\s*\(", source), relative)

    def test_system_allocator_given_to_products_is_cm_malloc(self):
        source = (ROOT / ADAPTERS[0]).read_text(encoding="utf-8")
        self.assertTrue(re.search(r"services->system\.allocate\s*=\s*cm_malloc\s*;", source))
        self.assertTrue(re.search(r"services->system\.release\s*=\s*cm_free\s*;", source))


if __name__ == "__main__":
    unittest.main()
