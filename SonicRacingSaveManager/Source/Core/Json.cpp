#include "Core/Json.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <locale.h>
#include "Core/Text.h"

namespace SonicRacingSaveManager::Json {

namespace {

const Value kNull;
const std::string kEmpty;
const Value::Array kEmptyArray;
const Value::Object kEmptyObject;

class Parser {
public:
    explicit Parser(std::string_view text) : s(text) {}

    bool Run(Value& out) {
        SkipBom();
        if (!ParseValue(out, 0)) return false;
        SkipSpace();
        return pos == s.size();
    }

private:
    void SkipBom() {
        if (s.size() >= 3 && static_cast<unsigned char>(s[0]) == 0xEF && static_cast<unsigned char>(s[1]) == 0xBB &&
            static_cast<unsigned char>(s[2]) == 0xBF)
            pos = 3;
    }

    void SkipSpace() {
        while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\r' || s[pos] == '\n')) ++pos;
    }

    bool Literal(std::string_view word) {
        if (s.substr(pos, word.size()) != word) return false;
        pos += word.size();
        return true;
    }

    bool ParseValue(Value& out, int depth) {
        if (depth > 200) return false;
        SkipSpace();
        if (pos >= s.size()) return false;
        const char c = s[pos];
        if (c == '{') return ParseObject(out, depth);
        if (c == '[') return ParseArray(out, depth);
        if (c == '"') {
            std::string str;
            if (!ParseString(str)) return false;
            out = Value(std::move(str));
            return true;
        }
        if (c == 't') { if (!Literal("true")) return false; out = Value(true); return true; }
        if (c == 'f') { if (!Literal("false")) return false; out = Value(false); return true; }
        if (c == 'n') { if (!Literal("null")) return false; out = Value(); return true; }
        return ParseNumber(out);
    }

    bool ParseNumber(Value& out) {
        const size_t start = pos;
        if (pos < s.size() && (s[pos] == '-' || s[pos] == '+')) ++pos;
        while (pos < s.size() && (std::isdigit(static_cast<unsigned char>(s[pos])) || s[pos] == '.' || s[pos] == 'e' ||
                                  s[pos] == 'E' || s[pos] == '-' || s[pos] == '+'))
            ++pos;
        if (pos == start) return false;
        const std::string number(s.substr(start, pos - start));
        char* end = nullptr;
        _locale_t c = _create_locale(LC_NUMERIC, "C");
        const double v = _strtod_l(number.c_str(), &end, c);
        _free_locale(c);
        if (end == number.c_str()) return false;
        out = Value(v);
        return true;
    }

    static void AppendUtf8(std::string& out, uint32_t cp) {
        if (cp < 0x80) out += static_cast<char>(cp);
        else if (cp < 0x800) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    bool Hex4(uint32_t& v) {
        if (pos + 4 > s.size()) return false;
        v = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = s[pos++];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= static_cast<uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') v |= static_cast<uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= static_cast<uint32_t>(c - 'A' + 10);
            else return false;
        }
        return true;
    }

    bool ParseString(std::string& out) {
        ++pos;  // opening quote
        while (pos < s.size()) {
            const char c = s[pos++];
            if (c == '"') return true;
            if (c != '\\') { out += c; continue; }
            if (pos >= s.size()) return false;
            const char e = s[pos++];
            switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    uint32_t cp = 0;
                    if (!Hex4(cp)) return false;
                    if (cp >= 0xD800 && cp <= 0xDBFF && pos + 6 <= s.size() && s[pos] == '\\' && s[pos + 1] == 'u') {
                        pos += 2;
                        uint32_t low = 0;
                        if (!Hex4(low)) return false;
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                    }
                    AppendUtf8(out, cp);
                    break;
                }
                default: return false;
            }
        }
        return false;
    }

    bool ParseArray(Value& out, int depth) {
        ++pos;
        Value::Array items;
        SkipSpace();
        if (pos < s.size() && s[pos] == ']') { ++pos; out = Value(std::move(items)); return true; }
        while (true) {
            Value item;
            if (!ParseValue(item, depth + 1)) return false;
            items.push_back(std::move(item));
            SkipSpace();
            if (pos >= s.size()) return false;
            if (s[pos] == ',') { ++pos; continue; }
            if (s[pos] == ']') { ++pos; break; }
            return false;
        }
        out = Value(std::move(items));
        return true;
    }

    bool ParseObject(Value& out, int depth) {
        ++pos;
        Value::Object items;
        SkipSpace();
        if (pos < s.size() && s[pos] == '}') { ++pos; out = Value(std::move(items)); return true; }
        while (true) {
            SkipSpace();
            if (pos >= s.size() || s[pos] != '"') return false;
            std::string key;
            if (!ParseString(key)) return false;
            SkipSpace();
            if (pos >= s.size() || s[pos] != ':') return false;
            ++pos;
            Value item;
            if (!ParseValue(item, depth + 1)) return false;
            items.emplace_back(std::move(key), std::move(item));
            SkipSpace();
            if (pos >= s.size()) return false;
            if (s[pos] == ',') { ++pos; continue; }
            if (s[pos] == '}') { ++pos; break; }
            return false;
        }
        out = Value(std::move(items));
        return true;
    }

    std::string_view s;
    size_t pos = 0;
};

