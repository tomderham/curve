# 音频设置和设备

选择 Settings > Audio Settings 来选择音频和 MIDI 设备。

## 选择接口和通道

选择一个输出接口，如果预设需要，再选择一个输入接口。然后启用所需的通道。这些通道会以灰色引脚的形式显示在 Audio Input 和 Audio Output 节点上。

如果你使用 Curve 的 [Interface Loopback (In) 节点](loopback.md)，则不需要输入接口。

Curve 会记住每个输出接口的采样率、缓冲区大小和已启用的输出通道，并在你切换回该接口时恢复它们。

## 跟随 macOS 的输出

打开 **Use output interface selected in macOS**，即可由 macOS 选择 Curve 的输出。当你在系统设置、控制中心或菜单栏中选择一个输出时，Curve 会在大约一秒内将处理移到该输出。如果该接口断开连接，Curve 会转到 macOS 接下来使用的输出。此选项打开时，Curve 的 Output 菜单会显示为灰色。

预设中连接到新接口所没有的输出的连接会被保留，但在你切换回具有这些输出的接口之前，它们不会发声。为某个接口设计的校正（例如特定耳机的 EQ）会应用到 macOS 切换到的任何输出上。

打开此选项后，选择 **Default Presets for each output interface...** 可为每个接口选择一个预设。当 macOS 切换到某个接口时，Curve 会载入该接口的默认预设。设为 None 的接口会保留当前载入的预设。打开 **Also load at launch** 后，Curve 启动时会载入 macOS 正在使用的接口的默认预设，而不是上次使用的预设。如果找不到默认预设，Curve 会保留当前预设，并在编辑器的状态栏中说明。

## 让 macOS 保持在 Curve 的接口上

**Force macOS system output to this interface** 默认打开。它会让 macOS 的系统输出保持在 Curve 的输出接口上，使 [Interface Loopback (In) 节点](loopback.md)接收所有系统音频。如果你想自行选择系统输出，请将其关闭。Use output interface selected in macOS 打开时，它会显示为灰色，因为此时 macOS 的输出始终就是 Curve 的输出。

## 采样率和缓冲区大小

在此处选择采样率和缓冲区大小。缓冲区越小，延迟越低，但会增加 Mac 的负载，并可能导致音频中断。

**Always enforce sample rate** 默认关闭。关闭时，如果音频接口或其他 App 更改了硬件的采样率，Curve 会随之调整。打开时，Curve 会强制恢复为你选择的采样率。

## MIDI 设备

启用要使用的 MIDI 输入和输出。使用 MIDI Input 或 MIDI Output 节点将它们连接到插件。用于 [MIDI 控制](midi-control.md)的控制器需要在此处启用。

## 设备发生变化时

Curve 会为你处理常见的设备变化：

- 除非打开了 Use output interface selected in macOS，否则它会保持使用你选择的接口。如果该接口断开连接，Curve 会保持静音，而不是回退到其他输出，例如 Mac 的扬声器。
- 接口恢复后，它会重新连接并恢复音频。
- Mac 从睡眠中唤醒后，它会重新启动音频。

## AirPods 和自动切换

当 Curve 打开音频接口时，macOS 可能不会自动切换 AirPods。要允许自动切换，请选择 Settings > Preferences...，然后打开 Release audio interface when idle。

此后，Curve 会在静音 5 秒后释放接口，并在其他 App 开始使用它时重新打开。只有在 Curve 能够可靠地再次唤醒时才会这样做，因此在以下情况下不会释放接口：

- 你的输入是独立设备上的物理麦克风或线路输入。BlackHole 等虚拟设备没有问题。如果预设中连接了 Audio Input 节点，输出接口上的输入也是如此。
- 其他 App 正在使用该设备。
- Curve 正忙，例如正在录音。
