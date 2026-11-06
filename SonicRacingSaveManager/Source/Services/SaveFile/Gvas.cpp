#include "Services/SaveFile/Gvas.h"
#include <cstring>

namespace SonicRacingSaveManager::Services::Gvas {

namespace {

// Flags of a 5.4 property tag
constexpr uint8_t kHasArrayIndex = 0x01, kHasGuid = 0x02, kHasExtensions = 0x04, kBoolTrue = 0x10;

// Structs written natively (no tags), by their size
size_t NativeStructSize(const std::string& name) {
    if (name == "Guid") return 16;
    if (name == "DateTime" || name == "Timespan") return 8;
    if (name == "Vector" || name == "Rotator") return 24;
    if (name == "Vector2D" || name == "LinearColor") return 16;
    if (name == "Quat") return 32;
    if (name == "IntVector") return 12;
    if (name == "IntPoint") return 8;
    if (name == "Color") return 4;
    return 0;
}

bool IsInteger(const std::string& t) {
    return t == "IntProperty" || t == "UInt32Property" || t == "Int64Property" || t == "UInt64Property" || t == "Int16Property" ||
           t == "UInt16Property" || t == "Int8Property";
}
int IntWidth(const std::string& t) {
    if (t == "Int64Property" || t == "UInt64Property") return 8;
    if (t == "Int16Property" || t == "UInt16Property") return 2;
    if (t == "Int8Property") return 1;
    return 4;
}
bool IsUnsigned(const std::string& t) { return t == "UInt32Property" || t == "UInt64Property" || t == "UInt16Property"; }
bool IsString(const std::string& t) {
    return t == "StrProperty" || t == "NameProperty" || t == "ObjectProperty" || t == "SoftObjectProperty" || t == "EnumProperty";
}
// A ByteProperty of an enum holds the value's name; a plain one, a byte
bool ByteIsEnum(const TypeName& t) { return !t.params.empty() && t.params[0].name != "None"; }

class Reader {
public:
    Reader(const std::string& d, size_t at, size_t end) : d(d), p(at), end(end) {}
    size_t pos() const { return p; }
    void seek(size_t at) { p = at; }
    void need(size_t n) const {
        if (p + n > end) throw Error("unexpected end of data");
    }
    uint8_t u8() { need(1); return static_cast<uint8_t>(d[p++]); }
    int32_t i32() { need(4); int32_t v; std::memcpy(&v, d.data() + p, 4); p += 4; return v; }
    int64_t sized(int width, bool isUnsigned) {
        need(static_cast<size_t>(width));
        int64_t v = 0;
        switch (width) {
            case 1: { int8_t x; std::memcpy(&x, d.data() + p, 1); v = x; break; }
            case 2: {
                if (isUnsigned) { uint16_t x; std::memcpy(&x, d.data() + p, 2); v = x; }
                else { int16_t x; std::memcpy(&x, d.data() + p, 2); v = x; }
                break;
            }
            case 4: {
                if (isUnsigned) { uint32_t x; std::memcpy(&x, d.data() + p, 4); v = x; }
                else { int32_t x; std::memcpy(&x, d.data() + p, 4); v = x; }
                break;
            }
            default: std::memcpy(&v, d.data() + p, 8); break;
        }
        p += static_cast<size_t>(width);
        return v;
    }
    std::string raw(size_t n) { need(n); std::string s = d.substr(p, n); p += n; return s; }
    // FString: length with the terminator; negative means UTF-16
    std::wstring fstr(bool& wide) {
        const int32_t n = i32();
        wide = n < 0;
        if (n == 0) return {};
        if (n > 0) {
            if (n > 1 << 24) throw Error("string too long");
            need(static_cast<size_t>(n));
            std::wstring s;
            s.reserve(static_cast<size_t>(n - 1));
            for (int32_t k = 0; k < n - 1; ++k) s.push_back(static_cast<unsigned char>(d[p + static_cast<size_t>(k)]));
            p += static_cast<size_t>(n);
            return s;
        }
        const int64_t count = -static_cast<int64_t>(n);
        if (count > 1 << 24) throw Error("string too long");
        need(static_cast<size_t>(count) * 2);
        std::wstring s(static_cast<size_t>(count - 1), L'\0');
        std::memcpy(s.data(), d.data() + p, static_cast<size_t>(count - 1) * 2);
        p += static_cast<size_t>(count) * 2;
        return s;
    }
    std::string name() {
        bool wide = false;
        const std::wstring w = fstr(wide);
        std::string s;
        for (wchar_t c : w) s.push_back(static_cast<char>(c));
        return s;
    }

private:
    const std::string& d;
    size_t p, end;
};

class Writer {
public:
    std::string out;
    void u8(uint8_t v) { out.push_back(static_cast<char>(v)); }
    void i32(int32_t v) { out.append(reinterpret_cast<const char*>(&v), 4); }
    void sized(int64_t v, int width) { out.append(reinterpret_cast<const char*>(&v), static_cast<size_t>(width)); }
    void fstr(const std::wstring& s, bool wide) {
        if (s.empty()) {
            i32(0);
            return;
        }
        bool needWide = wide;
        for (wchar_t c : s)
            if (c > 0xFF) needWide = true;
        if (needWide) {
            i32(-static_cast<int32_t>(s.size() + 1));
            out.append(reinterpret_cast<const char*>(s.data()), s.size() * 2);
            out.append(2, '\0');
        } else {
            i32(static_cast<int32_t>(s.size() + 1));
            for (wchar_t c : s) out.push_back(static_cast<char>(c));
            out.push_back('\0');
        }
    }
    void name(const std::string& s) {
        i32(static_cast<int32_t>(s.size() + 1));
        out.append(s);
        out.push_back('\0');
    }
};

TypeName ReadType(Reader& r, int depth = 0) {
    if (depth > 16) throw Error("type name too deep");
    TypeName t;
    t.name = r.name();
    const int32_t n = r.i32();
    if (n < 0 || n > 16) throw Error("bad type name");
    for (int32_t k = 0; k < n; ++k) t.params.push_back(ReadType(r, depth + 1));
    return t;
}

void WriteType(Writer& w, const TypeName& t) {
    w.name(t.name);
    w.i32(static_cast<int32_t>(t.params.size()));
    for (const auto& c : t.params) WriteType(w, c);
}

Props ReadProps(Reader& r, size_t end);

// One value of type `t` (`size` known only for a tag's own value; 0 inside containers)
Value ReadValue(Reader& r, const TypeName& t, size_t size, bool inContainer) {
    Value v;
    const std::string& k = t.name;
    if (IsInteger(k)) {
        v.kind = Value::Kind::Int;
        v.i = r.sized(IntWidth(k), IsUnsigned(k));
    } else if (k == "FloatProperty") {
        v.kind = Value::Kind::Float;
        const std::string b = r.raw(4);
        float f;
        std::memcpy(&f, b.data(), 4);
        v.f = f;
    } else if (k == "DoubleProperty") {
        v.kind = Value::Kind::Double;
        const std::string b = r.raw(8);
        std::memcpy(&v.f, b.data(), 8);
    } else if (k == "BoolProperty") {
        v.kind = Value::Kind::Bool;
        v.b = r.u8() != 0;   // a byte inside containers (a tag's bool is in its flags)
    } else if (IsString(k)) {
        v.kind = Value::Kind::Str;
        v.s = r.fstr(v.wide);
    } else if (k == "ByteProperty") {
        if (ByteIsEnum(t)) {
            v.kind = Value::Kind::Str;
            v.s = r.fstr(v.wide);
        } else {
            v.kind = Value::Kind::Int;
            v.i = r.u8();
        }
    } else if (k == "StructProperty") {
        if (t.params.empty()) throw Error("struct without a name");
        const size_t native = NativeStructSize(t.params[0].name);
        if (native) {
            v.kind = Value::Kind::Raw;
            v.raw = r.raw(native);
        } else {
            v.kind = Value::Kind::Struct;
            v.props = ReadProps(r, SIZE_MAX);
        }
    } else if (k == "ArrayProperty" || k == "SetProperty") {
        if (t.params.empty()) throw Error("container without a type");
        v.kind = Value::Kind::Array;
        if (k == "SetProperty" && r.i32() != 0) throw Error("set with removed keys");
        const int32_t n = r.i32();
        if (n < 0 || n > 1 << 24) throw Error("bad element count");
        v.items.reserve(static_cast<size_t>(n));
        for (int32_t e = 0; e < n; ++e) v.items.push_back(ReadValue(r, t.params[0], 0, true));
    } else if (k == "MapProperty") {
        if (t.params.size() < 2) throw Error("map without types");
        v.kind = Value::Kind::Map;
        if (r.i32() != 0) throw Error("map with removed keys");
        const int32_t n = r.i32();
        if (n < 0 || n > 1 << 24) throw Error("bad entry count");
        v.entries.reserve(static_cast<size_t>(n));
        for (int32_t e = 0; e < n; ++e) {
            Value key = ReadValue(r, t.params[0], 0, true);
            Value val = ReadValue(r, t.params[1], 0, true);
            v.entries.emplace_back(std::move(key), std::move(val));
        }
    } else {
        // TextProperty and anything else: only a tag's own value (its size known) can be kept
        if (inContainer) throw Error("unsupported type in a container: " + k);
        v.kind = Value::Kind::Raw;
        v.raw = r.raw(size);
    }
    return v;
}

void WriteProps(Writer& w, const Props& props);

void WriteValue(Writer& w, const TypeName& t, const Value& v) {
    const std::string& k = t.name;
    switch (v.kind) {
        case Value::Kind::Raw: w.out.append(v.raw); return;
        case Value::Kind::Int:
            if (k == "ByteProperty") w.u8(static_cast<uint8_t>(v.i));
            else w.sized(v.i, IntWidth(k));
            return;
        case Value::Kind::Float: {
            const float f = static_cast<float>(v.f);
            w.out.append(reinterpret_cast<const char*>(&f), 4);
            return;
        }
        case Value::Kind::Double: w.out.append(reinterpret_cast<const char*>(&v.f), 8); return;
        case Value::Kind::Bool: w.u8(v.b ? 1 : 0); return;
        case Value::Kind::Str: w.fstr(v.s, v.wide); return;
        case Value::Kind::Struct: WriteProps(w, v.props); return;
        case Value::Kind::Array:
            if (k == "SetProperty") w.i32(0);
            w.i32(static_cast<int32_t>(v.items.size()));
            for (const auto& e : v.items) WriteValue(w, t.params[0], e);
            return;
        case Value::Kind::Map:
            w.i32(0);
            w.i32(static_cast<int32_t>(v.entries.size()));
            for (const auto& [key, val] : v.entries) {
                WriteValue(w, t.params[0], key);
                WriteValue(w, t.params[1], val);
            }
            return;
        case Value::Kind::None: return;
    }
}

// Tagged properties until "None" (`end`: where the enclosing value ends, SIZE_MAX when unknown)
Props ReadProps(Reader& r, size_t end) {
    Props out;
    for (;;) {
        if (end != SIZE_MAX && r.pos() >= end) throw Error("struct overran its size");
        Property p;
        p.name = r.name();
        if (p.name == "None") return out;
        p.type = ReadType(r);
        const int32_t size = r.i32();
        if (size < 0) throw Error("negative property size");
        p.flags = r.u8();
        if (p.flags & kHasArrayIndex) p.arrayIndex = r.i32();
        if (p.flags & kHasGuid) p.guid = r.raw(16);
        if (p.flags & kHasExtensions) {
            if (r.u8() != 0) throw Error("property extensions are not supported");
        }
        const size_t at = r.pos();
        r.need(static_cast<size_t>(size));
        if (p.type.name == "BoolProperty") {
            p.value = Value::Bool((p.flags & kBoolTrue) != 0);
            r.seek(at + static_cast<size_t>(size));
        } else {
            // Read it; whatever doesn't read back to exactly its size stays raw bytes
            try {
                p.value = ReadValue(r, p.type, static_cast<size_t>(size), false);
                if (r.pos() != at + static_cast<size_t>(size)) throw Error("size mismatch");
            } catch (const Error&) {
                r.seek(at);
                p.value = Value{};
                p.value.kind = Value::Kind::Raw;
                p.value.raw = r.raw(static_cast<size_t>(size));
            }
        }
        out.push_back(std::move(p));
    }
}

void WriteProps(Writer& w, const Props& props) {
    for (const Property& p : props) {
        w.name(p.name);
        WriteType(w, p.type);
        const size_t sizeAt = w.out.size();
        w.i32(0);
        uint8_t flags = p.flags;
        if (p.type.name == "BoolProperty" && p.value.kind == Value::Kind::Bool) flags = (flags & ~kBoolTrue) | (p.value.b ? kBoolTrue : 0);
        w.u8(flags);
        if (flags & kHasArrayIndex) w.i32(p.arrayIndex);
        if (flags & kHasGuid) w.out.append(p.guid);
        if (flags & kHasExtensions) w.u8(0);
        const size_t start = w.out.size();
        if (p.type.name != "BoolProperty" || p.value.kind == Value::Kind::Raw) WriteValue(w, p.type, p.value);
        const int32_t size = static_cast<int32_t>(w.out.size() - start);
        std::memcpy(w.out.data() + sizeAt, &size, 4);
    }
    w.name("None");
}

} // namespace

File Parse(const std::string& bytes) {
    if (bytes.size() < 4 || bytes.compare(0, 4, "GVAS") != 0) throw Error("not a GVAS file");
    Reader r(bytes, 4, bytes.size());
    r.i32();   // save game version
    r.i32();   // UE4 package version
    r.i32();   // UE5 package version
    r.raw(6);  // engine major / minor / patch
    r.i32();   // changelist
    r.name();  // branch
    r.i32();   // custom version format
    const int32_t versions = r.i32();
    if (versions < 0 || versions > 4096) throw Error("bad custom version count");
    r.raw(static_cast<size_t>(versions) * 20);
    r.name();  // save game class
    r.u8();    // 5.4: one byte after the class name
    File f;
    f.header = bytes.substr(0, r.pos());
    f.root = ReadProps(r, SIZE_MAX);
    f.footer = bytes.substr(r.pos());
    return f;
}

std::string Serialize(const File& file) {
    Writer w;
    w.out = file.header;
    WriteProps(w, file.root);
    w.out.append(file.footer);
    return std::move(w.out);
}

// Property names are FNames: the game matches them ignoring case (its 2.0 update renamed "gadgetId" to
// "GadgetID" and "stickerId" to "StickerId", and reads saves of either spelling)
bool SameName(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t k = 0; k < a.size(); ++k) {
        const char x = a[k] >= 'A' && a[k] <= 'Z' ? static_cast<char>(a[k] + 32) : a[k];
        const char y = b[k] >= 'A' && b[k] <= 'Z' ? static_cast<char>(b[k] + 32) : b[k];
        if (x != y) return false;
    }
    return true;
}

Property* Find(Props& props, std::string_view name) {
    for (auto& p : props)
        if (SameName(p.name, name)) return &p;
    return nullptr;
}

const Property* Find(const Props& props, std::string_view name) {
    for (const auto& p : props)
        if (SameName(p.name, name)) return &p;
    return nullptr;
}

Props* Struct(Props& props, std::string_view name) {
    Property* p = Find(props, name);
    return p && p->value.kind == Value::Kind::Struct ? &p->value.props : nullptr;
}

Value* Container(Props& props, std::string_view name, Value::Kind kind) {
    Property* p = Find(props, name);
    return p && p->value.kind == kind ? &p->value : nullptr;
}

void SetBool(Property& p, bool v) {
    if (p.value.kind == Value::Kind::Bool) p.value.b = v;
}

bool GetBool(const Props& props, std::string_view name, bool fallback) {
    const Property* p = Find(props, name);
    return p && p->value.kind == Value::Kind::Bool ? p->value.b : fallback;
}

bool SetInt(Props& props, std::string_view name, int64_t v) {
    Property* p = Find(props, name);
    if (!p || p->value.kind != Value::Kind::Int) return false;
    p->value.i = v;
    return true;
}

} // namespace SonicRacingSaveManager::Services::Gvas
