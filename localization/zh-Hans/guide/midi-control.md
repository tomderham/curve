# MIDI 控制

MIDI 控制器或键盘可以演奏乐器插件、更改插件参数、旁通或静音节点，以及切换预设。请在 [Audio Settings](audio-settings.md) 的 MIDI 输入下启用它。

## 选择控制插件的方式

用控制器控制插件有两种方式：

- **将 MIDI 连接到插件。** MIDI Input 节点将 MIDI 直接发送到插件的琥珀色输入引脚，由插件决定如何处理。用这种方式演奏乐器，或使用插件内置的 MIDI 映射。
- **使用 Curve 的 MIDI Learn。** Curve 自己读取控制器，然后替你更改插件的参数、旁通或静音。这不需要 MIDI Input 节点，也不需要任何连接。这种方式适用于没有琥珀色输入引脚的插件，以及旁通和静音。

MIDI Learn 只能控制插件向宿主开放的参数。有些插件，尤其是大型乐器，只开放部分控件。如果在 MIDI Learn 期间移动插件窗口中的控件没有反应，说明 Curve 无法映射该控件。

如果插件有琥珀色输入引脚和自带的 MIDI Learn 功能，那通常是更简单的选择，因为它一般能控制所有控件。将 MIDI Input 节点连接到插件，然后在插件窗口中设置映射。具体方法请参阅插件的手册。

每个旋钮或按钮只使用一种方式。用 Curve 的 MIDI Learn 映射的控件不会通过 MIDI Input 节点到达插件，因此插件不会对其作出响应。

## 演奏乐器插件

1. 右键点按编辑器背景并添加你的乐器插件。
2. 右键点按编辑器背景并选择 MIDI I/O > MIDI Input。
3. 从 MIDI Input 节点的琥珀色引脚拖到乐器顶部的琥珀色引脚。MIDI 引脚和连接为琥珀色，音频引脚和连接为灰色。
4. 从乐器的灰色输出引脚拖到 Audio Output 节点。
5. 选择 Save As Preset... 以保留此设置。

响应 MIDI 的效果器插件也以相同方式连接。要将 MIDI 从插件发送到外部设备，请在 Audio Settings 中将该设备启用为 MIDI 输出，然后将插件的琥珀色输出引脚连接到 MIDI Output 节点。

## 使用 MIDI Learn 映射控件

1. 右键点按任意节点并选择 MIDI Learn。编辑器会显示高亮边框，状态栏会给出指引。
2. 选择要控制的对象：
   - 对于插件参数，打开插件窗口并移动控件。
   - 对于节点旁通，点按其电源按钮。
   - 要将节点静音，请右键点按它并选择 Mute。
3. 移动控制器上的旋钮、推子或按钮。映射会立即完成，你可以继续映射其他控件。
4. 按 Esc 或再次选择 MIDI Learn 以结束。

映射随预设一起存储，因此请使用 Save As Preset... 来保留它们。

## 移除映射

- 要移除所选参数、旁通或静音的映射，请在 MIDI Learn 期间按 Delete。
- 要移除节点的所有映射，请右键点按节点并选择 Clear MIDI Mappings。
- 要将控制改到其他目标，请移除它原来的映射，因为再次映射会添加目标。

## 已映射控件的行为

- 旋钮和推子（CC）覆盖参数的完整范围。可以在 MIDI Mappings for Current Preset... 中缩小范围（见下文）。
- 音符和程序更改用于切换目标。
- 对于旁通、静音以及 Record 等开/关参数，Curve 会判断 CC 按钮是瞬时式（每按一次切换）还是锁定式（跟随按钮状态）。
- 一个控制可以控制多个目标：用 MIDI Learn 再次映射即可添加目标。有多个目标的控制不会更改静音节点的参数，因此在[场景](presets.md)中，一个旋钮可以控制每个场景所用的乐器。

具有映射的节点会显示一个小的 MIDI 插座图标。将指针悬停在节点上，即可在状态栏中查看其映射。

已映射的 MIDI 消息不会通过 MIDI Input 节点传递给插件。其他所有 MIDI 都会传递。

## 调整映射

点按编辑器窗口左上角的菜单图标，然后选择 MIDI Mappings for Current Preset... 可查看当前载入预设的映射。使用 MIDI Learn 或切换预设时，窗口可以保持打开。每个映射可以设置：

- **Mode：** Range 表示由旋钮或推子设置数值；Toggle 表示每按一次在两个值之间切换；Follow 表示目标跟随按钮，按住或锁定时为开。
- **From 和 To：** 旋钮或推子移动的数值范围。范围越窄，步进越精细，例如将 Gain 节点设为 -20 dB 到 +6 dB。对于 Toggle 和 Follow，它们是关和开的值。输入参数单位的数值，或 25% 之类的百分比。
- **Invert：** 反转控制方向。

旁通和静音映射没有 From 和 To。点按 × 可移除映射。映射随预设一起存储，因此请使用 Save As Preset... 保留更改。

## 从控制器切换预设

选择 Settings > Preferences...，然后打开 MIDI 标签页。这些设置适用于所有预设：

- **Program Change Selects Preset：** 默认关闭。程序更改 0 载入菜单中的第一个预设，1 载入第二个，依此类推，顺序与[数字键快捷键](keyboard-shortcuts.md)相同。
- **Next Preset Control** 和 **Previous Preset Control：** 点按其中一项旁边的 MIDI Learn...，然后按下控制器上要使用的按钮。
- **Clear Preset Controls：** 移除这两个按钮分配。

如果载入的预设自己映射了同一个控制，则使用预设的映射。打开 Save As Preset... 或 Audio Settings 等窗口时，MIDI 不会切换预设。

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
