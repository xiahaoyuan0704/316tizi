#include "GimParser.h"

#include <cctype>
#include <fstream>
#include <cstring>
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
            error = "JSON 尾部存在多余字符。";
            return std::nullopt;
        }
        return root;
    }

private:
    std::optional<JsonValue> parseValue(std::string& error) {
        skipWs();
        if (pos_ >= src_.size()) {
            error = "JSON 提前结束。";
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
        error = "无法识别的 JSON 值。";
        return std::nullopt;
    }

    std::optional<JsonValue> parseObject(std::string& error) {
        JsonObject obj;
        ++pos_; // {
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
                error = "对象键后缺少 ':'。";
                return std::nullopt;
            }
            auto val = parseValue(error);
            if (!val) {
                return std::nullopt;
            }
            obj.emplace(*key, std::move(*val));

            skipWs();
            if (consume('}')) {
                return JsonValue{obj};
            }
            if (!consume(',')) {
                error = "对象字段分隔符错误。";
                return std::nullopt;
            }
            skipWs();
        }

        error = "对象未正确结束。";
        return std::nullopt;
    }

    std::optional<JsonValue> parseArray(std::string& error) {
        JsonArray arr;
        ++pos_; // [
        skipWs();
        if (consume(']')) {
            return JsonValue{arr};
        }

        while (pos_ < src_.size()) {
            auto v = parseValue(error);
            if (!v) {
                return std::nullopt;
            }
            arr.push_back(std::move(*v));
            skipWs();
            if (consume(']')) {
                return JsonValue{arr};
            }
            if (!consume(',')) {
                error = "数组元素分隔符错误。";
                return std::nullopt;
            }
            skipWs();
        }

        error = "数组未正确结束。";
        return std::nullopt;
    }

    std::optional<std::string> parseString(std::string& error) {
        if (!consume('"')) {
            error = "字符串必须以双引号开始。";
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
                    error = "转义字符不完整。";
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
                    error = "暂不支持该转义字符。";
                    return std::nullopt;
                }
            } else {
                out.push_back(c);
            }
        }

        error = "字符串未正确结束。";
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
            error = "数字解析失败。";
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
    if (const auto* p = std::get_if<double>(&value.value)) {
        return *p;
    }
    return std::nullopt;
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
        if (const auto* p = asString(*node)) {
            out = *p;
            return true;
        }
    }
    return false;
}

bool readNumber(const JsonObject& obj, const std::string& key, double& out) {
    if (const auto* node = find(obj, key)) {
        if (auto p = asNumber(*node)) {
            out = *p;
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

} // namespace

std::optional<GimAttributes> Parser::load(const std::filesystem::path& filePath, std::string& error) {
    std::ifstream input(filePath, std::ios::binary);
    if (!input) {
        error = "无法打开 GIM 文件。";
        return std::nullopt;
    }

    std::stringstream ss;
    ss << input.rdbuf();

    JsonReader reader(ss.str());
    auto rootValue = reader.parse(error);
    if (!rootValue) {
        return std::nullopt;
    }

    const JsonObject* root = asObject(*rootValue);
    if (!root) {
        error = "根节点必须是 JSON 对象。";
        return std::nullopt;
    }

    GimAttributes out;
    readString(*root, "format", out.format);
    readString(*root, "version", out.version);
    readString(*root, "project", out.projectName);
    readString(*root, "author", out.author);
    readString(*root, "unit", out.unit);

    if (out.format != "GIM-GridInformationModel") {
        error = "format 必须是 GIM-GridInformationModel。";
        return std::nullopt;
    }

    const JsonValue* gridVal = find(*root, "grid");
    const JsonObject* gridObj = gridVal ? asObject(*gridVal) : nullptr;
    if (!gridObj) {
        error = "缺少 grid 对象。";
        return std::nullopt;
    }

    if (!readUInt(*gridObj, "rows", out.grid.rows) || !readUInt(*gridObj, "cols", out.grid.cols)) {
        error = "grid.rows 和 grid.cols 必须为非负整数。";
        return std::nullopt;
    }

    if (!readNumber(*gridObj, "cellSizeMm", out.grid.cellSizeMm)) {
        error = "grid.cellSizeMm 必须为数字。";
        return std::nullopt;
    }
    readNumber(*gridObj, "originX", out.grid.originX);
    readNumber(*gridObj, "originY", out.grid.originY);
    readNumber(*gridObj, "rotationDeg", out.grid.rotationDeg);

    const JsonValue* cellsVal = find(*root, "cells");
    const JsonArray* cellsArray = cellsVal ? asArray(*cellsVal) : nullptr;
    if (!cellsArray) {
        error = "缺少 cells 数组。";
        return std::nullopt;
    }

    for (const auto& item : *cellsArray) {
        const JsonObject* cellObj = asObject(item);
        if (!cellObj) {
            continue;
        }
        Cell c;
        if (!readUInt(*cellObj, "row", c.row) || !readUInt(*cellObj, "col", c.col)) {
            continue;
        }
        readString(*cellObj, "category", c.category);
        readString(*cellObj, "usage", c.usage);
        readNumber(*cellObj, "elevationMm", c.elevationMm);
        if (c.row < out.grid.rows && c.col < out.grid.cols) {
            out.cells.push_back(std::move(c));
        }
    }

    return out;
}

} // namespace gim
