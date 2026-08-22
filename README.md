# Curve

Curve is a free, lightweight, background audio plugin host for macOS (Sonoma 14.2 and above), running natively on both Apple Silicon and Intel-based Macs.
It runs in the macOS system menu bar and provides a stable way to run a chain of audio plugins (effects and/or instruments) with a focus on a fast, preset-based workflow and audio device resilience.

A primary use case for this app is to host equalizer (EQ) plugins for real-time headphone and/or speaker calibration, allowing you to apply system-wide audio correction without needing hardware-based solutions or additional loopback drivers. Other use cases include always-active host for instrument plugins, which might be MIDI enabled. See the examples below.

## Features

- Supported external plugin formats: Audio Units (AU) and VST3.
- Universal 2 Binary, natively supporting both Apple Silicon and Intel based macOS Macs.
- Stable plugin hosting based on latest JUCE reference library.
- Discrete menu bar app with quick preset change control; create, load and save presets comprising an arbitrary chain of plugins.
- Automatic handling of preferred audio interfaces, recovery after audio interface disconnect/reconnect and device sleep, etc.
- Full control over audio device settings, including channel selection on input and output interfaces, sample rate, and buffer latency.
- Native bit-perfect loopback support (without additional drivers); also an (optional) feature to force system audio output to the loopback.
- Native (internal) plugins for headphone crosstalk and studio monitor emulation when using headphones.
- Native (internal) utility plugins for gain, phase inversion and tapped audio recording to wav file.


## Using the Curve app

For most users, the easiest way to get started is to download the pre-built `Curve.app` from the latest [GitHub Release](https://github.com/tomderham/curve/releases).
Simply copy the downloaded app to your Applications folder and open.

When the app opens, its icon will be added to the macOS menu bar as shown below. On very first use, the editor window should open, but otherwise you will not see any open windows, or any task bar icon. Curve's menu can be accessed either from the menu bar icon, or (when open) from the burger icon at the top left of the editor window.
</br>
<img width="37" height="29" alt="Screenshot 2026-02-08 at 12 03 19 AM" src="https://github.com/user-attachments/assets/4d7738ad-f9ca-4b98-9c4e-c98b868ae126" />

On first use, the following steps are recommended:

- If you plan to use third-party plugins with Curve, select Settings -> 'Plug-In Manager'.
  - Click Options... button at bottom left of the dialog, and select 'Scan for new or updated AudioUnit plug-ins'. The scan process may take a while, and you might be asked to allow permission for Curve to access folders that the plugins being scanned are using.
  - Then, if you intend to also use VST3 plugins with Curve, repeat the process by clicking 'Scan for new or updated VST3 plug-ins'.
  - Once the scan is complete, the dialog should show a list of plugins installed on your device, including both macOS system plugins and third-party plugins.
  - Close the dialog box (red cross at top left).
  - Note: If you only plan to use macOS system plugins (such as AUBandEQ parametric EQ), you can completely skip this step.
- Next, select Settings -> 'Audio Settings'.
  - Select the Output audio interface and (unless you are using the loopback feature) the Input audio interface, select the corresponding channels, and the desired sample rate and buffer latency.
  - If you intend to use MIDI-controlled plugins, select the MIDI interface(s) you want to use.
  - Close the dialog (red cross at top left).
- You can now create a preset. If the editor window is not already open, click on the menu bar icon and select 'Show Editor'.
  - When opening the app for the first time, you should see the Audio Input, Interface Loopback (in) and Audio Output nodes, each with some green dots that correspond to the channels you enabled in Audio settings. Nodes can be removed by right clicking them. Audio and MIDI I/O nodes can be added by right clicking the background of the Editor window. If you are not using the Interface Loopback node in a given preset (see examples below), remove it or at least ensure its pins are not connected (otherwise it can cause unexpected muting).
  - Next, to add plugins, right click the background of the Editor window and select a plugin. Select a plugin you want to use, and a corresponding node will appear with inputs at the top and outputs at the bottom. The 'Plugins' menu (right below Audio I/O and MIDI I/O) contains some useful internal plugins - see Internal plugins section below. Below that menu, all other scanned plugins can be found, organized by manufacturer. If the number of input and output pins for a given plugin is not what you want, right click the node and select 'Configure Audio I/O' to correct it.
  - Then, connect channel(s) on either the Audio Input or the Interface Loopback to inputs on the plugin, and connect channel(s) on the output of the plugin to the Audio Output. This is done by clicking a green dot on one node and dragging a connecting line to a green dot on another node. You can of course add additional plugin nodes and connect them together however you wish. If you are using the Interface Loopback node as the input, see the section on Loopback interfaces below.
  - Note: There is some loop-prevention handling, but you should obviously take care with crazy configurations that could cause hardware (or ear) damage.
  - If using MIDI controlled plugins (effects or instruments), add MIDI I/O nodes and connect them to the corresponding MIDI I/O of the plugins.
  - To open the editor of a plugin (e.g. to set the desired EQ in AUNBandEQ), simply double click on the plugin box. To save the preset, click the menu bar icon and select 'Save as preset'. Note the preset folder is ~/Library/Application Support/Curve/Presets/ where ~/Library is the user specific library at /Users/your_user_name/Library). Choose a suitable name and save the preset. The editor window can be hidden using red close (top left) or selecting 'Hide Editor' from the app menu.
  - If you want to modify a preset later, first load the preset, make the changes, 'Save as preset', select the existing file name and 'Replace'. Note that changes to presets are *not* saved unless you explicitly use 'Save as preset'. You can repeat the process to create multiple presets. You can rename or delete presets in the Save as dialog or using Finder in the preset folder.
  - If you click on the menu bar icon again, you should now see the list of presets you have created - simply click on them to instantly switch between them. The currently selected preset has a check mark.
  - By switching between plugins, you can switch between different settings of the same plugin(s), or switch between completely different (combinations of) plugins, and/or switch between different audio interface channel routings.