void Indent(std::string& out, int depth) {
    out += '\n';
    out.append(static_cast<size_t>(depth) * 2, ' ');
}

} // namespace

Value::Value(const wchar_t* s) : type(Type::String), string(Text::Narrow(s)) {}
Value::Value(const std::wstring& s) : type(Type::String), string(Text::Narrow(s)) {}

bool Value::Bool(bool fallback) const {
    if (type == Type::Bool) return boolean;
    if (type == Type::String) {
        if (string == "true") return true;
        if (string == "false") return false;
    }
    return fallback;
}

double Value::Number(double fallback) const {
    if (type == Type::Number) return number;
    if (type == Type::String) {
        if (auto v = Text::ToDouble(Text::Widen(string))) return *v;
    }
    return fallback;
}

long long Value::Int(long long fallback) const {
    if (type == Type::Number) return static_cast<long long>(std::llround(number));
    if (type == Type::String) {
        if (auto v = Text::ToInt(string)) return *v;
    }
    return fallback;
}

const std::string& Value::Str() const { return type == Type::String ? string : kEmpty; }

std::wstring Value::WStr() const {
    if (type == Type::String) return Text::Widen(string);
    if (type == Type::Number) {
        if (number == std::floor(number) && std::fabs(number) < 1e15) return Text::FromInt(static_cast<long long>(number));
        return Text::FromDouble(number, 6);
    }
    if (type == Type::Bool) return boolean ? L"true" : L"false";
    return {};
}

std::optional<std::wstring> Value::OptWStr() const {
    if (type == Type::Null) return std::nullopt;
    return WStr();
}

const Value& Value::operator[](std::string_view key) const {
    if (type != Type::Object) return kNull;
    for (const auto& [k, v] : *object)
        if (k == key) return v;
    return kNull;
}

Value& Value::operator[](std::string_view key) {
    if (type != Type::Object) {
        type = Type::Object;
        object = std::make_shared<Object>();
    }
    for (auto& [k, v] : *object)
        if (k == key) return v;
    object->emplace_back(std::string(key), Value());
    return object->back().second;
}

bool Value::Has(std::string_view key) const {
    if (type != Type::Object) return false;
    for (const auto& item : *object)
        if (item.first == key) return true;
    return false;
}

const Value::Object& Value::Items() const { return type == Type::Object ? *object : kEmptyObject; }

void Value::Remove(std::string_view key) {
    if (type != Type::Object) return;
    for (auto it = object->begin(); it != object->end(); ++it) {
        if (it->first == key) {
            object->erase(it);
            return;
        }
    }
}

const Value& Value::operator[](size_t index) const {
    if (type != Type::Array || index >= array->size()) return kNull;
    return (*array)[index];
}

size_t Value::Size() const {
    if (type == Type::Array) return array->size();
    if (type == Type::Object) return object->size();
    return 0;
}

const Value::Array& Value::Elements() const { return type == Type::Array ? *array : kEmptyArray; }

void Value::Push(Value v) {
    if (type != Type::Array) {
        type = Type::Array;
        array = std::make_shared<Array>();
    }
    array->push_back(std::move(v));
}

std::string Escape(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buffer[8];
                    sprintf_s(buffer, "\\u%04x", c);
                    out += buffer;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

void Value::Write(std::string& out, bool pretty, int depth) const {
    switch (type) {
        case Type::Null: out += "null"; break;
        case Type::Bool: out += boolean ? "true" : "false"; break;
        case Type::Number: {
            char buffer[64];
            if (number == std::floor(number) && std::fabs(number) < 1e15) sprintf_s(buffer, "%lld", static_cast<long long>(number));
            else {
                _locale_t c = _create_locale(LC_NUMERIC, "C");
                _sprintf_s_l(buffer, sizeof(buffer), "%.17g", c, number);
                _free_locale(c);
            }
            out += buffer;
            break;
        }
        case Type::String:
            out += '"';
            out += Escape(string);
            out += '"';
            break;
        case Type::Array: {
            out += '[';
            for (size_t i = 0; i < array->size(); ++i) {
                if (i) out += ',';
                if (pretty) Indent(out, depth + 1);
                (*array)[i].Write(out, pretty, depth + 1);
            }
            if (pretty && !array->empty()) Indent(out, depth);
            out += ']';
            break;
        }
        case Type::Object: {
            out += '{';
            for (size_t i = 0; i < object->size(); ++i) {
                if (i) out += ',';
                if (pretty) Indent(out, depth + 1);
                out += '"';
                out += Escape((*object)[i].first);
                out += pretty ? "\": " : "\":";
                (*object)[i].second.Write(out, pretty, depth + 1);
            }
            if (pretty && !object->empty()) Indent(out, depth);
            out += '}';
            break;
        }
    }
}

std::string Value::Serialize(bool pretty) const {
    std::string out;
    Write(out, pretty, 0);
    return out;
}

Value Parse(std::string_view text, bool* ok) {
    Value out;
    Parser parser(text);
    const bool success = parser.Run(out);
    if (ok) *ok = success;
    return success ? out : Value();
}

Value Parse(std::wstring_view text, bool* ok) { return Parse(std::string_view(Text::Narrow(text)), ok); }

} // namespace SonicRacingSaveManager::Json
