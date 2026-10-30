# Audio settings and devices

Select Settings > Audio Settings to choose your audio and MIDI devices.

## Choose interfaces and channels

Select an output interface, and an input interface if your preset uses one. Then enable the channels you want. They appear as green pins on the Audio Input and Audio Output nodes.

If you're using Curve's [Interface Loopback (In) node](loopback.md), you don't need an input interface.

## Sample rate and buffer size

Select the sample rate and buffer size here. A smaller buffer means lower latency, but it puts more load on your Mac and can cause dropouts.

**Always enforce sample rate** is off by default. When it's off, Curve adapts if the audio interface or another app changes the hardware's sample rate. When it's on, Curve forces the rate back to the one you chose.

## MIDI devices

Enable the MIDI inputs and output you want to use. Use a MIDI Input or MIDI Output node to connect them to plug-ins. A controller used for [MIDI control](midi-control.md) needs to be enabled here.

## When devices change

Curve handles common device changes for you:

- It stays on the interface you selected. If that interface is disconnected, Curve goes silent rather than falling back to another output, such as your Mac's speakers.
- It reconnects and restores audio when the interface comes back.
- It restarts audio after your Mac wakes from sleep.

## AirPods and automatic switching

macOS may not switch AirPods automatically while Curve has an audio interface open. To allow it, turn on Settings > Release audio interface when idle.

Curve then releases the interface after 5 seconds of silence, and reopens it when another app starts using it. This only happens if Curve can reliably wake again, so the interface isn't released in these cases:

- Your input is a physical microphone or line input on a separate device. Virtual devices such as BlackHole are fine.
- Another app is using the device.
- Curve is busy, for example recording.
