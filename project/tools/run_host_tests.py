"""Native regression tests. Never downloads tools or contacts a broker."""
from pathlib import Path
import argparse
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "project/build"))
from selection import INCLUDES

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", help="Native GCC/Clang/TinyCC executable")
    parser.add_argument("--sim", action="store_true", help="Run C suites in the bundled RV64 simulator")
    parser.add_argument("--suite", help="Run one named C suite and its configured variants")
    args = parser.parse_args()
    if args.sim:
        if args.cc:
            parser.error("--sim selects the bundled compiler; omit --cc")
        toolchain = ROOT / "tools/toolchain/riscv64-elf-tools-v3"
        compiler = toolchain / "bin/riscv64-unknown-elf-gcc.exe"
        simulator = toolchain / "bin/riscv64-unknown-elf-run.exe"
        specs = toolchain / "riscv64-unknown-elf/lib/sim.specs"
        if not all(path.is_file() for path in (compiler, simulator, specs)):
            parser.error("Bundled RV64 compiler, simulator or sim.specs is missing")
    else:
        compiler = args.cc or shutil.which("tcc") or shutil.which("gcc") or shutil.which("clang")
        if not compiler:
            parser.error("Supply --cc with a native compiler, or use --sim")
    out = ROOT / "out/host-tests"
    out.mkdir(parents=True, exist_ok=True)
    includes = [p for p in INCLUDES if not p.startswith("include/")]
    includes += ["project/tests", "project/tests/alarm", "project/tests/mocks",
                 "include/cmiot", "include/platform",
                 "project/tests/vendor/cJSON", "project/tests/vendor/mbedtls-3.2.1/include",
                 "project/tests/vendor/mbedtls-3.2.1/library"]
    device_mocks = ["project/tests/mocks/cm_alarm_devices.c"]
    core = ["project/src/alarm_button/alarm_core.c",
            "project/src/ml307y/alarm_key.c", "project/src/indicator.c"] + device_mocks
    app = core + ["project/src/alarm_button/alarm_button.c"]
    payload = ["project/src/alarm_button/handset_payload.c"]
    crypto = ["project/src/kaiwan/kaiwan_session.c",
              "project/src/kaiwan/kaiwan_protocol.c",
              "project/tests/vendor/cJSON/cJSON.c", "project/tests/kaiwan/kaiwan_protocol_test.c"]
    crypto += ["project/tests/vendor/mbedtls-3.2.1/library/" + f
               for f in ("aes.c", "platform_util.c", "platform.c")]
    suites = {"test_battery": ["project/src/ml307y/alarm_battery.c"],
              "test_power_port": [], "test_base_gpio": [], "test_base_bridge": [],
              "test_alarm_core": core, "test_product": app, "test_async_product": app,
              "test_payload": payload,
              "test_devices": ["project/src/ml307y/alarm_key.c",
                             "project/src/ml307y/alarm_led.c",
                             "project/src/ml307y/alarm_buzzer.c",
                             "project/src/ml307y/alarm_battery.c"] + device_mocks,
              "test_codec": payload + crypto,
              "test_snapshot": core + ["project/src/snapshot_store.c"],
              "test_mqtt_receive": ["project/src/mqtt/mqtt_receive.c"]}
    optional = {
        "test_runtime": app + payload + crypto + ["project/src/mqtt/mqtt_config.c", "project/src/ml307y/alarm_battery.c"],
        "test_offline_runtime": app + payload + crypto + ["project/src/mqtt/mqtt_config.c", "project/src/ml307y/alarm_battery.c"],
        "test_sleep_runtime": app + payload + crypto + ["project/src/mqtt/mqtt_config.c", "project/src/ml307y/alarm_battery.c"],
        "test_file_port": core + ["project/src/snapshot_store.c"],
        "test_mqtt_port": ["project/src/mqtt/mqtt_config.c", "project/src/mqtt/mqtt_receive.c"],
        "test_hal": ["project/src/ml307y/alarm_key.c",
                     "project/src/ml307y/alarm_led.c",
                     "project/src/ml307y/alarm_buzzer.c",
                     "project/src/ml307y/alarm_battery.c"] + device_mocks,
    }
    for name, sources in optional.items():
        if (ROOT / "project/tests/alarm" / (name + ".c")).exists():
            suites[name] = sources
    if args.suite:
        if args.suite not in suites:
            parser.error("Unknown suite: " + args.suite)
        suites = {args.suite: suites[args.suite]}
    for name, sources in suites.items():
        exe = out / (name + (".riscv.elf" if args.sim else ".exe"))
        command = [compiler, "-Wall", '-DMBEDTLS_CONFIG_FILE="host_mbedtls.h"']
        if args.sim:
            command += ["-march=rv64imac", "-mabi=lp64", "-O2", "-g", "-specs=" + str(specs)]
        if name == "test_power_port":
            command += ["-I" + (ROOT / "include/cmiot").as_posix(), "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections"]
        if name in ("test_base_bridge", "test_file_port"):
            command += ["-D_SYS_SELECT_H", "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections"]
            command += ["-I" + (ROOT / path).as_posix() for path in (
                "include/platform/network/lwip/lwip-2.1.3/include",
                "include/platform/network/lwip/lwip-2.1.3/lwip_config",
                "include/platform/network/lwip/lwip-2.1.3/lwip_port")]
        if name in ("test_file_port", "test_mqtt_port", "test_hal"):
            command += ["-I" + (ROOT / "project/tests/mocks").as_posix(), "-I" + (ROOT / "include/cmiot").as_posix()]
        command += ["-I" + (ROOT / p).as_posix() for p in includes]
        command += [(ROOT / "project/tests/alarm" / (name + ".c")).as_posix()]
        command += [(ROOT / p).as_posix() for p in sources]
        if name in ("test_file_port", "test_mqtt_port", "test_hal"):
            command += [(ROOT / "project/tests/mocks/cm_mem.c").as_posix()]
        if args.sim:
            command += ["-lm"]
        command += ["-o", str(exe)]
        subprocess.run(command, cwd=ROOT, check=True)
        run_command = [str(simulator), "--model", "RV64IMAC", str(exe)] if args.sim else [str(exe)]
        subprocess.run(run_command, cwd=ROOT, check=True)
        if name == "test_power_port":
            mode_exe = out / ("test_power_port_deep" + (".riscv.elf" if args.sim else ".exe"))
            mode_command = command[:-2] + ["-DML307Y_SLEEP_MODE=CM_PM_SLEEP_MODE_DEEP", "-o", str(mode_exe)]
            subprocess.run(mode_command, cwd=ROOT, check=True)
            mode_run = [str(simulator), "--model", "RV64IMAC", str(mode_exe)] if args.sim else [str(mode_exe)]
            subprocess.run(mode_run, cwd=ROOT, check=True)
        if name == "test_battery":
            for count in (3, 9):
                battery_exe = out / ("test_battery_" + str(count) + (".riscv.elf" if args.sim else ".exe"))
                battery_command = command[:-2] + ["-DALARM_BATTERY_SAMPLE_COUNT=" + str(count) + "U",
                    "-DALARM_BATTERY_EMPTY_MILLIVOLTS=3400U", "-DALARM_BATTERY_FULL_MILLIVOLTS=4400U",
                    "-o", str(battery_exe)]
                subprocess.run(battery_command, cwd=ROOT, check=True)
                battery_run = [str(simulator), "--model", "RV64IMAC", str(battery_exe)] if args.sim else [str(battery_exe)]
                subprocess.run(battery_run, cwd=ROOT, check=True)
        if name == "test_runtime":
            plaintext_exe = out / ("test_runtime_plaintext" + (".riscv.elf" if args.sim else ".exe"))
            plaintext_command = command[:-2] + ["-DTEST_PLAINTEXT_MODE=1", "-o", str(plaintext_exe)]
            subprocess.run(plaintext_command, cwd=ROOT, check=True)
            plaintext_run = [str(simulator), "--model", "RV64IMAC", str(plaintext_exe)] if args.sim else [str(plaintext_exe)]
            subprocess.run(plaintext_run, cwd=ROOT, check=True)
            for hours in (1, 8):
                period_exe = out / ("test_runtime_" + str(hours) + "h" + (".riscv.elf" if args.sim else ".exe"))
                period_command = command[:-2] + ["-DTEST_PLAINTEXT_MODE=1",
                    "-DALARM_BUTTON_HEARTBEAT_HOURS=" + str(hours) + "U", "-o", str(period_exe)]
                subprocess.run(period_command, cwd=ROOT, check=True)
                period_run = [str(simulator), "--model", "RV64IMAC", str(period_exe)] if args.sim else [str(period_exe)]
                subprocess.run(period_run, cwd=ROOT, check=True)
        if name == "test_offline_runtime":
            for hours in (1, 8):
                period_exe = out / ("test_offline_runtime_" + str(hours) + "h" + (".riscv.elf" if args.sim else ".exe"))
                period_command = command[:-2] + ["-DALARM_BUTTON_HEARTBEAT_HOURS=" + str(hours) + "U", "-o", str(period_exe)]
                subprocess.run(period_command, cwd=ROOT, check=True)
                period_run = [str(simulator), "--model", "RV64IMAC", str(period_exe)] if args.sim else [str(period_exe)]
                subprocess.run(period_run, cwd=ROOT, check=True)
        if name == "test_devices":
            # 用另一组宏核验真实驱动的输出，防止频率或占空比被写死。
            configured_exe = out / ("test_devices_configured" + (".riscv.elf" if args.sim else ".exe"))
            configured_command = command[:-2] + ["-DALARM_BUZZER_FREQUENCY_HZ=2000U",
                "-DALARM_BUZZER_DUTY_PERCENT=25U", "-o", str(configured_exe)]
            subprocess.run(configured_command, cwd=ROOT, check=True)
            configured_run = [str(simulator), "--model", "RV64IMAC", str(configured_exe)] if args.sim else [str(configured_exe)]
            subprocess.run(configured_run, cwd=ROOT, check=True)
    mode = "RV64 simulator" if args.sim else "native"
    print(str(len(suites)) + " " + mode + " C suites and their configured variants passed; no hardware validation implied.")

if __name__ == "__main__":
    main()
