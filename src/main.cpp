#include "GimParser.h"

#include <commdlg.h>
#include <windows.h>

#include <memory>
#include <sstream>
#include <string>

namespace {

constexpr wchar_t kWindowClass[] = L"GimViewerMainWindow";
constexpr UINT kMenuOpen = 1001;
constexpr UINT kMenuExit = 1002;

struct AppState {
    gim::Parser parser;
    gim::GimImage image;
    gim::GimAttributes attributes;
    std::wstring currentFile;
    std::wstring message = L"请点击 File -> Open 打开 GIM 文件。";
    bool hasImage = false;
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

void drawTextLine(HDC hdc, int x, int y, const std::wstring& text) {
    TextOutW(hdc, x, y, text.c_str(), static_cast<int>(text.size()));
}

void drawImage(HDC hdc, const RECT& client, const AppState& state) {
    if (!state.hasImage) {
        drawTextLine(hdc, 16, 16, state.message);
        return;
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = static_cast<LONG>(state.image.info.width);
    bmi.bmiHeader.biHeight = -static_cast<LONG>(state.image.info.height);
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    const int maxW = (client.right - client.left) - 32;
    const int maxH = (client.bottom - client.top) - 120;
    const double sx = static_cast<double>(maxW) / static_cast<double>(state.image.info.width);
    const double sy = static_cast<double>(maxH) / static_cast<double>(state.image.info.height);
    const double scale = maxW > 0 && maxH > 0 ? min(1.0, min(sx, sy)) : 1.0;

    const int drawW = static_cast<int>(state.image.info.width * scale);
    const int drawH = static_cast<int>(state.image.info.height * scale);
    const int x = 16;
    const int y = 100;

    std::vector<std::uint32_t> bgra(state.image.rgba.size());
    for (std::size_t i = 0; i < state.image.rgba.size(); ++i) {
        const auto c = state.image.rgba[i];
        const auto a = (c >> 24) & 0xFF;
        const auto r = (c >> 16) & 0xFF;
        const auto g = (c >> 8) & 0xFF;
        const auto b = c & 0xFF;
        bgra[i] = (a << 24) | (b << 16) | (g << 8) | r;
    }

    StretchDIBits(
        hdc,
        x,
        y,
        drawW,
        drawH,
        0,
        0,
        static_cast<int>(state.image.info.width),
        static_cast<int>(state.image.info.height),
        bgra.data(),
        &bmi,
        DIB_RGB_COLORS,
        SRCCOPY);

    Rectangle(hdc, x - 1, y - 1, x + drawW + 1, y + drawH + 1);
}

std::wstring formatInfo(const AppState& state) {
    std::wstringstream ss;
    ss << L"文件: " << state.currentFile << L"\n";
    ss << L"签名: " << toWide(state.attributes.signature) << L"  版本: " << state.attributes.version << L"\n";
    ss << L"尺寸: " << state.image.info.width << L" x " << state.image.info.height << L"\n";
    ss << L"Stride: " << state.image.info.stride << L"\n";
    ss << L"图片块数量: " << state.attributes.imageBlockCount;
    return ss.str();
}

void openFile(HWND hwnd, AppState& state) {
    OPENFILENAMEW ofn{};
    wchar_t filePath[MAX_PATH] = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"GIM Files (*.gim)\0*.gim\0All Files\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (!GetOpenFileNameW(&ofn)) {
        return;
    }

    std::string err;
    gim::GimAttributes attrs;
    auto image = state.parser.load(filePath, err, attrs);

    if (!image) {
        state.hasImage = false;
        state.message = L"加载失败: " + toWide(err);
        InvalidateRect(hwnd, nullptr, TRUE);
        return;
    }

    state.hasImage = true;
    state.currentFile = filePath;
    state.attributes = std::move(attrs);
    state.image = std::move(*image);
    state.message = formatInfo(state);
    InvalidateRect(hwnd, nullptr, TRUE);
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
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT client{};
        GetClientRect(hwnd, &client);

        FillRect(hdc, &client, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
        if (state) {
            drawTextLine(hdc, 16, 16, state->message);
            drawImage(hdc, client, *state);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
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
        MessageBoxW(nullptr, L"注册窗口类失败。", L"Error", MB_ICONERROR);
        return 1;
    }

    HWND hwnd = CreateWindowExW(
        0,
        kWindowClass,
        L"GIM Viewer (C++ / Win32)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1000,
        760,
        nullptr,
        createMenuBar(),
        hInstance,
        &state);

    if (!hwnd) {
        MessageBoxW(nullptr, L"创建窗口失败。", L"Error", MB_ICONERROR);
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
