#include "GimParser.h"

#include <cctype>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <variant>

namespace gim {
namespace {

struct JsonValue;
using JsonObject = std::map<std::string, JsonValue>;
using JsonArray = std::vector<JsonValue>;

struct JsonValue {
    using Variant = std::variant<std::nullptr_t, bool, double, std::string, JsonArray, JsonObject>;
    Variant value;
};

std::string toString(const JsonValue& v);

class JsonReader {
public:
    explicit JsonReader(std::string src) : src_(std::move(src)) {}

    std::optional<JsonValue> parse(std::string& error) {
        skipWs();
        auto root = parseValue(error);
        if (!root) {
            return std::nullopt;
        }
        skipWs();
        if (pos_ != src_.size()) {
            error = "Trailing characters after JSON root.";
            return std::nullopt;
        }
        return root;
    }

private:
    std::optional<JsonValue> parseValue(std::string& error) {
        skipWs();
        if (pos_ >= src_.size()) {
            error = "Unexpected end of JSON.";
            return std::nullopt;
        }

        const char c = src_[pos_];
        if (c == '{') {
            return parseObject(error);
        }
        if (c == '[') {
            return parseArray(error);
        }
        if (c == '"') {
            auto s = parseString(error);
            if (!s) {
                return std::nullopt;
            }
            return JsonValue{*s};
        }
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
            auto n = parseNumber(error);
            if (!n) {
                return std::nullopt;
            }
            return JsonValue{*n};
        }
        if (startsWith("true")) {
            pos_ += 4;
            return JsonValue{true};
        }
        if (startsWith("false")) {
            pos_ += 5;
            return JsonValue{false};
        }
        if (startsWith("null")) {
            pos_ += 4;
            return JsonValue{nullptr};
        }

        error = "Unrecognized JSON value.";
        return std::nullopt;
    }

    std::optional<JsonValue> parseObject(std::string& error) {
        JsonObject obj;
        ++pos_;
        skipWs();
        if (consume('}')) {
            return JsonValue{obj};
        }

        while (pos_ < src_.size()) {
            auto key = parseString(error);
            if (!key) {
                return std::nullopt;
            }
            skipWs();
            if (!consume(':')) {
                error = "Missing ':' after object key.";
                return std::nullopt;
            }
            auto value = parseValue(error);
            if (!value) {
                return std::nullopt;
            }
            obj.emplace(*key, std::move(*value));
            skipWs();

            if (consume('}')) {
                return JsonValue{obj};
            }
            if (!consume(',')) {
                error = "Invalid object field delimiter.";
                return std::nullopt;
            }
            skipWs();
        }

        error = "Object not terminated correctly.";
        return std::nullopt;
    }

    std::optional<JsonValue> parseArray(std::string& error) {
        JsonArray arr;
        ++pos_;
        skipWs();
        if (consume(']')) {
            return JsonValue{arr};
        }

        while (pos_ < src_.size()) {
            auto value = parseValue(error);
            if (!value) {
                return std::nullopt;
            }
            arr.push_back(std::move(*value));
            skipWs();

            if (consume(']')) {
                return JsonValue{arr};
            }
            if (!consume(',')) {
                error = "Invalid array delimiter.";
                return std::nullopt;
            }
            skipWs();
        }

        error = "Array not terminated correctly.";
        return std::nullopt;
    }

    std::optional<std::string> parseString(std::string& error) {
        if (!consume('"')) {
            error = "String must start with a double quote.";
            return std::nullopt;
        }

        std::string out;
        while (pos_ < src_.size()) {
            const char c = src_[pos_++];
            if (c == '"') {
                return out;
            }
            if (c == '\\') {
                if (pos_ >= src_.size()) {
                    error = "Incomplete escape sequence.";
                    return std::nullopt;
                }
                const char e = src_[pos_++];
                switch (e) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                default:
                    error = "Unsupported escape character.";
                    return std::nullopt;
                }
            } else {
                out.push_back(c);
            }
        }

        error = "String not terminated correctly.";
        return std::nullopt;
    }

    std::optional<double> parseNumber(std::string& error) {
        const std::size_t start = pos_;
        if (src_[pos_] == '-') {
            ++pos_;
        }
        while (pos_ < src_.size() && std::isdigit(static_cast<unsigned char>(src_[pos_]))) {
            ++pos_;
        }
        if (pos_ < src_.size() && src_[pos_] == '.') {
            ++pos_;
            while (pos_ < src_.size() && std::isdigit(static_cast<unsigned char>(src_[pos_]))) {
                ++pos_;
            }
        }
        if (pos_ < src_.size() && (src_[pos_] == 'e' || src_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < src_.size() && (src_[pos_] == '+' || src_[pos_] == '-')) {
                ++pos_;
            }
            while (pos_ < src_.size() && std::isdigit(static_cast<unsigned char>(src_[pos_]))) {
                ++pos_;
            }
        }

        try {
            return std::stod(src_.substr(start, pos_ - start));
        } catch (...) {
            error = "Failed to parse number.";
            return std::nullopt;
        }
    }

    bool startsWith(const char* literal) const {
        const std::size_t len = std::strlen(literal);
        if (pos_ + len > src_.size()) {
            return false;
        }
        return src_.compare(pos_, len, literal) == 0;
    }

    bool consume(char c) {
        if (pos_ < src_.size() && src_[pos_] == c) {
            ++pos_;
            return true;
        }
        return false;
    }

    void skipWs() {
        while (pos_ < src_.size() && std::isspace(static_cast<unsigned char>(src_[pos_]))) {
            ++pos_;
        }
    }

    std::string src_;
    std::size_t pos_ = 0;
};

const JsonObject* asObject(const JsonValue& value) {
    return std::get_if<JsonObject>(&value.value);
}

