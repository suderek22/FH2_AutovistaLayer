"""Build the Audi RS 4 Avant LOD0 showroom prototype from a stock Azimuth archive."""

import argparse
import json
from pathlib import Path
from struct import pack_into, unpack_from
import sys
from zipfile import ZipFile

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT.parent / "CarPackStudio"))
from cps.bundle import Bundle

CAR = "AUD_RS4Avant_13"
LOD0 = 2
SHOWROOM = 1
PAIRS = {
    "Exterior/BumperR/trunkBay_a__SLOD": ("Exterior/BumperR/trunkBay_a", 0x3E),
    "Exterior/Engine/engineBay_a__SLOD": ("Exterior/Engine/engineBay_a", 0x7E),
    "Exterior/Engine/engine_a": None,
    "Exterior/Hood/hoodLiner_a__SLOD": ("Exterior/Hood/hoodLiner_a", 0x3F),
    "Exterior/Trunk/trunkLiner_a__SLOD": ("Exterior/Trunk/trunkLiner_a", 0x3F),
    "Interior/Floor/pedals_a__SLOD": ("Interior/Floor/pedals_a", 0x03),
}


def read_mask(data, offset):
    return unpack_from("<H", data, offset)[0]


def model_path(part):
    return "scene/" + part + ".modelbin"


def patch_scene(scene, member, expected, replacement):
    path = f"game:/media/cars/{CAR}/{member}".replace("/", "\\").lower().encode()
    start = scene.lower().find(path)
    end = start + len(path)
    # A carbin render-model path is prefixed with its length and followed by
    # a 64-byte transform and the model's two-byte LOD mask.
    if start < 4 or scene.lower().find(path, end) != -1:
        raise ValueError(f"Expected one carbin reference to {member}")
    offset = end + 64
    if unpack_from("<I", scene, start - 4)[0] != len(path) or read_mask(scene, offset) != expected:
        raise ValueError(f"Unexpected carbin layout or LOD mask for {member}")
    pack_into("<H", scene, offset, replacement)


def promote_model(data, member):
    bundle = Bundle(data)
    if len(bundle.all("Modl")) != 1 or not bundle.all("Mesh"):
        raise ValueError(f"Missing model header or meshes in {member}")
    output = bytearray(data)
    for index, blob in enumerate(bundle.blobs):
        if blob.tag not in {"Modl", "Mesh"}:
            continue
        offset = unpack_from("<I", data, 16 + 24 * index + 12)[0]
        if blob.tag == "Modl":
            if (blob.major, blob.minor) != (1, 1) or blob.data[9:] != b"\xff\xff\x00\x01\x00":
                raise ValueError(f"Unexpected showroom model header in {member}")
            output[offset + 9] = 0
            pack_into("<H", output, offset + 12, SHOWROOM | LOD0)
        else:
            name = blob.meta.get("Name", b"").rstrip(b"\0").lower()
            if name == b"shadow" and read_mask(blob.data, 0) == 0xFFFF:
                continue
            if blob.data[4:8] != b"\x01\x00\xff\xff" or not name.endswith(b"_lods"):
                raise ValueError(f"Unexpected showroom mesh in {member}: {name!r}")
            pack_into("<H", output, offset + 4, SHOWROOM | LOD0)
            output[offset + 6] = 0
    Bundle(output)
    return output


def build(archive, output):
    carbin = CAR + ".carbin"
    files = {}
    with ZipFile(archive) as car:
        scene = bytearray(car.read(carbin))
        for showroom, stock in PAIRS.items():
            member = model_path(showroom)
            patch_scene(scene, member, SHOWROOM, SHOWROOM | LOD0)
            files[member] = promote_model(car.read(member), member)
            if stock:
                stock_part, stock_mask = stock
                patch_scene(scene, model_path(stock_part), stock_mask, stock_mask & ~LOD0)
        files[carbin] = scene

    if output.exists():
        raise FileExistsError(f"Choose a new output directory: {output}")
    car_directory = output / "media" / "cars" / CAR
    for member, data in files.items():
        path = car_directory / member
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    metadata = {
        "format_version": 1,
        "id": "fh2-autovista-layer",
        "name": "FH2_AutovistaLayer",
        "version": "0.1.0",
        "description": "Experimental LOD0 showroom geometry for the 2013 Audi RS 4 Avant.",
        "media": "media",
        "dependencies": [],
    }
    (output / "mod.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    print(f"Built {len(files)} Audi overrides in {output}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path, help=f"Path to the original {CAR}.zip")
    parser.add_argument("output", type=Path, help="New directory for the Azimuth mod")
    args = parser.parse_args()
    build(args.archive, args.output)
