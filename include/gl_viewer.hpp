#pragma once

#include "gim_model.hpp"

#include <string>

struct GLFWwindow;

namespace gim {

class GlViewerApp {
public:
    int run(const std::string& gimPath);

private:
    bool initWindow();
    void cleanup();

    void drawMenu();
    void drawModel();
    void drawPropertyPanel();
    void drawStatusBar();

    void resetCamera();
    void saveModel();

    GLFWwindow* window_ = nullptr;
    Model model_{};
    std::string sourcePath_;
    std::string pendingSavePath_;

    bool showWireframe_ = true;
    bool autoRotate_ = true;
    float yawDeg_ = 0.0F;
    float pitchDeg_ = 15.0F;
    float distance_ = 3.0F;

    bool hasParseError_ = false;
    std::string parseError_;
};

} // namespace gim
