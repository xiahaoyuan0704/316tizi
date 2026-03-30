#pragma once

#include "gim_model.hpp"

#include <string>

namespace gim {

class SoftwareRenderer {
public:
    void renderWireframeToPpm(const Model& model, const std::string& outputPath, int width = 800, int height = 600) const;
};

} // namespace gim
