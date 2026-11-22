# The editor

The editor shows your preset as a graph of nodes. Audio flows from each node's outputs, at the bottom of the node, to the inputs of the next node, at the top of the node. To open it, select Show Editor from Curve's menu. To close it, click its close button or select Hide Editor. Audio keeps running either way.

The status bar at the bottom of the editor gives hints, and describes whatever is under the pointer. For a plug-in, that includes its manufacturer and format, such as AudioUnit or VST3.

## Add a node

Right-click the editor background, then select from:

- **Audio I/O:** audio input, audio output and Interface Loopback (In) nodes.
- **MIDI I/O:** MIDI input and output nodes.
- **Curve Plugins:** [Curve's built-in plug-ins](curve-plugins.md).
- **Your scanned plug-ins:** listed below those menus, organised by manufacturer.

You can also drag a plug-in file, or an [EQ file](eq-and-autoeq.md), onto the editor window.

## Connect nodes

Drag from a pin on one node to a pin on another. Audio pins are grey and MIDI pins are amber. You can connect one output to several inputs and build any chain you like.

Consecutive channels running between the same two nodes, such as a stereo pair, are drawn as one thicker line that forks out to each pin. The channels stay separate, nothing is mixed down. Drag the line to reconnect one channel at a time.

## Arrange nodes

Drag a node to move it. Nodes snap to a grid, so aligned nodes get straight wires. Hold Option while dragging to place a node freely. Dragging it again without Option puts it back on the grid.

To arrange the whole graph by signal flow, right-click the background and select Tidy Up Layout. If the graph needs more room, the window grows to fit, up to the size of your screen. Undo Tidy Up then restores your layout and window size, until you next move a node.

## Bypass a node

Click the power button on the node. Click it again to re-enable the node. You can also right-click the node and select Bypass.

## Mute a node

Right-click the node and select Mute. A muted node passes on nothing and isn't processed, so it cuts the path it's on, where a bypassed node passes its input straight through. It's dimmed, with a dashed outline and red crosses on its outputs. To unmute it, click its power button, or select Mute again. In [scenes](presets.md), each scene has its own.

## Tempo and transport

The strip at the top right of the editor sets the tempo that tempo-synced plug-ins follow, such as delays, and starts and stops the transport for plug-ins that play along with the host, such as drum machines.

- To set the tempo, click it and type a value, or click Tap in time at least twice.
- Click the play button to start the transport from the beginning of bar 1, and click it again to stop it.

The tempo is saved with the preset, and in [scenes](presets.md) each scene has its own. The time signature is 4/4. The transport isn't saved, and is stopped when Curve starts.

## Open a plug-in's window

Double-click the node.

To open the window every time the preset loads, even with the editor closed, right-click the node, select Always Open Plug-in GUI with Preset, then use Save As Preset... The window opens where it was when you saved.

## Node menu

Right-click a node for these options:

| Item | What it does |
|---|---|
| Bypass | Turns bypass on or off. Not shown for I/O nodes and meters. |
| Mute | Mutes or unmutes the node. A muted node passes on nothing. See above. |
| Delete This Node | Removes the node. |
| Disconnect All Pins | Removes all of the node's connections. |
| Rename This Node | Changes the name shown on the node. |
| Clear MIDI Mappings | Removes the node's MIDI mappings. Shown only if it has some. |
| Always Open Plug-in GUI with Preset | Opens the plug-in's window whenever the preset loads. In [scenes](presets.md), each scene has its own. |
| Import EQ from AutoEQ... | Shown on Parametric EQ nodes. See [Headphone EQ and AutoEQ](eq-and-autoeq.md). |
| Import EQ from file... | Shown on Parametric EQ nodes. See [Headphone EQ and AutoEQ](eq-and-autoeq.md). |
| Configure Audio I/O | Changes a plug-in's channel layouts, and turns off or hides its pins. See below. |
| Audio Settings... | Shown on audio input and output nodes. Opens Audio Settings. |
| Save Plug-in State | Saves the plug-in's settings to a file. |
| Load Plug-in State | Loads the plug-in's settings from a file. |

## Configure a plug-in's inputs and outputs

Right-click a plug-in's node and select Configure Audio I/O. A plug-in's pins come in buses, such as a main stereo input and a sidechain input. Select a bus by its number, then use:

- **Channel Layout:** sets how many channels the bus has. Layouts with more than two channels are grouped by channel count. A surround or ambisonic plug-in can treat two layouts with the same number of channels differently, so choose the one that matches your audio.
- **Enabled:** turns the bus off, which removes its pins. It's greyed out if the plug-in can't run without the bus.
- **Shown:** hides the bus's pins from the graph, but the bus keeps running. Use it to tidy away a bus you don't use that can't be turned off. Hiding a bus removes its connections, and a hidden input receives silence. Shown is greyed out while the bus is off.

If the plug-in has a MIDI pin, MIDI Port has its own Shown setting. A side with only MIDI, such as an instrument's input, has just that setting.

Changing a layout or turning a bus on or off disconnects all of the node's pins. The settings are saved with the preset. When you point at a node with hidden pins, the status bar says so.
