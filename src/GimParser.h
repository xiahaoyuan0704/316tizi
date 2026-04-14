#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace gim {

struct GridDefinition {
    std::uint32_t rows = 0;
    std::uint32_t cols = 0;
    double cellSizeMm = 0.0;
    double originX = 0.0;
    double originY = 0.0;
    double rotationDeg = 0.0;
};

struct Cell {
    std::uint32_t row = 0;
    std::uint32_t col = 0;
    std::string category;
    std::string usage;
    double elevationMm = 0.0;
};

struct GimAttributes {
    std::string format;
    std::string version;
    std::string projectName;
    std::string author;
    std::string unit;
    GridDefinition grid;
    std::vector<Cell> cells;
};

class Parser {
public:
    std::optional<GimAttributes> load(const std::filesystem::path& filePath, std::string& error);
};

} // namespace gim
