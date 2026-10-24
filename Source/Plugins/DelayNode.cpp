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

#include "DelayNode.h"

namespace
{
    constexpr double speedOfSoundMetresPerSecond = 343.0;
    constexpr double defaultSampleRate = 48000.0;
}

//==============================================================================
class DelayEditor final : public juce::AudioProcessorEditor,
                          private juce::Slider::Listener,
                          private juce::ComboBox::Listener,
                          private juce::Timer
{
public:
    explicit DelayEditor (DelayNode& owner)
        : AudioProcessorEditor (owner),
          delayNode (owner)
    {
        setSize (260, 150);

        titleLabel.setText ("Delay", juce::dontSendNotification);
        titleLabel.setFont (juce::FontOptions (13.0f).withStyle ("Bold"));
        titleLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (titleLabel);

        // The slider always works in whole samples; the unit only changes how it is shown
        delaySlider.setSliderStyle (juce::Slider::LinearHorizontal);
        delaySlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 160, 20);
        delaySlider.setSkewFactor (0.3);
        updateRange();
        delaySlider.textFromValueFunction = [this] (double v) { return formatDelay ((int) v); };
        delaySlider.valueFromTextFunction = [this] (const juce::String& text) { return parseDelay (text); };
        delaySlider.setValue (delayNode.getDelaySamples(), juce::dontSendNotification);
        delaySlider.setDoubleClickReturnValue (true, 0.0);
        delaySlider.addListener (this);
        addAndMakeVisible (delaySlider);

        unitBox.addItem ("Samples", 1 + (int) DelayNode::DisplayUnit::samples);
        unitBox.addItem ("Milliseconds", 1 + (int) DelayNode::DisplayUnit::milliseconds);
        unitBox.addItem ("Centimeters", 1 + (int) DelayNode::DisplayUnit::centimeters);
        unitBox.setSelectedId (1 + (int) delayNode.getDisplayUnit(), juce::dontSendNotification);
        unitBox.addListener (this);
        addAndMakeVisible (unitBox);

        // The sample rate can change in Audio Settings while the editor is open
        startTimerHz (4);
    }

    ~DelayEditor() override
    {
        stopTimer();
        delayNode.editorBeingDeleted (this);
        delaySlider.removeListener (this);
        unitBox.removeListener (this);
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
        delaySlider.setBounds (16, 40, getWidth() - 32, 54);
        unitBox.setBounds (getWidth() / 2 - 60, 108, 120, 24);
    }

    void sliderValueChanged (juce::Slider* slider) override
    {
        if (slider == &delaySlider)
            delayNode.setDelaySamples ((int) delaySlider.getValue());
    }

    void comboBoxChanged (juce::ComboBox* box) override
    {
        if (box == &unitBox)
        {
            delayNode.setDisplayUnit ((DelayNode::DisplayUnit) (unitBox.getSelectedId() - 1));
            delaySlider.updateText();
        }
    }

private:
    void timerCallback() override
    {
        if (! juce::exactlyEqual (delayNode.getEffectiveSampleRate(), shownSampleRate))
            updateRange();
    }

    void updateRange()
    {
        shownSampleRate = delayNode.getEffectiveSampleRate();

        // Keep the node's setting even if it's beyond the new maximum; processing clamps it
        delaySlider.setRange (0.0, (double) delayNode.getMaxDelaySamples(), 1.0);
        delaySlider.setValue (delayNode.getDelaySamples(), juce::dontSendNotification);
        delaySlider.updateText();
    }

    juce::String formatDelay (int samples) const
    {
        const auto rate = delayNode.getEffectiveSampleRate();
        const auto ms = 1000.0 * samples / rate;

        switch (delayNode.getDisplayUnit())
        {
            case DelayNode::DisplayUnit::samples:
                return juce::String (samples) + " samples (" + juce::String (ms, 2) + " ms)";
            case DelayNode::DisplayUnit::centimeters:
                return juce::String (ms * speedOfSoundMetresPerSecond / 10.0, 1) + " cm (" + juce::String (samples) + " smp)";
            case DelayNode::DisplayUnit::milliseconds:
                break;
        }

        return juce::String (ms, 2) + " ms (" + juce::String (samples) + " smp)";
    }

    double parseDelay (const juce::String& text) const
    {
        const auto t = text.trim().toLowerCase();
        const auto value = t.getDoubleValue();
        const auto rate = delayNode.getEffectiveSampleRate();

        // A unit suffix straight after the number wins over the selected unit
        const auto suffix = t.trimCharactersAtStart ("0123456789.+- ");
        auto unit = delayNode.getDisplayUnit();
        if (suffix.startsWith ("ms"))
            unit = DelayNode::DisplayUnit::milliseconds;
        else if (suffix.startsWith ("cm"))
            unit = DelayNode::DisplayUnit::centimeters;
        else if (suffix.startsWith ("s"))
            unit = DelayNode::DisplayUnit::samples;

        switch (unit)
        {
            case DelayNode::DisplayUnit::samples:      return std::round (value);
            case DelayNode::DisplayUnit::milliseconds: return std::round (value * rate / 1000.0);
            case DelayNode::DisplayUnit::centimeters:  return std::round (value / 100.0 / speedOfSoundMetresPerSecond * rate);
        }

        return std::round (value);
    }

    DelayNode& delayNode;
    juce::Label titleLabel;
    juce::Slider delaySlider;
    juce::ComboBox unitBox;
    double shownSampleRate = 0.0;
};

