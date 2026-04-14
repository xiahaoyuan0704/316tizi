# BIM GIM Viewer（3D Grid Information Model）

你现在看到的是 **3D 立体版本**：

- 解析 GIM(Grid Information Model) JSON
- 以 3D 体块方式渲染 cell（带高度、颜色、层高）
- 点击体块查看属性
- 支持缩放/平移/旋转视角

> 注意：这不是完整 IFC/Bentley 几何内核，而是面向 GIM 网格模型的 3D 可视化基础框架。

## 支持的关键字段

- 顶层：`format/version/project/author/unit/properties/grid/levels/cells`
- `grid`：`rows/cols/cellSizeMm/originX/originY/rotationDeg`
- `cells[]`：
  - `row/col/level/category/usage`
  - `elevationMm`（底标高）
  - `heightMm`（体块高度，可选，默认 3000）
  - `properties`（任意扩展属性）

## 运行方式（Visual Studio 2022）

1. `File -> Open -> Folder...` 打开项目
2. 等待 CMake 配置
3. 运行 `GimViewer`
4. `File -> Open...` 选择 `examples/sample.gim.json`

## 3D 交互说明

- 鼠标滚轮：缩放
- 中键拖拽：平移
- 左键点击：选中体块并查看属性
- `A / D`：左右旋转视角（Yaw）
- `W / S`：抬高/降低俯仰角（Pitch）
- `← / →`：切换楼层过滤（ALL + levels）

## 样例

示例文件：`examples/sample.gim.json`
