#include "gim_model.hpp"

#include <sstream>

namespace gim {

std::string attributeValueToString(const AttributeValue& value) {
    return std::visit(
        [](const auto& v) {
            std::ostringstream oss;
            oss << v;
            return oss.str();
        },
        value
    );
}

} // namespace gim
