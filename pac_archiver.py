import sys
import zlib
from pathlib import Path
from struct import unpack, pack
import argparse
import json

def read_u32le(f):
    return unpack("<I", f.read(4))[0]

def pack_u32le(u32):
    return pack("<I", u32)

def ceil4(n):
    return ((n + 3) // 4) * 4

def unpack_pac(input_file, output_dir):
    p_in = Path(input_file).resolve()
    if output_dir:
        p_out = Path(output_dir).resolve() / p_in.stem
    else:
        p_out = p_in.parent / p_in.stem
    p_out.mkdir(parents=True, exist_ok=True)

    file_info = []

    with open(p_in, "rb") as f:
        magic = f.read(4)
        if magic != b"PAC\x00":
            print("magic != PAC\x00")
            sys.exit(1)
        file_count = read_u32le(f)
        for i in range(file_count):
            off = 8 + i * 4
            f.seek(off)
            file_off = read_u32le(f)
            file_off_end = read_u32le(f)
            file_size = file_off_end - file_off
            f.seek(file_off)
            data = f.read(file_size)
            file_entry = {"index": i, "size": file_size}
            file_name = f"{i:08d}"
            if data[:4] == b"NZIP":
                try:
                    u = zlib.decompress(data[8:], wbits=-15)
                    if u[:4] == b"GBIX":
                        p = p_out / (file_name + ".pvr")
                        p.write_bytes(u)
                        file_entry["type"] = "pvr"
                    elif u[:4] == b"SDCP":
                        p = p_out / (file_name + ".sdcp")
                        p.write_bytes(u)
                        file_entry["type"] = "sdcp"
                    else:
                        p = p_out / (file_name + ".bin")
                        p.write_bytes(u)
                        file_entry["type"] = "bin"
                except zlib.error:
                    p = p_out / (file_name + ".nzip")
                    p.write_bytes(data)
                    file_entry["type"] = "nzip"
            elif data[:4] == b"SDCP":
                p = p_out / (file_name + ".sdcp")
                p.write_bytes(data)
                file_entry["type"] = "sdcp"
            else:
                p = p_out / (file_name + ".bin")
                p.write_bytes(data)
                file_entry["type"] = "bin"
            file_info.append(file_entry)

    info_json_path = p_out / "info.json"
    with info_json_path.open("w") as info_json_file:
        json.dump(file_info, info_json_file, indent=4)

def pack_pac(input_folder, output_file):
    input_folder = Path(input_folder)
    output_file = Path(output_file)

    info_json_path = input_folder / "info.json"
    if info_json_path.exists():
        with info_json_path.open("r") as info_json_file:
            file_info = json.load(info_json_file)
    else:
        print("No info.json found. Processing files as usual.")
        file_info = None

    files = []
    if file_info:
        for entry in file_info:
            file_path = input_folder / f"{entry['index']:08d}.{entry['type']}"
            if file_path.exists():
                data = file_path.read_bytes()
                if entry['type'] == 'pvr':
                    c = zlib.compress(data, level=9, wbits=-15)
                    c = b"NZIP" + pack_u32le(len(data)) + c
                    files.append(c)
                elif entry['type'] == 'nzip':
                    u = file_path.read_bytes()
                    c = zlib.compress(u, level=9, wbits=-15)
                    c = b"NZIP" + pack_u32le(len(u)) + c
                    files.append(c)
                else:
                    files.append(data)
            else:
                print(f"File {file_path} not found. Skipping.")
    else:
        file_paths = sorted(input_folder.glob('*'))
        for file_path in file_paths:
            if file_path.suffix in ['.pvr', '.nzip', '.bin', '.sdcp']:
                data = file_path.read_bytes()
                if file_path.suffix == '.pvr':
                    c = zlib.compress(data, level=9, wbits=-15)
                    c = b"NZIP" + pack_u32le(len(data)) + c
                    files.append(c)
                elif file_path.suffix == '.nzip':
                    u = file_path.read_bytes()
                    c = zlib.compress(u, level=9, wbits=-15)
                    c = b"NZIP" + pack_u32le(len(u)) + c
                    files.append(c)
                else:
                    files.append(data)
            else:
                print(f"Skipping {file_path.name}")

    with open(output_file, "wb") as f:
        f.write(b"PAC\x00")
        f.write(pack_u32le(len(files)))
        off = 8 + len(files) * 4 + 4
        for i in range(len(files)):
            f.write(pack_u32le(off))
            off += ceil4(len(files[i]))
        f.write(pack_u32le(off))
        for i in range(len(files)):
            padded = files[i].ljust(ceil4(len(files[i])), b"\x00")
            f.write(padded)

def main():
    parser = argparse.ArgumentParser(
        description="Unpack or pack a PAC file.",
        epilog="Examples:\n"
               "  python script.py unpack example.pac -d ./uncompressed\n"
               "  python script.py unpack example.pac\n"
               "  python script.py pack ./uncompressed/example example.new.pac",
        formatter_class=argparse.RawTextHelpFormatter
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    unpack_parser = subparsers.add_parser("unpack", help="Unpack a PAC file.")
    unpack_parser.add_argument("input_file", type=str, help="The input PAC file to unpack.")
    unpack_parser.add_argument("-d", "--directory", type=str, default=None, help="The output directory for unpacked files.")

    pack_parser = subparsers.add_parser("pack", help="Pack files into a PAC file.")
    pack_parser.add_argument("input_folder", type=str, help="The input folder containing files to pack.")
    pack_parser.add_argument("output_file", type=str, help="The output PAC file.")

    args = parser.parse_args()

    if args.command == "unpack":
        unpack_pac(args.input_file, args.directory)
    elif args.command == "pack":
        pack_pac(args.input_folder, args.output_file)

if __name__ == "__main__":
    main()
