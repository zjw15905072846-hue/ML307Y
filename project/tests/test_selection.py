"""Explicit source/resource isolation and matched-artifact regressions."""
from pathlib import Path
import copy
import json
import shutil
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "project/build"))
from selection import select
from artifacts import BASE_FILES, record_base, verify_base

class ProductSelectionTests(unittest.TestCase):
    def setUp(self):
        (ROOT / "out").mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=ROOT / "out", prefix="selection-test-")
        self.root = Path(self.temp.name)
        for relative in ("project/build/products.json", "project/products/alarm_button/manifest.json",
                         "project/template/manifest.json"):
            target = self.root / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / relative, target)

    def tearDown(self):
        self.temp.cleanup()

    def alarm_change(self, **changes):
        path = self.root / "project/products/alarm_button/manifest.json"
        spec = json.loads(path.read_text())
        spec.update(changes)
        path.write_text(json.dumps(spec))

    def test_only_selected_sources_and_entry(self):
        alarm = select(self.root, {"product": "alarm_button"})
        template = select(self.root, {"product": "template_test"})
        self.assertIn("project/products/alarm_button/alarm_runtime.c", alarm["sources"])
        self.assertFalse(any("alarm_" in path for path in template["sources"]))
        self.assertNotEqual(alarm["entry"], template["entry"])
        self.assertNotEqual(alarm["storage_namespace"], template["storage_namespace"])
        for spec in (alarm, template):
            self.assertFalse(any("access_control" in path or "custom/" in path or "test/" in path
                                 for path in spec["sources"]))

    def test_vendor_entries_and_wrong_targets_rejected(self):
        for args in ({"demo": "mqtt"}, {"test": "y"}, {"xydemo": "y"}, {"oc_entry": "kernel"},
                     {"target": "obm"}, {"board": "other"}, {"platform": "other"}):
            with self.subTest(args=args), self.assertRaises(ValueError):
                select(self.root, {"product": "alarm_button", **args})

    def test_duplicate_resource(self):
        for resource in ("PIN:26", "PWM:0", "STORE:alarm_button"):
            with self.subTest(resource=resource), self.assertRaises(ValueError):
                select(self.root, {"product": "alarm_button", "extra_resources": resource})

    def test_duplicate_product_identity(self):
        self.alarm_change(id=0x54500101)
        with self.assertRaises(ValueError):
            select(self.root, {"product": "alarm_button"})

    def test_duplicate_storage_namespace(self):
        self.alarm_change(storage_namespace="template_test")
        with self.assertRaises(ValueError):
            select(self.root, {"product": "alarm_button"})

    def test_unknown_pin_gate(self):
        self.alarm_change(pinmap_verified=True)
        with self.assertRaises(ValueError):
            select(self.root, {"product": "alarm_button"})

    def test_path_escape(self):
        for path in ("../escape.c", "G:/escape.c", "custom/custom_main.c"):
            self.alarm_change(sources=[path])
            with self.subTest(path=path), self.assertRaises(ValueError):
                select(self.root, {"product": "alarm_button"})

    def test_foreign_base_and_corrupted_exports(self):
        base = self.root / "out/project-base/test-id"
        for name in BASE_FILES:
            file = base / name
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_bytes(b"matched-" + name.encode())
        record_base(self.root, "test-id")
        self.assertEqual(verify_base(self.root, "test-id")["base_id"], "test-id")
        (base / "ld/import_func.ld").write_bytes(b"wrong export addresses")
        with self.assertRaises(ValueError):
            verify_base(self.root, "test-id")
        with self.assertRaises(ValueError):
            verify_base(self.root, "other-id")

    def test_product_name_and_entry_cannot_escape_or_inject(self):
        self.alarm_change(name="../bad")
        with self.assertRaises(ValueError):
            select(self.root, {"product": "alarm_button"})

if __name__ == "__main__":
    unittest.main()
