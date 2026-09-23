"""Template expansion and collision tests in disposable workspace fixtures."""
from pathlib import Path
import json
import shutil
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "project/tools"))
sys.path.insert(0, str(ROOT / "project/build"))
from new_product import create_product
from selection import select

class TemplateTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(dir=ROOT / "out", prefix="template-test-")
        self.root = Path(self.temp.name)
        for relative in ("project/build/products.json", "project/products/alarm_button/manifest.json",
                         "project/template/manifest.json", "project/template/product_main.c"):
            target = self.root / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / relative, target)

    def tearDown(self):
        self.temp.cleanup()

    def test_new_product_has_distinct_binding_and_namespace(self):
        folder = create_product(self.root, "desk_caller", 0x44430101)
        spec = select(self.root, {"product": "desk_caller"})
        self.assertEqual(spec["entry"], "desk_caller_product_start")
        self.assertTrue(spec["test_only"])
        self.assertEqual(spec["storage_namespace"], "desk_caller")
        source = (folder / "product_main.c").read_text(encoding="utf-8")
        self.assertIn("desk_caller_product_start", source)
        self.assertNotIn("template_product_start", source)
        self.assertFalse(any("alarm_button" in p for p in spec["sources"]))

    def test_existing_or_invalid_identity_does_not_write(self):
        original = (self.root / "project/build/products.json").read_bytes()
        for name, identity in (("alarm_button", 0x1234), ("desk_caller", 0x41420101),
                               ("../bad", 0x1234), ("desk_caller", 0)):
            with self.subTest(name=name, identity=identity), self.assertRaises(ValueError):
                create_product(self.root, name, identity)
        self.assertEqual((self.root / "project/build/products.json").read_bytes(), original)
        self.assertFalse((self.root / "project/products/desk_caller").exists())

if __name__ == "__main__":
    unittest.main()
