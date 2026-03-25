# GIM Toolkit (C++)

这是一个用于 `.gim`（Grid Information Model）实验性处理的 C++ 原型，包含三个核心能力：

1. **属性解析与查看**：读取 GIM 文件并输出属性、顶点、面片统计。
2. **属性编辑**：命令行修改属性并写回新文件。
3. **模型渲染**：将网格线框渲染为 `.ppm` 图片。

> 说明：目前实现的是一个 `GIMv1` 的简化文本格式，便于你快速搭建工具链与架构。若你有正式的 GIM 二进制/规范文档，可以在 `GimParser` 中替换为真实解析逻辑。

## 构建

```bash
cmake -S . -B build
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

## 使用

```bash
# 查看属性
./build/gim_toolkit inspect sample/demo.gim

# 编辑属性
./build/gim_toolkit set-attr sample/demo.gim revision int 3 sample/demo_v3.gim

# 渲染线框
./build/gim_toolkit render sample/demo.gim sample/demo.ppm
```


## 在 VSCode 里运行

可以。推荐安装扩展：

- `C/C++`（ms-vscode.cpptools）
- `CMake Tools`（ms-vscode.cmake-tools）

在项目根目录打开终端后执行：

```bash
cmake -S . -B build
cmake --build build
```

然后可直接运行：

```bash
./build/gim_toolkit inspect sample/demo.gim
./build/gim_toolkit set-attr sample/demo.gim revision int 3 sample/demo_v3.gim
./build/gim_toolkit render sample/demo.gim sample/demo.ppm
```

如果你使用 Windows + MinGW，可把运行命令改为：

```bash
.\build\gim_toolkit.exe inspect sample\demo.gim
```

## 后续可扩展方向

- 对接真实 GIM 规范（binary chunk、压缩块、坐标系、材质等）
- 替换为 OpenGL/Vulkan 实时渲染
- 加入 GUI（Qt + ImGui）进行属性面板编辑
- 增加法线、UV、材质与多 mesh 支持
