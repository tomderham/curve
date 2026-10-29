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

#include "PanNode.h"

//==============================================================================
class PanEditor final : public juce::AudioProcessorEditor,
                        private juce::ComboBox::Listener
{
public:
    explicit PanEditor (PanNode& owner)
        : AudioProcessorEditor (owner),
          panNode (owner)
    {
        setSize (260, 200);

        titleLabel.setText (panNode.hasMonoInput() ? "Pan (mono to stereo)" : "Balance (stereo)",
                            juce::dontSendNotification);
        titleLabel.setFont (juce::FontOptions (13.0f).withStyle ("Bold"));
        titleLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (titleLabel);

        panSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        panSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
        panSlider.setDoubleClickReturnValue (true, 0.0);
        addAndMakeVisible (panSlider);

        lawLabel.setText ("Pan law", juce::dontSendNotification);
        lawLabel.setFont (juce::FontOptions (12.0f));
        lawLabel.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (lawLabel);

        lawBox.addItem ("0 dB", 1 + (int) PanNode::PanLaw::zeroDb);
        lawBox.addItem ("-3 dB", 1 + (int) PanNode::PanLaw::minus3Db);
        lawBox.addItem ("-4.5 dB", 1 + (int) PanNode::PanLaw::minus4p5Db);
        lawBox.addItem ("-6 dB", 1 + (int) PanNode::PanLaw::minus6Db);
        lawBox.setSelectedId (1 + (int) panNode.getPanLaw(), juce::dontSendNotification);
        lawBox.setEnabled (panNode.hasMonoInput());
        lawBox.addListener (this);
        addAndMakeVisible (lawBox);
    }

    ~PanEditor() override
    {
        panNode.editorBeingDeleted (this);
        lawBox.removeListener (this);
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
        panSlider.setBounds (getWidth() / 2 - 55, 34, 110, 100);
        lawLabel.setBounds (10, 152, getWidth() / 2 - 25, 24);
        lawBox.setBounds (getWidth() / 2 - 10, 152, 100, 24);
    }

    void comboBoxChanged (juce::ComboBox* box) override
    {
        if (box == &lawBox)
            panNode.setPanLaw ((PanNode::PanLaw) (lawBox.getSelectedId() - 1));
    }

private:
    PanNode& panNode;
    juce::Label titleLabel;
    juce::Slider panSlider;
    juce::Label lawLabel;
    juce::ComboBox lawBox;

    InternalSliderBinding panBinding { panNode.getPanParameter(), panSlider };
};

//==============================================================================
PanNode::PanNode()
    : AudioPluginInstance (BusesProperties().withInput ("Input", juce::AudioChannelSet::mono(), true)
                                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    InternalParameter::Options options;
    options.valueToText = [] (float v)
    {
        const auto amount = juce::roundToInt (std::abs (v));
        if (amount == 0)
            return juce::String ("C");

        return (v < 0.0f ? "L" : "R") + juce::String (amount);
    };
    options.textToValue = [] (const juce::String& text)
    {
        const auto t = text.trim().toUpperCase();
        const auto amount = t.retainCharacters ("0123456789.").getFloatValue();

        if (t.startsWith ("L"))
            return -amount;
        if (t.startsWith ("R"))
            return amount;

        return t.getFloatValue();
    };

    auto panParameter = std::make_unique<InternalParameter> ("pan", "Pan", juce::NormalisableRange<float> (-100.0f, 100.0f, 1.0f), 0.0f, options);
    panParam = panParameter.get();
    addHostedParameter (std::move (panParameter));
    panParam->addListener (this);

    updateTargetGains();
}

PanNode::~PanNode()
{
    panParam->removeListener (this);
}

void PanNode::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    monoInput.store (hasMonoInput(), std::memory_order_relaxed);
    updateTargetGains();

    const auto rate = sampleRate > 0.0 ? sampleRate : 96000.0;
    smoothedGainLeft.reset (rate, 0.02); // 20ms smoothing
    smoothedGainRight.reset (rate, 0.02);
    smoothedGainLeft.setCurrentAndTargetValue (targetGainLeft.load (std::memory_order_relaxed));
    smoothedGainRight.setCurrentAndTargetValue (targetGainRight.load (std::memory_order_relaxed));
}

void PanNode::releaseResources()
{
}

