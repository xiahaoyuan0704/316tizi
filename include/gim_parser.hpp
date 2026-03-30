#pragma once

#include "gim_model.hpp"

#include <string>

namespace gim {

class GimParser {
public:
    Model parseFromFile(const std::string& path) const;
    void writeToFile(const Model& model, const std::string& path) const;

private:
    static AttributeValue parseAttributeValue(const std::string& type, const std::string& raw);
};

} // namespace gim
