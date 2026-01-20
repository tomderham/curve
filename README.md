# Curve

Curve is a free, lightweight, background audio plugin host for macOS, running natively on both Apple Silicon and Intel-based Macs.
It runs in the macOS system menu bar and provides a stable way to run a chain of audio plugins with a focus on a fast, preset-based workflow and audio device resilience.

A primary use case for this app is to host equalizer (EQ) plugins for real-time headphone and speaker calibration, allowing you to apply system-wide audio correction without needing hardware-based solutions. See the examples below.

## Features

- Supported plugin formats: Audio Units (AU) and VST3.
- Universal 2 Binary, natively supporting both Apple Silicon and Intel based macOS Macs.
- Stable plugin hosting based on latest JUCE reference library.
- Discrete menu bar app with quick preset change control
- Automatic handling of preferred audio interfaces, recovery after audio interface disconnect/reconnect and device sleep, etc
- Create, save and quickly load presets comprising an arbitrary chain of plugins.
- Full control over audio device settings, including channel selection on input and output interfaces, sample rate, and buffer latency.

## Using the Curve app

For most users, the easiest way to get started is to download the pre-built `Curve.app` from the latest [GitHub Release](https://github.com/tomderham/curve/releases).
Simply copy the downloaded app to your Applications folder and open. If security warnings or other errors are displayed, see the troubleshooting notes below.

When the app opens, its icon will be added to the macOS menu bar as shown below. (You will not see any open windows, or any task bar icon). On first use, the following steps are recommended:

- Click on the menu bar icon, select 'Plugin manager'.
  - Click Options... button at bottom left of the dialog, and select 'Scan for new or updated AudioUnit plug-ins'. The scan process may take a while, and you might be asked to allow permission for Curve to access folders that the plugins being scanned are using.
  - Then, if you intend to also use VST3 plugins with Curve, repeat the process by clicking 'Scan for new or updated VST3 plug-ins'.
  - Once the scan is complete, the dialog should show a list of plugins installed on your device, including both macOS system plugins (e.g. AUNBandEQ that is often used for headphone/speaker EQ calibration) and third-party plugins.
  - Close the dialog box (red cross at top left).
- Click on the menu bar icon again, select 'Audio settings'.
  - Select the Output and Input audio interfaces (see Loopback interfaces section below) and corresponding channels, and the desired sample rate and buffer latency.
  - On a modern Mac, 96000 Hz sample rate and 256 samples (2.7 ms latency) should work fine.
  - Close the dialog (red cross at top left).
- You can now create a preset. Click on the menu bar icon again, select 'Show Editor'.
  - You should see Audio Input and Audio Output blocks, each with some green dots that correspond to the channels you enabled in Audio settings.
  - Right click the background of the Editor window - the list of installed plugins will show. Select a plugin you want to use, and a corresponding block will appear with inputs at the top and outputs at the bottom. If the number of inputs and outputs is not what you want, right click the block and select 'Configure Audio I/O' to correct it.
  - Then, connect channel(s) on the audio input and output blocks to inputs and output on the plugin by clicking a green dot on one block and dragging a connecting line to a green dot on another block. A trivial example is shown below, where the two channels (L/R) of the audio input are sent to an AUNBandEQ block, and the two channels (L/R) of the EQ output are sent to the first two channels of the audio output. You can of course add additional plugin blocks and connect them together however you wish.
  - To open the editor of a plugin (e.g. to set the desired EQ in AUNBandEQ), simply double click on the plugin box. To save the preset, click the menu bar icon and select 'Save as preset'. It should default to the correct preset folder but it's good to double check (it should be ~/Library/Application Support/Curve/Presets/ where ~/Library is the user specific library at /Users/<username>/Library). Choose a suitable name and save the preset. The editor window can be hidden using red close (top left) or selecting 'Hide Editor' from the app menu.
  - If you want to modify a preset later, first load the preset, make the changes, 'Save as preset' and select the existing file name. Note that changes to presets are *not* saved unless you explicitly use 'Save as preset'. You can repeat the process to create multiple presets. You can rename presets by manually changing their filenames in the preset folder using Finder.
  - If you click on the menu bar icon again, you should now see the list of presets you have created - simply click on them to instantly switch between them. The currently selected preset has a check mark.
  - By switching between plugins, you can switch between different settings of the same plugin(s), or switch between completely different (combinations of) plugins, and/or switch between different audio interface channel routings. For example, you might have one preset that does EQ correction and sends audio to output channels connected to headphones, and another preset that does different EQ correction and sends audio to output channels connected to monitor speakers - see the screenshots below.
- If you want the app to automatically load each time you log on, simply add it to your macOS login items under System Settings -> General -> Login Items & Extensions -> Open at Login (click the + icon and select Curve).
  
<img width="216" height="338" alt="MainMenu" src="https://github.com/user-attachments/assets/50c915da-c8cc-412e-b178-23c2cbdefa8a" />
<img width="282" height="338" alt="AudioSettings" src="https://github.com/user-attachments/assets/09f7eace-a963-40a3-9709-a286135bab95" />
<br>
<img width="375" height="338" alt="Preset1" src="https://github.com/user-attachments/assets/812e36cc-6048-4d78-a5a0-c71b818b835f" />
<img width="375" height="338" alt="Preset2" src="https://github.com/user-attachments/assets/9ad02aaf-f49e-4aeb-9813-ca0b473519a4" />


### Loopback interfaces

In some use cases, it is desirable to redirect the audio output of an app (e.g. Logic Pro) or the macOS system output to the input of Curve, in order to apply audio plugins (e.g. EQ) to the audio before it is sent to real output devices (such as headphones or speakers).

Some audio interface vendors (such as RME) provide a native loopback feature to enable this. For example, using RME Totalmix's Loopback function, you can redirect a pair of output channels to a spare pair of input channels. You would then configure Curve to use those input channels as its inputs, and configure (some of) the 'software playback' channels as Curve's output channels. Finally, in Totalmix you would assign the software playback channel(s) to the desired hardware outputs.

For other audio interface vendors who do not provide such native functionality, you can use a software based loopback driver instead. A popular free option is [BlackHole](https://github.com/ExistentialAudio/BlackHole). For simplicity and efficiency, just install the 2Ch (2 channel) version unless you need more.
Once Blackhole is installed, a new interface will appear in macOS audio settings. You can configure your DAW and/or macOS system audio to output to Blackhole, instead of the real hardware outputs. Then, in Curve, you set Blackhole as the input audio interface, and set the real hardware output as the output audio interface.


### Troubleshooting macOS Security Warnings

In some cases (depending on the signer of the binary), macOS will show a security warning when you first try to run the app. The exact warning and solution depend on your version of macOS (including Sonoma/Tahoe) and your security settings.

**Scenario 1: You see a warning that the developer cannot be verified.**

If you see a dialog saying `"Curve.app" can’t be opened because the developer cannot be verified.`, you have two options:

*   **Option A (Recommended): Right-Click to Open**
    1.  Right-click (or hold `Control` and click) on the `Curve.app` icon and select **Open** from the menu.
    2.  A new dialog will appear that is similar to the first, but this time it includes an **Open** button. Click it.
    3.  This grants a permanent exception for the app, and you can open it normally from now on.

*   **Option B: Use System Settings**
    1.  Double-click `Curve.app` normally. You will see the warning and be unable to proceed. Click "Cancel".
    2.  Open **System Settings**.
    3.  Go to **Privacy & Security**.
    4.  Scroll down and you will see a message about `"Curve.app"` being blocked. Click the **Open Anyway** button.

**Scenario 2: You see an error that the app is "damaged".**

If you see a more severe error message saying `"Curve.app" is damaged and can’t be opened. You should move it to the Trash.`, this is macOS's strictest security check. To fix this, you must manually remove the "quarantine" attribute that your web browser attached to the file upon download.

1.  Move `Curve.app` to your `/Applications` folder.
2.  Open the `Terminal` application (you can find it in `/Applications/Utilities`).
3.  Run the following command:
    ```sh
    xattr -cr /Applications/Curve.app
    ```
4.  After running this command, the app will no longer be quarantined and should open without any warnings. This `xattr` command completely bypasses the security checks mentioned in Scenario 1.

### Uninstallation

It is sufficient to simply delete Curve app from /Applications (or drag it to Trash).
If you want to completely uninstall all traces of Curve, additionally do the following:
1. delete the file: ~/Library/Preferences/Curve.settings
2. delete the folder: ~/Library/Application Support/Curve (note - this contains the presets folder, so back it up first if you might need it later)
3. from terminal, run: tccutil reset Microphone com.thomasderham.curve (note - this will remove the grant of microphone permissions to Curve)

## Building from source code on macOS

Please note that this project is licensed under the AGPLv3, as it is derived from the JUCE framework. Ensure that your use of this code and any derivative works complies with the terms of this license and includes the necessary copyright notices.

The CMake build process is configured to produce a Universal 2 Binary for macOS by default.

This application is a customized version of the `AudioPluginHost` example provided with the JUCE framework. Many thanks to the JUCE developers for making this framework and the reference host application available under open source license.

### Prerequisites

Before you begin, ensure you have the following installed:

1.  **Xcode Command Line Tools**: If you don't have them, open `Terminal` and run:
    ```sh
    xcode-select --install
    ```

2.  **CMake**: The easiest way to install CMake is using [Homebrew](https://brew.sh/):
    ```sh
    brew install cmake
    ```

### Build Instructions

The project is built using CMake. It is not necessary to use Projucer or Xcode IDE. These instructions assume you are at the root of the repository.

1.  **Create a build directory:
    ```sh
    mkdir -p build
    ```
2.  **Configure the project and compile with CMake:** Run CMake from the new build directory
    ```sh
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release && cmake Curve
    ```
3.  **Code sign the application:** Even if you are only running the newly built binary locally, explicit code signing is essential to avoid various issues with macOS privacy permissions (e.g. persistent microphone permissions dialogs). The minimal requirement for ad-hoc code signing is to run:
    ```sh
    sudo codesign -fs - ./Curve_artefacts/Release/curve.app
    ```
4.  **Run the application:** You can run the app directly, but it is better to copy/move it to your computer's `/Applications` folder.

    To open it from the terminal:
    ```sh
    open ./Curve.app
    ```

