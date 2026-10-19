#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Zip archives: reading (stored + deflate entries) and writing (deflate). Replaces
// System.IO.Compression.ZipFile / ZipArchive.
namespace SonicRacingSaveManager::Zip {

struct Entry {
    std::wstring fullName;   // "Mods/LoadManip/main.lua" (forward slashes, as stored)
    std::wstring name;       // last path segment ("" for directory entries)
    uint64_t compressedSize = 0;
    uint64_t length = 0;     // uncompressed size
    uint16_t method = 0;     // 0 stored, 8 deflate
    uint64_t localHeaderOffset = 0;
    bool IsDirectory() const { return !fullName.empty() && (fullName.back() == L'/' || fullName.back() == L'\\'); }
};

class Archive {
public:
    static std::optional<Archive> Open(const std::wstring& path);
    static std::optional<Archive> FromMemory(std::string bytes);

    const std::vector<Entry>& Entries() const { return entries; }
    const Entry* Find(const std::wstring& fullName) const;  // case-insensitive, '/' or '\\'

    std::optional<std::string> Read(const Entry& entry) const;
    bool ExtractEntry(const Entry& entry, const std::wstring& destinationFile) const;
    // Same as ZipFile.ExtractToDirectory. With overwrite = false, files that already exist are kept.
    bool ExtractAll(const std::wstring& directory, bool overwrite, std::wstring* error = nullptr) const;

private:
    bool Parse();
    std::string data;
    std::vector<Entry> entries;
};

bool ExtractToDirectory(const std::wstring& zipPath, const std::wstring& directory, bool overwrite,
                        std::wstring* error = nullptr);

// Builds a zip in memory.
class Writer {
public:
    // `lastWriteUtc` (FILETIME ticks, 0 = 1980-01-01) becomes the entry's DOS date, as ZipFile wrote it
    void Add(const std::wstring& entryName, const std::string& data, bool compress = true, int64_t lastWriteUtc = 0);
    std::string Finish();

private:
    struct Written {
        std::string name;
        uint32_t crc = 0;
        uint32_t compressed = 0, size = 0;
        uint16_t method = 0;
        uint16_t dosTime = 0, dosDate = 0x21;
        uint32_t offset = 0;
    };
    std::string out;
    std::vector<Written> written;
};

// Raw DEFLATE (RFC 1951) helpers.
bool Inflate(const uint8_t* data, size_t size, std::string& out, size_t expectedSize = 0);
std::string Deflate(const std::string& data);
uint32_t Crc32(const void* data, size_t size, uint32_t crc = 0);

} // namespace SonicRacingSaveManager::Zip