void PanNode::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (isSuspended())
        return;

    const int numSamples = buffer.getNumSamples();

    if (numSamples == 0 || buffer.getNumChannels() < 2)
        return;

    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getWritePointer (1);

    if (outL == nullptr || outR == nullptr)
        return;

    const float targetL = targetGainLeft.load (std::memory_order_acquire);
    const float targetR = targetGainRight.load (std::memory_order_acquire);

    if (! juce::exactlyEqual (smoothedGainLeft.getTargetValue(), targetL))
        smoothedGainLeft.setTargetValue (targetL);
    if (! juce::exactlyEqual (smoothedGainRight.getTargetValue(), targetR))
        smoothedGainRight.setTargetValue (targetR);

    // With a mono input, channel 0 holds the source and is fanned out to both outputs
    const bool mono = getTotalNumInputChannels() == 1;
    const float* inR = mono ? outL : outR;

    if (smoothedGainLeft.isSmoothing() || smoothedGainRight.isSmoothing())
    {
        for (int s = 0; s < numSamples; ++s)
        {
            const float l = outL[s];
            const float r = inR[s];
            outL[s] = l * smoothedGainLeft.getNextValue();
            outR[s] = r * smoothedGainRight.getNextValue();
        }

        return;
    }

    const float gainL = smoothedGainLeft.getCurrentValue();
    const float gainR = smoothedGainRight.getCurrentValue();

    if (mono)
        juce::FloatVectorOperations::multiply (outR, outL, gainR, numSamples);
    else if (gainR != 1.0f)
        juce::FloatVectorOperations::multiply (outR, gainR, numSamples);

    if (gainL != 1.0f)
        juce::FloatVectorOperations::multiply (outL, gainL, numSamples);
}

void PanNode::setPanLaw (PanLaw newLaw)
{
    panLaw.store ((int) newLaw, std::memory_order_relaxed);
    updateTargetGains();
}

void PanNode::updateTargetGains()
{
    const float p = getPan();

    // Balance: full level at centre, opposite side fades out
    float gainL = juce::jmin (1.0f, 1.0f - p);
    float gainR = juce::jmin (1.0f, 1.0f + p);

    if (monoInput.load (std::memory_order_relaxed))
    {
        const float x = (p + 1.0f) * 0.5f; // 0 = left, 1 = right
        const float angle = x * juce::MathConstants<float>::halfPi;
        const float powerL = juce::jmax (0.0f, std::cos (angle)); // cos (halfPi) is slightly negative in float
        const float powerR = juce::jmax (0.0f, std::sin (angle));
        const float linearL = 1.0f - x;
        const float linearR = x;

        switch (getPanLaw())
        {
            case PanLaw::zeroDb:     break;
            case PanLaw::minus3Db:   gainL = powerL; gainR = powerR; break;
            case PanLaw::minus4p5Db: gainL = std::sqrt (powerL * linearL); gainR = std::sqrt (powerR * linearR); break;
            case PanLaw::minus6Db:   gainL = linearL; gainR = linearR; break;
        }
    }

    targetGainLeft.store (gainL, std::memory_order_release);
    targetGainRight.store (gainR, std::memory_order_release);
}

juce::AudioProcessorEditor* PanNode::createEditor()
{
    return new PanEditor (*this);
}

bool PanNode::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto inSet  = layouts.getMainInputChannelSet();
    const auto outSet = layouts.getMainOutputChannelSet();

    if (outSet != juce::AudioChannelSet::stereo())
        return false;

    return inSet == juce::AudioChannelSet::mono() || inSet == juce::AudioChannelSet::stereo();
}

void PanNode::processorLayoutsChanged()
{
    // Switches between pan law (mono input) and balance (stereo input)
    monoInput.store (hasMonoInput(), std::memory_order_relaxed);
    updateTargetGains();
}

void PanNode::fillInPluginDescription (juce::PluginDescription& description) const
{
    description.name = getName();
    description.descriptiveName = getName();
    description.pluginFormatName = "Internal";
    description.category = "Plugins";
    description.fileOrIdentifier = "Pan";
    description.uniqueId = 0x50414E4E; // "PANN"
    description.isInstrument = false;
    description.numInputChannels = getMainBusNumInputChannels();
    description.numOutputChannels = getMainBusNumOutputChannels();
}

void PanNode::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement xml ("PAN_SETTINGS");
    xml.setAttribute ("pan", (double) getPan());
    xml.setAttribute ("panLaw", panLaw.load (std::memory_order_relaxed));
    copyXmlToBinary (xml, destData);
}

void PanNode::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName ("PAN_SETTINGS"))
        {
            panParam->set ((float) xml->getDoubleAttribute ("pan", 0.0) * 100.0f);
            setPanLaw ((PanLaw) juce::jlimit (0, 3, xml->getIntAttribute ("panLaw", (int) PanLaw::minus3Db)));
        }
    }
}
