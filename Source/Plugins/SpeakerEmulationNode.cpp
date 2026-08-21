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
    if (sampleRate <= 0.0)
        sampleRate = 44100.0;

    const auto maxBlock = juce::jmax (1, samplesPerBlock);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) maxBlock;
    spec.numChannels = 2;

    if (std::abs (sampleRate - lastSampleRate) > 0.1)
    {
        lastSampleRate = sampleRate;

        leftConvolution.reset();
        rightConvolution.reset();

        const void* lData = nullptr;
        size_t lSize = 0;
        const void* rData = nullptr;
        size_t rSize = 0;

        if (sampleRate >= 88200.0)
        {
            lData = SpeakerEmulationData::leftWav_96kData;
            lSize = SpeakerEmulationData::leftWav_96kSize;
            rData = SpeakerEmulationData::rightWav_96kData;
            rSize = SpeakerEmulationData::rightWav_96kSize;
        }
        else if (sampleRate >= 46000.0)
        {
            lData = SpeakerEmulationData::leftWav_48kData;
            lSize = SpeakerEmulationData::leftWav_48kSize;
            rData = SpeakerEmulationData::rightWav_48kData;
            rSize = SpeakerEmulationData::rightWav_48kSize;
        }
        else
        {
            lData = SpeakerEmulationData::leftWav_44kData;
            lSize = SpeakerEmulationData::leftWav_44kSize;
            rData = SpeakerEmulationData::rightWav_44kData;
            rSize = SpeakerEmulationData::rightWav_44kSize;
        }

        leftConvolution.loadImpulseResponse (
            lData,
            lSize,
            juce::dsp::Convolution::Stereo::yes,
            juce::dsp::Convolution::Trim::no,
            0,
            juce::dsp::Convolution::Normalise::no
        );

        rightConvolution.loadImpulseResponse (
            rData,
            rSize,
            juce::dsp::Convolution::Stereo::yes,
            juce::dsp::Convolution::Trim::no,
            0,
            juce::dsp::Convolution::Normalise::no
        );
    }

    leftConvolution.prepare (spec);
    rightConvolution.prepare (spec);

    leftBuffer.setSize (2, maxBlock, false, true, false);
    rightBuffer.setSize (2, maxBlock, false, true, false);
}

void SpeakerEmulationNode::releaseResources()
{
    leftConvolution.reset();
    rightConvolution.reset();
    leftBuffer.setSize (0, 0);
    rightBuffer.setSize (0, 0);
    lastSampleRate = 0.0;
}

void SpeakerEmulationNode::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    if (isSuspended())
        return;

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    if (numSamples == 0 || numChannels < 2)
        return;

    const int maxCapacity = leftBuffer.getNumSamples();
    if (maxCapacity <= 0)
        return;

    int samplesProcessed = 0;

    while (samplesProcessed < numSamples)
    {
        const int chunkSize = (maxCapacity > 0) ? juce::jmin (numSamples - samplesProcessed, maxCapacity)
                                                : (numSamples - samplesProcessed);

        // Feed Input Left into both channels of leftBuffer (processes LL -> Ch0, LR -> Ch1)
        leftBuffer.copyFrom (0, 0, buffer, 0, samplesProcessed, chunkSize);
        leftBuffer.copyFrom (1, 0, buffer, 0, samplesProcessed, chunkSize);

        // Feed Input Right into both channels of rightBuffer (processes RL -> Ch0, RR -> Ch1)
        rightBuffer.copyFrom (0, 0, buffer, 1, samplesProcessed, chunkSize);
        rightBuffer.copyFrom (1, 0, buffer, 1, samplesProcessed, chunkSize);

        // Process Left convolution: Ch0 = LL, Ch1 = LR
        juce::dsp::AudioBlock<float> leftBlock (leftBuffer);
        auto leftSub = leftBlock.getSubBlock (0, (size_t) chunkSize);
        leftConvolution.process (juce::dsp::ProcessContextReplacing<float> (leftSub));

        // Process Right convolution: Ch0 = RL, Ch1 = RR
        juce::dsp::AudioBlock<float> rightBlock (rightBuffer);
        auto rightSub = rightBlock.getSubBlock (0, (size_t) chunkSize);
        rightConvolution.process (juce::dsp::ProcessContextReplacing<float> (rightSub));

        // Sum true-stereo outputs: OutL = LL + RL, OutR = LR + RR
        auto* outL = buffer.getWritePointer (0, samplesProcessed);
        auto* outR = buffer.getWritePointer (1, samplesProcessed);
        const auto* lOutL = leftBuffer.getReadPointer (0);
        const auto* lOutR = leftBuffer.getReadPointer (1);
        const auto* rOutL = rightBuffer.getReadPointer (0);
        const auto* rOutR = rightBuffer.getReadPointer (1);

        juce::FloatVectorOperations::copy (outL, lOutL, chunkSize);
        juce::FloatVectorOperations::add (outL, rOutL, chunkSize);
        juce::FloatVectorOperations::copy (outR, lOutR, chunkSize);
        juce::FloatVectorOperations::add (outR, rOutR, chunkSize);

        samplesProcessed += chunkSize;
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
