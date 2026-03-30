#include "gl_viewer.hpp"

#include "gim_parser.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl2.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace gim {

namespace {

void drawAxis(float length) {
    glBegin(GL_LINES);
    glColor3f(1.0F, 0.0F, 0.0F);
    glVertex3f(0.0F, 0.0F, 0.0F);
    glVertex3f(length, 0.0F, 0.0F);

    glColor3f(0.0F, 1.0F, 0.0F);
    glVertex3f(0.0F, 0.0F, 0.0F);
    glVertex3f(0.0F, length, 0.0F);

    glColor3f(0.0F, 0.6F, 1.0F);
    glVertex3f(0.0F, 0.0F, 0.0F);
    glVertex3f(0.0F, 0.0F, length);
    glEnd();
}

} // namespace

bool GlViewerApp::initWindow() {
    if (!glfwInit()) {
        parseError_ = "Failed to initialize GLFW";
        hasParseError_ = true;
        return false;
    }

    window_ = glfwCreateWindow(1280, 768, "GIM Studio (OpenGL + ImGui)", nullptr, nullptr);
    if (!window_) {
        parseError_ = "Failed to create GLFW window";
        hasParseError_ = true;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL2_Init();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    return true;
}

void GlViewerApp::cleanup() {
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (window_) {
        glfwDestroyWindow(window_);
    }
    glfwTerminate();
}

void GlViewerApp::drawMenu() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Save As source_path.edited.gim")) {
                pendingSavePath_ = sourcePath_ + ".edited.gim";
                saveModel();
            }
            if (ImGui::MenuItem("Reset Camera")) {
                resetCamera();
            }
            if (ImGui::MenuItem("Exit")) {
                glfwSetWindowShouldClose(window_, GLFW_TRUE);
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void GlViewerApp::drawModel() {
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);

    glViewport(0, 0, width, height);
    glClearColor(0.08F, 0.09F, 0.12F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    const float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0F;
    const float fov = 45.0F;
    const float nearP = 0.1F;
    const float farP = 100.0F;
    const float top = std::tan((fov * 3.1415926F / 180.0F) / 2.0F) * nearP;
    const float right = top * aspect;
    glFrustum(-right, right, -top, top, nearP, farP);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0F, 0.0F, -distance_);
    glRotatef(pitchDeg_, 1.0F, 0.0F, 0.0F);
    glRotatef(yawDeg_, 0.0F, 1.0F, 0.0F);

    drawAxis(1.5F);

    if (showWireframe_) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    glColor3f(0.75F, 0.87F, 1.0F);
    glBegin(GL_TRIANGLES);
    for (std::size_t i = 0; i + 2 < model_.mesh.indices.size(); i += 3) {
        const auto i0 = model_.mesh.indices[i];
        const auto i1 = model_.mesh.indices[i + 1];
        const auto i2 = model_.mesh.indices[i + 2];

        if (i0 >= model_.mesh.vertices.size() || i1 >= model_.mesh.vertices.size() || i2 >= model_.mesh.vertices.size()) {
            continue;
        }

        const auto& v0 = model_.mesh.vertices[i0];
        const auto& v1 = model_.mesh.vertices[i1];
        const auto& v2 = model_.mesh.vertices[i2];
        glVertex3f(v0.x, v0.y, v0.z);
        glVertex3f(v1.x, v1.y, v1.z);
        glVertex3f(v2.x, v2.y, v2.z);
    }
    glEnd();

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void GlViewerApp::drawPropertyPanel() {
    ImGui::Begin("Property Panel");
    ImGui::Text("Model: %s", model_.name.c_str());
    ImGui::Text("Vertices: %d", static_cast<int>(model_.mesh.vertices.size()));
    ImGui::Text("Faces: %d", static_cast<int>(model_.mesh.indices.size() / 3));
    ImGui::Separator();

    ImGui::Checkbox("Wireframe", &showWireframe_);
    ImGui::Checkbox("Auto Rotate", &autoRotate_);
    ImGui::SliderFloat("Yaw", &yawDeg_, -180.0F, 180.0F);
    ImGui::SliderFloat("Pitch", &pitchDeg_, -89.0F, 89.0F);
    ImGui::SliderFloat("Distance", &distance_, 0.5F, 20.0F);

    ImGui::Separator();
    ImGui::Text("Attributes (editable)");

    for (auto& [key, value] : model_.attributes) {
        ImGui::PushID(key.c_str());
        if (std::holds_alternative<int>(value)) {
            int v = std::get<int>(value);
            if (ImGui::InputInt(key.c_str(), &v)) {
                value = v;
            }
        } else if (std::holds_alternative<float>(value)) {
            float v = std::get<float>(value);
            if (ImGui::InputFloat(key.c_str(), &v)) {
                value = v;
            }
        } else {
            std::string s = std::get<std::string>(value);
            char buffer[256] = {0};
            const auto copyLen = std::min<std::size_t>(s.size(), sizeof(buffer) - 1);
            std::copy_n(s.c_str(), copyLen, buffer);
            if (ImGui::InputText(key.c_str(), buffer, sizeof(buffer))) {
                value = std::string(buffer);
            }
        }
        ImGui::PopID();
    }

    if (ImGui::Button("Save Edited Model")) {
        pendingSavePath_ = sourcePath_ + ".edited.gim";
        saveModel();
    }

    ImGui::End();
}

void GlViewerApp::drawStatusBar() {
    ImGui::Begin("Status", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);
    if (hasParseError_) {
        ImGui::TextColored(ImVec4(1.0F, 0.5F, 0.5F, 1.0F), "Error: %s", parseError_.c_str());
    } else {
        ImGui::Text("Loaded: %s", sourcePath_.c_str());
        ImGui::Text("Save path: %s", pendingSavePath_.empty() ? "(not saved yet)" : pendingSavePath_.c_str());
    }
    ImGui::End();
}

void GlViewerApp::resetCamera() {
    yawDeg_ = 0.0F;
    pitchDeg_ = 15.0F;
    distance_ = 3.0F;
}

void GlViewerApp::saveModel() {
    try {
        GimParser parser;
        parser.writeToFile(model_, pendingSavePath_);
    } catch (const std::exception& ex) {
        hasParseError_ = true;
        parseError_ = ex.what();
    }
}

int GlViewerApp::run(const std::string& gimPath) {
    sourcePath_ = gimPath;
    try {
        GimParser parser;
        model_ = parser.parseFromFile(gimPath);
    } catch (const std::exception& ex) {
        hasParseError_ = true;
        parseError_ = ex.what();
    }

    if (!initWindow()) {
        cleanup();
        return 2;
    }

    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        if (autoRotate_) {
            yawDeg_ += 0.1F;
            if (yawDeg_ > 180.0F) {
                yawDeg_ -= 360.0F;
            }
        }

        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        drawMenu();
        drawPropertyPanel();
        drawStatusBar();

        drawModel();

        ImGui::Render();
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window_);
    }

    cleanup();
    return 0;
}

} // namespace gim
