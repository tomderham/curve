/*
==============================================================================
   Copyright (c) Thomas Derham

   You may also use this code under the terms of the AGPLv3:
   https://www.gnu.org/licenses/agpl-3.0.en.html

   CURVE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
   WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING WARRANTY OF
   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, ARE DISCLAIMED.
==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
    Lets the keyboard's volume keys control Curve's output volume (OutputVolume),
    for interfaces such as a Focusrite Scarlett that have no volume control of their own.

    The keys are caught with a Quartz event tap, which needs Accessibility access. A caught
    key is consumed, so macOS doesn't also show its "volume not available" overlay; Curve
    shows its own overlay instead. In automatic mode Curve takes the keys only while macOS's
    output is Curve's output interface and macOS's volume doesn't reach the channels Curve
    plays through, so an interface whose volume macOS controls keeps the normal macOS
    behaviour and overlay.

    The volume applies either to Curve's whole output or only to the Interface Loopback (In)
    nodes, i.e. the audio macOS plays, leaving other inputs such as virtual instruments at a
    fixed level. In the second case Curve takes the keys only while the preset has a connected
    loopback node.

    Curve's volume applies while Curve is taking the keys, and also whenever its output interface
    has no volume macOS can set, as then it's the only volume there is (set from the menu bar
    panel); otherwise the audio is left at full level. All functions are for the message thread.
*/
namespace VolumeKeyManager
{
    enum class Mode
    {
        off = 0,
        automatic = 1, // only for interfaces without a volume control
        always = 2
    };

    /** Restores the settings and the saved volume, and starts the event tap if enabled. */
    void start();
    void stop();

    Mode getMode();

    /** Saves the mode. Turning the keys on asks for Accessibility access if Curve doesn't have it. */
    void setMode (Mode);

    enum class Target
    {
        output = 0,  // Curve's whole output
        loopback = 1 // only the Interface Loopback (In) nodes
    };

    Target getTarget();
    void setTarget (Target);

    /** Called when an Interface Loopback (In) node starts or stops being used by the graph. */
    void loopbackUsageChanged();

    bool isOverlayEnabled();
    void setOverlayEnabled (bool);

    /** True while the keys are on but Curve can't catch them, because it lacks Accessibility access. */
    bool needsAccessibilityAccess();
    void openAccessibilitySettings();

    /** True while the volume keys change Curve's volume. */
    bool isControllingVolume();

    /** True while the graph has an Interface Loopback (In) node that's connected and not bypassed. */
    bool hasConnectedLoopback();

    /** What the menu bar panel's volume slider sets: Curve's own volume while Curve takes the keys,
        or whenever its output interface has no volume macOS can set (when Curve's volume applies
        whether or not the keys are on); otherwise the interface's own volume, as macOS's volume
        menu sets it. None without an output interface. */
    enum class VolumeControl { none, curve, interface };
    VolumeControl getVolumeControl();

    /** The output interface's own volume, 0 to 1, for Curve's output channels. */
    float getInterfaceVolume();
    void setInterfaceVolume (float);

    /** The volume, in OutputVolume's steps, which the volume keys and the menu bar panel set. */
    int getVolumeSteps();
    bool isVolumeMuted();
    void setVolume (int steps, bool muted);

    /** e.g. "-11.9 dB", "Muted" */
    juce::String describeVolume();

    /** The interface and channels (0-based) Curve outputs to, which automatic mode compares
        with macOS's output. */
    void setCurveOutput (const juce::String& deviceName, const juce::BigInteger& outputChannels);
}
