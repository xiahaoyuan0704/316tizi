#include "GimParser.h"

#include <commdlg.h>
#include <windows.h>

#include <algorithm>
#include <map>
#include <optional>
#include <sstream>
#include <string>

namespace {

constexpr wchar_t kWindowClass[] = L"GimGridViewerMainWindow";
constexpr UINT kMenuOpen = 1001;
constexpr UINT kMenuExit = 1002;

struct Viewport {
    double zoom = 1.0;
    double offsetX = 0.0;
    double offsetY = 0.0;
};

struct AppState {
    gim::Parser parser;
    gim::GimAttributes model;
    std::wstring currentFile;
    std::wstring message = L"File -> Open 打开 GIM(Grid Information Model) JSON。滚轮缩放，按住鼠标中键平移，左键选中单元格。";
    bool loaded = false;

    Viewport view;
    bool panning = false;
    POINT lastMouse{};

    std::optional<gim::Cell> selectedCell;
    std::string currentLevel = "ALL";
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

std::wstring modelSummary(const AppState& s) {
    std::wstringstream ss;
    ss << L"文件: " << s.currentFile << L"\n";
    ss << L"格式: " << toWide(s.model.format) << L"  版本: " << toWide(s.model.version) << L"\n";
    ss << L"项目: " << toWide(s.model.projectName) << L"  作者: " << toWide(s.model.author) << L"\n";
    ss << L"网格: " << s.model.grid.rows << L" x " << s.model.grid.cols << L"  Cell(mm): " << s.model.grid.cellSizeMm << L"\n";
    ss << L"原点: (" << s.model.grid.originX << L", " << s.model.grid.originY << L")  旋转: " << s.model.grid.rotationDeg << L" deg\n";
    ss << L"楼层数: " << s.model.levels.size() << L"  单元格数: " << s.model.cells.size() << L"  当前楼层: " << toWide(s.currentLevel);
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
    return RECT{16, 190, client.right - 300, client.bottom - 16};
}

RECT panelRect(const RECT& client) {
    return RECT{client.right - 280, 190, client.right - 16, client.bottom - 16};
}

void drawPropertiesPanel(HDC hdc, const RECT& panel, const AppState& state) {
    HBRUSH panelBrush = CreateSolidBrush(RGB(249, 249, 252));
    FillRect(hdc, &panel, panelBrush);
    DeleteObject(panelBrush);
    Rectangle(hdc, panel.left, panel.top, panel.right, panel.bottom);

    RECT inner{panel.left + 10, panel.top + 10, panel.right - 10, panel.bottom - 10};
    std::wstringstream ss;
    ss << L"属性面板\n\n";

    if (state.selectedCell) {
        const auto& c = *state.selectedCell;
        ss << L"Cell: (" << c.row << L", " << c.col << L")\n";
        ss << L"Level: " << toWide(c.level.empty() ? "N/A" : c.level) << L"\n";
        ss << L"Category: " << toWide(c.category) << L"\n";
        ss << L"Usage: " << toWide(c.usage) << L"\n";
        ss << L"Elevation(mm): " << c.elevationMm << L"\n\n";
        ss << L"自定义属性:\n" << propertiesToText(c.properties);
    } else {
        ss << L"未选中单元格。\n\n";
        ss << L"模型属性:\n" << propertiesToText(state.model.properties);
    }

    drawText(hdc, inner, ss.str());
}

void drawGrid(HDC hdc, const RECT& area, const AppState& state) {
    if (!state.loaded || state.model.grid.rows == 0 || state.model.grid.cols == 0) {
        return;
    }

    HBRUSH bg = CreateSolidBrush(RGB(245, 246, 248));
    FillRect(hdc, &area, bg);
    DeleteObject(bg);
    Rectangle(hdc, area.left, area.top, area.right, area.bottom);

    const double baseW = static_cast<double>(area.right - area.left) / state.model.grid.cols;
    const double baseH = static_cast<double>(area.bottom - area.top) / state.model.grid.rows;
    const double cellW = baseW * state.view.zoom;
    const double cellH = baseH * state.view.zoom;

    HPEN linePen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, linePen));

