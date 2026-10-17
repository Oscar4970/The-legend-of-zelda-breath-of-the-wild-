#pragma once
// NSO0 metadata parser for legitimate, user-provided *decrypted* Nintendo Switch modules.
// Does not execute code, bypass encryption, or ingest game assets.
#include <array>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>

namespace nso {
constexpr std::size_t kHeaderSize = 0x100;
constexpr std::uint64_t kMaxFileSize = 512ULL * 1024 * 1024;
constexpr std::uint64_t kMaxUncompressedSegment = 256ULL * 1024 * 1024;
struct Section {
    std::uint32_t fileOffset = 0;
    std::uint32_t memOffset = 0;
    std::uint32_t memSize = 0;
    std::uint32_t storedSize = 0;
    bool compressed = false;
    bool hashRequired = false;
};
struct Info {
    bool valid = false;
    std::uint32_t version = 0;
    std::uint32_t flags = 0;
    std::array<Section,3> sections{};
    std::uint32_t bssSize = 0;
    std::string moduleId;
    std::string reason;
};
inline std::uint32_t read32(const unsigned char* p, std::size_t at) {
    return static_cast<std::uint32_t>(p[at])
       | static_cast<std::uint32_t>(p[at+1]) << 8
       | static_cast<std::uint32_t>(p[at+2]) << 16
       | static_cast<std::uint32_t>(p[at+3]) << 24;
}
inline std::string hexByte(unsigned char value) {
    constexpr char chars[] = "0123456789abcdef";
    return std::string{chars[value >> 4],chars[value & 15]};
}
inline Info inspect(const unsigned char* bytes, std::size_t available, std::uint64_t fileSize) {
    Info out;
    if (!bytes || available < kHeaderSize) {
        out.reason = "Cabecera incompleta (se necesitan 256 bytes).";
        return out;
    }
    if (fileSize < kHeaderSize || fileSize > kMaxFileSize) {
        out.reason = "Tamano fuera del limite de lectura segura (256 bytes - 512 MiB).";
        return out;
    }
    if (bytes[0]!='N' || bytes[1]!='S' || bytes[2]!='O' || bytes[3]!='0') {
        out.reason = "Firma invalida: no es un ejecutable NSO0 sin cifrar.";
        return out;
    }
    out.version = read32(bytes,0x04);
    out.flags = read32(bytes,0x0c);
    // Segments: .text, .rodata, .data; offsets per switchbrew.org/wiki/NSO0
    constexpr std::size_t sectionOffsets[] = {0x10,0x20,0x30};
    for (int i=0; i<3; ++i) {
        Section& s = out.sections[i];
        const std::size_t at = sectionOffsets[i];
        s.fileOffset = read32(bytes,at);
        s.memOffset = read32(bytes,at+4);
        s.memSize = read32(bytes,at+8);
        s.storedSize = read32(bytes,0x60+4*i);
        s.compressed = (out.flags & (1u<<i)) != 0;
        s.hashRequired = (out.flags & (1u<<(i+3))) != 0;
        if (s.memSize > kMaxUncompressedSegment) {
            out.reason = "Segmento supera el maximo permitido de 256 MiB.";
            return out;
        }
        if (s.memSize == 0) {
            if (s.storedSize != 0) {
                out.reason = "Segmento vacio con bytes almacenados.";
                return out;
            }
            continue;
        }
        std::uint64_t physicalSize = s.compressed ? s.storedSize : s.memSize;
        if (!s.compressed && s.storedSize != 0 && s.storedSize != s.memSize) {
            out.reason = "Tamano almacenado inconsistente para segmento sin compresion.";
            return out;
        }
        if (physicalSize == 0 || s.fileOffset < kHeaderSize
            || static_cast<std::uint64_t>(s.fileOffset) + physicalSize > fileSize) {
            out.reason = "Segmento apunta fuera del archivo o tiene tamano cero.";
            return out;
        }
        if (static_cast<std::uint64_t>(s.memOffset) + s.memSize > 0x100000000ULL) {
            out.reason = "Segmento excede espacio de direccion de 32 bits.";
            return out;
        }
    }
    // Verify that stored regions never overlap (even if offsets are out of order).
    for (int i=0;i<3;i++) {
        const Section& a=out.sections[i];
        if (!a.memSize) continue;
        auto as=static_cast<std::uint64_t>(a.fileOffset);
        auto ae=as+(a.compressed?a.storedSize:a.memSize);
        for (int j=i+1;j<3;j++) {
            const Section& b=out.sections[j];
            if (!b.memSize) continue;
            auto bs=static_cast<std::uint64_t>(b.fileOffset);
            auto be=bs+(b.compressed?b.storedSize:b.memSize);
            if (as<be && bs<ae) {
                out.reason = "Segmentos superpuestos en el archivo.";
                return out;
            }
        }
    }
    out.bssSize = read32(bytes,0x3c);
    for (int i=0;i<32;++i) out.moduleId += hexByte(bytes[0x40+i]);
    out.valid = true;
    out.reason = "NSO0 reconocido. Solo se analizaron metadatos; NO se ejecuto codigo.";
    return out;
}
inline std::string report(const Info& info) {
    if (!info.valid) return "NSO rechazado: " + info.reason;
    static constexpr const char* names[] = {".text", ".rodata", ".data"};
    std::ostringstream s;
    s << "NSO0 válido (metadatos)\nVersion de cabecera: " << info.version
      << "\nModule ID: " << info.moduleId
      << "\nBSS: " << info.bssSize << " bytes";
    for(int i=0;i<3;i++){
        auto x=info.sections[i];
        s << "\n" << names[i] << ": " << x.memSize << " bytes, "
          << (x.compressed?"comprimido":"sin comprimir")
          << ", offset " << x.fileOffset << ", "
          << (x.hashRequired?"SHA-256 solicitado (NO verificado)":"hash no requerido");
    }
    s << "\n\nSolo inspeccion de cabecera; NO ejecuta juegos ni verifica firmas criptograficas.";
    return s.str();
}
} // namespace nso
