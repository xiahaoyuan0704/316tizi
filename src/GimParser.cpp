#include "GimParser.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <span>

namespace gim {

namespace {

constexpr std::uint32_t kChunkRoot = 0x00000002;
constexpr std::uint32_t kChunkPicture = 0x00000003;
constexpr std::uint32_t kChunkImage = 0x00000004;
constexpr std::uint32_t kChunkPalette = 0x00000005;

struct ChunkHeader {
    std::uint32_t type = 0;
    std::uint32_t size = 0;
    std::uint32_t next = 0;
    std::uint32_t child = 0;
};

class Reader {
public:
    explicit Reader(std::span<const std::uint8_t> data) : data_(data) {}

    bool seek(std::size_t pos) {
        if (pos > data_.size()) {
            return false;
        }
        pos_ = pos;
        return true;
    }

    std::size_t tell() const {
        return pos_;
    }

    bool skip(std::size_t bytes) {
        return seek(pos_ + bytes);
    }

    bool readU32(std::uint32_t& out) {
        if (pos_ + 4 > data_.size()) {
            return false;
        }
        std::memcpy(&out, data_.data() + pos_, 4);
        pos_ += 4;
        return true;
    }

    bool readBytes(std::vector<std::uint8_t>& out, std::size_t bytes) {
        if (pos_ + bytes > data_.size()) {
            return false;
        }
        out.assign(data_.begin() + static_cast<std::ptrdiff_t>(pos_), data_.begin() + static_cast<std::ptrdiff_t>(pos_ + bytes));
        pos_ += bytes;
        return true;
    }

private:
    std::span<const std::uint8_t> data_;
    std::size_t pos_ = 0;
};

std::optional<ChunkHeader> readChunkHeader(Reader& reader) {
    ChunkHeader header;
    if (!reader.readU32(header.type) || !reader.readU32(header.size) || !reader.readU32(header.next) || !reader.readU32(header.child)) {
        return std::nullopt;
    }
    return header;
}

std::uint32_t readLeU32(const std::vector<std::uint8_t>& data, std::size_t off) {
    if (off + 4 > data.size()) {
        return 0;
    }
    std::uint32_t value;
    std::memcpy(&value, data.data() + off, 4);
    return value;
}

std::uint32_t rgbaFromBytes(const std::uint8_t* p) {
    const std::uint8_t r = p[0];
    const std::uint8_t g = p[1];
    const std::uint8_t b = p[2];
    const std::uint8_t a = p[3];
    return (static_cast<std::uint32_t>(a) << 24) |
           (static_cast<std::uint32_t>(r) << 16) |
           (static_cast<std::uint32_t>(g) << 8) |
           static_cast<std::uint32_t>(b);
}

} // namespace

std::optional<GimImage> Parser::load(const std::filesystem::path& filePath, std::string& error, GimAttributes& attributes) {
    std::ifstream input(filePath, std::ios::binary);
    if (!input) {
        error = "无法打开文件。";
        return std::nullopt;
    }

    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (bytes.size() < 16) {
        error = "文件太小，不是有效的 GIM。";
        return std::nullopt;
    }

    auto raw = parseRaw(bytes, error, attributes);
    if (!raw) {
        return std::nullopt;
    }
    return convert(*raw, error);
}

std::optional<Parser::RawImage> Parser::parseRaw(const std::vector<std::uint8_t>& bytes, std::string& error, GimAttributes& attributes) {
    Reader reader(bytes);

    char signature[5]{};
    std::memcpy(signature, bytes.data(), 4);
    attributes.signature = std::string(signature, 4);
    attributes.version = readLeU32(bytes, 4);
    attributes.fileSize = static_cast<std::uint32_t>(bytes.size());

    if (attributes.signature != "MIG.") {
        error = "文件头不是 MIG.，当前实现只支持 PSP GIM(MIG.)。";
        return std::nullopt;
    }

    if (!reader.seek(16)) {
        error = "读取文件头失败。";
        return std::nullopt;
    }

    auto rootHeader = readChunkHeader(reader);
    if (!rootHeader || rootHeader->type != kChunkRoot) {
        error = "未找到 ROOT 块。";
        return std::nullopt;
    }

    std::size_t cursor = rootHeader->child;
    RawImage raw;
    bool imageFound = false;

    while (cursor != 0 && cursor + sizeof(ChunkHeader) <= bytes.size()) {
        if (!reader.seek(cursor)) {
            break;
        }
        auto chunk = readChunkHeader(reader);
        if (!chunk || chunk->size < sizeof(ChunkHeader)) {
            break;
        }

        if (chunk->type == kChunkPicture) {
            std::size_t child = chunk->child;
            while (child != 0 && child + sizeof(ChunkHeader) <= bytes.size()) {
                if (!reader.seek(child)) {
                    break;
                }
                auto sub = readChunkHeader(reader);
                if (!sub || sub->size < sizeof(ChunkHeader)) {
                    break;
                }

                attributes.imageBlockCount++;

                const std::size_t bodyStart = child + sizeof(ChunkHeader);
                const std::size_t bodySize = sub->size - sizeof(ChunkHeader);
                if (bodyStart + bodySize > bytes.size()) {
                    error = "块超出文件范围。";
                    return std::nullopt;
                }

                if (sub->type == kChunkImage) {
                    if (bodySize < 32) {
                        error = "IMAGE 块太小。";
                        return std::nullopt;
                    }
                    raw.info.width = readLeU32(bytes, bodyStart + 0);
                    raw.info.height = readLeU32(bytes, bodyStart + 4);
                    raw.info.stride = readLeU32(bytes, bodyStart + 8);
                    raw.info.format = static_cast<PixelFormat>(readLeU32(bytes, bodyStart + 12));
                    const auto dataOffset = readLeU32(bytes, bodyStart + 16);
                    const auto dataSize = readLeU32(bytes, bodyStart + 20);

                    const std::size_t pixelOff = bodyStart + dataOffset;
                    if (pixelOff + dataSize > bodyStart + bodySize || pixelOff + dataSize > bytes.size()) {
                        error = "像素数据偏移错误。";
                        return std::nullopt;
                    }
                    raw.pixels.assign(bytes.begin() + static_cast<std::ptrdiff_t>(pixelOff),
                                      bytes.begin() + static_cast<std::ptrdiff_t>(pixelOff + dataSize));

                    attributes.images.push_back(raw.info);
                    imageFound = true;
                } else if (sub->type == kChunkPalette) {
                    if (bodySize < 24) {
                        error = "PALETTE 块太小。";
                        return std::nullopt;
                    }
                    raw.paletteFormat = static_cast<PixelFormat>(readLeU32(bytes, bodyStart + 12));
                    const auto dataOffset = readLeU32(bytes, bodyStart + 16);
                    const auto dataSize = readLeU32(bytes, bodyStart + 20);
                    const std::size_t paletteOff = bodyStart + dataOffset;
                    if (paletteOff + dataSize > bodyStart + bodySize || paletteOff + dataSize > bytes.size()) {
                        error = "调色板偏移错误。";
                        return std::nullopt;
                    }
                    raw.palette.assign(bytes.begin() + static_cast<std::ptrdiff_t>(paletteOff),
                                       bytes.begin() + static_cast<std::ptrdiff_t>(paletteOff + dataSize));
                }

                if (sub->next == 0 || sub->next <= child) {
                    break;
                }
                child = sub->next;
            }
        }

        if (chunk->next == 0 || chunk->next <= cursor) {
            break;
        }
        cursor = chunk->next;
    }

    if (!imageFound) {
        error = "没有找到可解析的 IMAGE 块。";
        return std::nullopt;
    }
    return raw;
}

std::optional<GimImage> Parser::convert(const RawImage& raw, std::string& error) {
    GimImage out;
    out.info = raw.info;

    if (raw.info.width == 0 || raw.info.height == 0) {
        error = "无效尺寸。";
        return std::nullopt;
    }

    const std::size_t pixelCount = static_cast<std::size_t>(raw.info.width) * raw.info.height;
    out.rgba.resize(pixelCount);

    if (raw.info.format == PixelFormat::Rgba8888) {
        if (raw.pixels.size() < pixelCount * 4) {
            error = "RGBA8888 数据不足。";
            return std::nullopt;
        }
        for (std::size_t i = 0; i < pixelCount; ++i) {
            out.rgba[i] = rgbaFromBytes(raw.pixels.data() + i * 4);
        }
        return out;
    }

    if (raw.info.format != PixelFormat::Indexed4 && raw.info.format != PixelFormat::Indexed8) {
        error = "暂不支持该像素格式（仅支持 RGBA8888 / Indexed4 / Indexed8）。";
        return std::nullopt;
    }

    if (raw.paletteFormat != PixelFormat::Rgba8888) {
        error = "当前仅支持 RGBA8888 调色板。";
        return std::nullopt;
    }

    const std::size_t colors = raw.palette.size() / 4;
    if (colors == 0) {
        error = "缺少调色板数据。";
        return std::nullopt;
    }

    std::vector<std::uint32_t> palette(colors);
    for (std::size_t i = 0; i < colors; ++i) {
        palette[i] = rgbaFromBytes(raw.palette.data() + i * 4);
    }

    if (raw.info.format == PixelFormat::Indexed8) {
        if (raw.pixels.size() < pixelCount) {
            error = "Indexed8 数据不足。";
            return std::nullopt;
        }
        for (std::size_t i = 0; i < pixelCount; ++i) {
            const auto index = raw.pixels[i];
            out.rgba[i] = palette[index % palette.size()];
        }
    } else {
        if (raw.pixels.size() * 2 < pixelCount) {
            error = "Indexed4 数据不足。";
            return std::nullopt;
        }
        for (std::size_t i = 0; i < pixelCount; ++i) {
            const std::uint8_t packed = raw.pixels[i / 2];
            const std::uint8_t index = (i % 2 == 0) ? (packed & 0x0F) : ((packed >> 4) & 0x0F);
            out.rgba[i] = palette[index % palette.size()];
        }
    }

    return out;
}

} // namespace gim
