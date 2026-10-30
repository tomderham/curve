# Getting started with Curve

Curve needs macOS Sonoma 14.2 or later, on Apple silicon or Intel.

## Install Curve

1. Download Curve from the [latest release](https://github.com/tomderham/curve/releases).
2. Copy `Curve.app` to your Applications folder and open it.

Curve runs in the menu bar, so by default its icon appears there rather than in the Dock. The editor window opens on first use. After that Curve starts with no windows open.

To reach Curve's menu, click the menu bar icon, or click the menu icon at the top left of the editor window.

## Scan for plug-ins

Skip this if you only want macOS's built-in plug-ins, such as AUNBandEQ, or [Curve's own plug-ins](curve-plugins.md).

1. Select Settings > Plug-in Manager.
2. Click Options... at the bottom left, then select 'Scan for new or updated AudioUnit plug-ins'. This can take a while, and macOS may ask you to allow Curve to access folders that plug-ins use.
3. If you use VST3 plug-ins, repeat this with 'Scan for new or updated VST3 plug-ins'.
4. Close the window.

## Choose your audio devices

1. Select Settings > Audio Settings.
2. Select the output interface and, unless you're using [loopback](loopback.md), the input interface.
3. Select the channels you want, plus the sample rate and buffer size.
4. If you use MIDI, select your MIDI interfaces.
5. Close the window.

## Build your first preset

A preset is a chain of connected nodes in the editor. If the editor isn't open, select Show Editor from Curve's menu.

1. You'll see the Audio Input, Interface Loopback (In) and Audio Output nodes, with green pins for the channels you enabled.
2. Right-click the editor background and select a plug-in. Its node appears with inputs at the top and outputs at the bottom.
3. Drag from a green pin on the input node to an input pin on the plug-in.
4. Drag from the plug-in's output pins to the Audio Output node.
5. Double-click the plug-in node to open its window and adjust it.
6. Select Save As Preset... from Curve's menu and give the preset a name.

If a preset doesn't use the Interface Loopback (In) node, remove it, or at least leave its pins unconnected. Otherwise it can mute your audio.

Take care with unusual routing. Curve has some protection against feedback loops, but a bad configuration can still damage equipment or your hearing.

## Finish setting up

- To start Curve at login, select Settings > Open at Login.
- Automatic update checks are on by default. You can change this with Settings > Auto-Check for App Updates.

## Next steps

- [Presets](presets.md)
- [Headphone EQ and AutoEQ](eq-and-autoeq.md)
- [Loopback and system audio](loopback.md)
