#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace gim {

enum class PixelFormat : std::uint32_t {
    Unknown = 0,
    Rgba8888 = 3,
    Indexed4 = 4,
    Indexed8 = 5,
};

struct ImageInfo {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    PixelFormat format = PixelFormat::Unknown;
    std::uint32_t stride = 0;
};

struct GimImage {
    ImageInfo info;
    std::vector<std::uint32_t> rgba; // 0xAARRGGBB
};

struct GimAttributes {
    std::string signature;
    std::uint32_t version = 0;
    std::uint32_t fileSize = 0;
    std::uint32_t imageBlockCount = 0;
    std::vector<ImageInfo> images;
};

class Parser {
public:
    std::optional<GimImage> load(const std::filesystem::path& filePath, std::string& error, GimAttributes& attributes);

private:
    struct RawImage {
        ImageInfo info;
        std::vector<std::uint8_t> pixels;
        std::vector<std::uint8_t> palette;
        PixelFormat paletteFormat = PixelFormat::Unknown;
    };

    std::optional<RawImage> parseRaw(const std::vector<std::uint8_t>& bytes, std::string& error, GimAttributes& attributes);
    std::optional<GimImage> convert(const RawImage& raw, std::string& error);
};

} // namespace gim
