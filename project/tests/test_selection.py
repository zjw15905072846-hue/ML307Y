"""Explicit source/resource isolation and matched-artifact regressions."""
from pathlib import Path
import copy
import importlib.util
import json
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "project/build"))
from selection import select
from artifacts import BASE_FILES, record_base, verify_base

class ProductSelectionTests(unittest.TestCase):
    def setUp(self):
        (ROOT / "out").mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=ROOT / "out", prefix="selection-test-")
        self.root = Path(self.temp.name)
        for relative in ("project/build/products.json", "project/build/manifests/alarm_button.json",
                         "project/build/manifests/template_test.json"):
            target = self.root / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / relative, target)

    def tearDown(self):
        self.temp.cleanup()

    def alarm_change(self, **changes):
        path = self.root / "project/build/manifests/alarm_button.json"
        spec = json.loads(path.read_text())
        spec.update(changes)
        path.write_text(json.dumps(spec))

    def test_only_selected_sources_and_entry(self):
        alarm = select(self.root, {"product": "alarm_button"})
        template = select(self.root, {"product": "template_test"})
        self.assertIn("project/src/alarm_button/alarm_runtime.c", alarm["sources"])
        self.assertEqual(alarm["devices"], ["key", "led", "buzzer", "battery"])
        self.assertEqual(template["devices"], [])
        for device in ("key", "led", "buzzer", "battery"):
            self.assertIn("project/src/ml307y/alarm_" + device + ".c", alarm["sources"])
        self.assertNotIn("project/src/ml307y/alarm_board.c", alarm["sources"])
        self.assertFalse(any("alarm_" in path for path in template["sources"]))
        self.assertNotEqual(alarm["entry"], template["entry"])
        self.assertNotEqual(alarm["storage_namespace"], template["storage_namespace"])
        for spec in (alarm, template):
            self.assertFalse(any("access_control" in path or "custom/" in path or "test/" in path
                                for path in spec["sources"]))

    def test_sleep_mode_default_and_deep_selection(self):
        self.assertEqual(select(self.root, {"product": "alarm_button"})["sleep_mode"], "deep")
        self.assertEqual(select(self.root, {"product": "template_test"})["sleep_mode"], "light")
        self.assertEqual(select(self.root, {"product": "alarm_button", "sleep_mode": "deep"})["sleep_mode"], "deep")
        self.assertEqual(select(self.root, {"product": "alarm_button", "sleep_mode": "light"})["sleep_mode"], "light")

    def test_build_script_passes_resolved_product_sleep_mode(self):
        module_spec = importlib.util.spec_from_file_location("build_product_under_test", ROOT / "project/tools/build_product.py")
        builder = importlib.util.module_from_spec(module_spec)
        module_spec.loader.exec_module(builder)
        scenarios = ((["alarm_button"], "deep"), (["template_test"], "light"),
                     (["alarm_button", "--sleep-mode", "light"], "light"))
        for arguments, expected in scenarios:
            with self.subTest(arguments=arguments), patch.object(builder, "ROOT", self.root), \
                    patch.object(builder, "base_fingerprint", return_value="test-base"), \
                    patch.object(builder.sys, "argv", ["build_product.py"] + arguments), \
                    patch.object(builder.subprocess, "run") as run, patch("builtins.print"):
                run.return_value.returncode = 1
                with self.assertRaises(SystemExit):
                    builder.main()
                command = run.call_args.args[0]
                self.assertIn("sleep_mode=" + expected, command)
                self.assertIn("target=kernel", command)
                self.assertFalse((self.root / "out/project-build.lock").exists())

    def test_invalid_sleep_mode_and_template_override_rejected(self):
        for mode in ("", "DEEP", "deepsleep", "0", "active"):
            with self.subTest(mode=mode), self.assertRaises(ValueError):
                select(self.root, {"product": "alarm_button", "sleep_mode": mode})
        with self.assertRaises(ValueError):
            select(self.root, {"product": "template_test", "sleep_mode": "deep"})

    def test_led_requires_supported_hal_mapping_and_physical_resource(self):
        manifest = json.loads((self.root / "project/build/manifests/alarm_button.json").read_text())
        self.alarm_change(led_sdk_pin=41, resources=[r for r in manifest["resources"] if r != "PIN:96"])
        with self.assertRaises(ValueError):
            select(self.root, {"product": "alarm_button"})
        resources = [r for r in manifest["resources"] if r != "PIN:96"] + ["PIN:96"]
        for invalid_pin in (0, 26, 96, 100):
            self.alarm_change(led_sdk_pin=invalid_pin, resources=resources)
            with self.assertRaises(ValueError):
                select(self.root, {"product": "alarm_button"})
        self.alarm_change(led_sdk_pin=41, resources=resources)
        self.assertEqual(select(self.root, {"product": "alarm_button"})["led_sdk_pin"], 41)

    def test_unimplemented_wakeup_and_wrong_buzzer_pin_are_rejected(self):
        self.alarm_change(wake_verified=True)
        with self.assertRaises(ValueError):
            select(self.root, {"product": "alarm_button"})
        self.alarm_change(wake_verified=False, buzzer_sdk_pin=17)
        with self.assertRaises(ValueError):
            select(self.root, {"product": "alarm_button"})

    def test_uart0_diagnostic_is_built_and_prints_before_boot_checks(self):
        for product in ("alarm_button", "template_test"):
            spec = select(self.root, {"product": product})
            sources = spec["sources"]
            self.assertIn("project/src/ml307y/diag_uart.c", sources)
            for resource in ("UART:0", "PIN:17", "PIN:18"):
                self.assertIn(resource, spec["resources"])
        startup = (ROOT / "project/src/ml307y/product_start.c").read_text(encoding="utf-8")
        initialize = startup.split("static bool product_boot_initialize(void)", 1)[1]
        initialize = initialize.split("static void product_boot_task(void *argument)", 1)[0]
        boot = startup.split("static void product_boot_task(void *argument)", 1)[1]
        boot = boot.split("int cm_opencpu_entry(void *param)", 1)[0]
        self.assertIn("product_boot_initialize()", boot)
        init = initialize.find("ml307y_uart_diag_init()")
        ready = initialize.find("UART0 ready")
        base_check = initialize.find("project_base_identity()")
        self.assertGreaterEqual(init, 0)
        self.assertGreater(ready, init)
        self.assertGreater(base_check, ready)

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

    def test_buzzer_requires_pwm_resource(self):
        manifest = json.loads((self.root / "project/build/manifests/alarm_button.json").read_text())
        self.alarm_change(resources=[item for item in manifest["resources"] if item != "PWM:0"])
        with self.assertRaises(ValueError):
            select(self.root, {"product": "alarm_button"})

    def test_buzzer_frequency_has_no_legacy_manifest_override(self):
        for frequency in (0, 4000):
            with self.subTest(frequency=frequency):
                self.alarm_change(buzzer_hz=frequency)
                with self.assertRaises(ValueError):
                    select(self.root, {"product": "alarm_button"})

    def test_duplicate_storage_namespace(self):
        self.alarm_change(storage_namespace="template_test")
        with self.assertRaises(ValueError):
            select(self.root, {"product": "alarm_button"})

    def test_direct_key_has_no_mapping_gate(self):
        alarm = select(self.root, {"product": "alarm_button"})
        self.assertIn("PIN:26", alarm["resources"])
        self.assertNotIn("pinmap_verified", alarm)
        self.assertNotIn("key_sdk_pin", alarm)

    def test_invalid_device_configuration(self):
        original = ROOT / "project/build/manifests/alarm_button.json"
        selected = self.root / "project/build/manifests/alarm_button.json"
        for changes in ({"devices": ["key", "key"]}, {"devices": ["unknown"]},
                        {"led_sdk_pin": 26}, {"buzzer_sdk_pin": 26},
                        {"buzzer_hz": 20001}):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                shutil.copyfile(original, selected)
                self.alarm_change(**changes)
                select(self.root, {"product": "alarm_button"})

    def test_path_escape(self):
        for path in ("../escape.c", "G:/escape.c", "custom/custom_main.c"):
            self.alarm_change(sources=[path])
            with self.subTest(path=path), self.assertRaises(ValueError):
                select(self.root, {"product": "alarm_button"})

    @patch("artifacts.verify_base_machine_code")
    def test_foreign_base_and_corrupted_exports(self, verify_machine_code):
        base = self.root / "out/project-base/test-id"
        for name in BASE_FILES:
            file = base / name
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_bytes(b"matched-" + name.encode())
        record_base(self.root, "test-id")
        self.assertEqual(verify_base(self.root, "test-id")["base_id"], "test-id")
        self.assertEqual(verify_machine_code.call_count, 2)
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
