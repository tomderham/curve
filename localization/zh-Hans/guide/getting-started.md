# Curve 入门指南

Curve 需要 macOS Sonoma 14.2 或更高版本，支持 Apple 芯片或 Intel 处理器。

## 安装 Curve

1. 从[最新发行版本](https://github.com/tomderham/curve/releases)下载 Curve。
2. 将 `Curve.app` 拷贝到“应用程序”文件夹并打开。

Curve 运行在菜单栏中，因此默认情况下其图标显示在菜单栏而不是程序坞中。编辑器窗口会在首次使用时打开。此后 Curve 启动时不会打开任何窗口。

要打开 Curve 菜单，请点按菜单栏图标，或点按编辑器窗口左上角的菜单图标。

## 扫描插件

如果你只想使用 macOS 的内置插件（例如 AUNBandEQ）或 [Curve 自带的插件](curve-plugins.md)，可以跳过此步骤。

1. 选择 Settings > Plug-in Manager。
2. 点按左下角的 Options...，然后选择 'Scan for new or updated AudioUnit plug-ins'。这可能需要一些时间，macOS 可能会要求你允许 Curve 访问插件使用的文件夹。
3. 如果你使用 VST3 插件，请对 'Scan for new or updated VST3 plug-ins' 重复此操作。
4. 关闭窗口。

## 选择音频设备

1. 选择 Settings > Audio Settings。
2. 选择输出接口；除非你使用[环回](loopback.md)，否则还要选择输入接口。
3. 选择所需的通道，以及采样率和缓冲区大小。
4. 如果你使用 MIDI，请选择你的 MIDI 接口。
5. 关闭窗口。

## 创建你的第一个预设

预设是编辑器中一条由已连接节点组成的链。如果编辑器没有打开，请从 Curve 菜单中选择 Show Editor。

1. 你会看到 Audio Input、Interface Loopback (In) 和 Audio Output 节点，已启用的通道显示为绿色引脚。
2. 右键点按编辑器背景并选择一个插件。其节点随即出现，输入在顶部，输出在底部。
3. 从输入节点的绿色引脚拖到插件的输入引脚。
4. 从插件的输出引脚拖到 Audio Output 节点。
5. 连按插件节点，打开其窗口并进行调整。
6. 从 Curve 菜单中选择 Save As Preset...，然后为预设命名。

如果预设不使用 Interface Loopback (In) 节点，请将其移除，或至少不要连接其引脚。否则它可能会使你的音频静音。

使用特殊路由时请务必小心。Curve 对反馈回路有一定的保护，但错误的配置仍可能损坏设备或你的听力。

## 完成设置

- 要在登录时启动 Curve，请选择 Settings > Open at Login。
- 自动检查更新默认打开。你可以通过 Settings > Auto-Check for App Updates 更改此设置。

## 后续步骤

- [预设](presets.md)
- [耳机 EQ 和 AutoEQ](eq-and-autoeq.md)
- [环回和系统音频](loopback.md)
