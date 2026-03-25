#pragma once

#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace gim {

using AttributeValue = std::variant<int, float, std::string>;

struct Vec3 {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

struct Mesh {
    std::vector<Vec3> vertices;
    std::vector<unsigned int> indices;
};

struct Model {
    std::string name;
    std::unordered_map<std::string, AttributeValue> attributes;
    Mesh mesh;
};

std::string attributeValueToString(const AttributeValue& value);

} // namespace gim
