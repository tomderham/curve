/*
==============================================================================
   Curve
   Copyright (c) Thomas Derham

   The modifications for Curve are licensed to you solely under the terms of the
   AGPLv3 license terms.

   CURVE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
   WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING WARRANTY OF
   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, ARE DISCLAIMED.
==============================================================================
*/

#include "MidSideNode.h"

//==============================================================================
MidSideNode::MidSideNode (Mode modeToUse)
    : AudioPluginInstance (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      mode (modeToUse)
{
}

MidSideNode::~MidSideNode() = default;

void MidSideNode::prepareToPlay (double /*sampleRate*/, int /*samplesPerBlock*/)
{
}

void MidSideNode::releaseResources()
{
}

void MidSideNode::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (isSuspended())
        return;

    const int numSamples = buffer.getNumSamples();

    if (numSamples == 0 || buffer.getNumChannels() < 2)
        return;

    auto* ch0 = buffer.getWritePointer (0);
    auto* ch1 = buffer.getWritePointer (1);

    if (ch0 == nullptr || ch1 == nullptr)
        return;

    // Encode and decode are the same sum/difference matrix, differing only in scale.
    // Summing in double keeps Decode(Encode(x)) bit-exact for sources up to 24-bit.
    const double scale = mode == Mode::decode ? 1.0 : 0.5;

    for (int s = 0; s < numSamples; ++s)
    {
        const double a = ch0[s];
        const double b = ch1[s];
        ch0[s] = (float) ((a + b) * scale);
        ch1[s] = (float) ((a - b) * scale);
    }
}

bool MidSideNode::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MidSideNode::fillInPluginDescription (juce::PluginDescription& description) const
{
    const bool isDecoder = mode == Mode::decode;

    description.name = getName();
    description.descriptiveName = getName();
    description.pluginFormatName = "Internal";
    description.category = "Plugins";
    description.fileOrIdentifier = isDecoder ? "MidSideDecoder" : "MidSideEncoder";
    description.uniqueId = isDecoder ? 0x4D534443 : 0x4D53454E; // "MSDC" : "MSEN"
    description.isInstrument = false;
    description.numInputChannels = getMainBusNumInputChannels();
    description.numOutputChannels = getMainBusNumOutputChannels();
}
