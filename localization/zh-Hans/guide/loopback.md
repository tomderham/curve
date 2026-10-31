# 环回和系统音频

环回可让你在 macOS 的音频到达耳机或扬声器之前，先将其发送到 Curve。这样就可以对 Mac 播放的所有内容应用 EQ。

## 使用 Curve 的 Interface Loopback 节点

无需额外的软件或驱动程序。

1. 在 [Audio Settings](audio-settings.md) 中，选择你要用来收听音频的输出接口。
2. 在预设中，使用 Interface Loopback (In) 节点代替 Audio Input 作为输入。
3. 将它通过你的插件连接到 Audio Output 节点。

该节点会比特级精确地接收原本会直接进入输出接口的音频。如果节点不存在，请从编辑器右键菜单的 Audio I/O 中添加。

## 将所有系统音频发送到 Curve

Settings 菜单中的 **Force macOS System Audio to loopback** 默认打开。它会让 macOS 的系统输出保持在 Curve 中选择的接口上。如果你想自行选择系统输出，请将其关闭。

## 使用音频接口自带的环回

另外，如果你的接口有硬件环回，这也是一个不错的选择，因为它可以将延迟降到最低。例如，使用 RME TotalMix 时：

1. 使用 TotalMix 的 Loopback 功能，将一对输出通道发送到一对空闲的输入通道。
2. 在 Curve 中选择这些输入通道，并使用 Audio Input 节点。
3. 选择部分软件播放通道作为 Curve 的输出。
4. 在 TotalMix 中，将这些播放通道分配给硬件输出。

此设置下请勿使用 Interface Loopback (In) 节点。

## 重要：未使用的环回引脚

如果 Interface Loopback (In) 节点已连接，这些通道的直接音频将不再到达输出设备。如果它位于你不使用它的预设中，请将其移除或不要连接其引脚，否则可能会出现意外静音。

## 重定向的工作原理

当节点的引脚已连接时，所选输出接口的主数据流 Stream 0 的通道会被重定向到该节点。接口驱动程序分配给其他数据流的通道不受影响。
