"""Guard OpenCPU task entry points against falling off the end."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]


def function_body(relative_path, name):
    source = (ROOT / relative_path).read_text(encoding="utf-8")
    pattern = r"\bstatic\s+void\s+" + re.escape(name) + r"\s*\([^)]*\)\s*\{"
    match = re.search(pattern, source)
    if match is None:
        raise AssertionError(f"Missing task function: {name}")
    depth = 1
    for position in range(match.end(), len(source)):
        if source[position] == "{":
            depth += 1
        elif source[position] == "}":
            depth -= 1
            if depth == 0:
                return source[match.end():position]
    raise AssertionError(f"Unclosed task function: {name}")


class TaskLifecycleTests(unittest.TestCase):
    def test_platform_logs_thread_creation_result(self):
        adapter = (ROOT / "project/src/ml307y/system_port.c").read_text(encoding="utf-8")
        start = adapter.split("static bool ml307y_thread_start(", 1)[1]
        start = start.split("static bool ml307y_digits(", 1)[0]
        self.assertTrue("osThreadNew(entry, argument, &attributes)" in start)
        self.assertTrue("task %s created" in start)
        self.assertTrue("task %s create failed" in start)

    def test_product_start_reports_task_creation_failure(self):
        interface = (ROOT / "project/inc/product_interface.h").read_text(encoding="utf-8")
        startup = (ROOT / "project/src/ml307y/product_start.c").read_text(encoding="utf-8")
        alarm = (ROOT / "project/src/alarm_button/alarm_runtime.c").read_text(encoding="utf-8")
        self.assertTrue("bool (*start)(product_services_t *services);" in interface)
        self.assertTrue("extern bool PRODUCT_ENTRY(product_services_t *services);" in startup)
        self.assertTrue("if (!selected_product.start(&selected_product_services))" in startup)
        self.assertTrue("bool alarm_product_start(product_services_t *services)" in alarm)
        self.assertTrue("alarm-ui-start" in alarm)
        self.assertTrue("alarm-worker-start" in alarm)
        worker = alarm.split("static void alarm_background_task(void *argument)", 1)[1]
        front = alarm.split("static void alarm_front_task(void *argument)", 1)[1]
        start = alarm.split("bool alarm_product_start(product_services_t *services)", 1)[1]
        self.assertTrue("__atomic_load_n(&runtime->startup_ready" in worker)
        self.assertTrue("__atomic_load_n(&runtime->startup_ready" in front)
        self.assertGreater(start.find("__atomic_store_n(&runtime->startup_ready"),
                           start.find('thread_start("alarm-worker"'))

    def test_default_uart_task_cannot_return_or_spin(self):
        body = function_body("custom/custom_main/src/custom_main.c", "custom_uart_task")
        self.assertNotRegex(body, r"\breturn\s*;")
        self.assertRegex(body, r"while\s*\(\s*1\s*\)")
        self.assertRegex(body, r"\bosDelay\s*\(")

    def test_product_boot_task_cannot_return_or_spin(self):
        body = function_body("project/src/ml307y/product_start.c", "product_boot_task")
        self.assertNotRegex(body, r"\breturn\s*;")
        self.assertRegex(body, r"while\s*\(\s*1\s*\)")
        self.assertRegex(body, r"\bosDelay\s*\(")

    def test_alarm_workers_are_persistent(self):
        for name in ("alarm_front_task", "alarm_background_task"):
            with self.subTest(name=name):
                body = function_body("project/src/alarm_button/alarm_runtime.c", name)
                self.assertNotRegex(body, r"\breturn\s*;")
                self.assertRegex(body, r"while\s*\(\s*1\s*\)")


if __name__ == "__main__":
    unittest.main()