    for (std::uint32_t r = 0; r < state.model.grid.rows; ++r) {
        for (std::uint32_t c = 0; c < state.model.grid.cols; ++c) {
            const int x0 = static_cast<int>(area.left + state.view.offsetX + c * cellW);
            const int y0 = static_cast<int>(area.top + state.view.offsetY + r * cellH);
            const int x1 = static_cast<int>(area.left + state.view.offsetX + (c + 1) * cellW);
            const int y1 = static_cast<int>(area.top + state.view.offsetY + (r + 1) * cellH);
            if (x1 < area.left || y1 < area.top || x0 > area.right || y0 > area.bottom) {
                continue;
            }
            Rectangle(hdc, x0, y0, x1, y1);
        }
    }

    SelectObject(hdc, oldPen);
    DeleteObject(linePen);

    for (const auto& cell : state.model.cells) {
        if (state.currentLevel != "ALL" && !cell.level.empty() && cell.level != state.currentLevel) {
            continue;
        }
        const int x0 = static_cast<int>(area.left + state.view.offsetX + cell.col * cellW);
        const int y0 = static_cast<int>(area.top + state.view.offsetY + cell.row * cellH);
        const int x1 = static_cast<int>(area.left + state.view.offsetX + (cell.col + 1) * cellW);
        const int y1 = static_cast<int>(area.top + state.view.offsetY + (cell.row + 1) * cellH);
        if (x1 < area.left || y1 < area.top || x0 > area.right || y0 > area.bottom) {
            continue;
        }

        HBRUSH brush = CreateSolidBrush(colorFromCategory(cell.category));
        HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, brush));
        Rectangle(hdc, x0, y0, x1, y1);
        SelectObject(hdc, oldBrush);
        DeleteObject(brush);

        if (state.selectedCell && state.selectedCell->row == cell.row && state.selectedCell->col == cell.col &&
            state.selectedCell->level == cell.level) {
            HPEN high = CreatePen(PS_SOLID, 3, RGB(255, 30, 30));
            HPEN prev = static_cast<HPEN>(SelectObject(hdc, high));
            MoveToEx(hdc, x0, y0, nullptr);
            LineTo(hdc, x1, y0);
            LineTo(hdc, x1, y1);
            LineTo(hdc, x0, y1);
            LineTo(hdc, x0, y0);
            SelectObject(hdc, prev);
            DeleteObject(high);
        }
    }
}

std::optional<gim::Cell> hitTestCell(const AppState& state, int x, int y, const RECT& area) {
    if (!state.loaded) {
        return std::nullopt;
    }

    const double baseW = static_cast<double>(area.right - area.left) / state.model.grid.cols;
    const double baseH = static_cast<double>(area.bottom - area.top) / state.model.grid.rows;
    const double cellW = baseW * state.view.zoom;
    const double cellH = baseH * state.view.zoom;

    const int c = static_cast<int>((x - area.left - state.view.offsetX) / cellW);
    const int r = static_cast<int>((y - area.top - state.view.offsetY) / cellH);

    if (r < 0 || c < 0 || r >= static_cast<int>(state.model.grid.rows) || c >= static_cast<int>(state.model.grid.cols)) {
        return std::nullopt;
    }

    for (const auto& cell : state.model.cells) {
        if (static_cast<int>(cell.row) == r && static_cast<int>(cell.col) == c) {
            if (state.currentLevel != "ALL" && !cell.level.empty() && cell.level != state.currentLevel) {
                continue;
            }
            return cell;
        }
    }

    return std::nullopt;
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
        state.message = L"解析失败: " + toWide(error);
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
            state->view.zoom = std::clamp(state->view.zoom + (delta > 0 ? 0.1 : -0.1), 0.3, 4.0);
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
            RECT summary{16, 16, client.right - 16, 170};
            drawText(hdc, summary, state->loaded ? modelSummary(*state) : state->message);
            const RECT area = renderRect(client);
            const RECT panel = panelRect(client);
            drawGrid(hdc, area, *state);
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
        MessageBoxW(nullptr, L"窗口类注册失败。", L"Error", MB_ICONERROR);
        return 1;
    }

    HWND hwnd = CreateWindowExW(
        0, kWindowClass, L"BIM GIM Viewer (Grid Information Model)", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1400, 900, nullptr, createMenuBar(), hInstance, &state);

    if (!hwnd) {
        MessageBoxW(nullptr, L"窗口创建失败。", L"Error", MB_ICONERROR);
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
