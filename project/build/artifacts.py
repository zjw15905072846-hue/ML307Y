"""Verify executable base payloads as well as paired artifact hashes."""
from pathlib import Path
import hashlib
import hmac
import json
import lzma
import struct
from elftools.elf.elffile import ELFFile

BASE_FILES = ("baseline/ap.img", "baseline/ap_elf/ap.elf", "ld/import_func.ld")
AP_SECTIONS = {
    "ap_flash.bin": (".flash.text", ".flash.rodata"),
    "ap_sysram.bin": (".text", ".rodata", ".data"),
    "ap_psram.bin": (".psram.text", ".psram.rodata", ".psram.data"),
}


def read_packed_images(path):
    """Read SDK IMG or MIMGX payloads without trusting their recorded hashes alone."""
    data = Path(path).read_bytes()
    try:
        if data[:4] == b"HEAD":
            _, log_size, image_size = struct.unpack_from("<4sII", data)
            offset = 12 + log_size
            data = data[offset:offset + image_size]
            if len(data) != image_size:
                raise ValueError("Truncated merged image")
        magic, version = struct.unpack_from("<II", data)
        if magic not in (5847, 9527) or version != 1:
            raise ValueError("Invalid SDK image header")
        if hmac.new(b"XY1100", data[28:], hashlib.sha1).digest() != data[8:28]:
            raise ValueError("Image HMAC mismatch")
        count, = struct.unpack_from("<I", data, 28)
        position = 32
        entries = []
        for _ in range(count):
            _, _, _, _, name_size = struct.unpack_from("<5I", data, position)
            position += 20
            name = data[position:position + name_size].decode("ascii")
            position += name_size
            size, = struct.unpack_from("<I", data, position)
            entries.append((name, size, data[position + 4:position + 24]))
            position += 24
        images = {}
        for name, size, digest in entries:
            content = data[position:position + size]
            position += size
            if name in images or len(content) != size:
                raise ValueError("Duplicate or truncated image: " + name)
            if hmac.new(b"XY1100", content, hashlib.sha1).digest() != digest:
                raise ValueError("Segment HMAC mismatch: " + name)
            images[name] = content
        if position != len(data):
            raise ValueError("Unexpected image payload length")
        return images
    except (struct.error, UnicodeError) as error:
        raise ValueError("Malformed SDK image") from error


def verify_ap_image(path, expected_bins):
    """Boot ROM decompresses once; that result must be the exact linked machine code."""
    images = read_packed_images(path)
    try:
        header = images["ap_flash.bin"][:4096]
        magic, info_size, count = struct.unpack_from("<III", header)
        if magic != 0x1A2B3C4D or count != len(AP_SECTIONS) or info_size > 4084:
            raise ValueError("Invalid AP compression header")
        position = 12
        for name in AP_SECTIONS:
            packed = images[name]
            expected = expected_bins[name]
            compressed, = struct.unpack_from("<I", header, position)
            position += 4
            if name == "ap_flash.bin":
                if compressed != 0 or len(packed) != len(expected) or packed[4096:] != expected[4096:]:
                    raise ValueError("AP flash machine code mismatch")
                continue
            if compressed != 1:
                raise ValueError("Missing AP compression descriptor: " + name)
            size, = struct.unpack_from("<I", header, position)
            position += 4
            if size == 0 or size % 12 or position + size > 12 + info_size:
                raise ValueError("Invalid AP block descriptors")
            blocks = [struct.unpack_from("<III", header, start)
                      for start in range(position, position + size, 12)]
            position += size
            if sum(block[0] for block in blocks) != len(expected):
                raise ValueError("Decoded machine code size mismatch: " + name)
            cursor = 0
            decoded = bytearray()
            for raw_size, compressed_size, aligned_size in blocks:
                if not 5 < compressed_size <= aligned_size or cursor + aligned_size > len(packed):
                    raise ValueError("Invalid compressed block size")
                chunk = packed[cursor:cursor + compressed_size]
                properties = chunk[0]
                dictionary, = struct.unpack_from("<I", chunk, 1)
                if dictionary != 65536:
                    raise ValueError("Unexpected AP LZMA dictionary")
                decoder = lzma.LZMADecompressor(format=lzma.FORMAT_RAW, filters=[{
                    "id": lzma.FILTER_LZMA1, "dict_size": dictionary,
                    "lc": properties % 9, "lp": properties // 9 % 5, "pb": properties // 45,
                }])
                block = decoder.decompress(chunk[5:], max_length=raw_size + 1)
                if len(block) != raw_size:
                    raise ValueError("Decoded machine code block size mismatch")
                decoded.extend(block)
                cursor += aligned_size
            if cursor != len(packed) or decoded != expected:
                raise ValueError("Decoded machine code mismatch: " + name)
        if position != 12 + info_size:
            raise ValueError("Unexpected AP descriptor length")
    except (KeyError, IndexError, struct.error, lzma.LZMAError) as error:
        raise ValueError("Malformed AP compression data") from error


def verify_base_machine_code(base, image_path=None):
    """Reconstruct objcopy section bytes from the ELF, including address-gap zero fill."""
    expected = {}
    with (base / "baseline/ap_elf/ap.elf").open("rb") as stream:
        elf = ELFFile(stream)
        for name, section_names in AP_SECTIONS.items():
            sections = [elf.get_section_by_name(section) for section in section_names]
            if any(section is None for section in sections):
                raise ValueError("Missing linked AP sections: " + name)
            first = min(section["sh_addr"] for section in sections)
            last = max(section["sh_addr"] + section["sh_size"] for section in sections)
            content = bytearray(last - first)
            for section in sections:
                start = section["sh_addr"] - first
                content[start:start + section["sh_size"]] = section.data()
            expected[name] = bytes(content)
    verify_ap_image(image_path or base / "baseline/ap.img", expected)

def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def record_base(root, base_id):
    base = Path(root) / "out/project-base" / base_id
    verify_base_machine_code(base)
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
        verify_base_machine_code(base)
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
    verify_base_machine_code(Path(root) / "out/project-base" / base_id, image / names[0])
    result = {"product": spec["name"], "product_id": spec["id"],
              "board": spec["board"], "platform": spec["platform"],
              "storage_namespace": spec["storage_namespace"], "base": base,
              "sources": spec["sources"], "entry": spec["entry"],
              "hardware_verified": False,
              "sleep_mode": spec["sleep_mode"],
              "files": {p: sha256(image / p) for p in names}}
    (output / "release.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    return output
