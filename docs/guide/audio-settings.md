# Audio settings and devices

Select Settings > Audio Settings to choose your audio and MIDI devices.

## Choose interfaces and channels

Select an output interface, and an input interface if your preset uses one. Then enable the channels you want. They appear as grey pins on the Audio Input and Audio Output nodes.

If you're using Curve's [Interface Loopback (In) node](loopback.md), you don't need an input interface.

Curve remembers the sample rate, buffer size and enabled output channels for each output interface, and restores them when you switch back to it.

## Follow macOS's output

Turn on **Use output interface selected in macOS** to let macOS choose Curve's output. When you choose an output in System Settings, Control Center or the menu bar, Curve moves its processing to it within about a second. If that interface is disconnected, Curve moves to the output macOS uses next. Curve's Output menu is greyed out while this is on.

Your preset's connections to outputs the new interface doesn't have are kept, but they're silent until you switch back to an interface that has them. Correction made for one interface, such as EQ for particular headphones, is applied to whatever macOS switches to.

While it's on, select **Default Presets for each output interface...** to choose a preset for each interface. When macOS switches to an interface, Curve loads its default preset. Interfaces set to None keep the preset that's loaded. Turn on **Also load at launch** to load the default preset of the interface macOS is using when Curve starts, instead of the preset used last. If a default preset can't be found, Curve keeps the current preset and says so in the editor's status bar.

## Keep macOS on Curve's interface

**Force macOS system output to this interface** is on by default. It keeps macOS's system output set to Curve's output interface, so that the [Interface Loopback (In) node](loopback.md) receives all system audio. Turn it off if you want to choose the system output yourself. It's greyed out while Use output interface selected in macOS is on, because macOS's output is then always Curve's.

## Sample rate and buffer size

Select the sample rate and buffer size here. A smaller buffer means lower latency, but it puts more load on your Mac and can cause dropouts.

**Always enforce sample rate** is off by default. When it's off, Curve adapts if the audio interface or another app changes the hardware's sample rate. When it's on, Curve forces the rate back to the one you chose.

## MIDI devices

Enable the MIDI inputs and output you want to use. Use a MIDI Input or MIDI Output node to connect them to plug-ins. A controller used for [MIDI control](midi-control.md) needs to be enabled here.

## When devices change

Curve handles common device changes for you:

- It stays on the interface you selected, unless Use output interface selected in macOS is on. If that interface is disconnected, Curve goes silent rather than falling back to another output, such as your Mac's speakers.
- It reconnects and restores audio when the interface comes back.
- It restarts audio after your Mac wakes from sleep.

## AirPods and automatic switching

macOS may not switch AirPods automatically while Curve has an audio interface open. To allow it, select Settings > Preferences... and turn on Release audio interface when idle.

Curve then releases the interface after 5 seconds of silence, and reopens it when another app starts using it. This only happens if Curve can reliably wake again, so the interface isn't released in these cases:

- Your input is a physical microphone or line input on a separate device. Virtual devices such as BlackHole are fine. The same applies to an input on your output interface if the Audio Input node is connected in your preset.
- Another app is using the device.
- Curve is busy, for example recording.
