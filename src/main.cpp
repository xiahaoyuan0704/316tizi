#include "GimParser.h"

#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr wchar_t kWindowClass[] = L"GimGridViewerMainWindow";
constexpr UINT kMenuOpen = 1001;
constexpr UINT kMenuExit = 1002;

struct Viewport {
    double zoom = 1.0;
    double offsetX = 0.0;
    double offsetY = 0.0;
    double yaw = 45.0;
    double pitch = 28.0;
};

struct AppState {
    gim::Parser parser;
    gim::GimAttributes model;
    std::wstring currentFile;
    std::wstring message = L"Open a GIM JSON. Wheel: zoom, middle drag: pan, left click: select, A/D rotate, W/S pitch.";
    bool loaded = false;

    Viewport view;
    bool panning = false;
    POINT lastMouse{};

    std::optional<gim::Cell> selectedCell;
    std::string currentLevel = "ALL";
};

struct Vec3 {
    double x = 0;
    double y = 0;
    double z = 0;
};

struct Vec2 {
    int x = 0;
    int y = 0;
};

std::wstring toWide(const std::string& s) {
    if (s.empty()) {
        return {};
    }
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring out(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), n);
    return out;
}

COLORREF colorFromCategory(const std::string& category) {
    static const std::map<std::string, COLORREF> lut = {
        {"Core", RGB(66, 135, 245)}, {"Wall", RGB(90, 90, 90)}, {"Door", RGB(210, 145, 45)},
        {"Window", RGB(140, 210, 235)}, {"Column", RGB(130, 85, 185)}, {"MEP", RGB(220, 75, 130)},
        {"Slab", RGB(150, 140, 120)}, {"Beam", RGB(120, 100, 70)}, {"Empty", RGB(240, 240, 240)}};

    const auto it = lut.find(category);
    return it == lut.end() ? RGB(160, 200, 160) : it->second;
}

COLORREF shade(COLORREF c, double factor) {
    auto ch = [factor](int v) { return std::clamp(static_cast<int>(v * factor), 0, 255); };
    return RGB(ch(GetRValue(c)), ch(GetGValue(c)), ch(GetBValue(c)));
}

std::wstring modelSummary(const AppState& s) {
    std::wstringstream ss;
    ss << L"File: " << s.currentFile << L"\n";
    ss << L"Format: " << toWide(s.model.format) << L"  Version: " << toWide(s.model.version) << L"\n";
    ss << L"Project: " << toWide(s.model.projectName) << L"  Author: " << toWide(s.model.author) << L"\n";
    ss << L"Grid: " << s.model.grid.rows << L" x " << s.model.grid.cols << L"  Cell(mm): " << s.model.grid.cellSizeMm << L"\n";
    ss << L"Levels: " << s.model.levels.size() << L"  Cells: " << s.model.cells.size() << L"  Current level: " << toWide(s.currentLevel) << L"\n";
    ss << L"3D View - Yaw: " << s.view.yaw << L" Pitch: " << s.view.pitch << L" Zoom: " << s.view.zoom;
    return ss.str();
}

std::wstring propertiesToText(const gim::Properties& props) {
    std::wstringstream ss;
    for (const auto& [k, v] : props) {
        ss << toWide(k) << L": " << toWide(v) << L"\n";
    }
    return ss.str();
}

void drawText(HDC hdc, const RECT& rect, const std::wstring& text) {
    DrawTextW(hdc, text.c_str(), -1, const_cast<RECT*>(&rect), DT_LEFT | DT_TOP | DT_WORDBREAK);
}

RECT renderRect(const RECT& client) {
    return RECT{16, 200, client.right - 300, client.bottom - 16};
}

RECT panelRect(const RECT& client) {
    return RECT{client.right - 280, 200, client.right - 16, client.bottom - 16};
}

