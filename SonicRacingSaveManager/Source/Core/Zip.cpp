#include "Core/Zip.h"
#include <windows.h>
#include <algorithm>
#include <array>
#include <cstring>
#include "Core/Files.h"
#include "Core/Text.h"

namespace SonicRacingSaveManager::Zip {

// ── CRC-32 ──────────────────────────────────────────────────────────────────

uint32_t Crc32(const void* data, size_t size, uint32_t crc) {
    static const std::array<uint32_t, 256> table = [] {
        std::array<uint32_t, 256> t{};
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[i] = c;
        }
        return t;
    }();
    crc = ~crc;
    const auto* p = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < size; ++i) crc = table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

// ── Inflate ─────────────────────────────────────────────────────────────────

namespace {

struct BitReader {
    const uint8_t* data;
    size_t size;
    size_t pos = 0;
    uint32_t bitBuffer = 0;
    int bitCount = 0;

    bool Need(int n) {
        while (bitCount < n) {
            if (pos >= size) return false;
            bitBuffer |= static_cast<uint32_t>(data[pos++]) << bitCount;
            bitCount += 8;
        }
        return true;
    }
    bool Bits(int n, uint32_t& v) {
        if (n == 0) { v = 0; return true; }
        if (!Need(n)) return false;
        v = bitBuffer & ((1u << n) - 1);
        bitBuffer >>= n;
        bitCount -= n;
        return true;
    }
    void AlignByte() {
        bitBuffer >>= bitCount & 7;
        bitCount -= bitCount & 7;
    }
};

struct Huffman {
    uint16_t counts[16]{};
    uint16_t symbols[288]{};

    bool Build(const uint8_t* lengths, int n) {
        std::memset(counts, 0, sizeof(counts));
        for (int i = 0; i < n; ++i) counts[lengths[i]]++;
        counts[0] = 0;
        uint16_t offsets[16]{};
        for (int i = 1; i < 16; ++i) offsets[i] = static_cast<uint16_t>(offsets[i - 1] + counts[i - 1]);
        for (int i = 0; i < n; ++i)
            if (lengths[i]) symbols[offsets[lengths[i]]++] = static_cast<uint16_t>(i);
        return true;
    }

    int Decode(BitReader& br) const {
        int code = 0, first = 0, index = 0;
        for (int len = 1; len < 16; ++len) {
            uint32_t bit = 0;
            if (!br.Bits(1, bit)) return -1;
            code |= static_cast<int>(bit);
            const int count = counts[len];
            if (code - count < first) return symbols[index + (code - first)];
            index += count;
            first += count;
            first <<= 1;
            code <<= 1;
        }
        return -1;
    }
};

const uint16_t kLengthBase[] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
const uint8_t kLengthExtra[] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
const uint16_t kDistBase[] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
const uint8_t kDistExtra[] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

bool InflateBlock(BitReader& br, std::string& out, const Huffman& lit, const Huffman& dist) {
    while (true) {
        const int sym = lit.Decode(br);
        if (sym < 0) return false;
        if (sym < 256) {
            out += static_cast<char>(sym);
            continue;
        }
        if (sym == 256) return true;
        const int li = sym - 257;
        if (li >= 29) return false;
        uint32_t extra = 0;
        if (!br.Bits(kLengthExtra[li], extra)) return false;
        const size_t length = kLengthBase[li] + extra;
        const int ds = dist.Decode(br);
        if (ds < 0 || ds >= 30) return false;
        if (!br.Bits(kDistExtra[ds], extra)) return false;
        const size_t distance = kDistBase[ds] + extra;
        if (distance > out.size()) return false;
        const size_t from = out.size() - distance;
        for (size_t i = 0; i < length; ++i) out += out[from + i];
    }
}

} // namespace