const JsonArray* asArray(const JsonValue& value) {
    return std::get_if<JsonArray>(&value.value);
}

const std::string* asString(const JsonValue& value) {
    return std::get_if<std::string>(&value.value);
}

std::optional<double> asNumber(const JsonValue& value) {
    if (const auto* n = std::get_if<double>(&value.value)) {
        return *n;
    }
    return std::nullopt;
}

std::string toString(const JsonValue& v) {
    if (const auto* s = std::get_if<std::string>(&v.value)) {
        return *s;
    }
    if (const auto* n = std::get_if<double>(&v.value)) {
        std::ostringstream os;
        os << *n;
        return os.str();
    }
    if (const auto* b = std::get_if<bool>(&v.value)) {
        return *b ? "true" : "false";
    }
    if (std::holds_alternative<std::nullptr_t>(v.value)) {
        return "null";
    }
    if (std::holds_alternative<JsonArray>(v.value)) {
        return "[array]";
    }
    return "{object}";
}

const JsonValue* find(const JsonObject& obj, const std::string& key) {
    const auto it = obj.find(key);
    if (it == obj.end()) {
        return nullptr;
    }
    return &it->second;
}

bool readString(const JsonObject& obj, const std::string& key, std::string& out) {
    if (const auto* node = find(obj, key)) {
        if (const auto* text = asString(*node)) {
            out = *text;
            return true;
        }
    }
    return false;
}

bool readNumber(const JsonObject& obj, const std::string& key, double& out) {
    if (const auto* node = find(obj, key)) {
        if (const auto num = asNumber(*node)) {
            out = *num;
            return true;
        }
    }
    return false;
}

bool readUInt(const JsonObject& obj, const std::string& key, std::uint32_t& out) {
    double n = 0.0;
    if (!readNumber(obj, key, n) || n < 0.0) {
        return false;
    }
    out = static_cast<std::uint32_t>(n);
    return true;
}

void readProperties(const JsonObject& obj, const std::string& key, Properties& props) {
    const JsonValue* v = find(obj, key);
    const JsonObject* pObj = v ? asObject(*v) : nullptr;
    if (!pObj) {
        return;
    }
    for (const auto& [k, vv] : *pObj) {
        props[k] = toString(vv);
    }
}

} // namespace

std::optional<GimAttributes> Parser::load(const std::filesystem::path& filePath, std::string& error) {
    std::ifstream input(filePath, std::ios::binary);
    if (!input) {
        error = "Cannot open GIM file.";
        return std::nullopt;
    }

    std::stringstream buffer;
    buffer << input.rdbuf();

    JsonReader reader(buffer.str());
    auto rootValue = reader.parse(error);
    if (!rootValue) {
        return std::nullopt;
    }

    const JsonObject* root = asObject(*rootValue);
    if (!root) {
        error = "Root node must be a JSON object.";
        return std::nullopt;
    }

    GimAttributes out;
    readString(*root, "format", out.format);
    readString(*root, "version", out.version);
    readString(*root, "project", out.projectName);
    readString(*root, "author", out.author);
    readString(*root, "unit", out.unit);
    readProperties(*root, "properties", out.properties);

    if (out.format != "GIM-GridInformationModel") {
        error = "format must be GIM-GridInformationModel.";
        return std::nullopt;
    }

    const JsonValue* gridVal = find(*root, "grid");
    const JsonObject* gridObj = gridVal ? asObject(*gridVal) : nullptr;
    if (!gridObj) {
        error = "Missing grid object.";
        return std::nullopt;
    }

    if (!readUInt(*gridObj, "rows", out.grid.rows) || !readUInt(*gridObj, "cols", out.grid.cols)) {
        error = "grid.rows and grid.cols must be non-negative integers.";
        return std::nullopt;
    }
    if (!readNumber(*gridObj, "cellSizeMm", out.grid.cellSizeMm)) {
        error = "grid.cellSizeMm must be numeric.";
        return std::nullopt;
    }
    readNumber(*gridObj, "originX", out.grid.originX);
    readNumber(*gridObj, "originY", out.grid.originY);
    readNumber(*gridObj, "rotationDeg", out.grid.rotationDeg);

    if (const JsonValue* levelsVal = find(*root, "levels")) {
        if (const JsonArray* levels = asArray(*levelsVal)) {
            for (const auto& lv : *levels) {
                if (const JsonObject* lvObj = asObject(lv)) {
                    Level level;
                    readString(*lvObj, "name", level.name);
                    readNumber(*lvObj, "elevationMm", level.elevationMm);
                    if (!level.name.empty()) {
                        out.levels.push_back(std::move(level));
                    }
                }
            }
        }
    }

    const JsonValue* cellsVal = find(*root, "cells");
    const JsonArray* cells = cellsVal ? asArray(*cellsVal) : nullptr;
    if (!cells) {
        error = "Missing cells array.";
        return std::nullopt;
    }

    for (const auto& item : *cells) {
        const JsonObject* cObj = asObject(item);
        if (!cObj) {
            continue;
        }

        Cell cell;
        if (!readUInt(*cObj, "row", cell.row) || !readUInt(*cObj, "col", cell.col)) {
            continue;
        }
        if (cell.row >= out.grid.rows || cell.col >= out.grid.cols) {
            continue;
        }

        readString(*cObj, "level", cell.level);
        readString(*cObj, "category", cell.category);
        readString(*cObj, "usage", cell.usage);
        readNumber(*cObj, "elevationMm", cell.elevationMm);
        readNumber(*cObj, "heightMm", cell.heightMm);
        readProperties(*cObj, "properties", cell.properties);

        out.cells.push_back(std::move(cell));
    }

    return out;
}

} // namespace gim
