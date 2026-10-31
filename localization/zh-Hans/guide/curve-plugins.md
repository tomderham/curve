# Curve Plugins

Curve 包含以下插件。要将插件添加到预设，请右键点按编辑器背景并选择 Curve Plugins。

## EQ

- **Parametric EQ：** 指向 Apple 多频段参数均衡器 AUNBandEQ 的快捷方式，常用于校准。多通道。同一菜单还可以[导入耳机配置文件或 EQ 文件](eq-and-autoeq.md)。

## 耳机处理

- **Headphone Crossfeed：** 交叉馈送，让耳机上的立体声声像更自然，强度可调。立体声。
- **Headphone Speaker Emulation：** 在耳机上模拟中场录音室音箱，包含交叉馈送。立体声。

## 电平和安全

- **Gain：** 从 -60 dB 到 +60 dB 的音量调整，带静音开关。多通道。
- **Clipper：** 在 -50 到 0 dBFS 的上限处对每个通道进行硬削波，用于防范 EQ 提升，并带有削波指示器。只有当上限低于 0 dBFS 时才会增加保护。无延迟。多通道。
- **Invert Phase：** 反转每个通道的极性。多通道。

## 路由和立体声

- **Pan：** 将单声道输入放置在立体声输出中，中央声像定律可选 0、-3、-4.5 或 -6 dB。通过 Configure Audio I/O 设置为立体声输入时，它用作平衡控制。
- **Stereo Utility：** 交换左右声道、单声道混合、仅左声道或仅右声道，以及 0 到 200% 的立体声宽度。立体声。
- **Mid/Side Encoder：** 将左右声道转换为第一个引脚上的 Mid 和第二个引脚上的 Side。立体声。
- **Mid/Side Decoder：** 将 Mid 和 Side 转换回左右声道。将编码器和解码器放在对两个引脚分别处理的环节前后。往返转换逐位精确。立体声。
- **Delay：** 最长 2 秒的整采样延迟，可用采样数、毫秒或厘米显示，用于音箱对齐或音视频同步。在播放时更改可能会产生短暂的咔嗒声。多通道。

## 录音

- **Audio Recorder：** 录制为桌面上的 WAV 文件。多通道。

## 可通过 MIDI Learn 控制的项目

请参阅 [MIDI 控制](midi-control.md)，了解每个插件提供的内容。
