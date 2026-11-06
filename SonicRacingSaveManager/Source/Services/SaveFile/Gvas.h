#pragma once
#include <cstdint>
#include <exception>
#include <string>
#include <utility>
#include <vector>

// The game's saves are Unreal Engine 5.4 GVAS files (SaveGameSystem): a header, then tagged
// properties until "None". 5.4 writes the complete type name in each tag (a tree of names: an
// ArrayProperty's names its inner type, a StructProperty its struct and package), then the value's
// size and a flags byte. Parse → edit → Serialize gives back the same bytes when nothing changed;
// a property the reader doesn't fully understand is kept as its raw bytes, so it survives as is.
namespace SonicRacingSaveManager::Services::Gvas {

class Error : public std::exception {
public:
    explicit Error(std::string message) : text(std::move(message)) {}
    const char* what() const noexcept override { return text.c_str(); }

private:
    std::string text;
};

struct TypeName {
    std::string name;
    std::vector<TypeName> params;
};

struct Property;

struct Value {
    enum class Kind { None, Int, Float, Double, Bool, Str, Struct, Array, Map, Raw };
    Kind kind = Kind::None;
    int64_t i = 0;       // Int (every integer width; the type says which)
    double f = 0.0;      // Float / Double
    bool b = false;
    std::wstring s;      // Str (FString, FName, enum value names)
    bool wide = false;   // the FString was UTF-16
    std::vector<Property> props;                      // Struct
    std::vector<Value> items;                         // Array / Set
    std::vector<std::pair<Value, Value>> entries;     // Map
    std::string raw;                                  // Raw (also native structs: Guid, DateTime...)

    static Value Int(int64_t v) { Value x; x.kind = Kind::Int; x.i = v; return x; }
    static Value Bool(bool v) { Value x; x.kind = Kind::Bool; x.b = v; return x; }
    static Value Str(std::wstring v) { Value x; x.kind = Kind::Str; x.s = std::move(v); return x; }
};

struct Property {
    std::string name;
    TypeName type;
    uint8_t flags = 0;
    int32_t arrayIndex = 0;   // flags & 1
    std::string guid;         // flags & 2 (16 bytes)
    Value value;
};
using Props = std::vector<Property>;

struct File {
    std::string header;   // everything before the first property (kept verbatim)
    Props root;
    std::string footer;   // what follows the root "None"
};

File Parse(const std::string& bytes);
std::string Serialize(const File& file);

// ── Helpers for the editor ──
Property* Find(Props& props, std::string_view name);
const Property* Find(const Props& props, std::string_view name);
// The value of struct property `name` (nullptr when missing or not a parsed struct)
Props* Struct(Props& props, std::string_view name);
// A map / array property's value (nullptr when missing or kept raw)
Value* Container(Props& props, std::string_view name, Value::Kind kind);
// Sets a BoolProperty (its value lives in the tag's flags)
void SetBool(Property& p, bool v);
bool GetBool(const Props& props, std::string_view name, bool fallback = false);
// Property `name` of `props` set to `v` when it is an integer property
bool SetInt(Props& props, std::string_view name, int64_t v);

} // namespace SonicRacingSaveManager::Services::Gvas
