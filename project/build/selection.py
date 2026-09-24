"""Product selection and isolated SCons configuration. No vendor source discovery."""
from pathlib import Path
import hashlib
import json
import os
import re

MODULES = {
    "indicator": ["src/indicator.c"],
    "reporting": ["src/alarm_button/alarm_core.c"],
    "kaiwan": ["src/kaiwan/kaiwan_protocol.c", "src/kaiwan/kaiwan_session.c"],
    "mqtt": ["src/mqtt/mqtt_config.c", "src/mqtt/mqtt_receive.c", "src/ml307y/mqtt_port.c"],
    "storage": ["src/snapshot_store.c", "src/ml307y/file_port.c"],
}
DEVICES = {
    "key": "src/ml307y/alarm_key.c",
    "led": "src/ml307y/alarm_led.c",
    "buzzer": "src/ml307y/alarm_buzzer.c",
    "battery": "src/ml307y/alarm_battery.c",
}
PLATFORM_SOURCES = ["project/src/ml307y/diag_uart.c",
                    "project/src/ml307y/system_port.c",
                    "project/src/ml307y/product_start.c"]
INCLUDES = ["project/inc",
            "include/platform/ssl/mbedtls-3.6.4/include"]

def select(root, arguments):
    root = Path(root)
    registry = json.loads((root / "project/build/products.json").read_text(encoding="utf-8"))
    name = arguments.get("product", "")
    if name not in registry:
        raise ValueError("Unknown product: " + name)
    product = json.loads((root / registry[name]).read_text(encoding="utf-8"))
    if product["name"] != name:
        raise ValueError("Manifest name must match registry key")
    for field in ("name", "board", "platform", "storage_namespace"):
        if not re.fullmatch(r"[a-z][a-z0-9_]*", product[field]):
            raise ValueError("Invalid manifest identifier: " + field)
    if not re.fullmatch(r"[a-zA-Z_][a-zA-Z0-9_]*", product["entry"]):
        raise ValueError("Invalid C entry identifier: entry")
    if type(product["id"]) is not int or not 0 < product["id"] <= 0xffffffff:
        raise ValueError("Product ID must be a nonzero uint32")
    if any(arguments.get(k, "n").lower() != "n" for k in ("demo", "test", "xydemo")):
        raise ValueError("A product cannot be combined with vendor demo/test entrypoints")
    if arguments.get("target", "userapp") not in ("kernel", "userapp"):
        raise ValueError("Products require kernel/userapp")
    if arguments.get("oc_entry", "userapp") != "userapp":
        raise ValueError("Products use matched dual-bin firmware")
    if arguments.get("board", product["board"]) != product["board"]:
        raise ValueError("Board does not match product manifest")
    if arguments.get("platform", "ml307y") != product["platform"]:
        raise ValueError("Unsupported platform")
    if product["platform"] != "ml307y":
        raise ValueError("No adapter for platform")
    if arguments.get("module", "ML307Y") != "ML307Y" or arguments.get("sub_module", "DL") != "DL":
        raise ValueError("This adapter requires ML307Y-DL")
    identities = []
    namespaces = []
    for item in registry.values():
        other = json.loads((root / item).read_text(encoding="utf-8"))
        identities.append(other["id"])
        namespaces.append(other["storage_namespace"])
    if len(set(identities)) != len(identities) or len(set(namespaces)) != len(namespaces):
        raise ValueError("Duplicate product/storage identity")
    if not re.fullmatch(r"[a-z][a-z0-9_]*", product["storage_namespace"]):
        raise ValueError("Invalid storage namespace")
    resources = product["resources"] + arguments.get("extra_resources", "").split(",")
    resources = [r for r in resources if r]
    if len(resources) != len(set(resources)):
        raise ValueError("Resource collision")
    for resource in resources:
        if not re.fullmatch(r"(PIN|PWM|RTC|TIMER|UART|I2C|SPI):[0-9]+|STORE:[a-z][a-z0-9_]*", resource):
            raise ValueError("Invalid resource claim: " + resource)
    devices = product["devices"]
    if not isinstance(devices, list) or any(not isinstance(device, str) or device not in DEVICES
                                              for device in devices) or len(devices) != len(set(devices)):
        raise ValueError("Invalid or duplicate device")
    configured_pins = []
    if "key" in devices:
        configured_pins.append(26)
    for device, field, minimum in (("led", "led_sdk_pin", -1),
                                   ("buzzer", "buzzer_sdk_pin", 0)):
        if device in devices:
            pin = product.get(field)
            if type(pin) is not int or pin < minimum:
                raise ValueError("Invalid device pin: " + field)
            if pin >= 0:
                configured_pins.append(pin)
    if len(configured_pins) != len(set(configured_pins)):
        raise ValueError("Device SDK pin collision")
    if "buzzer" in devices and (type(product.get("buzzer_hz")) is not int or
                                not 0 <= product["buzzer_hz"] <= 20000):
        raise ValueError("Invalid buzzer frequency")
    if "STORE:" + product["storage_namespace"] not in resources:
        raise ValueError("Declare the product storage namespace in resources")
    sources = PLATFORM_SOURCES + product["sources"]
    for module in product["modules"]:
        if module not in MODULES:
            raise ValueError("Unknown component: " + module)
        sources += ["project/" + f for f in MODULES[module]]
    sources += ["project/" + DEVICES[device] for device in devices]
    if len(sources) != len(set(sources)):
        raise ValueError("Duplicate source")
    for source in sources:
        if not source.startswith("project/") or ".." in Path(source).parts or ":" in source:
            raise ValueError("Source escapes project")
    product["sources"] = sources
    product["resources"] = resources
    product["include_dirs"] = INCLUDES
    return product