void drawPropertiesPanel(HDC hdc, const RECT& panel, const AppState& state) {
    HBRUSH panelBrush = CreateSolidBrush(RGB(249, 249, 252));
    FillRect(hdc, &panel, panelBrush);
    DeleteObject(panelBrush);
    Rectangle(hdc, panel.left, panel.top, panel.right, panel.bottom);

    RECT inner{panel.left + 10, panel.top + 10, panel.right - 10, panel.bottom - 10};
    std::wstringstream ss;
    ss << L"Properties\n\n";

    if (state.selectedCell) {
        const auto& c = *state.selectedCell;
        ss << L"Cell: (" << c.row << L", " << c.col << L")\n";
        ss << L"Level: " << toWide(c.level.empty() ? "N/A" : c.level) << L"\n";
        ss << L"Category: " << toWide(c.category) << L"\n";
        ss << L"Usage: " << toWide(c.usage) << L"\n";
        ss << L"Elevation(mm): " << c.elevationMm << L"\n";
        ss << L"Height(mm): " << c.heightMm << L"\n\n";
        ss << L"Custom properties:\n" << propertiesToText(c.properties);
    } else {
        ss << L"No cell selected.\n\n";
        ss << L"Model properties:\n" << propertiesToText(state.model.properties);
    }

    drawText(hdc, inner, ss.str());
}

Vec2 project(const Vec3& p, const RECT& area, const Viewport& v, double scale) {
    const double yaw = v.yaw * 3.1415926535 / 180.0;
    const double pitch = v.pitch * 3.1415926535 / 180.0;

    const double cy = std::cos(yaw);
    const double sy = std::sin(yaw);
    const double cp = std::cos(pitch);
    const double sp = std::sin(pitch);

    const double rx = p.x * cy - p.y * sy;
    const double ry = p.x * sy + p.y * cy;
    const double rz = p.z;

    const double ry2 = ry * cp - rz * sp;

    Vec2 out;
    out.x = static_cast<int>((area.left + area.right) * 0.5 + v.offsetX + rx * scale * v.zoom);
    out.y = static_cast<int>((area.top + area.bottom) * 0.5 + v.offsetY - ry2 * scale * v.zoom);
    return out;
}

void fillQuad(HDC hdc, const Vec2& a, const Vec2& b, const Vec2& c, const Vec2& d, COLORREF color) {
    POINT pts[4]{{a.x, a.y}, {b.x, b.y}, {c.x, c.y}, {d.x, d.y}};
    HBRUSH brush = CreateSolidBrush(color);
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, brush));
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(30, 30, 30));
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    Polygon(hdc, pts, 4);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void drawCell3D(HDC hdc, const RECT& area, const AppState& state, const gim::Cell& cell) {
    const double cellSize = state.model.grid.cellSizeMm;
    const double gridW = state.model.grid.cols * cellSize;
    const double gridH = state.model.grid.rows * cellSize;

    const double x0 = cell.col * cellSize - gridW / 2.0;
    const double y0 = cell.row * cellSize - gridH / 2.0;
    const double x1 = x0 + cellSize;
    const double y1 = y0 + cellSize;
    const double z0 = cell.elevationMm;
    const double z1 = z0 + cell.heightMm;

    constexpr double kScale = 0.05;

    Vec2 p000 = project({x0, y0, z0}, area, state.view, kScale);
    Vec2 p100 = project({x1, y0, z0}, area, state.view, kScale);
    Vec2 p110 = project({x1, y1, z0}, area, state.view, kScale);
    Vec2 p010 = project({x0, y1, z0}, area, state.view, kScale);

    Vec2 p001 = project({x0, y0, z1}, area, state.view, kScale);
    Vec2 p101 = project({x1, y0, z1}, area, state.view, kScale);
    Vec2 p111 = project({x1, y1, z1}, area, state.view, kScale);
    Vec2 p011 = project({x0, y1, z1}, area, state.view, kScale);

    COLORREF base = colorFromCategory(cell.category);
    fillQuad(hdc, p001, p101, p111, p011, shade(base, 1.15)); // top
    fillQuad(hdc, p000, p100, p101, p001, shade(base, 0.95)); // side 1
    fillQuad(hdc, p100, p110, p111, p101, shade(base, 0.75)); // side 2

    if (state.selectedCell && state.selectedCell->row == cell.row && state.selectedCell->col == cell.col && state.selectedCell->level == cell.level) {
        HPEN pen = CreatePen(PS_SOLID, 2, RGB(255, 40, 40));
        HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
        MoveToEx(hdc, p001.x, p001.y, nullptr); LineTo(hdc, p101.x, p101.y); LineTo(hdc, p111.x, p111.y); LineTo(hdc, p011.x, p011.y); LineTo(hdc, p001.x, p001.y);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);
    }

    // subtle ground grid for context
    HPEN gpen = CreatePen(PS_SOLID, 1, RGB(220, 220, 220));
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, gpen));
    MoveToEx(hdc, p000.x, p000.y, nullptr); LineTo(hdc, p100.x, p100.y); LineTo(hdc, p110.x, p110.y); LineTo(hdc, p010.x, p010.y); LineTo(hdc, p000.x, p000.y);
    SelectObject(hdc, oldPen);
    DeleteObject(gpen);
}