//==============================================================================
DelayNode::DelayNode()
    : AudioPluginInstance (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

DelayNode::~DelayNode() = default;

double DelayNode::getEffectiveSampleRate() const
{
    const auto rate = getSampleRate();
    return rate > 0.0 ? rate : defaultSampleRate;
}

int DelayNode::getMaxDelaySamples() const
{
    return (int) std::ceil (maxDelaySeconds * getEffectiveSampleRate());
}

void DelayNode::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    const auto rate = sampleRate > 0.0 ? sampleRate : defaultSampleRate;
    const auto length = (int) std::ceil (maxDelaySeconds * rate) + 1;

    delayLine.setSize (juce::jmax (1, getTotalNumInputChannels()), length);
    delayLine.clear();
    writePosition = 0;
}

void DelayNode::releaseResources()
{
    delayLine.setSize (0, 0);
    writePosition = 0;
}

void DelayNode::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (isSuspended())
        return;

    const int numSamples = buffer.getNumSamples();
    const int length = delayLine.getNumSamples();
    const int numChannels = juce::jmin (buffer.getNumChannels(), delayLine.getNumChannels());

    if (numSamples == 0 || numChannels == 0 || length == 0)
        return;

    // Clamped here rather than stored, so a lower sample rate doesn't lose the setting.
    // The line keeps recording at any delay, so a change takes effect immediately.
    const int delay = juce::jlimit (0, length - 1, delaySamples.load (std::memory_order_relaxed));

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* io = buffer.getWritePointer (ch);
        auto* line = delayLine.getWritePointer (ch);

        if (io == nullptr || line == nullptr)
            continue;

        int writePos = writePosition;
        int readPos = writePos - delay;
        if (readPos < 0)
            readPos += length;

        for (int s = 0; s < numSamples; ++s)
        {
            line[writePos] = io[s];
            io[s] = line[readPos];

            if (++writePos == length)
                writePos = 0;
            if (++readPos == length)
                readPos = 0;
        }
    }

    writePosition = (writePosition + numSamples) % length;
}

void DelayNode::setDelaySamples (int newDelaySamples)
{
    delaySamples.store (juce::jmax (0, newDelaySamples), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* DelayNode::createEditor()
{
    return new DelayEditor (*this);
}

bool DelayNode::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto inSet  = layouts.getMainInputChannelSet();
    const auto outSet = layouts.getMainOutputChannelSet();

    if (inSet.isDisabled() || inSet != outSet)
        return false;

    return inSet.size() > 0;
}

void DelayNode::fillInPluginDescription (juce::PluginDescription& description) const
{
    description.name = getName();
    description.descriptiveName = getName();
    description.pluginFormatName = "Internal";
    description.category = "Plugins";
    description.fileOrIdentifier = "Delay";
    description.uniqueId = 0x444C4159; // "DLAY"
    description.isInstrument = false;
    description.numInputChannels = getMainBusNumInputChannels();
    description.numOutputChannels = getMainBusNumOutputChannels();
}

void DelayNode::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement xml ("DELAY_SETTINGS");
    xml.setAttribute ("delaySamples", delaySamples.load (std::memory_order_relaxed));
    xml.setAttribute ("displayUnit", displayUnit.load (std::memory_order_relaxed));
    copyXmlToBinary (xml, destData);
}

void DelayNode::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName ("DELAY_SETTINGS"))
        {
            setDelaySamples (xml->getIntAttribute ("delaySamples", 0));
            setDisplayUnit ((DisplayUnit) juce::jlimit (0, 2, xml->getIntAttribute ("displayUnit", (int) DisplayUnit::milliseconds)));
        }
    }
}
