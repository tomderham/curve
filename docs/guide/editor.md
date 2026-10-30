# The editor

The editor shows your preset as a graph of nodes. Audio flows from each node's outputs, at the bottom of the node, to the inputs of the next node, at the top of the node. To open it, select Show Editor from Curve's menu. To close it, click its close button or select Hide Editor. Audio keeps running either way.

The status bar at the bottom of the editor gives hints, and describes whatever is under the pointer.

## Add a node

Right-click the editor background, then select from:

- **Audio I/O:** audio input, audio output and Interface Loopback (In) nodes.
- **MIDI I/O:** MIDI input and output nodes.
- **Curve Plugins:** [Curve's built-in plug-ins](curve-plugins.md).
- **Your scanned plug-ins:** listed below those menus, organised by manufacturer.

You can also drag a plug-in file, or an [EQ file](eq-and-autoeq.md), onto the editor window.

## Connect nodes

Drag from a pin on one node to a pin on another. Audio pins are green and MIDI pins are red. You can connect one output to several inputs and build any chain you like.

## Bypass a node

Click the power button on the node. Click it again to re-enable the node. You can also right-click the node and select Toggle Bypass.

## Open a plug-in's window

Double-click the node, or right-click it and select Show Plug-in GUI.

## Node menu

Right-click a node for these options:

| Item | What it does |
|---|---|
| Delete This Node | Removes the node. |
| Disconnect All Pins | Removes all of the node's connections. |
| Toggle Bypass | Turns bypass on or off. |
| Rename This Node | Changes the name shown on the node. |
| MIDI Learn | Starts [MIDI Learn](midi-control.md). |
| Clear MIDI Mappings | Removes the node's MIDI mappings. Shown only if it has some. |
| Show Plug-in GUI | Opens the plug-in's window. |
| Import EQ from AutoEQ... | Shown on Parametric EQ nodes. See [Headphone EQ and AutoEQ](eq-and-autoeq.md). |
| Import EQ from file... | Shown on Parametric EQ nodes. See [Headphone EQ and AutoEQ](eq-and-autoeq.md). |
| Configure Audio I/O | Changes the number of input and output pins on a plug-in. |
| Audio Settings... | Shown on audio input and output nodes. Opens Audio Settings. |
| Save Plug-in State | Saves the plug-in's settings to a file. |
| Load Plug-in State | Loads the plug-in's settings from a file. |

## Change a plug-in's channel count

If a plug-in doesn't have the number of pins you want, right-click its node and select Configure Audio I/O.
