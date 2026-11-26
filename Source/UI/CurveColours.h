/*
==============================================================================
   Curve - UI Colours
   Shared accent and chrome colours so custom controls stay visually consistent.
==============================================================================
*/

#pragma once

#include <JuceHeader.h>

namespace Curve
{
namespace Colours
{
    // Surfaces, darkest first: window backgrounds, then panels, cards and lists on them, then
    // controls such as buttons, text fields and graph nodes. About as light as macOS's own dark windows
    inline const juce::Colour canvas  { 0xff24252a };
    inline const juce::Colour surface { 0xff2b2c32 };
    inline const juce::Colour control { 0xff35363d };
    inline const juce::Colour menu    { 0xff2e2f35 };

    // Edges of panels and controls
    inline const juce::Colour edge    { 0xff47484f };

    inline const juce::Colour text    { 0xffe8e8ec };
    inline const juce::Colour dimText { 0xff9a9ba5 }; // Secondary text, hints and units

    // Primary accent: active/enabled states and hover highlights
    inline const juce::Colour accent { 0xff3b82f6 };
    inline const juce::Colour link   { 0xff60a5fa };

    // Status
    inline const juce::Colour success { 0xff4ade80 };
    inline const juce::Colour warning { 0xfffbbf24 };
    inline const juce::Colour danger  { 0xffef4444 };

    // Graph wires and pins
    inline const juce::Colour audioWire { 0xff8fa3ad };
    inline const juce::Colour midiWire  { 0xfff59e0b };

    // Graph node kinds, shown as a strip on the node's left edge
    inline const juce::Colour nodeInterface { 0xff2dd4bf };
    inline const juce::Colour nodeInternal  { 0xff3b82f6 };
    inline const juce::Colour nodeExternal  { 0xffa78bfa };
}
}
