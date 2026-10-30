# Curve

Curve is a free, lightweight, background audio plugin host for macOS (Sonoma 14.2 and above), running natively on both Apple Silicon and Intel-based Macs.
It runs in the macOS system menu bar and provides a stable way to run a chain of audio plugins (effects and/or instruments) with a focus on a fast, preset-based workflow and audio device resilience.

A primary use case for this app is to host equalizer (EQ) plugins for real-time headphone and/or speaker calibration, allowing you to apply system-wide audio correction without needing hardware-based solutions or additional loopback drivers. Other use cases include always-active host for instrument plugins, which might be MIDI enabled. See the examples below.

## Curve Help

[Curve Help](docs/guide/README.md) explains how to use everything in Curve, from your first preset to loopback, AutoEQ and MIDI control. It's also in the app: select Settings -> 'Curve Help'.

## Features

- Supported external plugin formats: Audio Units (AU) and VST3.
- Universal 2 Binary, natively supporting both Apple Silicon and Intel based macOS Macs.
- Stable plugin hosting based on JUCE 8.0.16, with backported fixes.
- Discrete menu bar app with quick preset change control; create, load and save presets comprising an arbitrary chain of plugins.
- Fast preset switching from the menu, a global keyboard shortcut and number keys, or a MIDI controller.
- Automatic handling of preferred audio interfaces, recovery after audio interface disconnect/reconnect and device sleep, etc.
- Full control over audio device settings, including channel selection on input and output interfaces, sample rate, and buffer latency.
- Native bit-perfect loopback support (without additional drivers); also an (optional) feature to force system audio output to the loopback.
- Headphone EQ from the AutoEQ database, or imported from AutoEQ, Squig.link and REW files.
- Native (internal) plugins for headphone crosstalk and studio monitor emulation when using headphones.
- Native (internal) utility plugins for gain, safety clipping, phase inversion, panning, stereo/mid-side processing, sample-accurate delay and tapped audio recording to wav file.
- MIDI Learn: control plugin parameters, node bypass and preset switching from a MIDI controller.
- Optional release of the audio interface when idle, which allows AirPods automatic switching.
- Optional Dock icon, open at login and automatic update checks.


## Quick start

Download the pre-built `Curve.app` from the latest [GitHub Release](https://github.com/tomderham/curve/releases), copy it to your Applications folder and open it. Curve needs macOS Sonoma 14.2 or later.

Curve's icon is added to the macOS menu bar as shown below. The editor window opens on first use, but after that you won't see any open windows or a Dock icon. Curve's menu can be accessed from the menu bar icon or, when open, from the burger icon at the top left of the editor window.
</br>
<img width="37" height="29" alt="Screenshot 2026-02-08 at 12 03 19 AM" src="https://github.com/user-attachments/assets/4d7738ad-f9ca-4b98-9c4e-c98b868ae126" />

1. If you use third-party plugins, select Settings -> 'Plug-in Manager', click Options... and scan for AU and VST3 plugins.
2. Select Settings -> 'Audio Settings' and choose your interfaces, channels, sample rate and buffer size.
3. In the editor, right-click the background to add plugins, then drag between the green pins to connect them.
4. Select 'Save As Preset...' from Curve's menu. Your presets then appear in the menu, ready to switch between.

The [Getting started](docs/guide/getting-started.md) page has the full walkthrough.

<img width="282" height="338" alt="MainMenu" src="https://github.com/user-attachments/assets/899a10a6-071f-45e0-ab74-0988ea2d8695" />
<img width="282" height="338" alt="AudioSettings" src="https://github.com/user-attachments/assets/09f7eace-a963-40a3-9709-a286135bab95" />
<br>
<img width="740" height="578" alt="Preset 1" src="https://github.com/user-attachments/assets/d03977ae-2191-47b3-b9ff-99206a4a813d" />
<img width="740" height="578" alt="Preset 2" src="https://github.com/user-attachments/assets/9a57d5fe-b416-4b4e-86cd-c9f4b30a5e1b" />
<img width="740" height="578" alt="Preset 3" src="https://github.com/user-attachments/assets/42b1d05f-b8b8-4bba-8fb2-8e07245f3725" />


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

The project is built using CMake. It is not necessary to use Projucer or Xcode IDE. The build also generates the Curve Help book from `docs/guide/` using `scripts/build_help.py`, which needs Python 3.

1.  **Clone the repository with its JUCE submodule, and create a build directory:**
    ```sh
    git clone --recurse-submodules https://github.com/tomderham/curve.git
    cd curve
    mkdir -p build
    ```
    In an existing clone, run `git submodule update --init` instead.
2.  **Configure the project and compile with CMake:** Run CMake from the new build directory
    ```sh
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build .
    ```
3.  **Run the application:** You can run the app directly, but it is better to copy/move it to your computer's `/Applications` folder. The CMake script attempts to code-sign the binary with the necessary entitlements, but if you find the binary fails to run or there are issues with persistent macOS privacy permissions, double-check that your code-signing setup is correct. Curve Help only opens when the app is in `/Applications`.

    To open it from the terminal:
    ```sh
    open ./Curve_artefacts/Release/Curve.app
    ```

