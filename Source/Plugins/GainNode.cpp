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

#include "GainNode.h"

//==============================================================================
class GainEditor final : public juce::AudioProcessorEditor
{
public:
    explicit GainEditor (GainNode& owner)
        : AudioProcessorEditor (owner),
          gainNode (owner)
    {
        setSize (260, 180);

        gainSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        gainSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
        gainSlider.setDoubleClickReturnValue (true, 0.0);
        addAndMakeVisible (gainSlider);

        muteButton.setButtonText ("Mute");
        muteButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::red.darker (0.2f));
        addAndMakeVisible (muteButton);

        titleLabel.setText ("Gain Utility", juce::dontSendNotification);
        titleLabel.setFont (juce::FontOptions (13.0f).withStyle ("Bold"));
        titleLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (titleLabel);
    }

    ~GainEditor() override
    {
        gainNode.editorBeingDeleted (this);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff18181c));

        g.setColour (juce::Colour (0xff2a2a32));
        g.drawRect (getLocalBounds().toFloat(), 1.0f);
    }

    void resized() override
    {
        titleLabel.setBounds (10, 10, getWidth() - 20, 20);
        gainSlider.setBounds (getWidth() / 2 - 55, 34, 110, 100);
        muteButton.setBounds (getWidth() / 2 - 40, 140, 80, 26);
    }

private:
    GainNode& gainNode;
    juce::Label titleLabel;
    juce::Slider gainSlider;
    juce::TextButton muteButton;

    // Keep the controls and parameters in sync both ways (e.g. under MIDI control)
    InternalSliderBinding gainBinding { gainNode.getGainParameter(), gainSlider };
    InternalButtonBinding muteBinding { gainNode.getMuteParameter(), muteButton };
};

//==============================================================================
GainNode::GainNode()
    : AudioPluginInstance (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    InternalParameter::Options gainOptions;
    gainOptions.label = "dB";
    gainOptions.valueToText = [] (float v) { return juce::String (v, 1) + " dB"; };

    auto gain = std::make_unique<InternalParameter> ("gain", "Gain", juce::NormalisableRange<float> (-60.0f, 60.0f, 0.1f), 0.0f, gainOptions);
    gainParam = gain.get();
    addHostedParameter (std::move (gain));

    auto mute = InternalParameter::makeBool ("mute", "Mute", false);
    muteParam = mute.get();
    addHostedParameter (std::move (mute));

    gainParam->addListener (this);
    muteParam->addListener (this);
    updateTargetGain();
}

GainNode::~GainNode()
{
    gainParam->removeListener (this);
    muteParam->removeListener (this);
}

void GainNode::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    smoothedGain.reset (sampleRate > 0.0 ? sampleRate : 96000.0, 0.02); // 20ms smoothing
    updateTargetGain();
    smoothedGain.setCurrentAndTargetValue (targetGainLinear.load (std::memory_order_relaxed));
}

void GainNode::releaseResources()
{
}

void GainNode::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (isSuspended())
        return;

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    if (numSamples == 0 || numChannels == 0)
        return;

    float target = targetGainLinear.load (std::memory_order_acquire);
    if (std::abs (smoothedGain.getTargetValue() - target) > 0.0001f)
        smoothedGain.setTargetValue (target);

    if (smoothedGain.isSmoothing())
    {
        if (numChannels == 2 && buffer.getWritePointer (0) != nullptr && buffer.getWritePointer (1) != nullptr)
        {
            auto* out0 = buffer.getWritePointer (0);
            auto* out1 = buffer.getWritePointer (1);
            for (int s = 0; s < numSamples; ++s)
            {
                float g = smoothedGain.getNextValue();
                out0[s] *= g;
                out1[s] *= g;
            }
        }
        else
        {
            auto* const* channelPointers = buffer.getArrayOfWritePointers();
            for (int s = 0; s < numSamples; ++s)
            {
                float g = smoothedGain.getNextValue();
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    if (auto* channelData = channelPointers[ch])
                        channelData[s] *= g;
                }
            }
        }
    }
    else
    {
        float current = smoothedGain.getCurrentValue();
        if (current == 0.0f)
        {
            buffer.clear();
        }
        else if (current != 1.0f)
        {
            buffer.applyGain (current);
        }
    }
}

void GainNode::updateTargetGain()
{
    if (muteParam->isOn())
    {
        targetGainLinear.store (0.0f, std::memory_order_release);
    }
    else
    {
        float db = gainParam->get();
        float lin = juce::Decibels::decibelsToGain (db);
        targetGainLinear.store (lin, std::memory_order_release);
    }
}

juce::AudioProcessorEditor* GainNode::createEditor()
{
    return new GainEditor (*this);
}

bool GainNode::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto inSet  = layouts.getMainInputChannelSet();
    const auto outSet = layouts.getMainOutputChannelSet();

    if (inSet.isDisabled() || inSet != outSet)
        return false;

    return inSet.size() > 0;
}

void GainNode::fillInPluginDescription (juce::PluginDescription& description) const
{
    description.name = getName();
    description.descriptiveName = getName();
    description.pluginFormatName = "Internal";
    description.category = "Plugins";
    description.fileOrIdentifier = "Gain";
    description.uniqueId = 0x4741494E; // "GAIN"
    description.isInstrument = false;
    description.numInputChannels = getMainBusNumInputChannels();
    description.numOutputChannels = getMainBusNumOutputChannels();
}

void GainNode::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement xml ("GAIN_SETTINGS");
    xml.setAttribute ("gainDb", (double) gainParam->get());
    xml.setAttribute ("muted", muteParam->isOn());
    copyXmlToBinary (xml, destData);
}

void GainNode::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName ("GAIN_SETTINGS"))
        {
            gainParam->set ((float) xml->getDoubleAttribute ("gainDb", 0.0));
            muteParam->set (xml->getBoolAttribute ("muted", false) ? 1.0f : 0.0f);
        }
    }
}
