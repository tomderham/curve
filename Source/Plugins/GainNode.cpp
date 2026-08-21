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
class GainEditor final : public juce::AudioProcessorEditor,
                         private juce::Slider::Listener,
                         private juce::Button::Listener
{
public:
    explicit GainEditor (GainNode& owner)
        : AudioProcessorEditor (owner),
          gainNode (owner)
    {
        setSize (260, 180);

        gainSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        gainSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
        gainSlider.setRange (-96.0, 24.0, 0.1);
        gainSlider.setTextValueSuffix (" dB");
        gainSlider.setValue (gainNode.getGainDb(), juce::dontSendNotification);
        gainSlider.setDoubleClickReturnValue (true, 0.0);
        gainSlider.addListener (this);
        addAndMakeVisible (gainSlider);

        resetButton.setButtonText ("0 dB");
        resetButton.onClick = [this]
        {
            gainSlider.setValue (0.0);
        };
        addAndMakeVisible (resetButton);

        muteButton.setButtonText ("Mute");
        muteButton.setClickingTogglesState (true);
        muteButton.setToggleState (gainNode.isMuted(), juce::dontSendNotification);
        muteButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::red.darker (0.2f));
        muteButton.addListener (this);
        addAndMakeVisible (muteButton);

        titleLabel.setText ("Gain Utility", juce::dontSendNotification);
        titleLabel.setFont (juce::FontOptions (13.0f).withStyle ("Bold"));
        titleLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (titleLabel);
    }

    ~GainEditor() override
    {
        gainNode.editorBeingDeleted (this);
        gainSlider.removeListener (this);
        muteButton.removeListener (this);
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
        resetButton.setBounds (getWidth() / 2 - 75, 140, 65, 26);
        muteButton.setBounds (getWidth() / 2 + 10, 140, 65, 26);
    }

    void sliderValueChanged (juce::Slider* slider) override
    {
        if (slider == &gainSlider)
            gainNode.setGainDb ((float) gainSlider.getValue());
    }

    void buttonClicked (juce::Button* button) override
    {
        if (button == &muteButton)
            gainNode.setMuted (muteButton.getToggleState());
    }

private:
    GainNode& gainNode;
    juce::Label titleLabel;
    juce::Slider gainSlider;
    juce::TextButton resetButton;
    juce::TextButton muteButton;
};

//==============================================================================
GainNode::GainNode()
    : AudioPluginInstance (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    updateTargetGain();
}

GainNode::~GainNode() = default;

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
        for (int s = 0; s < numSamples; ++s)
        {
            float g = smoothedGain.getNextValue();
            for (int ch = 0; ch < numChannels; ++ch)
            {
                if (auto* channelData = buffer.getWritePointer (ch))
                    channelData[s] *= g;
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

void GainNode::setGainDb (float newGainDb)
{
    gainDb.store (newGainDb, std::memory_order_relaxed);
    updateTargetGain();
}

void GainNode::setMuted (bool shouldMute)
{
    muted.store (shouldMute, std::memory_order_relaxed);
    updateTargetGain();
}

void GainNode::updateTargetGain()
{
    if (muted.load (std::memory_order_relaxed))
    {
        targetGainLinear.store (0.0f, std::memory_order_release);
    }
    else
    {
        float db = gainDb.load (std::memory_order_relaxed);
        float lin = (db <= -95.9f) ? 0.0f : juce::Decibels::decibelsToGain (db);
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
    xml.setAttribute ("gainDb", (double) gainDb.load (std::memory_order_relaxed));
    xml.setAttribute ("muted", muted.load (std::memory_order_relaxed));
    copyXmlToBinary (xml, destData);
}

void GainNode::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName ("GAIN_SETTINGS"))
        {
            float db = (float) xml->getDoubleAttribute ("gainDb", 0.0);
            bool m = xml->getBoolAttribute ("muted", false);
            setGainDb (db);
            setMuted (m);
        }
    }
}
