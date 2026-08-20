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

#include "SpeakerEmulationNode.h"
#include "SpeakerEmulationData.h"

//==============================================================================
SpeakerEmulationNode::SpeakerEmulationNode()
    : AudioPluginInstance (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

SpeakerEmulationNode::~SpeakerEmulationNode() = default;

//==============================================================================
bool SpeakerEmulationNode::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void SpeakerEmulationNode::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = 2;

    leftConvolution.reset();
    leftConvolution.loadImpulseResponse (
        SpeakerEmulationData::leftWavData,
        SpeakerEmulationData::leftWavSize,
        juce::dsp::Convolution::Stereo::yes,
        juce::dsp::Convolution::Trim::no,
        0,
        juce::dsp::Convolution::Normalise::no
    );
    leftConvolution.prepare (spec);

    rightConvolution.reset();
    rightConvolution.loadImpulseResponse (
        SpeakerEmulationData::rightWavData,
        SpeakerEmulationData::rightWavSize,
        juce::dsp::Convolution::Stereo::yes,
        juce::dsp::Convolution::Trim::no,
        0,
        juce::dsp::Convolution::Normalise::no
    );
    rightConvolution.prepare (spec);

    leftBuffer.setSize (2, samplesPerBlock);
    rightBuffer.setSize (2, samplesPerBlock);
}

void SpeakerEmulationNode::releaseResources()
{
    leftConvolution.reset();
    rightConvolution.reset();
    leftBuffer.setSize (0, 0);
    rightBuffer.setSize (0, 0);
}

void SpeakerEmulationNode::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();
    const auto numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    if (totalNumInputChannels < 2 || totalNumOutputChannels < 2 || numSamples == 0)
        return;

    leftBuffer.setSize (2, numSamples, false, false, true);
    rightBuffer.setSize (2, numSamples, false, false, true);

    // Feed Input Left into both channels of leftBuffer (processes LL -> Ch0, LR -> Ch1)
    leftBuffer.copyFrom (0, 0, buffer, 0, 0, numSamples);
    leftBuffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

    // Feed Input Right into both channels of rightBuffer (processes RL -> Ch0, RR -> Ch1)
    rightBuffer.copyFrom (0, 0, buffer, 1, 0, numSamples);
    rightBuffer.copyFrom (1, 0, buffer, 1, 0, numSamples);

    // Process Left convolution: Ch0 = LL, Ch1 = LR
    juce::dsp::AudioBlock<float> leftBlock (leftBuffer);
    leftConvolution.process (juce::dsp::ProcessContextReplacing<float> (leftBlock));

    // Process Right convolution: Ch0 = RL, Ch1 = RR
    juce::dsp::AudioBlock<float> rightBlock (rightBuffer);
    rightConvolution.process (juce::dsp::ProcessContextReplacing<float> (rightBlock));

    // Sum true-stereo outputs: OutL = LL + RL, OutR = LR + RR
    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getWritePointer (1);
    const auto* lOutL = leftBuffer.getReadPointer (0);
    const auto* lOutR = leftBuffer.getReadPointer (1);
    const auto* rOutL = rightBuffer.getReadPointer (0);
    const auto* rOutR = rightBuffer.getReadPointer (1);

    for (int i = 0; i < numSamples; ++i)
    {
        outL[i] = lOutL[i] + rOutL[i];
        outR[i] = lOutR[i] + rOutR[i];
    }
}

void SpeakerEmulationNode::fillInPluginDescription (juce::PluginDescription& description) const
{
    description.name = getName();
    description.descriptiveName = getName();
    description.pluginFormatName = "Internal";
    description.category = "Plugins";
    description.manufacturerName = "Thomas Derham";
    description.version = "1.0";
    description.fileOrIdentifier = "SpeakerEmulation";
    description.uniqueId = 0x53504B52; // "SPKR"
    description.isInstrument = false;
    description.numInputChannels = 2;
    description.numOutputChannels = 2;
}
