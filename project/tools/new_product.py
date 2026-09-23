"""Create a virtual product from the template; never overwrite an existing product."""
from pathlib import Path
import argparse
import json
import re

ROOT = Path(__file__).resolve().parents[2]

def create_product(root, name, product_id):
    root = Path(root)
    if not re.fullmatch(r"[a-z][a-z0-9_]*", name):
        raise ValueError("Use lowercase letters, digits and underscores")
    if not 0 < product_id <= 0xffffffff:
        raise ValueError("Product ID must be a nonzero uint32")
    registry_file = root / "project/build/products.json"
    original = registry_file.read_text(encoding="utf-8")
    registry = json.loads(original)
    destination = root / "project/src" / (name + ".c")
    manifest = root / "project/build/manifests" / (name + ".json")
    if name in registry or destination.exists() or manifest.exists():
        raise ValueError("Product already exists; no files changed")
    for path in registry.values():
        spec = json.loads((root / path).read_text(encoding="utf-8"))
        if spec["id"] == product_id or spec["storage_namespace"] == name:
            raise ValueError("Product/storage identity already used")
    spec = json.loads((root / "project/build/manifests/template_test.json").read_text(encoding="utf-8"))
    shared_resources = [item for item in spec["resources"] if not item.startswith("STORE:")]
    spec.update(name=name, id=product_id, storage_namespace=name,
                entry=name + "_product_start", board_prepare=name + "_board_prepare",
                 sources=["project/src/" + name + ".c"],
                 resources=["STORE:" + name] + shared_resources)
    source = (root / "project/src/product_main.c").read_text(encoding="utf-8")
    source = source.replace("template_product_start", spec["entry"])
    source = source.replace("template_board_prepare", spec["board_prepare"])
    source = source.replace("template-test-started", name + "-test-started")
    destination.parent.mkdir(parents=True, exist_ok=True)
    manifest.parent.mkdir(parents=True, exist_ok=True)
    # First batch: the new source and manifest (2 files).
    destination.write_text(source, encoding="utf-8")
    manifest.write_text(json.dumps(spec, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    # Second batch: register only after both files exist; never erase partial work on failure.
    if registry_file.read_text(encoding="utf-8") != original:
        raise ValueError("Registry changed concurrently; product files retained, registration not written")
    registry[name] = manifest.relative_to(root).as_posix()
    registry_file.write_text(json.dumps(registry, indent=2) + "\n", encoding="utf-8")
    return destination

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("name")
    parser.add_argument("--id", required=True, type=lambda value: int(value, 0))
    args = parser.parse_args()
    print(create_product(ROOT, args.name, args.id))
    print("Virtual test product only. Implement business and bind a verified board before deployment.")

if __name__ == "__main__":
    main()
