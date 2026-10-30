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

## A plug-in has the wrong number of pins

Right-click its node and select Configure Audio I/O.

## The sample rate keeps changing

Another app may be changing the hardware's sample rate. To make Curve force it back, turn on Always enforce sample rate in [Audio Settings](audio-settings.md).

## AirPods won't switch automatically

Turn on Settings > Release audio interface when idle. See [Audio settings and devices](audio-settings.md).

## Curve doesn't respond to my MIDI controller

Check that the controller is enabled under MIDI inputs in Audio Settings, and that the mapping was saved with the preset. See [MIDI control](midi-control.md).