def base_fingerprint(root):
    root = Path(root)
    inputs = sorted((root / "kernel/prebuilts/open_mode/libs").glob("*.a"))
    inputs += sorted((root / "kernel/prebuilts/open_mode/ld").glob("*"))
    inputs += [root / "kernel/export/open_mode/xy_export.list",
               root / "project/src/ml307y/base_bridge.c",
               root / "project/src/ml307y/extra_exports.list",
               root / "project/build/selection.py", root / "project/build/artifacts.py",
               root / "project/src/ml307y/SConscript", root / "SConscript-k", root / "SConstruct",
               root / "tools/scons/KernelTools.py", root / "tools/scons/EnvironConfig.py",
               root / "kernel/platform/memmap/src/memmap_open.c"]
    digest = hashlib.sha256()
    for path in inputs:
        if path.is_file():
            digest.update(path.relative_to(root).as_posix().encode())
            digest.update(path.read_bytes())
    return digest.hexdigest()[:20]

def apply(env, root, arguments):
    from SCons.Script import SConsignFile, GetOption
    root = Path(root)
    spec = select(root, arguments)
    identity = "/".join([spec["name"], spec["board"], spec["platform"]])
    variant = root / "out/products" / identity
    base_id = base_fingerprint(root)
    base = root / "out/project-base" / base_id
    target = env["BUILD_TARGET"]
    output = base if target == "kernel" else variant
    generated = output / "generated"
    env["PROJECT_MODE"] = True
    # 厂商 SCons 默认只转发 PATH；保留调用者指定的工程内临时目录。
    for name in ("TEMP", "TMP", "TMPDIR"):
        if name in os.environ:
            env["ENV"][name] = os.environ[name]
    env["PROJECT_SPEC"] = spec
    env["PROJECT_BASE_ID"] = base_id
    env["PROJECT_BASE_ROOT"] = str(base)
    env["PROJECT_LD_DIR"] = str(base / "ld")
    env["PROJECT_BASELINE_DIR"] = str(base / "baseline")
    env["OBJECT_DIR"] = str(output / "obj")
    env["IMAGE_DIR"] = str(output / "image")
    env["IMAGE_ORIGIN_DIR"] = str(output / "image/origin_bins")
    env["IMAGE_INFO_JSON"] = str(output / "image/temp_image_info.json")
    env["TARGET_NAME"] = "ML307Y_" + spec["name"]
    env["PROJECT_GENERATED"] = str(generated)
    env["PROJECT_SOURCES"] = spec["sources"]
    if arguments.get("provision"):
        provision = (root / arguments["provision"]).resolve()
        provision.relative_to(root / "project")
        if not provision.is_file():
            raise ValueError("Provision header must exist within project")
        env["PROJECT_PROVISION"] = str(provision)
        env.Append(CPPDEFINES=[("ALARM_BUTTON_PROVISION_HEADER", "<product_private_config.h>")])
    env.PrependUnique(CPPPATH=[str(generated)])
    SConsignFile(str(output / ".sconsign.dblite"))
    if not GetOption("no_exec") and not GetOption("clean"):
        generated.mkdir(parents=True, exist_ok=True)
        if env.get("PROJECT_PROVISION"):
            private_header = '#include "' + Path(env["PROJECT_PROVISION"]).as_posix() + '"\n'
            (generated / "product_private_config.h").write_text(private_header, encoding="utf-8")
        (output / "image").mkdir(parents=True, exist_ok=True)
        header = ["#pragma once",
            "/*-------------------------------------------define---------------------------------------------*/",
            '#define PROJECT_BASE_ID "' + base_id + '"',
            '#define PRODUCT_NAME "' + spec["name"] + '"',
            "#define PRODUCT_ID " + str(spec["id"]) + "U",
            "#define PRODUCT_ENTRY " + spec["entry"],
            "#define PRODUCT_HAS_STORAGE " + str(int("storage" in spec["modules"])),
            "#define PRODUCT_HAS_MQTT " + str(int("mqtt" in spec["modules"])) ,
            '#define PRODUCT_STORAGE_NAMESPACE "' + spec["storage_namespace"] + '"']
        for device in DEVICES:
            header.append("#define PRODUCT_HAS_" + device.upper() + " " + str(int(device in spec["devices"])))
        for field, default in [("led_sdk_pin",-1),("buzzer_sdk_pin",16),
                               ("wake_verified",False),("buzzer_hz",0)]:
            header.append("#define ALARM_BUTTON_" + field.upper() + " " + str(int(spec.get(field, default))))
        (generated / "product_build_config.h").write_text("\n".join(header)+"\n", encoding="utf-8")
        (output / "product-manifest.json").write_text(json.dumps(
            {"product":spec,"base_id":base_id,"target":target},indent=2),encoding="utf-8")
    if target == "kernel":
        env["LIB_RSP"] = str(base / "image/prebuilt_lib.rsp")
        env["EXPORT_RSP"] = str(base / "image/mandatory_link.rsp")
        env["EXPORT_SRC"] = str(generated / "project_export.list")
        if not GetOption("no_exec") and not GetOption("clean"):
            exports = (root / "kernel/export/open_mode/xy_export.list").read_text(encoding="utf-8")
            exports += "\n" + (root / "project/src/ml307y/extra_exports.list").read_text(encoding="utf-8")
            (generated / "project_export.list").write_text(exports,encoding="utf-8")
    else:
        if not GetOption("no_exec") and not GetOption("clean") and not (base / "ld/import_func.ld").exists():
            raise ValueError("Build matching base first with project/tools/build_product.py")
        if not GetOption("no_exec") and not GetOption("clean"):
            from artifacts import verify_base
            verify_base(root, base_id)
        env.Prepend(LINKFLAGS=["-L" + str(base / "ld")])
    return spec
