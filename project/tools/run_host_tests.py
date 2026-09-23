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
    args = parser.parse_args()
    compiler = args.cc or shutil.which("tcc") or shutil.which("gcc") or shutil.which("clang")
    if not compiler:
        parser.error("Supply --cc with a native compiler; the RV64 compiler cannot run host tests")
    out = ROOT / "out/host-tests"
    out.mkdir(parents=True, exist_ok=True)
    includes = [p for p in INCLUDES if not p.startswith("include/")]
    includes += ["project/tests", "project/tests/alarm", "include/platform",
                 "project/tests/vendor/cJSON", "project/tests/vendor/mbedtls-3.2.1/include",
                 "project/tests/vendor/mbedtls-3.2.1/library"]
    core = ["project/src/alarm_button/alarm_core.c",
            "project/src/key.c", "project/src/indicator.c"]
    app = core + ["project/src/alarm_button/alarm_button.c"]
    payload = ["project/src/alarm_button/handset_payload.c"]
    crypto = ["project/src/kaiwan/kw_session.c",
              "project/src/kaiwan/kw_protocol.c",
              "project/tests/vendor/cJSON/cJSON.c", "project/tests/kaiwan/kw_protocol_test.c"]
    crypto += ["project/tests/vendor/mbedtls-3.2.1/library/" + f
               for f in ("aes.c", "platform_util.c", "platform.c")]
    suites = {"test_alarm_core": core, "test_product": app, "test_async_product": app,
              "test_payload": payload, "test_board": ["project/src/board.c"],
              "test_codec": payload + crypto,
              "test_snapshot": core + ["project/src/snapshot_store.c"],
              "test_mqtt_rx": ["project/src/mqtt/mqtt_rx.c"]}
    optional = {
        "test_runtime": app + payload + crypto + ["project/src/mqtt/mqtt_config.c"],
        "test_file_port": core + ["project/src/snapshot_store.c"],
        "test_mqtt_port": ["project/src/mqtt/mqtt_config.c", "project/src/mqtt/mqtt_rx.c"],
        "test_hal": ["project/src/board.c"],
    }
    for name, sources in optional.items():
        if (ROOT / "project/tests/alarm" / (name + ".c")).exists():
            suites[name] = sources
    for name, sources in suites.items():
        exe = out / (name + ".exe")
        command = [compiler, "-Wall", '-DMBEDTLS_CONFIG_FILE="host_mbedtls.h"']
        if name in ("test_file_port", "test_mqtt_port", "test_hal"):
            command += ["-I" + (ROOT / "project/tests/mocks").as_posix(), "-I" + (ROOT / "include/cmiot").as_posix()]
        command += ["-I" + (ROOT / p).as_posix() for p in includes]
        command += [(ROOT / "project/tests/alarm" / (name + ".c")).as_posix()]
        command += [(ROOT / p).as_posix() for p in sources]
        command += ["-o", str(exe)]
        subprocess.run(command, cwd=ROOT, check=True)
        subprocess.run([str(exe)], cwd=ROOT, check=True)
    print(str(len(suites)) + " native C suites passed; no hardware/platform validation implied.")

if __name__ == "__main__":
    main()
