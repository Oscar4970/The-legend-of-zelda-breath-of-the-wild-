#!/usr/bin/env python3
"""Generate deterministic C++ SPIR-V arrays from open, self-authored GLSL.
Requires glslangValidator and spirv-val on PATH. No game shaders involved.
"""
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "app/src/main/cpp/generated_triangle_spirv.h"


def generate():
    glslang = shutil.which("glslangValidator")
    validator = shutil.which("spirv-val")
    if not glslang or not validator:
        raise SystemExit("Install glslang-tools and spirv-tools before Android build")
    parts = ["#pragma once", "#include <cstdint>", 
             "// Generated from this repository's own GLSL; not Nintendo code.",
             "namespace triangle_spirv {"]
    with tempfile.TemporaryDirectory() as folder:
        for stage, name in [("vert", "vertex"), ("frag", "fragment")]:
            source = ROOT / f"shaders/triangle.{stage}"
            target = Path(folder) / f"{stage}.spv"
            subprocess.run([glslang, "-V", "--target-env", "vulkan1.3",
                            "-S", stage, str(source), "-o", str(target)],
                           check=True)
            subprocess.run([validator, "--target-env", "vulkan1.3", str(target)],
                           check=True)
            raw = target.read_bytes()
            if len(raw) % 4:
                raise SystemExit(f"SPIR-V byte count not word-aligned: {stage}")
            words = struct.unpack(f"<{len(raw)//4}I", raw)
            if words[0] != 0x07230203:
                raise SystemExit("Invalid SPIR-V magic")
            parts.append(f"static constexpr std::uint32_t {name}[] = {{")
            for offset in range(0, len(words), 8):
                parts.append("    " + ", ".join(f"0x{value:08x}u"
                          for value in words[offset:offset + 8]) + ",")
            parts.append("};")
    parts.append("} // namespace triangle_spirv")
    OUT.write_text("\n".join(parts) + "\n", encoding="utf-8")
    print(f"Generated and validated SPIR-V header: {OUT}")


if __name__ == "__main__":
    generate()
