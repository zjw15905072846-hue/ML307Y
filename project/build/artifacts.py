"""Hash receipts keep base image, symbol exports and application paired."""
from pathlib import Path
import hashlib
import json

BASE_FILES = ("baseline/ap.img", "baseline/ap_elf/ap.elf", "ld/import_func.ld")

def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def record_base(root, base_id):
    base = Path(root) / "out/project-base" / base_id
    receipt = {"base_id": base_id, "files": {p: sha256(base / p) for p in BASE_FILES}}
    (base / "pair.json").write_text(json.dumps(receipt, indent=2), encoding="utf-8")
    return receipt

def verify_base(root, base_id):
    base = Path(root) / "out/project-base" / base_id
    try:
        receipt = json.loads((base / "pair.json").read_text(encoding="utf-8"))
        if receipt["base_id"] != base_id or set(receipt["files"]) != set(BASE_FILES):
            raise ValueError("Invalid base receipt")
        for relative, expected in receipt["files"].items():
            if sha256(base / relative) != expected:
                raise ValueError("Mismatched base artifact: " + relative)
    except (OSError, KeyError, json.JSONDecodeError) as error:
        raise ValueError("Build and verify the matching base using project/tools/build_product.py") from error
    return receipt

def record_application(root, spec, base_id):
    base = verify_base(root, base_id)
    output = Path(root) / "out/products" / spec["name"] / spec["board"] / spec["platform"]
    image = output / "image"
    if sha256(image / "pkg/ap.img") != base["files"]["baseline/ap.img"]:
        raise ValueError("Packaged base differs from verified base")
    names = ("ML307Y_" + spec["name"] + ".mimgx",
             "ML307Y_" + spec["name"] + ".elf", "app.img", "pkg/ap.img")
    result = {"product": spec["name"], "product_id": spec["id"],
              "board": spec["board"], "platform": spec["platform"],
              "storage_namespace": spec["storage_namespace"], "base": base,
              "sources": spec["sources"], "entry": spec["entry"],
              "hardware_verified": False,
              "files": {p: sha256(image / p) for p in names}}
    (output / "release.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    return output