bool Inflate(const uint8_t* data, size_t size, std::string& out, size_t expectedSize) {
    BitReader br{ data, size };
    if (expectedSize) out.reserve(out.size() + expectedSize);

    Huffman fixedLit, fixedDist;
    {
        uint8_t lengths[288];
        for (int i = 0; i < 144; ++i) lengths[i] = 8;
        for (int i = 144; i < 256; ++i) lengths[i] = 9;
        for (int i = 256; i < 280; ++i) lengths[i] = 7;
        for (int i = 280; i < 288; ++i) lengths[i] = 8;
        fixedLit.Build(lengths, 288);
        uint8_t d[30];
        std::fill(std::begin(d), std::end(d), static_cast<uint8_t>(5));
        fixedDist.Build(d, 30);
    }

    uint32_t last = 0;
    do {
        uint32_t type = 0;
        if (!br.Bits(1, last) || !br.Bits(2, type)) return false;
        if (type == 0) {
            br.AlignByte();
            uint32_t len = 0, nlen = 0;
            if (!br.Bits(16, len) || !br.Bits(16, nlen)) return false;
            if ((len ^ 0xFFFF) != nlen) return false;
            // Remaining buffered bits are byte aligned now
            while (len > 0 && br.bitCount >= 8) {
                uint32_t byte = 0;
                br.Bits(8, byte);
                out += static_cast<char>(byte);
                --len;
            }
            if (br.pos + len > br.size) return false;
            out.append(reinterpret_cast<const char*>(br.data + br.pos), len);
            br.pos += len;
        } else if (type == 1) {
            if (!InflateBlock(br, out, fixedLit, fixedDist)) return false;
        } else if (type == 2) {
            uint32_t hlit = 0, hdist = 0, hclen = 0;
            if (!br.Bits(5, hlit) || !br.Bits(5, hdist) || !br.Bits(4, hclen)) return false;
            hlit += 257;
            hdist += 1;
            hclen += 4;
            static const uint8_t order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
            uint8_t codeLengths[19]{};
            for (uint32_t i = 0; i < hclen; ++i) {
                uint32_t v = 0;
                if (!br.Bits(3, v)) return false;
                codeLengths[order[i]] = static_cast<uint8_t>(v);
            }
            Huffman lenCode;
            lenCode.Build(codeLengths, 19);
            uint8_t lengths[320]{};
            uint32_t n = 0;
            while (n < hlit + hdist) {
                const int sym = lenCode.Decode(br);
                if (sym < 0) return false;
                if (sym < 16) {
                    lengths[n++] = static_cast<uint8_t>(sym);
                } else {
                    uint32_t repeat = 0;
                    uint8_t value = 0;
                    if (sym == 16) {
                        if (n == 0 || !br.Bits(2, repeat)) return false;
                        value = lengths[n - 1];
                        repeat += 3;
                    } else if (sym == 17) {
                        if (!br.Bits(3, repeat)) return false;
                        repeat += 3;
                    } else {
                        if (!br.Bits(7, repeat)) return false;
                        repeat += 11;
                    }
                    if (n + repeat > hlit + hdist) return false;
                    while (repeat--) lengths[n++] = value;
                }
            }
            Huffman lit, dist;
            lit.Build(lengths, static_cast<int>(hlit));
            dist.Build(lengths + hlit, static_cast<int>(hdist));
            if (!InflateBlock(br, out, lit, dist)) return false;
        } else {
            return false;
        }
    } while (!last);
    return true;
}

// ── Deflate (fixed Huffman + LZ77 hash chains; good enough for uploads) ─────

