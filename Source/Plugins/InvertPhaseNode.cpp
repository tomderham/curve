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

#include "InvertPhaseNode.h"

//==============================================================================
InvertPhaseNode::InvertPhaseNode()
    : AudioPluginInstance (BusesProperties()
                            .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                            .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

InvertPhaseNode::~InvertPhaseNode()
{
}

//==============================================================================
bool InvertPhaseNode::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto inSet  = layouts.getMainInputChannelSet();
    const auto outSet = layouts.getMainOutputChannelSet();

    if (inSet.isDisabled() || inSet != outSet)
        return false;

    return inSet.size() > 0;
}

void InvertPhaseNode::prepareToPlay (double /*sampleRate*/, int /*samplesPerBlock*/)
{
}

void InvertPhaseNode::releaseResources()
{
}

void InvertPhaseNode::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (isSuspended())
        return;

    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();

    if (numSamples == 0 || numChannels == 0)
        return;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        if (auto* channelData = buffer.getWritePointer (ch))
            juce::FloatVectorOperations::negate (channelData, channelData, numSamples);
    }
}

void InvertPhaseNode::fillInPluginDescription (juce::PluginDescription& description) const
{
    description.name = "Invert Phase";
    description.descriptiveName = "Invert Phase";
    description.pluginFormatName = "Internal";
    description.category = "Plugins";
    description.manufacturerName = "Curve";
    description.version = "1.0.0";
    description.fileOrIdentifier = "InvertPhase";
    description.uniqueId = 0x494E5650; // 'INVP'
    description.isInstrument = false;
    description.numInputChannels = getMainBusNumInputChannels();
    description.numOutputChannels = getMainBusNumOutputChannels();
    description.hasSharedContainer = false;
}
