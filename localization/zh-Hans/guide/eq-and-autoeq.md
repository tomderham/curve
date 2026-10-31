# 耳机 EQ 和 AutoEQ

Curve 可以根据测量得到的耳机配置文件设置 Parametric EQ，让你在 Mac 的所有音频上校正耳机的频率响应。

## 使用 AutoEQ 耳机配置文件

此操作需要互联网连接。

1. 右键点按编辑器背景，然后选择 Curve Plugins > Parametric EQ (import from AutoEQ...)。
2. 在搜索框中输入耳机型号，例如 HD 650 或 AirPods。
3. 选择一个结果，查看其响应曲线和滤波器。
4. 点按 Use this EQ。Curve 会使用这些设置创建一个 Parametric EQ 节点。
5. 将[节点连接](editor.md)到你的链中。
6. 选择 Save As Preset... 以保留它。

要更新现有的 Parametric EQ 节点，请右键点按它并选择 Import EQ from AutoEQ...

如果缺少某个耳机型号，请点按 Sync Index，检查 AutoEQ 数据库最近新增的内容。

## 微调或查找其他配置文件

- 要调整目标曲线和声音特征，请使用 [autoeq.app](https://autoeq.app) 并导出 EQ 曲线。
- 如果你的耳机未列出，也可以试试 [squig.link](https://squig.link)，它提供由社区贡献的耳机和入耳式监听耳机测量数据。

## 导入 EQ 文件

Curve 可以从 `.txt`、`.csv`、`.tsv` 或 `.eq` 文件导入来自 AutoEQ、Squig.link 和 REW 的 EQ 曲线。

- 要创建新节点，请右键点按编辑器背景并选择 Curve Plugins > Parametric EQ (import from file...)，或将文件拖到编辑器窗口上。
- 要更新现有的 Parametric EQ 节点，请右键点按它并选择 Import EQ from file...

## 手动调整 EQ

连按 Parametric EQ 节点以打开其窗口。
