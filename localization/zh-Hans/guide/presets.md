# 预设

预设会存储整个图：其中的插件及其设置、连接、音频通道路由和 MIDI 映射。切换预设可以更改相同插件的设置、换用不同的插件，或更改通道路由。

## 存储预设

1. 从 Curve 菜单中选择 Save As Preset...。
2. 输入名称并点按 Save。

更改不会自动存储。只要想保留更改，请使用 Save As Preset...。

## 更改现有预设

1. 在 Curve 菜单中选择预设以将其载入。
2. 进行更改。
3. 选择 Save As Preset...，选择现有名称，然后确认要替换它。

## 切换预设

你的预设按字母顺序列在 Curve 菜单中，当前预设带有勾号。选择一个预设即可立即切换。Curve 不会先询问，因此请在切换前存储想保留的更改。

你也可以[用键盘](keyboard-shortcuts.md)或[通过 MIDI 控制器](midi-control.md)切换预设。要直接载入预设文件，请将其拖到编辑器窗口上。

## 开始新预设

要从空白的图重新开始，请点按编辑器窗口左上角的菜单图标，然后选择 New Preset。除 Audio Input、Interface Loopback (In) 和 Audio Output 外，所有节点都会被移除。这些节点没有连接，因此在连接它们之前不会有声音。

除非图中只有这些节点，否则 Curve 会先询问，因为在插件窗口中更改的设置不会被记录为更改。选择 Save As Preset... 以保留当前设置，或选择 Discard and Start New。

## 重命名或删除预设

在 Save Preset 窗口中，右键点按预设可将其重命名、移到废纸篓或在访达中显示。

预设是位于 `~/Library/Application Support/Curve/Presets/` 中的 `.filtergraph` 文件，因此你也可以在访达中管理它们。
