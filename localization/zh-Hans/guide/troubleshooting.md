# 故障排除

## 没有声音，或部分通道被静音

- 检查节点是否一路连接到 Audio Output。
- 如果预设中有 Interface Loopback (In) 节点但你没有使用它，请将其移除或不要连接其引脚。请参阅[环回](loopback.md)。
- 如果你拔掉了所选接口，Curve 会保持静音直到接口恢复，而不是使用其他输出。要使用其他接口，请在 Audio Settings 中选择它。
- 检查是否没有节点被旁通或静音，以及 [Audio Settings](audio-settings.md) 中是否启用了正确的通道。

## Curve Help 提示内容不可用

只有当 Curve 位于“应用程序”文件夹中时，Curve Help 才能打开。请将其移到该文件夹后重试。

## 打开 Curve 后找不到它

Curve 运行在菜单栏中，通常不打开任何窗口。点按其菜单栏图标，然后选择 Show Editor。要同时在程序坞中显示，请选择 Settings > Preferences...，然后打开 Show in Dock。

## 我对预设的更改不见了

Curve 不会自动存储更改。请使用 Save As Preset... 并替换现有预设。请参阅[预设](presets.md)。

## 菜单中缺少某个插件

选择 Settings > Plug-in Manager 并对该插件的格式运行扫描。请参阅[入门指南](getting-started.md)。

如果它以红色显示在 Plug-in Manager 列表底部，且类别为 Scan failed，说明该插件崩溃了或加载时间超过 30 秒，之后的扫描会跳过它。右键点按它并选择 Retry Scan，或选择 Remove from List 将其清除。

## 插件的引脚数量不对

右键点按其节点并选择 Configure Audio I/O。

## 采样率不断变化

可能是其他 App 正在更改硬件的采样率。要让 Curve 强制恢复，请在 [Audio Settings](audio-settings.md) 中打开 Always enforce sample rate。

## AirPods 无法自动切换

选择 Settings > Preferences...，然后打开 Release audio interface when idle。请参阅[音频设置和设备](audio-settings.md)。

## 音量键无法改变 Curve 的音量

- 检查 Settings > Preferences... 的 Keyboard 标签页中 Volume Keys 是否未设为 Off。
- 如果 Keyboard 标签页显示 Allow Accessibility Access...，请点按它，并在“辅助功能”中打开 Curve。如果 Curve 已经打开，请先关闭再重新打开。
- 使用 When macOS Can't Control the Interface's Volume 时，如果 macOS 能设置 Curve 所用声道的音量，或者 macOS 的输出不是 Curve 的输出接口，按键会由 macOS 处理。请在 Audio Settings 中打开 Force macOS system output to this interface，或选择 Always。请参阅[键盘快捷键](keyboard-shortcuts.md)。
- 使用 Control Interface Loopback Volume 时，只有当预设中有已连接的 Interface Loopback (In) 节点时，Curve 才会处理按键。请参阅[环回](loopback.md)。

## Curve 对我的 MIDI 控制器没有响应

检查是否已在 Audio Settings 的 MIDI 输入下启用控制器，以及映射是否已随预设一起存储。请参阅 [MIDI 控制](midi-control.md)。
