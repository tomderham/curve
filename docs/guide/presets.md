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

If several presets have parts in common, you can make them scenes. For example, scenes can share the same speaker correction but use different instruments or signal paths, or use the same instruments with different speaker correction. Scenes share their plug-ins and connections, so you only make a change once to update them all. Each scene stays in Curve's menu like any other preset, and you switch to it the same way.

Scenes that share with each other are marked in the same colour in Curve's menu and in the Save Preset window.

Switching to another scene is quick: Curve changes only what that scene has of its own, without loading its plug-ins again. If you've changed a shared setting since loading, or a plug-in is missing, the scene loads in full instead, and the change is lost as it is when you switch presets.

What each scene keeps for itself:

- Whether each node is bypassed or muted. A bypassed node passes its input straight through. A muted node passes on nothing, so it cuts the path it's on. See [Mute a node](editor.md).
- The settings of nodes set to Vary by Scene. Right-click a node and select Vary by Scene, and it shows a small stacked-cards badge.
- Which plug-in windows open with it (Always Open Plug-in GUI with Preset), and where they are. A node you add to a scene doesn't open with the others.

Everything else is shared: the other nodes and their settings, connections, channel routing and MIDI mappings. Change a shared node in any scene, save it, and every scene gets the change. A node set to Vary by Scene still shares its channels and connections.

To make a scene:

1. Load the preset the new scene should start from.
2. For each plug-in that should be set differently in the new scene, right-click it and select Vary by Scene.
3. Make the changes for the new scene. To leave out a path, mute a node on it.
4. Select Save As Preset..., enter a new name, choose the Scene option, and click Save.

Before you save, the Save Preset window says which other scenes the save updates, and names any shared nodes whose settings will change in them. If a change is meant for this scene only, cancel, right-click the node and select Vary by Scene, then save again. Each scene then keeps its own settings for that node. It stays a scene, and the scenes still share everything else.

To change a scene, load it, make your changes, then save it under the same name. Saving over another scene it shares with replaces that scene, and it stays a scene. A node you add appears in the other scenes too, bypassed, so they sound the same until you turn it on there. A bypassed node still passes its input on, so to keep a new path silent in the other scenes, mute one of its nodes there.

To stop a scene sharing with the others, right-click it in the Save Preset window and select Convert to Standalone Preset. It keeps its current settings.

## Rename or delete a preset

In the Save Preset window, right-click a preset to rename it, move it to the Trash or reveal it in Finder.

Presets are `.filtergraph` files in `~/Library/Application Support/Curve/Presets/`, so you can also manage them in Finder.
