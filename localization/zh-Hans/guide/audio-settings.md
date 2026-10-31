# 音频设置和设备

选择 Settings > Audio Settings 来选择音频和 MIDI 设备。

## 选择接口和通道

选择一个输出接口，如果预设需要，再选择一个输入接口。然后启用所需的通道。这些通道会以绿色引脚的形式显示在 Audio Input 和 Audio Output 节点上。

如果你使用 Curve 的 [Interface Loopback (In) 节点](loopback.md)，则不需要输入接口。

## 采样率和缓冲区大小

在此处选择采样率和缓冲区大小。缓冲区越小，延迟越低，但会增加 Mac 的负载，并可能导致音频中断。

**Always enforce sample rate** 默认关闭。关闭时，如果音频接口或其他 App 更改了硬件的采样率，Curve 会随之调整。打开时，Curve 会强制恢复为你选择的采样率。

## MIDI 设备

启用要使用的 MIDI 输入和输出。使用 MIDI Input 或 MIDI Output 节点将它们连接到插件。用于 [MIDI 控制](midi-control.md)的控制器需要在此处启用。

## 设备发生变化时

Curve 会为你处理常见的设备变化：

- 它会保持使用你选择的接口。如果该接口断开连接，Curve 会保持静音，而不是回退到其他输出，例如 Mac 的扬声器。
- 接口恢复后，它会重新连接并恢复音频。
- Mac 从睡眠中唤醒后，它会重新启动音频。

## AirPods 和自动切换

当 Curve 打开音频接口时，macOS 可能不会自动切换 AirPods。要允许自动切换，请打开 Settings > Release audio interface when idle。

此后，Curve 会在静音 5 秒后释放接口，并在其他 App 开始使用它时重新打开。只有在 Curve 能够可靠地再次唤醒时才会这样做，因此在以下情况下不会释放接口：

- 你的输入是独立设备上的物理麦克风或线路输入。BlackHole 等虚拟设备没有问题。如果预设中连接了 Audio Input 节点，输出接口上的输入也是如此。
- 其他 App 正在使用该设备。
- Curve 正忙，例如正在录音。