namespace {

struct BitWriter {
    std::string out;
    uint32_t buffer = 0;
    int count = 0;
    void Put(uint32_t bits, int n) {
        buffer |= bits << count;
        count += n;
        while (count >= 8) {
            out += static_cast<char>(buffer & 0xFF);
            buffer >>= 8;
            count -= 8;
        }
    }
    // Huffman codes are written most significant bit first
    void PutReversed(uint32_t code, int n) {
        uint32_t r = 0;
        for (int i = 0; i < n; ++i) r |= ((code >> i) & 1u) << (n - 1 - i);
        Put(r, n);
    }
    void Flush() {
        if (count > 0) out += static_cast<char>(buffer & 0xFF);
        buffer = 0;
        count = 0;
    }
};

void PutLiteral(BitWriter& bw, int sym) {
    if (sym < 144) bw.PutReversed(0x30 + sym, 8);
    else if (sym < 256) bw.PutReversed(0x190 + (sym - 144), 9);
    else if (sym < 280) bw.PutReversed(sym - 256, 7);
    else bw.PutReversed(0xC0 + (sym - 280), 8);
}

void PutMatch(BitWriter& bw, int length, int distance) {
    int li = 28;
    while (li > 0 && kLengthBase[li] > length) --li;
    PutLiteral(bw, 257 + li);
    bw.Put(static_cast<uint32_t>(length - kLengthBase[li]), kLengthExtra[li]);
    int di = 29;
    while (di > 0 && kDistBase[di] > distance) --di;
    bw.PutReversed(static_cast<uint32_t>(di), 5);
    bw.Put(static_cast<uint32_t>(distance - kDistBase[di]), kDistExtra[di]);
}

} // namespace

std::string Deflate(const std::string& data) {
    BitWriter bw;
    bw.Put(1, 1);  // final block
    bw.Put(1, 2);  // fixed Huffman
    // 32 KB window with the chain links in a ring buffer, so memory stays constant for big inputs
    constexpr int64_t kWindow = 32768, kHashSize = 1 << 15, kMaxChain = 48;
    std::vector<int64_t> head(kHashSize, -1), prev(kWindow, -1);
    const auto* p = reinterpret_cast<const uint8_t*>(data.data());
    const int64_t n = static_cast<int64_t>(data.size());
    const auto hash = [&](int64_t i) { return ((p[i] << 10) ^ (p[i + 1] << 5) ^ p[i + 2]) & (kHashSize - 1); };
    const auto link = [&](int64_t i) {
        const int64_t h = hash(i);
        prev[i & (kWindow - 1)] = head[h];
        head[h] = i;
    };
    int64_t i = 0;
    while (i < n) {
        int bestLen = 0, bestDist = 0;
        if (i + 2 < n) {
            int64_t candidate = head[hash(i)];
            int chain = 0;
            while (candidate >= 0 && i - candidate < kWindow && chain++ < kMaxChain) {
                int len = 0;
                const int maxLen = static_cast<int>(std::min<int64_t>(258, n - i));
                while (len < maxLen && p[candidate + len] == p[i + len]) ++len;
                if (len > bestLen) {
                    bestLen = len;
                    bestDist = static_cast<int>(i - candidate);
                    if (len == maxLen) break;
                }
                const int64_t next = prev[candidate & (kWindow - 1)];
                if (next >= candidate) break;   // ring slot reused by a newer position
                candidate = next;
            }
            link(i);
        }
        if (bestLen >= 3) {
            PutMatch(bw, bestLen, bestDist);
            for (int k = 1; k < bestLen; ++k)
                if (i + k + 2 < n) link(i + k);
            i += bestLen;
        } else {
            PutLiteral(bw, p[i]);
            ++i;
        }
    }
    PutLiteral(bw, 256);
    bw.Flush();
    return bw.out;
}

// ── Archive reading ─────────────────────────────────────────────────────────

