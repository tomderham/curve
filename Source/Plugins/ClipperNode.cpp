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

#include "ClipperNode.h"

//==============================================================================
class ClipperEditor final : public juce::AudioProcessorEditor,
                            private juce::Slider::Listener,
                            private juce::Timer
{
public:
    explicit ClipperEditor (ClipperNode& owner)
        : AudioProcessorEditor (owner),
          clipperNode (owner)
    {
        setSize (260, 210);

        titleLabel.setText ("Clipper", juce::dontSendNotification);
        titleLabel.setFont (juce::FontOptions (13.0f).withStyle ("Bold"));
        titleLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (titleLabel);

        ceilingSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        ceilingSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
        ceilingSlider.setRange (ClipperNode::minCeilingDb, ClipperNode::maxCeilingDb, 0.1);
        ceilingSlider.setTextValueSuffix (" dBFS");
        ceilingSlider.setValue (clipperNode.getCeilingDb(), juce::dontSendNotification);
        ceilingSlider.setDoubleClickReturnValue (true, -1.0);
        ceilingSlider.addListener (this);
        addAndMakeVisible (ceilingSlider);

        infoLabel.setText ("Hard clip at ceiling, 0 latency.\nOnly protects when set below 0 dBFS.",
                           juce::dontSendNotification);
        infoLabel.setFont (juce::FontOptions (11.0f));
        infoLabel.setJustificationType (juce::Justification::centred);
        infoLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
        addAndMakeVisible (infoLabel);

        startTimerHz (30);
    }

    ~ClipperEditor() override
    {
        stopTimer();
        clipperNode.editorBeingDeleted (this);
        ceilingSlider.removeListener (this);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff18181c));

        g.setColour (juce::Colour (0xff2a2a32));
        g.drawRect (getLocalBounds().toFloat(), 1.0f);

        // Clip indicator
        const auto led = getClipIndicatorBounds();
        g.setColour (clipHoldTicks > 0 ? juce::Colours::red : juce::Colour (0xff3a1a1a));
        g.fillEllipse (led);
        g.setColour (juce::Colours::lightgrey);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText ("CLIP", led.translated (16.0f, 0.0f).withWidth (40.0f), juce::Justification::centredLeft);
    }

    void resized() override
    {
        titleLabel.setBounds (10, 10, getWidth() - 20, 20);
        ceilingSlider.setBounds (getWidth() / 2 - 55, 34, 110, 100);
        infoLabel.setBounds (10, 166, getWidth() - 20, 34);
    }

    void sliderValueChanged (juce::Slider* slider) override
    {
        if (slider == &ceilingSlider)
            clipperNode.setCeilingDb ((float) ceilingSlider.getValue());
    }

private:
    juce::Rectangle<float> getClipIndicatorBounds() const
    {
        return { (float) getWidth() / 2.0f - 22.0f, 144.0f, 12.0f, 12.0f };
    }

    void timerCallback() override
    {
        const auto wasLit = clipHoldTicks > 0;

        if (clipperNode.consumeClipFlag())
            clipHoldTicks = 30; // hold for ~1 second
        else if (clipHoldTicks > 0)
            --clipHoldTicks;

        if (wasLit != (clipHoldTicks > 0))
            repaint (getClipIndicatorBounds().getSmallestIntegerContainer());
    }

    ClipperNode& clipperNode;
    juce::Label titleLabel;
    juce::Slider ceilingSlider;
    juce::Label infoLabel;
    int clipHoldTicks = 0;
};

//==============================================================================
ClipperNode::ClipperNode()
    : AudioPluginInstance (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

ClipperNode::~ClipperNode() = default;

void ClipperNode::prepareToPlay (double /*sampleRate*/, int /*samplesPerBlock*/)
{
    clipped.store (false, std::memory_order_relaxed);
}

void ClipperNode::releaseResources()
{
}

void ClipperNode::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (isSuspended())
        return;

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    if (numSamples == 0 || numChannels == 0)
        return;

    const float ceiling = ceilingLinear.load (std::memory_order_relaxed);
    bool anyClipped = false;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        if (auto* channelData = buffer.getWritePointer (ch))
        {
            const auto range = juce::FloatVectorOperations::findMinAndMax (channelData, numSamples);

            if (range.getStart() < -ceiling || range.getEnd() > ceiling)
            {
                juce::FloatVectorOperations::clip (channelData, channelData, -ceiling, ceiling, numSamples);
                anyClipped = true;
            }
        }
    }

    if (anyClipped)
        clipped.store (true, std::memory_order_relaxed);
}

void ClipperNode::setCeilingDb (float newCeilingDb)
{
    const auto db = juce::jlimit (minCeilingDb, maxCeilingDb, newCeilingDb);
    ceilingDb.store (db, std::memory_order_relaxed);
    ceilingLinear.store (juce::Decibels::decibelsToGain (db), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* ClipperNode::createEditor()
{
    return new ClipperEditor (*this);
}

bool ClipperNode::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto inSet  = layouts.getMainInputChannelSet();
    const auto outSet = layouts.getMainOutputChannelSet();

    if (inSet.isDisabled() || inSet != outSet)
        return false;

    return inSet.size() > 0;
}

void ClipperNode::fillInPluginDescription (juce::PluginDescription& description) const
{
    description.name = getName();
    description.descriptiveName = getName();
    description.pluginFormatName = "Internal";
    description.category = "Plugins";
    description.fileOrIdentifier = "Clipper";
    description.uniqueId = 0x434C4950; // "CLIP"
    description.isInstrument = false;
    description.numInputChannels = getMainBusNumInputChannels();
    description.numOutputChannels = getMainBusNumOutputChannels();
}

void ClipperNode::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement xml ("CLIPPER_SETTINGS");
    xml.setAttribute ("ceilingDb", (double) ceilingDb.load (std::memory_order_relaxed));
    copyXmlToBinary (xml, destData);
}

void ClipperNode::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName ("CLIPPER_SETTINGS"))
            setCeilingDb ((float) xml->getDoubleAttribute ("ceilingDb", -1.0));
    }
}
