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
    // Primary accent: active/enabled states and hover highlights
    inline const juce::Colour accent { 0xff3b82f6 };

    // Window title bar button glyphs
    inline const juce::Colour titleBarGlyph { 0xffd4d4d4 };

    // Graph wires and pins
    inline const juce::Colour audioWire { 0xff8fa3ad };
    inline const juce::Colour midiWire  { 0xfff59e0b };

    // Graph node kinds, shown as a strip on the node's left edge
    inline const juce::Colour nodeInterface { 0xff2dd4bf };
    inline const juce::Colour nodeInternal  { 0xff3b82f6 };
    inline const juce::Colour nodeExternal  { 0xffa78bfa };
}
}
