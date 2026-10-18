#!/usr/bin/env python3
"""Recopila SOLO referencias libres fijadas de Eden para investigar BOTW nativo.

No genera un juego, un motor nativo, ROM, APK, shaders de BOTW ni claves.
Todas las fuentes recopiladas conservan sus cabeceras SPDX y la GPL de Eden.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "port_botw" / "eden_source_manifest.json"
OWN_FILES = (
    "port_botw/README.md",
    "port_botw/build_port.py",
    "port_botw/game.cpp",
    "recomp_aot/build_upstream_recomp.py",
)
KIT_NOTE = """KIT DE REFERENCIAS EDEN / PORT NATIVO BOTW - NO JUGABLE

Origen: https://github.com/eden-emulator/mirror
Eden: GPL-3.0-or-later; preservar ademas las licencias SPDX individuales.
El listado exacto de archivos, hashes Git y commit esta en manifest.json.

Este ZIP NO compila como port independiente: son componentes interdependientes
del emulador, acompanados de la base privada de recompilacion ya existente.
No contiene codigo de Zelda ni ROM ni firmware ni claves; no arranca Zelda.

La ruta de port utiliza SwitchGamesRecomp (base MIT) para recompilar un modulo
Switch legitimamente extraido de forma local; Eden aporta referencias tecnicas
de Vulkan, cachés y decodificacion Maxwell, NO el codigo del juego.

Mezclar/modificar codigo GPL de Eden en un runtime distribuido implica cumplir
su GPLv3 y revisar las licencias de los demas componentes. No copiar a ciegas.

La Xclipse 550 usa el Vulkan del sistema Samsung: no usar Turnip Adreno
ni suplantar capacidades. Priorizar estabilidad y pruebas reproducibles.
"""


class KitError(Exception):
    pass


def git_blob_sha1(data: bytes) -> str:
    payload = b"blob " + str(len(data)).encode("ascii") + b"\\0" + data
    return hashlib.sha1(payload).hexdigest()


def load_manifest() -> dict:
    data = json.loads(MANIFEST.read_text(encoding="utf-8"))
    if data.get("schema") != 1 or len(data.get("files", [])) < 3:
        raise KitError("Manifest vacio o incompatible")
    seen = set()
    for item in data["files"]:
        path = item["path"]
        p = PurePosixPath(path)
        if (not path.startswith("src/") or p.is_absolute() or
                ".." in p.parts or "\\\\" in path or path in seen):
            raise KitError("Ruta insegura o duplicada: " + path)
        if len(item["sha"]) != 40 or any(c not in "0123456789abcdef" for c in item["sha"]):
            raise KitError("SHA Git invalido: " + path)
        seen.add(path)
    return data


def verify_source_checkout(eden_root: Path, manifest: dict) -> None:
    if not (eden_root / ".git").exists():
        raise KitError("Se requiere checkout Git de Eden")
    actual = subprocess.check_output(
        ["git", "-C", str(eden_root), "rev-parse", "HEAD"], text=True
    ).strip()
    if actual != manifest["source_commit"]:
        raise KitError("Revision de Eden incorrecta: " + actual)


def make_kit(eden_root: Path, output: Path) -> int:
    data = load_manifest()
    verify_source_checkout(eden_root, data)
    license_path = eden_root / "LICENSE.txt"
    if not license_path.is_file() or license_path.is_symlink():
        raise KitError("Falta el texto original de licencia de Eden")
    staged = [("LICENSE_Eden_GPLv3.txt", license_path.read_bytes())]
    for item in data["files"]:
        source = eden_root / item["path"]
        if not source.is_file() or source.is_symlink():
            raise KitError("Archivo ausente o enlace no permitido: " + item["path"])
        contents = source.read_bytes()
        if git_blob_sha1(contents) != item["sha"]:
            raise KitError("SHA distinto del fijado: " + item["path"])
        if b"SPDX-License-Identifier:" not in contents[:1000]:
            raise KitError("Falta atribucion SPDX: " + item["path"])
        staged.append(("eden/" + item["path"], contents))

    for rel in OWN_FILES:
        source = ROOT / rel
        if not source.is_file() or source.is_symlink():
            raise KitError("Falta fuente del port existente: " + rel)
        staged.append(("port_existente/" + rel, source.read_bytes()))

    output = output.resolve()
    if output.suffix.lower() != ".zip":
        raise KitError("La salida debe terminar en .zip")
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED) as z:
        z.writestr("LEEME_PRIMERO.txt", KIT_NOTE)
        z.writestr("manifest.json", json.dumps(data, indent=2, ensure_ascii=False) + "\\n")
        for name, payload in staged:
            z.writestr(name, payload)
    print(f"KIT VERIFICADO: {output} ({len(data['files'])} archivos de referencia)")
    print("NO es un port ni una APK de Zelda.")
    return 0


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--eden-root", required=True, type=Path)
    p.add_argument("--output", required=True, type=Path)
    args = p.parse_args()
    try:
        return make_kit(args.eden_root.resolve(), args.output)
    except (KitError, OSError, subprocess.CalledProcessError) as exc:
        print("KIT BLOQUEADO:", exc, file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
