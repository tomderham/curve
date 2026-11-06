# 编辑器

编辑器以节点图的形式显示你的预设。音频从每个节点底部的输出流向下一个节点顶部的输入。要打开编辑器，请从 Curve 菜单中选择 Show Editor。要关闭编辑器，请点按其关闭按钮或选择 Hide Editor。无论哪种方式，音频都会继续运行。

编辑器底部的状态栏会显示提示，并描述指针下方的内容。对于插件，还会显示制造商和格式，例如 AudioUnit 或 VST3。

## 添加节点

右键点按编辑器背景，然后从以下项目中选择：

- **Audio I/O：** 音频输入、音频输出和 Interface Loopback (In) 节点。
- **MIDI I/O：** MIDI 输入和输出节点。
- **Curve Plugins：** [Curve 的内置插件](curve-plugins.md)。
- **你扫描到的插件：** 列在这些菜单下方，按制造商排列。

你也可以将插件文件或 [EQ 文件](eq-and-autoeq.md)拖到编辑器窗口上。

## 连接节点

从一个节点的引脚拖到另一个节点的引脚。音频引脚为灰色，MIDI 引脚为琥珀色。你可以将一个输出连接到多个输入，并搭建任何你想要的链。

同一对节点之间的连续通道（例如立体声对）会绘制为一条较粗的线，并分叉到各个引脚。通道仍然彼此独立，不会被混音。拖动这条线可以一次重新连接一个通道。

## 排列节点

拖动节点即可移动。节点会吸附到网格，因此对齐的节点之间连线是直的。拖动时按住 Option 键可自由放置节点。之后不按 Option 再次拖动，节点会回到网格上。

要按信号流排列整个图，请右键点击背景并选择 Tidy Up Layout。如果图需要更多空间，窗口会随之扩大，最大到屏幕尺寸。之后，在你再次移动节点之前，Undo Tidy Up 都可以恢复布局和窗口大小。

## 旁通节点

点按节点上的电源按钮。再次点按可重新启用节点。你也可以右键点按节点并选择 Toggle Bypass。

## 打开插件窗口

连按节点。

要在每次载入预设时打开窗口（即使编辑器已关闭），请右键点按节点，选择 Always Open Plug-in GUI with Preset，然后使用 Save As Preset...。窗口会在存储时的位置打开。

## 节点菜单

右键点按节点可显示以下选项：

| 项目 | 功能 |
|---|---|
| Delete This Node | 移除节点。 |
| Disconnect All Pins | 移除节点的所有连接。 |
| Toggle Bypass | 打开或关闭旁通。 |
| Rename This Node | 更改节点上显示的名称。 |
| MIDI Learn | 启动 [MIDI Learn](midi-control.md)。 |
| Clear MIDI Mappings | 移除节点的 MIDI 映射。仅在节点有映射时显示。 |
| Always Open Plug-in GUI with Preset | 每次载入预设时打开插件的窗口。 |
| Import EQ from AutoEQ... | 显示在 Parametric EQ 节点上。请参阅[耳机 EQ 和 AutoEQ](eq-and-autoeq.md)。 |
| Import EQ from file... | 显示在 Parametric EQ 节点上。请参阅[耳机 EQ 和 AutoEQ](eq-and-autoeq.md)。 |
| Configure Audio I/O | 更改插件的通道布局，并关闭或隐藏其引脚。见下文。 |
| Audio Settings... | 显示在音频输入和输出节点上。打开 Audio Settings。 |
| Save Plug-in State | 将插件的设置存储到文件。 |
| Load Plug-in State | 从文件载入插件的设置。 |

## 配置插件的输入和输出

右键点按插件的节点并选择 Configure Audio I/O。插件的引脚分组为总线，例如主立体声输入和侧链输入。按编号选择一条总线，然后使用：

- **Channel Layout:** 设置总线的通道数。超过两个通道的布局按通道数分组。环绕声或 Ambisonic 插件可能会以不同方式处理通道数相同的两种布局，因此请选择与你的音频相符的布局。
- **Enabled:** 关闭总线，这会移除其引脚。如果插件离不开该总线，此项会显示为灰色。
- **Shown:** 在图中隐藏总线的引脚，但总线仍在运行。可用它收起不使用且无法关闭的总线。隐藏总线会移除其连接，隐藏的输入接收的是静音。总线关闭时，Shown 显示为灰色。

如果插件有 MIDI 引脚，MIDI Port 有自己的 Shown 设置。只有 MIDI 的一侧（例如乐器的输入）只有这一项设置。

更改布局或打开、关闭总线会断开节点的所有引脚。这些设置随预设一起存储。指向带有隐藏引脚的节点时，状态栏会提示这一点。
