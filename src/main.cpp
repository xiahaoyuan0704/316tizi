#include "GimParser.h"

#include <commdlg.h>
#include <windows.h>

#include <map>
#include <sstream>
#include <string>

namespace {

constexpr wchar_t kWindowClass[] = L"GimGridViewerMainWindow";
constexpr UINT kMenuOpen = 1001;
constexpr UINT kMenuExit = 1002;

struct AppState {
    gim::Parser parser;
    gim::GimAttributes model;
    std::wstring currentFile;
    std::wstring message = L"请点击 File -> Open 打开 GIM(Grid Information Model) JSON 文件。";
    bool loaded = false;
};

std::wstring toWide(const std::string& s) {
    if (s.empty()) {
        return {};
    }
    const int count = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring out(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), count);
    return out;
}

std::wstring buildSummary(const AppState& state) {
    std::wstringstream ss;
    ss << L"文件: " << state.currentFile << L"\n";
    ss << L"格式: " << toWide(state.model.format) << L"\n";
    ss << L"版本: " << toWide(state.model.version) << L"\n";
    ss << L"项目: " << toWide(state.model.projectName) << L"\n";
    ss << L"作者: " << toWide(state.model.author) << L"\n";
    ss << L"网格: " << state.model.grid.rows << L" x " << state.model.grid.cols
       << L", Cell(mm): " << state.model.grid.cellSizeMm << L"\n";
    ss << L"原点(mm): (" << state.model.grid.originX << L", " << state.model.grid.originY
       << L"), 旋转(deg): " << state.model.grid.rotationDeg << L"\n";
    ss << L"Cells: " << state.model.cells.size();
    return ss.str();
}

COLORREF colorFromCategory(const std::string& category) {
    static const std::map<std::string, COLORREF> lut = {
        {"Core", RGB(66, 135, 245)},
        {"Wall", RGB(90, 90, 90)},
        {"Door", RGB(210, 145, 45)},
        {"Window", RGB(140, 210, 235)},
        {"Column", RGB(130, 85, 185)},
        {"MEP", RGB(220, 75, 130)},
        {"Empty", RGB(240, 240, 240)},
    };
    const auto it = lut.find(category);
    if (it != lut.end()) {
        return it->second;
    }
    return RGB(160, 200, 160);
}

void drawTextBlock(HDC hdc, const std::wstring& text) {
    RECT rc{16, 16, 980, 180};
    DrawTextW(hdc, text.c_str(), -1, &rc, DT_LEFT | DT_TOP | DT_WORDBREAK);
}

void drawGrid(HDC hdc, const RECT& client, const AppState& state) {
    if (!state.loaded || state.model.grid.rows == 0 || state.model.grid.cols == 0) {
        return;
    }

    const int panelTop = 170;
    const int panelMargin = 16;
    const int panelWidth = (client.right - client.left) - panelMargin * 2;
    const int panelHeight = (client.bottom - client.top) - panelTop - panelMargin;

    if (panelWidth <= 0 || panelHeight <= 0) {
        return;
    }

    const double cellW = static_cast<double>(panelWidth) / state.model.grid.cols;
    const double cellH = static_cast<double>(panelHeight) / state.model.grid.rows;

    HBRUSH emptyBrush = CreateSolidBrush(RGB(245, 245, 245));
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, emptyBrush));
    HPEN gridPen = CreatePen(PS_SOLID, 1, RGB(180, 180, 180));
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, gridPen));

    Rectangle(hdc, panelMargin, panelTop, panelMargin + panelWidth, panelTop + panelHeight);

    for (std::uint32_t r = 0; r < state.model.grid.rows; ++r) {
        for (std::uint32_t c = 0; c < state.model.grid.cols; ++c) {
            const int x0 = panelMargin + static_cast<int>(c * cellW);
            const int y0 = panelTop + static_cast<int>(r * cellH);
            const int x1 = panelMargin + static_cast<int>((c + 1) * cellW);
            const int y1 = panelTop + static_cast<int>((r + 1) * cellH);
            Rectangle(hdc, x0, y0, x1, y1);
        }
    }

    SelectObject(hdc, oldPen);
    DeleteObject(gridPen);

    for (const auto& cell : state.model.cells) {
        const int x0 = panelMargin + static_cast<int>(cell.col * cellW);
        const int y0 = panelTop + static_cast<int>(cell.row * cellH);
        const int x1 = panelMargin + static_cast<int>((cell.col + 1) * cellW);
        const int y1 = panelTop + static_cast<int>((cell.row + 1) * cellH);

        HBRUSH brush = CreateSolidBrush(colorFromCategory(cell.category));
        HBRUSH prev = static_cast<HBRUSH>(SelectObject(hdc, brush));
        Rectangle(hdc, x0, y0, x1, y1);
        SelectObject(hdc, prev);
        DeleteObject(brush);
    }

    SelectObject(hdc, oldBrush);
    DeleteObject(emptyBrush);
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
        InvalidateRect(hwnd, nullptr, TRUE);
        return;
    }

    state.loaded = true;
    state.currentFile = filePath;
    state.model = std::move(*model);
    state.message = buildSummary(state);
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
        auto* appState = reinterpret_cast<AppState*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(appState));
        return 0;
    }
    case WM_COMMAND:
        if (!state) {
            return 0;
        }
        switch (LOWORD(wParam)) {
        case kMenuOpen:
            openFile(hwnd, *state);
            return 0;
        case kMenuExit:
            PostQuitMessage(0);
            return 0;
        default:
            break;
        }
        break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT client{};
        GetClientRect(hwnd, &client);
        FillRect(hdc, &client, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));

        if (state) {
            drawTextBlock(hdc, state->message);
            drawGrid(hdc, client, *state);
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
    wc.cbSize = sizeof(WNDCLASSEXW);
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
        0,
        kWindowClass,
        L"GIM Grid Information Model Viewer",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1200,
        850,
        nullptr,
        createMenuBar(),
        hInstance,
        &state);

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
