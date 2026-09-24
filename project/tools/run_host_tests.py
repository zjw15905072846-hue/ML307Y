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
    includes += ["project/tests", "project/tests/alarm", "include/platform",
                 "project/tests/vendor/cJSON", "project/tests/vendor/mbedtls-3.2.1/include",
                 "project/tests/vendor/mbedtls-3.2.1/library"]
    core = ["project/src/alarm_button/alarm_core.c",
            "project/src/key.c", "project/src/indicator.c"]
    app = core + ["project/src/alarm_button/alarm_button.c"]
    payload = ["project/src/alarm_button/handset_payload.c"]
    crypto = ["project/src/kaiwan/kaiwan_session.c",
              "project/src/kaiwan/kaiwan_protocol.c",
              "project/tests/vendor/cJSON/cJSON.c", "project/tests/kaiwan/kaiwan_protocol_test.c"]
    crypto += ["project/tests/vendor/mbedtls-3.2.1/library/" + f
               for f in ("aes.c", "platform_util.c", "platform.c")]
    suites = {"test_alarm_core": core, "test_product": app, "test_async_product": app,
              "test_payload": payload, "test_board": ["project/src/board.c"],
              "test_codec": payload + crypto,
              "test_snapshot": core + ["project/src/snapshot_store.c"],
              "test_mqtt_receive": ["project/src/mqtt/mqtt_receive.c"]}
    optional = {
        "test_runtime": app + payload + crypto + ["project/src/mqtt/mqtt_config.c"],
        "test_file_port": core + ["project/src/snapshot_store.c"],
        "test_mqtt_port": ["project/src/mqtt/mqtt_config.c", "project/src/mqtt/mqtt_receive.c"],
        "test_hal": ["project/src/board.c"],
    }
    for name, sources in optional.items():
        if (ROOT / "project/tests/alarm" / (name + ".c")).exists():
            suites[name] = sources
    for name, sources in suites.items():
        exe = out / (name + (".riscv.elf" if args.sim else ".exe"))
        command = [compiler, "-Wall", '-DMBEDTLS_CONFIG_FILE="host_mbedtls.h"']
        if args.sim:
            command += ["-march=rv64imac", "-mabi=lp64", "-O2", "-g", "-specs=" + str(specs)]
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
    mode = "RV64 simulator" if args.sim else "native"
    print(str(len(suites)) + " " + mode + " C suites passed; no hardware validation implied.")

if __name__ == "__main__":
    main()
