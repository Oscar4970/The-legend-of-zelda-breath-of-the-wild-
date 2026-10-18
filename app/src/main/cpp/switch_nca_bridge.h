// Bridge milestone: bounded samples for *local* NCA header inspection.
// This copies only 0x400 ciphertext bytes per NCA, with no key handling.
// A user-provided key stream stays in Java during a local-only check.
#pragma once
#include "switch_deep_analyzer.h"
#include <array>
#include <cerrno>
#include <stdexcept>
#include <vector>
#include <sys/stat.h>

namespace ncabridge {
constexpr size_t kHeaderBytes = 0x400;
constexpr size_t kMaxNcaSamples = 32;
using Header = std::array<unsigned char, kHeaderBytes>;

inline std::vector<Header> sampleHeaders(int fd) {
    struct stat st{};
    if (fd < 0 || fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 16)
        throw std::invalid_argument("Selecciona un NSP local con acceso aleatorio.");
    const uint64_t size = static_cast<uint64_t>(st.st_size);
    const auto directory = switchdeep::parseDirectory(fd, 0, size, size, false);
    if (!directory.ok)
        throw std::invalid_argument("El indice PFS0/NSP no es legible.");
    std::vector<Header> result;
    for (const auto& file : directory.entries) {
        if (!switchdeep::suffix(file.name, ".nca")) continue;
        if (result.size() == kMaxNcaSamples)
            throw std::invalid_argument("Demasiados NCA; máximo 32 muestras.");
        if (file.bytes < kHeaderBytes)
            throw std::invalid_argument("NCA demasiado pequeño para cabecera.");
        Header data{};
        if (!switchdeep::read(fd, data.data(), data.size(), file.absolute))
            throw std::invalid_argument("No se pudo leer cabecera NCA.");
        result.push_back(data);
    }
    if (result.empty())
        throw std::invalid_argument("El NSP no contiene archivos NCA.");
    return result;
}
}  // namespace ncabridge
