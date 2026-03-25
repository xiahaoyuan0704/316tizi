#include "software_renderer.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace gim {
namespace {

struct Pixel {
    unsigned char r = 0;
    unsigned char g = 0;
    unsigned char b = 0;
};

void setPixel(std::vector<Pixel>& buffer, int width, int height, int x, int y, Pixel color) {
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return;
    }
    buffer[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)] = color;
}

void drawLine(std::vector<Pixel>& buffer, int width, int height, int x0, int y0, int x1, int y1, Pixel color) {
    const int dx = std::abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (true) {
        setPixel(buffer, width, height, x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        const int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

} // namespace

void SoftwareRenderer::renderWireframeToPpm(const Model& model, const std::string& outputPath, int width, int height) const {
    if (model.mesh.vertices.empty()) {
        throw std::runtime_error("Model has no vertices.");
    }

    std::vector<Pixel> buffer(static_cast<std::size_t>(width * height), Pixel{15, 18, 28});

    float minX = model.mesh.vertices[0].x;
    float maxX = model.mesh.vertices[0].x;
    float minY = model.mesh.vertices[0].y;
    float maxY = model.mesh.vertices[0].y;

    for (const auto& v : model.mesh.vertices) {
        minX = std::min(minX, v.x);
        maxX = std::max(maxX, v.x);
        minY = std::min(minY, v.y);
        maxY = std::max(maxY, v.y);
    }

    const float spanX = std::max(0.0001F, maxX - minX);
    const float spanY = std::max(0.0001F, maxY - minY);

    auto project = [&](const Vec3& v) {
        const float nx = (v.x - minX) / spanX;
        const float ny = (v.y - minY) / spanY;
        const int px = static_cast<int>(nx * static_cast<float>(width - 1));
        const int py = height - 1 - static_cast<int>(ny * static_cast<float>(height - 1));
        return std::pair<int, int>{px, py};
    };

    const Pixel edgeColor{129, 207, 255};

    for (std::size_t i = 0; i + 2 < model.mesh.indices.size(); i += 3) {
        const auto i0 = model.mesh.indices[i];
        const auto i1 = model.mesh.indices[i + 1];
        const auto i2 = model.mesh.indices[i + 2];
        if (i0 >= model.mesh.vertices.size() || i1 >= model.mesh.vertices.size() || i2 >= model.mesh.vertices.size()) {
            continue;
        }

        const auto p0 = project(model.mesh.vertices[i0]);
        const auto p1 = project(model.mesh.vertices[i1]);
        const auto p2 = project(model.mesh.vertices[i2]);

        drawLine(buffer, width, height, p0.first, p0.second, p1.first, p1.second, edgeColor);
        drawLine(buffer, width, height, p1.first, p1.second, p2.first, p2.second, edgeColor);
        drawLine(buffer, width, height, p2.first, p2.second, p0.first, p0.second, edgeColor);
    }

    std::ofstream ofs(outputPath, std::ios::binary);
    if (!ofs) {
        throw std::runtime_error("Cannot write render output: " + outputPath);
    }

    ofs << "P6\n" << width << ' ' << height << "\n255\n";
    for (const auto& px : buffer) {
        ofs.put(static_cast<char>(px.r));
        ofs.put(static_cast<char>(px.g));
        ofs.put(static_cast<char>(px.b));
    }
}

} // namespace gim
