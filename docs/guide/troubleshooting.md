# Troubleshooting

## There's no sound, or some channels are muted

- Check that the nodes are connected all the way through to Audio Output.
- If the Interface Loopback (In) node is in a preset but you aren't using it, remove it or leave its pins unconnected. See [Loopback](loopback.md).
- If you've unplugged your selected interface, Curve stays silent until it's back, rather than using another output. To use a different one, select it in Audio Settings.
- Check that no node is bypassed or muted, and that the right channels are enabled in [Audio Settings](audio-settings.md).

## Curve Help says the content is unavailable

Curve Help only opens when Curve is in your Applications folder. Move it there and try again.

## I can't find Curve after opening it

Curve runs in the menu bar and normally opens no windows. Click its menu bar icon, then select Show Editor. To also show it in the Dock, turn on Settings > Show in Dock.

## My changes to a preset are gone

Curve doesn't save changes automatically. Use Save As Preset... and replace the existing preset. See [Presets](presets.md).

## A plug-in is missing from the menu

Select Settings > Plug-in Manager and run the scan for that plug-in's format. See [Getting started](getting-started.md).

If it's shown in red at the bottom of the Plug-in Manager list, with Scan failed as its category, the plug-in crashed or took more than 30 seconds to load, and scans now skip it. Right-click it and select Retry Scan, or select Remove from List to clear it.

## A plug-in has the wrong number of pins

Right-click its node and select Configure Audio I/O.

## The sample rate keeps changing

Another app may be changing the hardware's sample rate. To make Curve force it back, turn on Always enforce sample rate in [Audio Settings](audio-settings.md).

## AirPods won't switch automatically

Turn on Settings > Release audio interface when idle. See [Audio settings and devices](audio-settings.md).

## The volume keys don't change Curve's volume

- Check that Settings > Volume Keys isn't set to Off.
- If the menu shows Allow Accessibility Access..., select it and turn on Curve in Accessibility. If Curve is already on there, turn it off and on again.
- With When macOS Can't Control the Interface's Volume, macOS keeps the keys if it can set the volume of the channels Curve plays through, or if macOS's output isn't Curve's output interface. Turn on Force macOS system output to this interface in Audio Settings, or select Always. See [Keyboard shortcuts](keyboard-shortcuts.md).
- With Control Interface Loopback Volume, Curve takes the keys only while the preset has a connected Interface Loopback (In) node. See [Loopback](loopback.md).

## Curve doesn't respond to my MIDI controller

Check that the controller is enabled under MIDI inputs in Audio Settings, and that the mapping was saved with the preset. See [MIDI control](midi-control.md).
