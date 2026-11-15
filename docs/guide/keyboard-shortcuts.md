# Keyboard shortcuts

## Open Curve's menu from anywhere

The global shortcut opens Curve's menu even when another app is in front. It's off until you set it up.

1. Select Settings > Preferences..., then the Keyboard tab, and click Change... next to Global Menu Shortcut.
2. Click Record Shortcut, then press a key combination that includes Command, Option or Control. Press Esc to cancel.
3. Click Save.

Click Reset to Default to use ⌥⇧C, or Disable to turn the shortcut off.

## Set Curve's volume with the volume keys

Some audio interfaces have no volume control that macOS can use, so the keyboard's volume keys do nothing. Curve can use the keys to set its own output volume instead.

1. Select Settings > Preferences..., then the Keyboard tab. Set Volume Keys to When macOS Can't Control the Interface's Volume.
2. When macOS asks, give Curve Accessibility access in System Settings > Privacy & Security > Accessibility. Curve needs it to catch the keys.

Curve then shows its own volume overlay under the Sound icon in the menu bar, as macOS does.

- The volume keys step through 16 levels, as in macOS. Hold Shift and Option with a volume key for finer steps.
- The mute key turns Curve's output off and on.

Curve only takes the keys while macOS's output is Curve's output interface and macOS can't set the volume of the channels Curve plays through. This includes interfaces with no volume control at all, and interfaces whose macOS volume control covers only some of their channels, not the ones Curve plays through. Otherwise the keys work as usual in macOS. To make the keys always set Curve's volume, select Always instead.

By default, Curve's volume applies to everything Curve outputs, after your plug-ins. To set only the volume of the audio from macOS and other apps, select Control Interface Loopback Volume in the menu below Volume Keys. The keys then turn down only the [Interface Loopback (In)](loopback.md) node, so audio from Audio Input and from instrument plug-ins in Curve keeps its level. If the preset has no connected Interface Loopback (In) node, the keys work as usual in macOS. Select Control Curve's Output Volume to go back.

Curve's volume applies only while the keys set it. Curve remembers it when you quit. At full volume the audio is left untouched.

To hide the overlay, turn off Show Volume Overlay in the same menu.

## Switch presets from the keyboard

While Curve's menu is open, press a key to select a preset. The key is shown beside each preset.

| Presets | Keys |
|---|---|
| 1 to 10 | 1 thru 0 |
| 11 to 20 | Option with 1 thru 0 |
| 21 to 30 | Shift with 1 thru 0 |

For example, press the global shortcut and then 2 to switch to your second preset from any app.

## In the editor during MIDI Learn

| Key | Action |
|---|---|
| Esc | Finish MIDI Learn |
| Delete | Remove the mapping for the selected control |
