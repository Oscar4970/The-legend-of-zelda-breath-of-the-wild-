#!/usr/bin/env python3
"""Privacy-preserving local inspection of a user-provided Switch container.

Supported container headers: PFS0 (.nsp) and HEAD (.xci).
This is NOT an NSP/XCI decryptor, an emulator, a loader or a Zelda executable.
It never uploads, rewrites or extracts a game and never reads the whole file.
No copyrighted sample data, keys or firmware are included.
"""
import argparse
import json
import os
from pathlib import Path
import struct
import sys


class ContainerError(ValueError):
    pass


MAX_FILES = 20000
MAX_STRING_TABLE = 8 * 1024 * 1024
MAX_HEADER_READ = 1024 * 1024
READ_XCI = 0x104


def inspect_file(path: Path) -> dict:
    size = path.stat().st_size
    if size < 16:
        raise ContainerError("Archivo demasiado pequeño para una cabecera Switch.")
    with path.open("rb") as stream:
        signature = stream.read(4)
        if signature == b"PFS0":
            return _inspect_pfs0(stream, size)
        if size >= READ_XCI:
            stream.seek(0x100)
            if stream.read(4) == b"HEAD":
                return {
                    "format": "XCI", "bytes": size, "valid_header": True,
                    "status": "Cabecera HEAD identificada. Particiones y contenido no extraidos.",
                    "encrypted_contents": "No verificable sin extraccion autorizada.",
                    "game_detected": False,
                    "bootable": False,
                }
        raise ContainerError("Formato no reconocido. Se esperaba PFS0 (NSP) o HEAD en 0x100 (XCI).")


def _inspect_pfs0(stream, size: int) -> dict:
    counts = stream.read(12)
    if len(counts) != 12:
        raise ContainerError("Cabecera PFS0 truncada.")
    count, string_length, reserved = struct.unpack("<III", counts)
    if count > MAX_FILES or string_length > MAX_STRING_TABLE:
        raise ContainerError("PFS0 declara una tabla desproporcionada; se rechaza.")
    end_entries = 16 + count * 24
    first_data = end_entries + string_length
    if first_data > size:
        raise ContainerError("PFS0: tabla de archivos o nombres truncada.")
    # Read metadata only. Never load NCA payloads or keys into memory.
    table = stream.read(count * 24)
    names = stream.read(string_length)
    if len(table) != count * 24 or len(names) != string_length:
        raise ContainerError("PFS0: metadatos incompletos.")
    entries = []
    for index in range(count):
        off, length, nameoff, _ = struct.unpack_from("<QQII", table, index * 24)
        if nameoff >= len(names):
            raise ContainerError(f"PFS0: nombre fuera de rango en entrada {index}.")
        end = names.find(b"\0", nameoff)
        if end < 0:
            raise ContainerError(f"PFS0: nombre sin terminador en entrada {index}.")
        raw_name = names[nameoff:end]
        try:
            name = raw_name.decode("utf-8", errors="strict")
        except UnicodeDecodeError as exc:
            raise ContainerError("PFS0: nombre no es UTF-8 valido.") from exc
        if not name or name in (".", "..") or "/" in name or "\\" in name or any(ord(c) < 32 for c in name):
            raise ContainerError("PFS0: nombre de archivo no seguro.")
        # Files are stored at offsets relative to the data region.
        if off > size - first_data or length > size - first_data - off:
            raise ContainerError(f"PFS0: entrada {index} excede el tamaño del contenedor.")
        entries.append({"name": name, "bytes": length})
    kinds = {}
    for item in entries:
        suffix = Path(item["name"]).suffix.lower() or "(sin extension)"
        kinds[suffix] = kinds.get(suffix, 0) + 1
    return {
        "format": "NSP/PFS0", "bytes": size, "valid_header": True,
        "entries": count, "types": kinds,
        "file_list": entries[:100],
        "list_truncated": count > 100,
        "encrypted_contents": "NSP normalmente contiene NCAs cifrados; no se descifran.",
        "game_detected": False,
        "bootable": False,
    }


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Inspeccion local de cabeceras NSP/XCI. Sin descifrar, ejecutar ni subir archivos.")
    ap.add_argument("container", type=Path, help="Ruta local a una copia del juego obtenida legalmente")
    ap.add_argument("--json", type=Path, default=None,
                    help="Guardar SOLO el informe de metadatos JSON en una ruta local")
    args = ap.parse_args()
    try:
        report = inspect_file(args.container)
    except (OSError, ContainerError) as exc:
        print(f"No se pudo inspeccionar: {exc}", file=sys.stderr)
        return 2
    output = json.dumps(report, ensure_ascii=False, indent=2)
    if args.json is not None:
        args.json.write_text(output + "\n", encoding="utf-8")
        print(f"Informe de metadatos guardado localmente: {args.json}")
    else:
        print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
