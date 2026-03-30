#include "gim_parser.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace gim {

namespace {

std::vector<std::string> split(const std::string& value, char delim) {
    std::vector<std::string> parts;
    std::stringstream ss(value);
    std::string token;
    while (std::getline(ss, token, delim)) {
        parts.push_back(token);
    }
    return parts;
}

} // namespace

AttributeValue GimParser::parseAttributeValue(const std::string& type, const std::string& raw) {
    if (type == "int") {
        return std::stoi(raw);
    }
    if (type == "float") {
        return std::stof(raw);
    }
    if (type == "string") {
        return raw;
    }
    throw std::runtime_error("Unsupported attribute type: " + type);
}

Model GimParser::parseFromFile(const std::string& path) const {
    std::ifstream ifs(path);
    if (!ifs) {
        throw std::runtime_error("Cannot open file: " + path);
    }

    Model model;
    std::string line;
    bool hasHeader = false;

    while (std::getline(ifs, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        if (!hasHeader) {
            if (line != "GIMv1") {
                throw std::runtime_error("Invalid GIM header, expected GIMv1");
            }
            hasHeader = true;
            continue;
        }

        if (line.rfind("name=", 0) == 0) {
            model.name = line.substr(5);
            continue;
        }

        if (line.rfind("attr:", 0) == 0) {
            auto content = line.substr(5);
            auto parts = split(content, ':');
            if (parts.size() < 3) {
                throw std::runtime_error("Invalid attr line: " + line);
            }
            model.attributes[parts[0]] = parseAttributeValue(parts[1], parts[2]);
            continue;
        }

        if (line.rfind("v ", 0) == 0) {
            std::stringstream ss(line.substr(2));
            Vec3 v{};
            ss >> v.x >> v.y >> v.z;
            if (ss.fail()) {
                throw std::runtime_error("Invalid vertex line: " + line);
            }
            model.mesh.vertices.push_back(v);
            continue;
        }

        if (line.rfind("f ", 0) == 0) {
            std::stringstream ss(line.substr(2));
            unsigned int a = 0;
            unsigned int b = 0;
            unsigned int c = 0;
            ss >> a >> b >> c;
            if (ss.fail()) {
                throw std::runtime_error("Invalid face line: " + line);
            }
            model.mesh.indices.push_back(a);
            model.mesh.indices.push_back(b);
            model.mesh.indices.push_back(c);
            continue;
        }

        throw std::runtime_error("Unknown line token: " + line);
    }

    if (!hasHeader) {
        throw std::runtime_error("Missing GIM header");
    }
    return model;
}

void GimParser::writeToFile(const Model& model, const std::string& path) const {
    std::ofstream ofs(path);
    if (!ofs) {
        throw std::runtime_error("Cannot write file: " + path);
    }

    ofs << "GIMv1\n";
    ofs << "name=" << model.name << "\n";

    for (const auto& [key, value] : model.attributes) {
        std::string type = "string";
        if (std::holds_alternative<int>(value)) {
            type = "int";
        } else if (std::holds_alternative<float>(value)) {
            type = "float";
        }
        ofs << "attr:" << key << ':' << type << ':' << attributeValueToString(value) << "\n";
    }

    for (const auto& v : model.mesh.vertices) {
        ofs << "v " << v.x << ' ' << v.y << ' ' << v.z << "\n";
    }

    for (std::size_t i = 0; i + 2 < model.mesh.indices.size(); i += 3) {
        ofs << "f " << model.mesh.indices[i] << ' ' << model.mesh.indices[i + 1] << ' ' << model.mesh.indices[i + 2] << "\n";
    }
}

} // namespace gim
