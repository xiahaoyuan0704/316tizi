# GIM Viewer（C++ / Visual Studio 2022）

这是一个可直接在 **Windows + Visual Studio 2022** 使用的 GIM 示例项目：

- 解析 GIM 文件头与基础属性（签名、版本、尺寸、格式、Stride、块数量）
- 支持渲染以下图像格式：
  - RGBA8888
  - Indexed4（4-bit，需 RGBA8888 调色板）
  - Indexed8（8-bit，需 RGBA8888 调色板）

> 说明：GIM 在不同工具链下存在变体。本项目专注于常见 **PSP MIG.** 风格 GIM。若你的文件来自其他平台/变体，可在 `GimParser.cpp` 里扩展格式表和 chunk 字段。

## 在 Visual Studio 2022 运行

### 方法 1（推荐）：打开 CMake 项目
1. 启动 Visual Studio 2022。
2. `File -> Open -> Folder...`，选择本项目根目录。
3. VS 会自动识别 `CMakeLists.txt` 并配置。
4. 选择 `GimViewer` 作为启动项，按 `F5` 运行。

### 方法 2：命令行构建
```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```
可执行文件在：`build/Release/GimViewer.exe`

## 使用方式
1. 启动程序。
2. 菜单栏 `File -> Open...`。
3. 选择 `.gim` 文件。
4. 窗口顶部会显示解析后的属性，下面区域显示渲染结果。

## 项目结构

- `src/main.cpp`：Win32 UI、文件打开、信息展示与图像绘制
- `src/GimParser.h`：数据结构与解析器接口
- `src/GimParser.cpp`：GIM chunk 解析 + 像素解码

## 扩展建议

- 增加 swizzle/unswizzle 支持（部分 GIM 数据是 tiled/swizzled）
- 增加更多像素/调色板格式（例如 5650、5551、4444）
- 支持导出 PNG（可接入 WIC 或 stb_image_write）
- 支持一份 GIM 中多图选择预览