void drawScene3D(HDC hdc, const RECT& area, const AppState& state) {
    HBRUSH bg = CreateSolidBrush(RGB(242, 244, 248));
    FillRect(hdc, &area, bg);
    DeleteObject(bg);
    Rectangle(hdc, area.left, area.top, area.right, area.bottom);

    if (!state.loaded || state.model.grid.rows == 0 || state.model.grid.cols == 0) {
        return;
    }

    std::vector<const gim::Cell*> visible;
    for (const auto& c : state.model.cells) {
        if (state.currentLevel != "ALL" && !c.level.empty() && c.level != state.currentLevel) {
            continue;
        }
        visible.push_back(&c);
    }

    std::sort(visible.begin(), visible.end(), [](const gim::Cell* a, const gim::Cell* b) {
        return (a->row + a->col + a->elevationMm / 1000.0) < (b->row + b->col + b->elevationMm / 1000.0);
    });

    for (const auto* c : visible) {
        drawCell3D(hdc, area, state, *c);
    }
}

std::optional<gim::Cell> hitTestCell(const AppState& state, int x, int y, const RECT& area) {
    if (!state.loaded) {
        return std::nullopt;
    }
    constexpr int radius = 16;
    double best = 1e18;
    std::optional<gim::Cell> hit;

    for (const auto& c : state.model.cells) {
        if (state.currentLevel != "ALL" && !c.level.empty() && c.level != state.currentLevel) {
            continue;
        }
        const double cellSize = state.model.grid.cellSizeMm;
        const double gridW = state.model.grid.cols * cellSize;
        const double gridH = state.model.grid.rows * cellSize;
        const double cx = (c.col + 0.5) * cellSize - gridW / 2.0;
        const double cy = (c.row + 0.5) * cellSize - gridH / 2.0;
        const double cz = c.elevationMm + c.heightMm;
        Vec2 p = project({cx, cy, cz}, area, state.view, 0.05);

        const double d = std::hypot(static_cast<double>(p.x - x), static_cast<double>(p.y - y));
        if (d < radius && d < best) {
            best = d;
            hit = c;
        }
    }
    return hit;
}

void switchLevel(AppState& state, int direction) {
    if (state.model.levels.empty()) {
        state.currentLevel = "ALL";
        return;
    }
    std::vector<std::string> names{"ALL"};
    for (const auto& lv : state.model.levels) {
        names.push_back(lv.name);
    }
    auto it = std::find(names.begin(), names.end(), state.currentLevel);
    int idx = (it == names.end()) ? 0 : static_cast<int>(std::distance(names.begin(), it));
    idx += direction;
    if (idx < 0) idx = static_cast<int>(names.size()) - 1;
    if (idx >= static_cast<int>(names.size())) idx = 0;
    state.currentLevel = names[static_cast<std::size_t>(idx)];
}

void openFile(HWND hwnd, AppState& state) {
    OPENFILENAMEW ofn{};
    wchar_t filePath[MAX_PATH] = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"GIM Grid Model (*.gim.json;*.json)\0*.gim.json;*.json\0All Files\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (!GetOpenFileNameW(&ofn)) {
        return;
    }

    std::string error;
    auto model = state.parser.load(filePath, error);
    if (!model) {
        state.loaded = false;
        state.message = L"Parse failed: " + toWide(error);
        state.selectedCell.reset();
        InvalidateRect(hwnd, nullptr, TRUE);
        return;
    }

    state.loaded = true;
    state.model = std::move(*model);
    state.currentFile = filePath;
    state.selectedCell.reset();
    state.currentLevel = "ALL";
    state.view = {};
    state.message = modelSummary(state);
    InvalidateRect(hwnd, nullptr, TRUE);
}

