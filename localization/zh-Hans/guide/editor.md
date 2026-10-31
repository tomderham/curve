# 编辑器

编辑器以节点图的形式显示你的预设。音频从每个节点底部的输出流向下一个节点顶部的输入。要打开编辑器，请从 Curve 菜单中选择 Show Editor。要关闭编辑器，请点按其关闭按钮或选择 Hide Editor。无论哪种方式，音频都会继续运行。

编辑器底部的状态栏会显示提示，并描述指针下方的内容。

## 添加节点

右键点按编辑器背景，然后从以下项目中选择：

- **Audio I/O：** 音频输入、音频输出和 Interface Loopback (In) 节点。
- **MIDI I/O：** MIDI 输入和输出节点。
- **Curve Plugins：** [Curve 的内置插件](curve-plugins.md)。
- **你扫描到的插件：** 列在这些菜单下方，按制造商排列。

你也可以将插件文件或 [EQ 文件](eq-and-autoeq.md)拖到编辑器窗口上。

## 连接节点

从一个节点的引脚拖到另一个节点的引脚。音频引脚为绿色，MIDI 引脚为红色。你可以将一个输出连接到多个输入，并搭建任何你想要的链。

## 旁通节点

点按节点上的电源按钮。再次点按可重新启用节点。你也可以右键点按节点并选择 Toggle Bypass。

## 打开插件窗口

连按节点，或右键点按节点并选择 Show Plug-in GUI。

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
| Show Plug-in GUI | 打开插件的窗口。 |
| Import EQ from AutoEQ... | 显示在 Parametric EQ 节点上。请参阅[耳机 EQ 和 AutoEQ](eq-and-autoeq.md)。 |
| Import EQ from file... | 显示在 Parametric EQ 节点上。请参阅[耳机 EQ 和 AutoEQ](eq-and-autoeq.md)。 |
| Configure Audio I/O | 更改插件的输入和输出引脚数量。 |
| Audio Settings... | 显示在音频输入和输出节点上。打开 Audio Settings。 |
| Save Plug-in State | 将插件的设置存储到文件。 |
| Load Plug-in State | 从文件载入插件的设置。 |

## 更改插件的通道数

如果插件的引脚数量不符合需要，请右键点按其节点并选择 Configure Audio I/O。
