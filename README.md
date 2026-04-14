# BIM GIM Viewer（Grid Information Model）

这个版本按你的目标升级为“类似 BIMBase / Bentley / 国遥 / 构力”的工作流方向：

- ✅ 读取 GIM 模型属性（项目、网格、楼层、模型自定义属性）
- ✅ 渲染网格模型（按 category 上色）
- ✅ 交互查看属性（点击单元格查看明细）
- ✅ 视图交互（滚轮缩放、中键平移）
- ✅ 楼层切换（键盘 `← / →`）

> 这是一个可二次开发的 Win32/C++ 基础版，不是商业软件完整替代品，但架构已对齐“属性 + 渲染 + 交互”的核心能力。

## 1) 支持的 GIM JSON 结构

```json
{
  "format": "GIM-GridInformationModel",
  "version": "1.1",
  "project": "Hospital-A",
  "author": "Team BIM",
  "unit": "mm",
  "properties": {
    "phase": "CD",
    "discipline": "Architecture"
  },
  "grid": {
    "rows": 20,
    "cols": 30,
    "cellSizeMm": 600,
    "originX": 0,
    "originY": 0,
    "rotationDeg": 0
  },
  "levels": [
    { "name": "B1", "elevationMm": -4200 },
    { "name": "L1", "elevationMm": 0 },
    { "name": "L2", "elevationMm": 4200 }
  ],
  "cells": [
    {
      "row": 5,
      "col": 8,
      "level": "L1",
      "category": "Wall",
      "usage": "Partition",
      "elevationMm": 0,
      "properties": {
        "fireRating": "2h",
        "material": "AAC"
      }
    }
  ]
}
```

## 2) 运行（Visual Studio 2022）

### 方式 A（推荐）
1. VS2022 -> `File -> Open -> Folder...`
2. 打开本项目目录
3. 直接运行 `GimViewer`

### 方式 B（命令行）
```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

## 3) 使用说明

- `File -> Open...` 打开 `.gim.json`
- 鼠标滚轮：缩放
- 鼠标中键拖动：平移
- 鼠标左键：选中单元格并在右侧属性面板查看细节
- 键盘 `←/→`：在 `ALL + levels[]` 间切换显示楼层

## 4) 与商业 BIM 查看器的差距（下一步可做）

- IFC/OBJ/GLTF 真实几何渲染（当前是网格抽象渲染）
- 构件树（按专业/系统/楼层分组）
- 属性检索和条件过滤器
- 剖切、测量、标注、批注
- 多模型叠加和坐标配准
