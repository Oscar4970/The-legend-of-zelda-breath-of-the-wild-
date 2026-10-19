// Read-only, persistent virtual mount for a user-selected NSP.
// This is an INPUT API for a future BOTW runtime, not a Nintendo game executor.
// PFS0 records map virtual names to byte ranges inside the original local FD.
// No game bytes, keys, firmware or decrypted data are stored/exported.
#pragma once
#include "switch_deep_analyzer.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
#include <unistd.h>
#include <sys/stat.h>

namespace nspvfs {

class Mount final {
public:
    explicit Mount(int selectedFd) {
        if (selectedFd < 0) throw std::invalid_argument("Descriptor NSP invalido.");
        fd_ = dup(selectedFd);
        if (fd_ < 0) throw std::runtime_error("No se pudo conservar el acceso al NSP.");
        try {
            struct stat st{};
            if (fstat(fd_, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 16)
                throw std::invalid_argument("NSP requiere acceso aleatorio a archivo regular.");
            const auto length = static_cast<uint64_t>(st.st_size);
            const auto parsed = switchdeep::parseDirectory(fd_, 0, length, length, false);
            if (!parsed.ok || parsed.entries.empty())
                throw std::invalid_argument("PFS0 invalido o sin archivos.");
            entries_ = parsed.entries;
            names_.reserve(entries_.size());
            for (size_t i=0; i<entries_.size(); ++i) {
                const auto& item=entries_[i];
                // Duplicate virtual names must not select an ambiguous NCA.
                if (!names_.emplace(item.name, i).second)
                    throw std::invalid_argument("Nombre NSP duplicado.");
                if (switchdeep::suffix(item.name, ".nca")) ++ncaCount_;
            }
            if (ncaCount_ == 0)
                throw std::invalid_argument("El NSP montado no contiene NCA.");
        } catch (...) {
            close(fd_);
            fd_=-1;
            throw;
        }
    }

    Mount(const Mount&)=delete;
    Mount& operator=(const Mount&)=delete;
    Mount(Mount&&)=delete;
    Mount& operator=(Mount&&)=delete;
    ~Mount() { if (fd_>=0) close(fd_); }

    [[nodiscard]] size_t count() const noexcept { return entries_.size(); }
    [[nodiscard]] size_t ncaCount() const noexcept { return ncaCount_; }

    [[nodiscard]] bool exists(std::string_view name) const {
        return names_.find(std::string(name)) != names_.end();
    }

    [[nodiscard]] uint64_t sizeOf(std::string_view name) const {
        return lookup(name).bytes;
    }

    // A future runtime can request any bounded slice on demand. The returned
    // bytes are RAW (the NCA remains encrypted). This does not load the NSP in RAM.
    void read(std::string_view name, uint64_t relativeOffset, void* dest, size_t bytes) const {
        if (bytes != 0 && dest == nullptr)
            throw std::invalid_argument("Buffer nulo.");
        const auto& item=lookup(name);
        if (relativeOffset > item.bytes || bytes > item.bytes-relativeOffset)
            throw std::out_of_range("Lectura virtual fuera del NCA.");
        if (bytes==0) return;
        if (!switchfile::readAt(fd_, dest, bytes, item.absolute+relativeOffset))
            throw std::runtime_error("No se pudo leer una pagina del NSP.");
    }

    [[nodiscard]] std::string diagnostic() const {
        return "RUNTIME VFS / NSP MONTADO (SOLO LECTURA)\n"
               "Entradas virtuales: "+std::to_string(count())+"\n"
               "NCA disponibles como archivos virtuales: "+std::to_string(ncaCount())+"\n"
               "Los NCA permanecen cifrados; aun NO se montan ExeFS ni RomFS.\n"
               "El descriptor del NSP queda abierto SOLO mientras exista el montaje.\n"
               "No se instala firmware ni se ejecuta Zelda.\n";
    }

    [[nodiscard]] std::string selfTest() const {
        // Only 3 bytes read from the NCA with largest file size, in separate
        // virtual offsets. Demonstrates that the runtime can page game data.
        const switchdeep::Entry* largest=nullptr;
        for (const auto& file: entries_) {
            if (switchdeep::suffix(file.name,".nca") &&
                (!largest || file.bytes>largest->bytes))
                largest=&file;
        }
        if (!largest || largest->bytes<3)
            throw std::runtime_error("NCA sin capacidad para prueba aleatoria.");
        unsigned char sample[3]{};
        read(largest->name,0,sample,1);
        read(largest->name,largest->bytes/2,sample+1,1);
        read(largest->name,largest->bytes-1,sample+2,1);
        // Do not print bytes of proprietary game content.
        return "PRUEBA RUNTIME VFS\n"
               "3 lecturas aleatorias del mismo NCA: OK (inicio/mitad/final).\n"
               "Tamano del NCA virtual: "+std::to_string(largest->bytes)+" bytes\n"
               "Memoria utilizada para paginas: 3 bytes de datos.\n"
               "Se leyo desde el NSP original, sin crear copias ni extraer archivos.\n"
               "Esto aun NO descifra NCA ni ejecuta instrucciones del juego.\n";
    }

private:
    const switchdeep::Entry& lookup(std::string_view name) const {
        const auto iter=names_.find(std::string(name));
        if (iter == names_.end()) throw std::out_of_range("Archivo virtual inexistente.");
        return entries_.at(iter->second);
    }
    int fd_=-1;
    std::vector<switchdeep::Entry> entries_{};
    std::unordered_map<std::string,size_t> names_{};
    size_t ncaCount_=0;
};

} // namespace nspvfs
