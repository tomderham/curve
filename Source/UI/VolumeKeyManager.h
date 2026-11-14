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

    Curve's volume applies only while Curve is taking the keys; otherwise the output is
    left at full level. All functions are for the message thread.
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

    bool isOverlayEnabled();
    void setOverlayEnabled (bool);

    /** True while the keys are on but Curve can't catch them, because it lacks Accessibility access. */
    bool needsAccessibilityAccess();
    void openAccessibilitySettings();

    /** True while the volume keys change Curve's output volume. */
    bool isControllingVolume();

    /** e.g. "-11.9 dB", "Muted" */
    juce::String describeVolume();

    /** The interface and channels (0-based) Curve outputs to, which automatic mode compares
        with macOS's output. */
    void setCurveOutput (const juce::String& deviceName, const juce::BigInteger& outputChannels);
}
