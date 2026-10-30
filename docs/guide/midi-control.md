# MIDI control

A MIDI controller or keyboard can play instrument plug-ins, change plug-in parameters, bypass nodes and switch presets. Enable it under MIDI inputs in [Audio Settings](audio-settings.md).

Playing an instrument, or sending MIDI to any other plug-in, needs a MIDI Input node connected to that plug-in. Controlling parameters, bypass and presets with MIDI Learn doesn't need a MIDI Input node or any connections in the preset.

## Play an instrument plug-in

1. Right-click the editor background and add your instrument plug-in.
2. Right-click the editor background and select MIDI I/O > MIDI Input.
3. Drag from the red pin on the MIDI Input node to the red pin at the top of the instrument. MIDI pins and connections are red, and audio pins and connections are green.
4. Drag from the instrument's green output pins to the Audio Output node.
5. Select Save As Preset... to keep the setup.

Effect plug-ins that respond to MIDI connect the same way. To send MIDI from a plug-in to an external device, enable the device as the MIDI output in Audio Settings, then connect the plug-in's red output pin to a MIDI Output node.

## Map a control with MIDI Learn

1. Right-click any node and select MIDI Learn. The editor gets a highlighted border, and the status bar guides you.
2. Choose what to control:
   - For a plug-in parameter, open the plug-in's window and move the control.
   - For a node's bypass, click its power button.
3. Move a knob, fader or button on your controller. The mapping is made straight away, and you can carry on mapping other controls.
4. Press Esc, or select MIDI Learn again, to finish.

Mappings are saved with the preset, so use Save As Preset... to keep them.

## Remove a mapping

- To remove the mapping for the selected parameter or bypass, press Delete during MIDI Learn.
- To remove all of a node's mappings, right-click it and select Clear MIDI Mappings.

## How mapped controls behave

- Knobs and faders (CC) cover a parameter's full range.
- Notes and program changes toggle the target.
- For bypass, Curve works out whether a CC button is momentary, where each press toggles bypass, or latching, where bypass follows the button's state.

Nodes with mappings show a small MIDI socket icon. Hover over the node to see its mappings in the status bar.

Mapped MIDI messages aren't passed on to plug-ins through the MIDI Input node. All other MIDI is.

## Switch presets from a controller

Open Settings > MIDI Control. These settings apply to all presets:

- **Program Change Selects Preset:** off by default. Program change 0 loads the first preset in the menu, 1 the second, and so on, in the same order as the [number key shortcuts](keyboard-shortcuts.md).
- **Next Preset Control** and **Previous Preset Control:** select one, then press the button on your controller that you want to use.
- **Clear Preset Controls:** removes both of those button assignments.

## Controls on Curve's own plug-ins

| Plug-in | Controls |
|---|---|
| Gain | Gain and mute |
| Pan | Pan |
| Clipper | Ceiling |
| Stereo Utility | Mode and width |
| Headphone Crossfeed | Amount |
| Audio Recorder | Record start and stop |

Headphone Speaker Emulation, Invert Phase and the Mid/Side Encoder and Decoder have no settings, so map their bypass instead. Delay isn't mappable.
