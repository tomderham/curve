# Presets

A preset stores a whole graph: its plug-ins and their settings, connections, audio channel routing, and MIDI mappings. Switching presets lets you change the settings of the same plug-ins, swap to different plug-ins, or change channel routing.

## Save a preset

1. Select Save As Preset... from Curve's menu.
2. Enter a name and click Save.

Changes aren't saved automatically. Use Save As Preset... whenever you want to keep them.

## Change an existing preset

1. Load the preset by selecting it in Curve's menu.
2. Make your changes.
3. Select Save As Preset..., choose the existing name, then confirm that you want to replace it.

## Switch presets

Your presets are listed in Curve's menu, in alphabetical order, and the current one has a check mark. Select one to switch to it straight away. Curve doesn't ask first, so save any changes you want to keep before you switch.

You can also switch presets [from the keyboard](keyboard-shortcuts.md) or [from a MIDI controller](midi-control.md). To load a preset file directly, drag it onto the editor window.

## Start a new preset

To start again from an empty graph, click the menu icon at the top left of the editor window and select New Preset. Every node is removed except Audio Input, Interface Loopback (In) and Audio Output, which aren't connected, so there's no sound until you connect them.

Unless the graph has only those nodes, Curve asks first, because settings changed in a plug-in's window aren't tracked as changes. Select Save As Preset... to keep the current setup, or Discard and Start New.

## Rename or delete a preset

In the Save Preset window, right-click a preset to rename it, move it to the Trash or reveal it in Finder.

Presets are `.filtergraph` files in `~/Library/Application Support/Curve/Presets/`, so you can also manage them in Finder.
