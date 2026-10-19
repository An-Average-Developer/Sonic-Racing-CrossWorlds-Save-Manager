#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// Small JSON DOM: parse UTF-8 text, read values with forgiving accessors (a missing key or a
// wrong type just gives the default), and build / serialize values for settings files and
// web requests. Object keys keep their insertion order.
namespace SonicRacingSaveManager::Json {

enum class Type { Null, Bool, Number, String, Array, Object };

class Value {
public:
    using Array = std::vector<Value>;
    using Object = std::vector<std::pair<std::string, Value>>;

    Value() = default;
    Value(std::nullptr_t) {}
    Value(bool b) : type(Type::Bool), boolean(b) {}
    Value(int n) : type(Type::Number), number(n) {}
    Value(unsigned n) : type(Type::Number), number(n) {}
    Value(long long n) : type(Type::Number), number(static_cast<double>(n)) {}
    Value(unsigned long long n) : type(Type::Number), number(static_cast<double>(n)) {}
    Value(double n) : type(Type::Number), number(n) {}
    Value(const char* s) : type(Type::String), string(s) {}
    Value(std::string s) : type(Type::String), string(std::move(s)) {}
    Value(const wchar_t* s);
    Value(const std::wstring& s);
    Value(Array a) : type(Type::Array), array(std::make_shared<Array>(std::move(a))) {}
    Value(Object o) : type(Type::Object), object(std::make_shared<Object>(std::move(o))) {}

    static Value MakeArray() { return Value(Array{}); }
    static Value MakeObject() { return Value(Object{}); }

    Type GetType() const { return type; }
    bool IsNull() const { return type == Type::Null; }
    bool IsBool() const { return type == Type::Bool; }
    bool IsNumber() const { return type == Type::Number; }
    bool IsString() const { return type == Type::String; }
    bool IsArray() const { return type == Type::Array; }
    bool IsObject() const { return type == Type::Object; }

    // Forgiving reads
    bool Bool(bool fallback = false) const;
    double Number(double fallback = 0.0) const;
    long long Int(long long fallback = 0) const;
    const std::string& Str() const;           // UTF-8, empty when not a string
    std::wstring WStr() const;                // UTF-16, numbers are converted too
    std::optional<std::wstring> OptWStr() const;  // nullopt for null / missing

    // Object access
    const Value& operator[](std::string_view key) const;  // null value when missing
    Value& operator[](std::string_view key);               // inserts (turns null into an object)
    bool Has(std::string_view key) const;
    const Object& Items() const;
    void Set(std::string_view key, Value v) { (*this)[key] = std::move(v); }
    void Remove(std::string_view key);

    // Array access
    const Value& operator[](size_t index) const;
    size_t Size() const;
    const Array& Elements() const;
    void Push(Value v);

    std::string Serialize(bool pretty = false) const;

private:
    void Write(std::string& out, bool pretty, int depth) const;

    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    std::shared_ptr<Array> array;
    std::shared_ptr<Object> object;
};

// Returns a null value (and `ok` false) on malformed input.
Value Parse(std::string_view text, bool* ok = nullptr);
Value Parse(std::wstring_view text, bool* ok = nullptr);

std::string Escape(std::string_view s);  // without the surrounding quotes

} // namespace SonicRacingSaveManager::Json
