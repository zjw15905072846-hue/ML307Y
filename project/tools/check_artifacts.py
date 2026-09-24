"""Verify actual ELF symbols, generated headers, object roots and release receipts."""
from pathlib import Path
import json
import sys
from elftools.elf.elffile import ELFFile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "project/build"))
from artifacts import verify_base, sha256
from selection import base_fingerprint, select

def main():
    receipts = list((ROOT / "out/products").glob("*/*/*/release.json"))
    if len(receipts) < 2:
        raise SystemExit("Build alarm_button and template_test before this check")
    seen = set()
    current_base = base_fingerprint(ROOT)
    for path in receipts:
        receipt = json.loads(path.read_text())
        product = receipt["product"]
        base_id = receipt["base"]["base_id"]
        assert base_id == current_base, "Receipt uses an outdated base build"
        spec = select(ROOT, {"product": product})
        assert receipt["sources"] == spec["sources"]
        assert receipt["entry"] == spec["entry"]
        assert receipt["product_id"] == spec["id"]
        assert receipt["storage_namespace"] == spec["storage_namespace"]
        verify_base(ROOT, base_id)
        output = path.parent
        if output in seen:
            raise SystemExit("Output directory reused")
        seen.add(output)
        for relative, expected in receipt["files"].items():
            if sha256(output / "image" / relative) != expected:
                raise SystemExit("Artifact changed since receipt: " + relative)
        elf_path = output / "image" / ("ML307Y_" + product + ".elf")
        with elf_path.open("rb") as stream:
            elf = ELFFile(stream)
            names = {symbol.name for symbol in elf.get_section_by_name(".symtab").iter_symbols()
                     if symbol["st_shndx"] != "SHN_UNDEF"}
        assert "cm_opencpu_entry" in names
        assert receipt["entry"] in names
        if product == "template_test":
            assert "alarm_product_start" not in names
            assert not any(n.startswith(("ar_", "al_reporter_", "kw_protocol_", "alarm_", "kaiwan_")) for n in names)
        elif product == "alarm_button":
            assert "template_product_start" not in names
        if product in ("alarm_button", "template_test"):
            assert not any(n.startswith(("kw_app_", "kw_io_", "gate_")) for n in names)
        assert (output / ".sconsign.dblite").is_file()
        header = (output / "generated/product_build_config.h").read_text()
        assert '#define PRODUCT_NAME "' + product + '"' in header
        print(product + ": ELF entry, sources, generated config, cache, package and base hashes OK")
    escaped = list((ROOT / "project").rglob("*.o"))
    assert not escaped, escaped

if __name__ == "__main__":
    main()
