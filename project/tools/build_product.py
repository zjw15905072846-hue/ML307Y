"""Build one explicit product and its matched base; never flashes a device."""
from pathlib import Path
import argparse
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "project/build"))
from selection import select, base_fingerprint, resolve_provision
from artifacts import record_base, record_application

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("product")
    parser.add_argument("--board")
    parser.add_argument("--sleep-mode", choices=("light", "deep"),
                        help="Override product sleep mode; alarm_button defaults to DEEP, other products to LIGHT")
    parser.add_argument("--platform", default="ml307y")
    parser.add_argument("--provision", help="Header inside project/private; no credentials printed")
    parser.add_argument("-j", "--jobs", type=int, default=4)
    args = parser.parse_args()
    values = {"product": args.product, "platform": args.platform}
    if args.sleep_mode:
        values["sleep_mode"] = args.sleep_mode
    if args.board:
        values["board"] = args.board
    if args.provision:
        values["provision"] = args.provision
    if not 1 <= args.jobs <= 32:
        parser.error("jobs must be 1..32")
    spec = select(ROOT, values)
    values["sleep_mode"] = spec["sleep_mode"]
    print("Application sleep mode: " + spec["sleep_mode"], flush=True)
    provision = resolve_provision(ROOT, spec["name"], values)
    if provision:
        values["provision"] = provision.relative_to(ROOT).as_posix()
        print("Provision header: " + values["provision"], flush=True)
    logs = ROOT / "out/products" / spec["name"] / spec["board"] / spec["platform"] / "logs"
    logs.mkdir(parents=True, exist_ok=True)
    lock = ROOT / "out/project-build.lock"
    try:
        descriptor = os.open(lock, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
    except FileExistsError:
        raise SystemExit("Another project build owns out/project-build.lock; check the PID before removing a stale lock")
    try:
        with os.fdopen(descriptor, "w") as stream:
            stream.write(str(os.getpid()))
        base_id = base_fingerprint(ROOT)
        for target in ("kernel", "userapp"):
            command = [sys.executable, "-m", "SCons", "target=" + target,
                       "-j" + str(args.jobs)] + [k + "=" + v for k, v in values.items()]
            log = logs / (target + ".log")
            print("Building " + target + "; log: " + str(log), flush=True)
            with log.open("wb") as stream:
                result = subprocess.run(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT)
            if result.returncode:
                raise SystemExit("Build failed; inspect " + str(log))
            if target == "kernel":
                record_base(ROOT, base_id)
        output = record_application(ROOT, spec, base_id)
        print("BUILD_AND_PACKAGE_OK " + str(output / "release.json"))
    finally:
        lock.unlink()

if __name__ == "__main__":
    main()