namespace {

uint16_t U16(const std::string& d, size_t at) {
    return static_cast<uint16_t>(static_cast<uint8_t>(d[at]) | static_cast<uint8_t>(d[at + 1]) << 8);
}
uint32_t U32(const std::string& d, size_t at) { return U16(d, at) | static_cast<uint32_t>(U16(d, at + 2)) << 16; }

std::wstring DecodeName(const std::string& raw, bool utf8) {
    if (utf8) return Text::Widen(raw);
    // Legacy names use the OEM code page; plain ASCII names are the same either way
    const int n = MultiByteToWideChar(CP_OEMCP, 0, raw.data(), static_cast<int>(raw.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_OEMCP, 0, raw.data(), static_cast<int>(raw.size()), out.data(), n);
    return out;
}

std::wstring NormalizeName(std::wstring s) {
    for (auto& c : s) if (c == L'\\') c = L'/';
    return Text::Lower(s);
}

} // namespace

std::optional<Archive> Archive::Open(const std::wstring& path) {
    auto bytes = Files::ReadBytes(path);
    if (!bytes) return std::nullopt;
    return FromMemory(std::move(*bytes));
}

std::optional<Archive> Archive::FromMemory(std::string bytes) {
    Archive archive;
    archive.data = std::move(bytes);
    if (!archive.Parse()) return std::nullopt;
    return archive;
}

bool Archive::Parse() {
    if (data.size() < 22) return false;
    // End of central directory: search backwards (comment can be up to 64 KB)
    size_t eocd = std::string::npos;
    const size_t minPos = data.size() > 22 + 65535 ? data.size() - 22 - 65535 : 0;
    for (size_t i = data.size() - 22 + 1; i-- > minPos;) {
        if (U32(data, i) == 0x06054b50) {
            eocd = i;
            break;
        }
    }
    if (eocd == std::string::npos) return false;
    const uint16_t count = U16(data, eocd + 10);
    size_t pos = U32(data, eocd + 16);
    for (uint16_t i = 0; i < count; ++i) {
        if (pos + 46 > data.size() || U32(data, pos) != 0x02014b50) return false;
        const uint16_t flags = U16(data, pos + 8);
        Entry entry;
        entry.method = U16(data, pos + 10);
        entry.compressedSize = U32(data, pos + 20);
        entry.length = U32(data, pos + 24);
        const uint16_t nameLen = U16(data, pos + 28), extraLen = U16(data, pos + 30), commentLen = U16(data, pos + 32);
        entry.localHeaderOffset = U32(data, pos + 42);
        if (pos + 46 + nameLen > data.size()) return false;
        entry.fullName = DecodeName(data.substr(pos + 46, nameLen), (flags & 0x800) != 0);
        const size_t slash = entry.fullName.find_last_of(L"/\\");
        entry.name = slash == std::wstring::npos ? entry.fullName : entry.fullName.substr(slash + 1);
        entries.push_back(std::move(entry));
        pos += 46 + nameLen + extraLen + commentLen;
    }
    return true;
}

const Entry* Archive::Find(const std::wstring& fullName) const {
    const std::wstring wanted = NormalizeName(fullName);
    for (const auto& e : entries)
        if (NormalizeName(e.fullName) == wanted) return &e;
    return nullptr;
}

std::optional<std::string> Archive::Read(const Entry& entry) const {
    const size_t at = static_cast<size_t>(entry.localHeaderOffset);
    if (at + 30 > data.size() || U32(data, at) != 0x04034b50) return std::nullopt;
    const size_t start = at + 30 + U16(data, at + 26) + U16(data, at + 28);
    if (start + entry.compressedSize > data.size()) return std::nullopt;
    const auto* raw = reinterpret_cast<const uint8_t*>(data.data() + start);
    if (entry.method == 0) return std::string(reinterpret_cast<const char*>(raw), static_cast<size_t>(entry.compressedSize));
    if (entry.method != 8) return std::nullopt;
    std::string out;
    if (!Inflate(raw, static_cast<size_t>(entry.compressedSize), out, static_cast<size_t>(entry.length))) return std::nullopt;
    return out;
}

bool Archive::ExtractEntry(const Entry& entry, const std::wstring& destinationFile) const {
    auto bytes = Read(entry);
    return bytes && Files::WriteBytes(destinationFile, *bytes);
}

bool Archive::ExtractAll(const std::wstring& directory, bool overwrite, std::wstring* error) const {
    Files::CreateDirectories(directory);
    const std::wstring root = Files::FullPath(directory);
    for (const auto& entry : entries) {
        std::wstring relative = entry.fullName;
        for (auto& c : relative) if (c == L'/') c = L'\\';
        const std::wstring target = Files::FullPath(Files::Combine(root, relative));
        // Zip-slip guard: never write outside the target folder
        if (!Text::StartsWith(target, root, true)) continue;
        if (entry.IsDirectory()) {
            Files::CreateDirectories(target);
            continue;
        }
        if (!overwrite && Files::Exists(target)) continue;
        if (!ExtractEntry(entry, target)) {
            if (error) *error = L"Could not extract " + entry.fullName;
            return false;
        }
    }
    return true;
}

bool ExtractToDirectory(const std::wstring& zipPath, const std::wstring& directory, bool overwrite, std::wstring* error) {
    auto archive = Archive::Open(zipPath);
    if (!archive) {
        if (error) *error = L"Invalid zip: " + zipPath;
        return false;
    }
    return archive->ExtractAll(directory, overwrite, error);
}

// ── Archive writing ─────────────────────────────────────────────────────────

namespace {

void Put16(std::string& out, uint32_t v) {
    out += static_cast<char>(v & 0xFF);
    out += static_cast<char>((v >> 8) & 0xFF);
}
void Put32(std::string& out, uint32_t v) {
    Put16(out, v & 0xFFFF);
    Put16(out, v >> 16);
}

} // namespace

void Writer::Add(const std::wstring& entryName, const std::string& data, bool compress, int64_t lastWriteUtc) {
    Written w;
    if (lastWriteUtc > 0) {
        // Local DOS time, as ZipArchive stores LastWriteTime
        const FILETIME utc{ static_cast<DWORD>(lastWriteUtc & 0xFFFFFFFF), static_cast<DWORD>(lastWriteUtc >> 32) };
        FILETIME local{};
        WORD date = 0, time = 0;
        if (FileTimeToLocalFileTime(&utc, &local) && FileTimeToDosDateTime(&local, &date, &time)) {
            w.dosDate = date;
            w.dosTime = time;
        }
    }
    w.name = Text::Narrow(entryName);
    w.crc = Crc32(data.data(), data.size());
    w.size = static_cast<uint32_t>(data.size());
    std::string payload = data;
    w.method = 0;
    if (compress && !data.empty()) {
        std::string packed = Deflate(data);
        if (packed.size() < data.size()) {
            payload = std::move(packed);
            w.method = 8;
        }
    }
    w.compressed = static_cast<uint32_t>(payload.size());
    w.offset = static_cast<uint32_t>(out.size());
    Put32(out, 0x04034b50);
    Put16(out, 20);
    Put16(out, 0x800);  // UTF-8 names
    Put16(out, w.method);
    Put16(out, w.dosTime);
    Put16(out, w.dosDate);   // 0x21 = 1980-01-01
    Put32(out, w.crc);
    Put32(out, w.compressed);
    Put32(out, w.size);
    Put16(out, static_cast<uint32_t>(w.name.size()));
    Put16(out, 0);
    out += w.name;
    out += payload;
    written.push_back(std::move(w));
}

std::string Writer::Finish() {
    const uint32_t directoryStart = static_cast<uint32_t>(out.size());
    for (const auto& w : written) {
        Put32(out, 0x02014b50);
        Put16(out, 20);
        Put16(out, 20);
        Put16(out, 0x800);
        Put16(out, w.method);
        Put16(out, w.dosTime);
        Put16(out, w.dosDate);
        Put32(out, w.crc);
        Put32(out, w.compressed);
        Put32(out, w.size);
        Put16(out, static_cast<uint32_t>(w.name.size()));
        Put16(out, 0);
        Put16(out, 0);
        Put16(out, 0);
        Put16(out, 0);
        Put32(out, 0);
        Put32(out, w.offset);
        out += w.name;
    }
    const uint32_t directorySize = static_cast<uint32_t>(out.size()) - directoryStart;
    Put32(out, 0x06054b50);
    Put16(out, 0);
    Put16(out, 0);
    Put16(out, static_cast<uint32_t>(written.size()));
    Put16(out, static_cast<uint32_t>(written.size()));
    Put32(out, directorySize);
    Put32(out, directoryStart);
    Put16(out, 0);
    return std::move(out);
}

} // namespace SonicRacingSaveManager::Zip
