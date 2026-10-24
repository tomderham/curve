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

#include "StereoUtilityNode.h"

//==============================================================================
class StereoUtilityEditor final : public juce::AudioProcessorEditor,
                                  private juce::Slider::Listener,
                                  private juce::ComboBox::Listener
{
public:
    explicit StereoUtilityEditor (StereoUtilityNode& owner)
        : AudioProcessorEditor (owner),
          stereoNode (owner)
    {
        setSize (260, 210);

        titleLabel.setText ("Stereo Utility", juce::dontSendNotification);
        titleLabel.setFont (juce::FontOptions (13.0f).withStyle ("Bold"));
        titleLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (titleLabel);

        modeBox.addItem ("Stereo", 1 + (int) StereoUtilityNode::Mode::stereo);
        modeBox.addItem ("Swap L/R", 1 + (int) StereoUtilityNode::Mode::swap);
        modeBox.addItem ("Mono (L+R)", 1 + (int) StereoUtilityNode::Mode::mono);
        modeBox.addItem ("Left only", 1 + (int) StereoUtilityNode::Mode::leftOnly);
        modeBox.addItem ("Right only", 1 + (int) StereoUtilityNode::Mode::rightOnly);
        modeBox.setSelectedId (1 + (int) stereoNode.getMode(), juce::dontSendNotification);
        modeBox.addListener (this);
        addAndMakeVisible (modeBox);

        widthSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        widthSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
        widthSlider.setRange (0.0, 200.0, 1.0);
        widthSlider.setTextValueSuffix (" % width");
        widthSlider.setValue (stereoNode.getWidth() * 100.0, juce::dontSendNotification);
        widthSlider.setDoubleClickReturnValue (true, 100.0);
        widthSlider.addListener (this);
        addAndMakeVisible (widthSlider);

        updateWidthEnablement();
    }

    ~StereoUtilityEditor() override
    {
        stereoNode.editorBeingDeleted (this);
        modeBox.removeListener (this);
        widthSlider.removeListener (this);
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
        modeBox.setBounds (getWidth() / 2 - 60, 38, 120, 24);
        widthSlider.setBounds (getWidth() / 2 - 55, 74, 110, 120);
    }

    void sliderValueChanged (juce::Slider* slider) override
    {
        if (slider == &widthSlider)
            stereoNode.setWidth ((float) (widthSlider.getValue() / 100.0));
    }

    void comboBoxChanged (juce::ComboBox* box) override
    {
        if (box == &modeBox)
        {
            stereoNode.setMode ((StereoUtilityNode::Mode) (modeBox.getSelectedId() - 1));
            updateWidthEnablement();
        }
    }

private:
    void updateWidthEnablement()
    {
        widthSlider.setEnabled (StereoUtilityNode::modeUsesWidth (stereoNode.getMode()));
    }

    StereoUtilityNode& stereoNode;
    juce::Label titleLabel;
    juce::ComboBox modeBox;
    juce::Slider widthSlider;
};

//==============================================================================
StereoUtilityNode::StereoUtilityNode()
    : AudioPluginInstance (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

StereoUtilityNode::~StereoUtilityNode() = default;

StereoUtilityNode::Matrix StereoUtilityNode::computeTargetMatrix() const
{
    // Width scales the side signal: L' = M + wS, R' = M - wS
    const float w = width.load (std::memory_order_relaxed);
    const float same = (1.0f + w) * 0.5f;
    const float cross = (1.0f - w) * 0.5f;

    switch (getMode())
    {
        case Mode::swap:      return { cross, same, same, cross };
        case Mode::mono:      return { 0.5f, 0.5f, 0.5f, 0.5f };
        case Mode::leftOnly:  return { 1.0f, 0.0f, 1.0f, 0.0f };
        case Mode::rightOnly: return { 0.0f, 1.0f, 0.0f, 1.0f };
        case Mode::stereo:    break;
    }

    return { same, cross, cross, same };
}

void StereoUtilityNode::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    const auto target = computeTargetMatrix();

    for (size_t i = 0; i < smoothedMatrix.size(); ++i)
    {
        smoothedMatrix[i].reset (sampleRate > 0.0 ? sampleRate : 96000.0, 0.02); // 20ms smoothing
        smoothedMatrix[i].setCurrentAndTargetValue (target[i]);
    }
}

void StereoUtilityNode::releaseResources()
{
}

void StereoUtilityNode::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (isSuspended())
        return;

    const int numSamples = buffer.getNumSamples();

    if (numSamples == 0 || buffer.getNumChannels() < 2)
        return;

    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);

    if (left == nullptr || right == nullptr)
        return;

    const auto target = computeTargetMatrix();
    bool smoothing = false;

    for (size_t i = 0; i < smoothedMatrix.size(); ++i)
    {
        if (! juce::exactlyEqual (smoothedMatrix[i].getTargetValue(), target[i]))
            smoothedMatrix[i].setTargetValue (target[i]);

        smoothing = smoothing || smoothedMatrix[i].isSmoothing();
    }

    if (smoothing)
    {
        for (int s = 0; s < numSamples; ++s)
        {
            const float l = left[s];
            const float r = right[s];
            left[s]  = smoothedMatrix[0].getNextValue() * l + smoothedMatrix[1].getNextValue() * r;
            right[s] = smoothedMatrix[2].getNextValue() * l + smoothedMatrix[3].getNextValue() * r;
        }

        return;
    }

    if (target == Matrix { 1.0f, 0.0f, 0.0f, 1.0f })
        return;

    for (int s = 0; s < numSamples; ++s)
    {
        const float l = left[s];
        const float r = right[s];
        left[s]  = target[0] * l + target[1] * r;
        right[s] = target[2] * l + target[3] * r;
    }
}

void StereoUtilityNode::setMode (Mode newMode)
{
    mode.store ((int) newMode, std::memory_order_relaxed);
}

void StereoUtilityNode::setWidth (float newWidth)
{
    width.store (juce::jlimit (0.0f, 2.0f, newWidth), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* StereoUtilityNode::createEditor()
{
    return new StereoUtilityEditor (*this);
}

bool StereoUtilityNode::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void StereoUtilityNode::fillInPluginDescription (juce::PluginDescription& description) const
{
    description.name = getName();
    description.descriptiveName = getName();
    description.pluginFormatName = "Internal";
    description.category = "Plugins";
    description.fileOrIdentifier = "StereoUtility";
    description.uniqueId = 0x53545554; // "STUT"
    description.isInstrument = false;
    description.numInputChannels = getMainBusNumInputChannels();
    description.numOutputChannels = getMainBusNumOutputChannels();
}

void StereoUtilityNode::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement xml ("STEREO_UTILITY_SETTINGS");
    xml.setAttribute ("mode", mode.load (std::memory_order_relaxed));
    xml.setAttribute ("width", (double) width.load (std::memory_order_relaxed));
    copyXmlToBinary (xml, destData);
}

void StereoUtilityNode::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName ("STEREO_UTILITY_SETTINGS"))
        {
            setMode ((Mode) juce::jlimit (0, 4, xml->getIntAttribute ("mode", (int) Mode::stereo)));
            setWidth ((float) xml->getDoubleAttribute ("width", 1.0));
        }
    }
}
