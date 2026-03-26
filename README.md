# GIM Toolkit (C++)

这是一个用于 `.gim`（Grid Information Model）实验性处理的 C++ 原型，包含三个核心能力：

1. **属性解析与查看**：读取 GIM 文件并输出属性、顶点、面片统计。
2. **属性编辑**：命令行修改属性并写回新文件。
3. **模型渲染**：支持离线线框渲染（PPM）与实时 OpenGL 预览。

> 说明：目前实现的是一个 `GIMv1` 的简化文本格式，便于你快速搭建工具链与架构。若你有正式的 GIM 二进制/规范文档，可以在 `GimParser` 中替换为真实解析逻辑。

## 构建

```bash
cmake -S . -B build
cmake --build build
```

> 默认会构建 `gim_toolkit`（CLI）和 `gim_studio`（OpenGL + ImGui GUI）。如果你的环境不支持 OpenGL/GLFW，可以关闭 GUI：

```bash
cmake -S . -B build -DGIM_ENABLE_STUDIO=OFF
cmake --build build
```

## 简化 GIMv1 文件格式

```txt
GIMv1
name=DemoModel
attr:author:string:alice
attr:revision:int:2
attr:scale:float:1.2
v 0 0 0
v 1 0 0
v 0 1 0
f 0 1 2
```

- `attr:<key>:<type>:<value>` 支持 `int / float / string`
- `v x y z` 定义顶点
- `f i j k` 定义三角面索引（从 0 开始）

## CLI 使用

```bash
# 查看属性
./build/gim_toolkit inspect sample/demo.gim

# 编辑属性
./build/gim_toolkit set-attr sample/demo.gim revision int 3 sample/demo_v3.gim

# 离线渲染线框
./build/gim_toolkit render sample/demo.gim sample/demo.ppm
```

## GUI 使用（OpenGL + ImGui）

```bash
./build/gim_studio sample/demo.gim
```

GUI 功能：

- 实时旋转预览模型（可开关自动旋转）
- 线框/实体模式切换
- 属性面板直接编辑 `int / float / string`
- 一键保存编辑结果为 `*.edited.gim`

## 在 VSCode 里运行

推荐扩展：

- `C/C++`（ms-vscode.cpptools）
- `CMake Tools`（ms-vscode.cmake-tools）

在项目根目录打开终端后执行：

```bash
cmake -S . -B build
cmake --build build
```

运行：

```bash
./build/gim_toolkit inspect sample/demo.gim
./build/gim_toolkit set-attr sample/demo.gim revision int 3 sample/demo_v3.gim
./build/gim_toolkit render sample/demo.gim sample/demo.ppm
./build/gim_studio sample/demo.gim
```

Windows + MinGW 示例：

```bash
.\build\gim_toolkit.exe inspect sample\demo.gim
.\build\gim_studio.exe sample\demo.gim
```


## VSCode / PowerShell 常见报错排查


### Windows（PowerShell）快速修复你当前报错

你现在的两个报错都说明：

1. `cmake` 没装好或不在 PATH；
2. 所以并没有产出 `gim_studio.exe`，后面的运行命令自然也找不到。

按下面做（一步一步复制执行）：

```powershell
# 1) 在仓库根目录运行，自动查找 cmake（PATH / Visual Studio / 常见安装目录）并构建
.\scripts\windows_build.ps1

# 2) 运行 studio（脚本会自动在 build\ 或 build\Release\ 下查找 exe）
.\scripts\run_studio.ps1 -ModelPath sample\demo.gim
```

如果你电脑限制了 PowerShell 脚本执行（ExecutionPolicy），先在**当前终端**临时放开：

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
```

如果第一步提示找不到 CMake，请安装任一方案：

- 安装 **CMake 官方安装包**，安装时勾选 `Add CMake to the system PATH`；
- 或安装 **Visual Studio 2022** 的 “Desktop development with C++” + CMake 组件。

安装后重开 VSCode 终端，再执行上面两条脚本命令。


如果你在 Windows PowerShell 看到：

`无法将“./build/gim_studio”项识别为 cmdlet...`

通常是以下原因：

1. **Windows 可执行文件需要 `.exe` 后缀**：
   - 用 `./build/gim_studio` 会失败
   - 要改成：`.\build\gim_studio.exe sample\demo.gim`
2. **`gim_studio` 目标没有成功构建**：
   - 先执行 `cmake -S . -B build`
   - 再执行 `cmake --build build --config Release`
   - 检查 `build` 目录中是否存在 `gim_studio.exe`
3. **你在多配置生成器（Visual Studio）下构建**：
   - 可执行文件可能在 `build\Release\gim_studio.exe`

建议在 VSCode 里直接用 `Terminal -> Run Task...`，按顺序执行：

- `Configure (with Studio)`
- `Build`
- `Run Studio`

仓库里已经提供了对应任务文件：`.vscode/tasks.json`。

## Qt + ImGui 说明

当前版本优先交付了 `OpenGL + ImGui` 实时编辑器，已覆盖属性面板编辑与实时渲染主流程。后续如果你确定使用 Qt 技术栈（例如 Qt6 + QOpenGLWidget + ImGui backend），可在此基础上增加 Qt 容器层并复用现有 `gim_core` 数据与解析模块。

## 后续可扩展方向

- 对接真实 GIM 规范（binary chunk、压缩块、坐标系、材质等）
- 升级为 OpenGL 3.3+/Vulkan 渲染管线（shader、VAO/VBO、PBR）
- 增加法线、UV、材质与多 mesh 支持
- 加入 undo/redo、属性变更历史、场景树
