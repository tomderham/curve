# MIDI 控制

MIDI 控制器或键盘可以演奏乐器插件、更改插件参数、旁通节点和切换预设。请在 [Audio Settings](audio-settings.md) 的 MIDI 输入下启用它。

要演奏乐器或向其他任何插件发送 MIDI，需要有一个 MIDI Input 节点连接到该插件。使用 MIDI Learn 控制参数、旁通和预设时，不需要 MIDI Input 节点，也不需要在预设中进行任何连接。

## 演奏乐器插件

1. 右键点按编辑器背景并添加你的乐器插件。
2. 右键点按编辑器背景并选择 MIDI I/O > MIDI Input。
3. 从 MIDI Input 节点的红色引脚拖到乐器顶部的红色引脚。MIDI 引脚和连接为红色，音频引脚和连接为绿色。
4. 从乐器的绿色输出引脚拖到 Audio Output 节点。
5. 选择 Save As Preset... 以保留此设置。

响应 MIDI 的效果器插件也以相同方式连接。要将 MIDI 从插件发送到外部设备，请在 Audio Settings 中将该设备启用为 MIDI 输出，然后将插件的红色输出引脚连接到 MIDI Output 节点。

## 使用 MIDI Learn 映射控件

1. 右键点按任意节点并选择 MIDI Learn。编辑器会显示高亮边框，状态栏会给出指引。
2. 选择要控制的对象：
   - 对于插件参数，打开插件窗口并移动控件。
   - 对于节点旁通，点按其电源按钮。
3. 移动控制器上的旋钮、推子或按钮。映射会立即完成，你可以继续映射其他控件。
4. 按 Esc 或再次选择 MIDI Learn 以结束。

映射随预设一起存储，因此请使用 Save As Preset... 来保留它们。

## 移除映射

- 要移除所选参数或旁通的映射，请在 MIDI Learn 期间按 Delete。
- 要移除节点的所有映射，请右键点按节点并选择 Clear MIDI Mappings。

## 已映射控件的行为

- 旋钮和推子（CC）覆盖参数的完整范围。
- 音符和程序更改用于切换目标。
- 对于旁通，Curve 会判断 CC 按钮是瞬时式（每按一次切换旁通）还是锁定式（旁通跟随按钮状态）。

具有映射的节点会显示一个小的 MIDI 插座图标。将指针悬停在节点上，即可在状态栏中查看其映射。

已映射的 MIDI 消息不会通过 MIDI Input 节点传递给插件。其他所有 MIDI 都会传递。

## 从控制器切换预设

打开 Settings > MIDI Control。这些设置适用于所有预设：

- **Program Change Selects Preset：** 默认关闭。程序更改 0 载入菜单中的第一个预设，1 载入第二个，依此类推，顺序与[数字键快捷键](keyboard-shortcuts.md)相同。
- **Next Preset Control** 和 **Previous Preset Control：** 选择其中一项，然后按下控制器上要使用的按钮。
- **Clear Preset Controls：** 移除这两个按钮分配。

## Curve 自带插件的控件

| 插件 | 控件 |
|---|---|
| Gain | 增益和静音 |
| Pan | 声像 |
| Clipper | 上限 |
| Stereo Utility | 模式和宽度 |
| Headphone Crossfeed | 强度 |
| Audio Recorder | 开始和停止录音 |

Headphone Speaker Emulation、Invert Phase、Mid/Side Encoder 和 Mid/Side Decoder 没有设置，因此请改为映射它们的旁通。Delay 无法映射。
