#include "gl_viewer.hpp"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: gim_studio <file.gim>\n";
        return 1;
    }

    gim::GlViewerApp app;
    return app.run(argv[1]);
}
