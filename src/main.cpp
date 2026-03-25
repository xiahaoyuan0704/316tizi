#include "gim_model.hpp"
#include "gim_parser.hpp"
#include "software_renderer.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void printUsage() {
    std::cout << "Usage:\n"
              << "  gim_toolkit inspect <file.gim>\n"
              << "  gim_toolkit set-attr <input.gim> <key> <type:int|float|string> <value> <output.gim>\n"
              << "  gim_toolkit render <input.gim> <output.ppm>\n";
}

gim::AttributeValue parseValue(const std::string& type, const std::string& raw) {
    if (type == "int") {
        return std::stoi(raw);
    }
    if (type == "float") {
        return std::stof(raw);
    }
    if (type == "string") {
        return raw;
    }
    throw std::runtime_error("Unsupported type: " + type);
}

void inspect(const gim::Model& model) {
    std::cout << "Model name: " << model.name << "\n";
    std::cout << "Attributes (" << model.attributes.size() << "):\n";
    for (const auto& [key, value] : model.attributes) {
        std::cout << "  - " << key << " = " << gim::attributeValueToString(value) << "\n";
    }

    std::cout << "Vertices: " << model.mesh.vertices.size() << "\n";
    std::cout << "Faces: " << model.mesh.indices.size() / 3 << "\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 3) {
            printUsage();
            return 1;
        }

        const std::string command = argv[1];
        gim::GimParser parser;

        if (command == "inspect") {
            const auto model = parser.parseFromFile(argv[2]);
            inspect(model);
            return 0;
        }

        if (command == "set-attr") {
            if (argc != 7) {
                printUsage();
                return 1;
            }
            auto model = parser.parseFromFile(argv[2]);
            model.attributes[argv[3]] = parseValue(argv[4], argv[5]);
            parser.writeToFile(model, argv[6]);
            std::cout << "Attribute updated and saved to " << argv[6] << "\n";
            return 0;
        }

        if (command == "render") {
            if (argc != 4) {
                printUsage();
                return 1;
            }
            const auto model = parser.parseFromFile(argv[2]);
            gim::SoftwareRenderer renderer;
            renderer.renderWireframeToPpm(model, argv[3]);
            std::cout << "Rendered wireframe image: " << argv[3] << "\n";
            return 0;
        }

        printUsage();
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 2;
    }
}
