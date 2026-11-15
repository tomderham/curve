# 键盘快捷键

## 从任何位置打开 Curve 菜单

即使其他 App 在前台，全局快捷键也能打开 Curve 菜单。在你设置之前，它处于关闭状态。

1. 选择 Settings > Preferences...，打开 Keyboard 标签页，然后点按 Global Menu Shortcut 旁边的 Change...。
2. 点按 Record Shortcut，然后按下包含 Command、Option 或 Control 的组合键。按 Esc 可取消。
3. 点按 Save。

点按 Reset to Default 可使用 ⌥⇧C，点按 Disable 可关闭快捷键。

## 用音量键调节 Curve 的音量

有些音频接口没有 macOS 可以使用的音量控制，因此键盘的音量键不起作用。Curve 可以改用这些键来调节自己的输出音量。

1. 选择 Settings > Preferences...，打开 Keyboard 标签页，然后将 Volume Keys 设为 When macOS Can't Control the Interface's Volume。
2. 当 macOS 询问时，在“系统设置”>“隐私与安全性”>“辅助功能”中允许 Curve 访问。Curve 需要此权限才能接收按键。

之后 Curve 会像 macOS 一样，在菜单栏的“声音”图标下方显示自己的音量指示。

- 音量键与 macOS 一样分 16 级调节。按住 Shift 和 Option 再按音量键可以更精细地调节。
- 静音键可关闭和打开 Curve 的输出。

只有当 macOS 的输出是 Curve 的输出接口，并且 macOS 无法设置 Curve 所用声道的音量时，Curve 才会接管按键。这包括完全没有音量控制的接口，以及在 macOS 中的音量控制只作用于部分声道、而不作用于 Curve 所用声道的接口。否则，按键在 macOS 中照常工作。要让按键始终调节 Curve 的音量，请改为选择 Always。

默认情况下，Curve 的音量作用于 Curve 输出的所有声音，位于你的插件之后。若只想调节来自 macOS 和其他 App 的音频音量，请在 Volume Keys 下方的菜单中选择 Control Interface Loopback Volume。这样按键只会调低 [Interface Loopback (In)](loopback.md) 节点的音量，Audio Input 和 Curve 中乐器插件的声音保持原有电平。如果预设中没有已连接的 Interface Loopback (In) 节点，按键在 macOS 中照常工作。选择 Control Curve's Output Volume 即可恢复。

Curve 的音量仅在按键调节它时生效。退出 Curve 后它仍会记住音量。音量最大时，音频保持原样。

要隐藏音量指示，请在同一菜单中关闭 Show Volume Overlay。

## 用键盘切换预设

当 Curve 菜单打开时，按下某个键即可选择预设。按键显示在每个预设旁边。

| 预设 | 按键 |
|---|---|
| 1 到 10 | 1 到 0 |
| 11 到 20 | Option 加 1 到 0 |
| 21 到 30 | Shift 加 1 到 0 |

例如，按下全局快捷键后再按 2，即可从任何 App 切换到第二个预设。

## MIDI Learn 期间在编辑器中

| 按键 | 操作 |
|---|---|
| Esc | 结束 MIDI Learn |
| Delete | 移除所选控件的映射 |
