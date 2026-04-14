# GIM Grid Information Model Viewer（C++ / Visual Studio 2022）

你刚才澄清的 GIM 是 **BIM 领域的 Grid Information Model**，不是 PSP 贴图文件。
本项目已改成：

- 解析 GIM(Grid Information Model) 的 JSON 数据
- 显示项目/网格属性（rows、cols、cellSize、origin、rotation）
- 把 cells 渲染为 2D 网格视图（按 category 着色）

## 1. 支持的输入格式（`.gim.json`）

```json
{
  "format": "GIM-GridInformationModel",
  "version": "1.0",
  "project": "Hospital-A",
  "author": "Team BIM",
  "unit": "mm",
  "grid": {
    "rows": 20,
    "cols": 30,
    "cellSizeMm": 600,
    "originX": 0,
    "originY": 0,
    "rotationDeg": 0
  },
  "cells": [
    { "row": 1, "col": 1, "category": "Core", "usage": "Shaft", "elevationMm": 0 },
    { "row": 1, "col": 2, "category": "Wall", "usage": "Partition", "elevationMm": 0 },
    { "row": 2, "col": 5, "category": "Door", "usage": "MainDoor", "elevationMm": 0 }
  ]
}
```

## 2. 在 Visual Studio 2022 运行

### 方法 A（推荐）：直接打开文件夹
1. 打开 Visual Studio 2022
2. `File -> Open -> Folder...` 选择本项目目录
3. 等待 CMake 配置完成
4. 运行 `GimViewer`

### 方法 B：命令行生成 VS 工程
```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

## 3. 使用方式
1. 启动程序
2. 菜单栏 `File -> Open...`
3. 选择 `.gim.json` 文件
4. 顶部查看属性信息，下面查看网格着色渲染

## 4. category 着色规则（可扩展）

- Core: 蓝色
- Wall: 深灰
- Door: 橙色
- Window: 浅蓝
- Column: 紫色
- MEP: 洋红
- Empty: 浅灰
- 未知 category: 绿色

## 5. 后续建议

- 增加图例面板（Legend）
- 支持缩放/平移/框选
- 支持多楼层（levels）切换
- 支持导出 SVG / PNG
- 对接 IFC 或 Revit 导出的网格数据
