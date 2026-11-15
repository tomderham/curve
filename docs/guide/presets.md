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

## Scenes

If several presets are the same apart from a few settings, such as a node that's bypassed or a plug-in set differently, you can make them scenes. Scenes share their plug-ins and connections, so you only make a change once to update them all. Each scene stays in Curve's menu like any other preset, and you switch to it the same way. In the Save Preset window, scenes that share with each other have the same colour, and selecting one highlights the others.

What each scene keeps for itself:

- Whether each node is bypassed.
- The settings of nodes set to Vary by Scene. Right-click a node and select Vary by Scene, and it shows a small stacked-cards badge.

Everything else is shared: the other nodes and their settings, connections, channel routing and MIDI mappings. Change a shared node in any scene, save it, and every scene gets the change. A node set to Vary by Scene still shares its channels and connections.

To make a scene:

1. Load the preset the new scene should start from.
2. For each plug-in that should be set differently in the new scene, right-click it and select Vary by Scene.
3. Make the changes for the new scene.
4. Select Save As Preset..., enter a new name, choose the Scene option, and click Save.

Before you save, the Save Preset window says which other scenes the save updates, and names any shared nodes whose settings will change in them. If a setting is meant for this scene only, cancel, set that node to Vary by Scene, and save again.

To change a scene, load it, make your changes, then save it under the same name. Saving over another scene it shares with replaces that scene, and it stays a scene. A node you add appears in the other scenes too, bypassed, so they sound the same until you turn it on there.

To stop a scene sharing with the others, right-click it in the Save Preset window and select Convert to Standalone Preset. It keeps its current settings.

## Rename or delete a preset

In the Save Preset window, right-click a preset to rename it, move it to the Trash or reveal it in Finder.

Presets are `.filtergraph` files in `~/Library/Application Support/Curve/Presets/`, so you can also manage them in Finder.