HMENU createMenuBar() {
    HMENU menubar = CreateMenu();
    HMENU fileMenu = CreatePopupMenu();
    AppendMenuW(fileMenu, MF_STRING, kMenuOpen, L"Open...");
    AppendMenuW(fileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(fileMenu, MF_STRING, kMenuExit, L"Exit");
    AppendMenuW(menubar, MF_POPUP, reinterpret_cast<UINT_PTR>(fileMenu), L"File");
    return menubar;
}

LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<AppState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_CREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* app = reinterpret_cast<AppState*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        return 0;
    }
    case WM_COMMAND:
        if (!state) return 0;
        if (LOWORD(wParam) == kMenuOpen) {
            openFile(hwnd, *state);
            return 0;
        }
        if (LOWORD(wParam) == kMenuExit) {
            PostQuitMessage(0);
            return 0;
        }
        break;
    case WM_MOUSEWHEEL:
        if (state && state->loaded) {
            const short delta = GET_WHEEL_DELTA_WPARAM(wParam);
            state->view.zoom = std::clamp(state->view.zoom + (delta > 0 ? 0.1 : -0.1), 0.3, 5.0);
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        return 0;
    case WM_MBUTTONDOWN:
        if (state) {
            state->panning = true;
            state->lastMouse.x = GET_X_LPARAM(lParam);
            state->lastMouse.y = GET_Y_LPARAM(lParam);
            SetCapture(hwnd);
        }
        return 0;
    case WM_MOUSEMOVE:
        if (state && state->panning) {
            const int x = GET_X_LPARAM(lParam);
            const int y = GET_Y_LPARAM(lParam);
            state->view.offsetX += (x - state->lastMouse.x);
            state->view.offsetY += (y - state->lastMouse.y);
            state->lastMouse = POINT{x, y};
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        return 0;
    case WM_MBUTTONUP:
        if (state) {
            state->panning = false;
            ReleaseCapture();
        }
        return 0;
    case WM_LBUTTONDOWN:
        if (state && state->loaded) {
            RECT client{};
            GetClientRect(hwnd, &client);
            const RECT area = renderRect(client);
            const int x = GET_X_LPARAM(lParam);
            const int y = GET_Y_LPARAM(lParam);
            if (x >= area.left && x <= area.right && y >= area.top && y <= area.bottom) {
                state->selectedCell = hitTestCell(*state, x, y, area);
                InvalidateRect(hwnd, nullptr, TRUE);
            }
        }
        return 0;
    case WM_KEYDOWN:
        if (state && state->loaded) {
            if (wParam == VK_LEFT) {
                switchLevel(*state, -1);
            } else if (wParam == VK_RIGHT) {
                switchLevel(*state, 1);
            } else if (wParam == 'A') {
                state->view.yaw -= 5.0;
            } else if (wParam == 'D') {
                state->view.yaw += 5.0;
            } else if (wParam == 'W') {
                state->view.pitch = std::clamp(state->view.pitch + 3.0, 5.0, 80.0);
            } else if (wParam == 'S') {
                state->view.pitch = std::clamp(state->view.pitch - 3.0, 5.0, 80.0);
            } else {
                break;
            }
            state->message = modelSummary(*state);
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        }
        break;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT client{};
        GetClientRect(hwnd, &client);
        FillRect(hdc, &client, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));

        if (state) {
            RECT summary{16, 16, client.right - 16, 180};
            drawText(hdc, summary, state->loaded ? modelSummary(*state) : state->message);
            const RECT area = renderRect(client);
            const RECT panel = panelRect(client);
            drawScene3D(hdc, area, *state);
            drawPropertiesPanel(hdc, panel, *state);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    AppState state;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = wndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kWindowClass;

    if (!RegisterClassExW(&wc)) {
        MessageBoxW(nullptr, L"Failed to register window class.", L"Error", MB_ICONERROR);
        return 1;
    }

    HWND hwnd = CreateWindowExW(
        0, kWindowClass, L"BIM GIM Viewer (3D Grid)", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1500, 900, nullptr, createMenuBar(), hInstance, &state);

    if (!hwnd) {
        MessageBoxW(nullptr, L"Failed to create window.", L"Error", MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}