- If you want the app to automatically check for updates (recommended), ensure Settings -> 'Auto check for app updates" is checked.
- If you want the app to automatically open when you login to macOS, and didn't accept the dialog on initial open, click Settings -> "Open at Login...". (No checkbox will be shown, but the app should appear in "Open at Login" items in macOS System Settings).

<img width="282" height="338" alt="MainMenu" src="https://github.com/user-attachments/assets/899a10a6-071f-45e0-ab74-0988ea2d8695" />
<img width="282" height="338" alt="AudioSettings" src="https://github.com/user-attachments/assets/09f7eace-a963-40a3-9709-a286135bab95" />
<br>
<img width="740" height="578" alt="Preset 1" src="https://github.com/user-attachments/assets/d03977ae-2191-47b3-b9ff-99206a4a813d" />
<img width="740" height="578" alt="Preset 2" src="https://github.com/user-attachments/assets/9a57d5fe-b416-4b4e-86cd-c9f4b30a5e1b" />
<img width="740" height="578" alt="Preset 3" src="https://github.com/user-attachments/assets/42b1d05f-b8b8-4bba-8fb2-8e07245f3725" />


## Loopback interfaces

In some use cases, it is desirable to redirect the audio output of an app (e.g. Logic Pro) or the macOS system output to the input of Curve, in order to apply audio plugins (e.g. EQ) to the audio before it is sent to real output devices (such as headphones or speakers).

If your audio interface provides a native loopback feature, this is usually the best choice since it minimizes latency imparted by the loopback. For example, using RME Totalmix's Loopback function, you can redirect a pair of output channels to a spare pair of input channels. You would then configure Curve to use those input channels as its inputs (using the Audio Input node), and configure (some of) the 'software playback' channels as Curve's output channels. Finally, in Totalmix you would assign the software playback channel(s) to the desired hardware outputs. Curve's Interface Loopback node would not be used in this case.

Alternatively, you can use the native Interface Loopback (in) node in Curve. Simply use this node instead of the Audio Input node as the input for your preset chain. No additional software or driver installation is needed. This node receives the redirected bit-perfect audio that would otherwise have been sent to the output audio interface you configured in Curve.
If you always want macOS system output to be redirected to the Interface Loopback, check Settings -> 'Force macOS System Audio to loopback'. With this enabled, Curve will enforce macOS system audio output to be the selected audio interface in Curve's audio settings.

Notes for nerds - The audio channels of 'Stream 0' (the primary stream) of the selected output audio interface are redirected to the loopback node when its pins are connected. Therefore, the 'direct' audio for those channels does not reach the actual output device. However, if some of the audio interface's channels are assigned to a different stream, they would be unaffected. The allocation of output channels to streams is usually determined by the audio interface driver.

## Internal plugins

Curve provides the following native plugins for use within Curve, accessible from the 'Plugins' menu
- Parametric EQ - A convenience shortcut to Apple's AUNBandEQ (multi-band parametric EQ, often used for calibration EQ) (multi-channel)
- Headphone Crossfeed - Crossfeed processor for natural soundstage imaging when monitoring with headphones (stereo)
- Headphone Speaker Emulation - Mid-field studio speaker emulation processor (including crossfeed) for monitoring with headphones (stereo)
- Gain - Volume trim and mute toggle utility (multi-channel)
- Invert Phase - Negates polarity of audio signal on each channel (multi-channel)
- Audio Recorder - Audio recorder utility, saves WAV files to Desktop (multi-channel)


# Building from source code on macOS

Please note that this project is licensed under the AGPLv3, as it is derived from the JUCE framework. Ensure that your use of this code and any derivative works complies with the terms of this license and includes the necessary copyright notices.

The CMake build process is configured to produce a Universal 2 Binary for macOS by default.

This application was developed based on the `AudioPluginHost` example provided with the JUCE framework. Many thanks to the JUCE developers for making this framework and the reference host application available under open source license.

## Prerequisites

Before you begin, ensure you have the following installed:

1.  **Xcode Command Line Tools**: If you don't have them, open `Terminal` and run:
    ```sh
    xcode-select --install
    ```

2.  **CMake**: The easiest way to install CMake is using [Homebrew](https://brew.sh/):
    ```sh
    brew install cmake
    ```

## Build Instructions

The project is built using CMake. It is not necessary to use Projucer or Xcode IDE. These instructions assume you are at the root of the repository.

1.  **Create a build directory:**
    ```sh
    mkdir -p build
    ```
2.  **Configure the project and compile with CMake:** Run CMake from the new build directory
    ```sh
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build .
    ```
3.  **Run the application:** You can run the app directly, but it is better to copy/move it to your computer's `/Applications` folder. The CMake script attempts to code-sign the binary with the necessary entitlements, but if you find the binary fails to run or there are issues with persistent macOS privacy permissions, double-check that your code-signing setup is correct.

    To open it from the terminal:
    ```sh
    open ./Curve.app
    ```

