# Settings reference

Open Settings from Curve's menu.

| Item | What it does |
|---|---|
| Audio Settings | Choose audio and MIDI devices. See [Audio settings and devices](audio-settings.md). |
| Plug-in Manager | Scan for and manage plug-ins. See [Getting started](getting-started.md). |
| Preferences... | Opens the Preferences window, below. |
| Curve Help | Opens this guide. |
| About... | Shows version and licence details. |
| Quit | Quits Curve and stops its audio processing. |

## Preferences

The Preferences window has three tabs. It can stay open while you use Curve.

### General

| Item | What it does | Default |
|---|---|---|
| Open at Login | Starts Curve when you log in. | |
| Show in Dock | Shows Curve in the Dock and the Command-Tab switcher. | Off |
| Prevent macOS from automatically sleeping | Stops your Mac idle sleeping while Curve's audio is running. | Off |
| Release audio interface when idle | Releases the interface after 5 seconds of silence, which allows AirPods automatic switching. See [Audio settings](audio-settings.md). | Off |
| Unsaved changes to a preset: Keep them when Curve quits | Keeps changes to the loaded preset that haven't been saved, plug-in settings included, and restores them the next time Curve opens. The preset file isn't changed unless you save it. Curve asks before restoring if it didn't shut down cleanly, if it was asked to open another preset, or if the interface in use has a different default preset. | On |
| Unsaved changes to a preset: Save them every 5 minutes | Also saves them while Curve runs, so they can be restored after a crash. A few plug-ins hold up their audio briefly while their settings are saved. | Off |
| Auto-Check for App Updates | Checks for new versions automatically. | On |
| Check for Updates... | Checks for a new version now. | |

### Keyboard

| Item | What it does | Default |
|---|---|---|
| Volume Keys | Lets the keyboard's volume keys set Curve's output volume, or only the Interface Loopback (In) volume, for interfaces without a volume control of their own. See [Keyboard shortcuts](keyboard-shortcuts.md). | Off |
| Show Volume Overlay | Shows Curve's volume under the Sound icon in the menu bar when the volume keys change it. | On |
| Global Menu Shortcut | Sets a key combination that opens Curve's menu from anywhere. See [Keyboard shortcuts](keyboard-shortcuts.md). | Off |

### MIDI

| Item | What it does | Default |
|---|---|---|
| Program Change Selects Preset | Program changes from a controller load presets. See [MIDI control](midi-control.md). | Off |
| Next Preset Control, Previous Preset Control | Buttons on a controller that step through the presets. | |
| Clear Preset Controls | Removes both of those button assignments. | |

The loaded preset's own MIDI mappings are in MIDI Mappings for Current Preset..., in the editor's menu. See [MIDI control](midi-control.md).
