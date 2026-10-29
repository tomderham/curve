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

#pragma once

#include <JuceHeader.h>

//==============================================================================
struct InternalParameterOptions
{
    juce::String label;
    std::function<juce::String (float)> valueToText;
    std::function<float (const juce::String&)> textToValue;
    bool isBoolean = false;
    bool isDiscrete = false;    // a choice between steps, e.g. modes
    bool isAutomatable = true;
};

//==============================================================================
/**
    A parameter for Curve's internal nodes.

    Internal nodes are AudioPluginInstances, which may only hold
    HostedAudioProcessorParameters, so JUCE's AudioParameterFloat/Bool (and the
    attachments built on them) can't be used. Values are snapped to the range's
    interval; get() returns the real-world value.
*/
class InternalParameter final : public juce::HostedAudioProcessorParameter
{
public:
    using Options = InternalParameterOptions;

    InternalParameter (const juce::String& parameterID, const juce::String& parameterName,
                       juce::NormalisableRange<float> valueRange, float defaultValueToUse, Options optionsToUse = {})
        : paramID (parameterID), name (parameterName), range (valueRange),
          defaultValue (range.convertTo0to1 (defaultValueToUse)), options (std::move (optionsToUse)),
          value (defaultValue)
    {
    }

    static std::unique_ptr<InternalParameter> makeBool (const juce::String& parameterID, const juce::String& parameterName,
                                                        bool defaultValueToUse, bool automatable = true)
    {
        Options o;
        o.isBoolean = true;
        o.isAutomatable = automatable;
        o.valueToText = [] (float v) { return v >= 0.5f ? juce::String ("On") : juce::String ("Off"); };
        o.textToValue = [] (const juce::String& t)
        {
            return (t.equalsIgnoreCase ("on") || t.equalsIgnoreCase ("true") || t.getIntValue() != 0) ? 1.0f : 0.0f;
        };

        return std::make_unique<InternalParameter> (parameterID, parameterName, juce::NormalisableRange<float> (0.0f, 1.0f, 1.0f),
                                                    defaultValueToUse ? 1.0f : 0.0f, std::move (o));
    }

    /** A choice between named options; get() returns the option's index. */
    static std::unique_ptr<InternalParameter> makeChoice (const juce::String& parameterID, const juce::String& parameterName,
                                                          const juce::StringArray& choices, int defaultIndex)
    {
        Options o;
        o.isDiscrete = true;
        o.valueToText = [choices] (float v) { return choices[juce::roundToInt (v)]; };
        o.textToValue = [choices] (const juce::String& t)
        {
            const auto index = choices.indexOf (t, true);
            return (float) (index >= 0 ? index : t.getIntValue());
        };

        return std::make_unique<InternalParameter> (parameterID, parameterName,
                                                    juce::NormalisableRange<float> (0.0f, (float) juce::jmax (1, choices.size() - 1), 1.0f),
                                                    (float) defaultIndex, std::move (o));
    }

    int getIndex() const noexcept   { return juce::roundToInt (get()); }

    //==============================================================================
    float get() const noexcept      { return range.convertFrom0to1 (value.load (std::memory_order_relaxed)); }
    bool isOn() const noexcept      { return get() >= 0.5f; }

    /** Sets the real-world value and notifies listeners and the host. */
    void set (float newValue)       { setValueNotifyingHost (range.convertTo0to1 (range.snapToLegalValue (newValue))); }

    const juce::NormalisableRange<float>& getRange() const noexcept { return range; }

    //==============================================================================
    float getValue() const override             { return value.load (std::memory_order_relaxed); }

    void setValue (float newValue) override
    {
        const auto snapped = range.snapToLegalValue (range.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, newValue)));
        value.store (range.convertTo0to1 (snapped), std::memory_order_relaxed);
    }

    float getDefaultValue() const override      { return defaultValue; }
    juce::String getName (int maximumLength) const override { return name.substring (0, maximumLength); }
    juce::String getLabel() const override      { return options.label; }
    juce::String getParameterID() const override { return paramID; }
    bool isBoolean() const override             { return options.isBoolean; }
    bool isDiscrete() const override            { return options.isBoolean || options.isDiscrete; }
    bool isAutomatable() const override         { return options.isAutomatable; }

    int getNumSteps() const override
    {
        if (range.interval > 0.0f)
            return juce::roundToInt ((range.end - range.start) / range.interval) + 1;

        return AudioProcessorParameter::getNumSteps();
    }

    juce::String getText (float normalisedValue, int maximumLength) const override
    {
        const auto v = range.convertFrom0to1 (normalisedValue);
        const auto text = options.valueToText != nullptr ? options.valueToText (v) : juce::String (v, 2);
        return text.substring (0, maximumLength);
    }

    float getValueForText (const juce::String& text) const override
    {
        const auto v = options.textToValue != nullptr ? options.textToValue (text) : text.getFloatValue();
        return range.convertTo0to1 (range.snapToLegalValue (v));
    }

private:
    const juce::String paramID, name;
    const juce::NormalisableRange<float> range;
    const float defaultValue;
    const Options options;
    std::atomic<float> value;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InternalParameter)
};

//==============================================================================
/**
    Keeps a Slider and an InternalParameter in sync both ways, reporting gestures
    to the host (which MIDI learn uses to pick the parameter).
*/
class InternalSliderBinding final : private juce::Slider::Listener,
                                    private juce::AudioProcessorParameter::Listener,
                                    private juce::AsyncUpdater
{
public:
    InternalSliderBinding (InternalParameter& p, juce::Slider& s)
        : parameter (p), slider (s)
    {
        const auto& range = parameter.getRange();
        slider.setNormalisableRange ({ (double) range.start, (double) range.end, (double) range.interval });
        slider.textFromValueFunction = [this] (double v) { return parameter.getText (parameter.getRange().convertTo0to1 ((float) v), 32); };
        slider.valueFromTextFunction = [this] (const juce::String& t) { return (double) parameter.getRange().convertFrom0to1 (parameter.getValueForText (t)); };
        slider.setValue (parameter.get(), juce::dontSendNotification);
        slider.updateText();

        slider.addListener (this);
        parameter.addListener (this);
    }

    ~InternalSliderBinding() override
    {
        cancelPendingUpdate();
        parameter.removeListener (this);
        slider.removeListener (this);
    }

private:
    void sliderDragStarted (juce::Slider*) override     { dragging = true;  parameter.beginChangeGesture(); }
    void sliderDragEnded (juce::Slider*) override       { dragging = false; parameter.endChangeGesture(); }

    void sliderValueChanged (juce::Slider*) override
    {
        if (updatingSlider)
            return;

        // Text entry and double-click reset aren't drags, so make them gestures of their own
        if (! dragging)
            parameter.beginChangeGesture();

        parameter.set ((float) slider.getValue());

        if (! dragging)
            parameter.endChangeGesture();
    }

    void parameterValueChanged (int, float) override    { triggerAsyncUpdate(); }
    void parameterGestureChanged (int, bool) override   {}

    void handleAsyncUpdate() override
    {
        const juce::ScopedValueSetter<bool> svs (updatingSlider, true);
        slider.setValue (parameter.get(), juce::dontSendNotification);
    }

    InternalParameter& parameter;
    juce::Slider& slider;
    bool dragging = false;
    bool updatingSlider = false;
};

/** Keeps a toggle Button and a boolean InternalParameter in sync both ways. */
class InternalButtonBinding final : private juce::AudioProcessorParameter::Listener,
                                    private juce::AsyncUpdater
{
public:
    InternalButtonBinding (InternalParameter& p, juce::Button& b)
        : parameter (p), button (b)
    {
        button.setClickingTogglesState (true);
        button.setToggleState (parameter.isOn(), juce::dontSendNotification);
        button.onClick = [this]
        {
            parameter.beginChangeGesture();
            parameter.set (button.getToggleState() ? 1.0f : 0.0f);
            parameter.endChangeGesture();
        };

        parameter.addListener (this);
    }

    ~InternalButtonBinding() override
    {
        cancelPendingUpdate();
        parameter.removeListener (this);
        button.onClick = nullptr;
    }

private:
    void parameterValueChanged (int, float) override    { triggerAsyncUpdate(); }
    void parameterGestureChanged (int, bool) override   {}

    void handleAsyncUpdate() override
    {
        button.setToggleState (parameter.isOn(), juce::dontSendNotification);
    }

    InternalParameter& parameter;
    juce::Button& button;
};

/**
    Keeps a ComboBox and a choice InternalParameter in sync both ways. Item IDs
    are the option index + 1. onParameterChanged runs on the message thread after
    the ComboBox has been updated, however the parameter was changed.
*/
class InternalComboBoxBinding final : private juce::AudioProcessorParameter::Listener,
                                      private juce::AsyncUpdater
{
public:
    InternalComboBoxBinding (InternalParameter& p, juce::ComboBox& c)
        : parameter (p), comboBox (c)
    {
        comboBox.setSelectedId (parameter.getIndex() + 1, juce::dontSendNotification);
        comboBox.onChange = [this]
        {
            if (updatingComboBox || comboBox.getSelectedId() == 0)
                return;

            parameter.beginChangeGesture();
            parameter.set ((float) (comboBox.getSelectedId() - 1));
            parameter.endChangeGesture();
        };

        parameter.addListener (this);
    }

    ~InternalComboBoxBinding() override
    {
        cancelPendingUpdate();
        parameter.removeListener (this);
        comboBox.onChange = nullptr;
    }

    std::function<void()> onParameterChanged;

private:
    void parameterValueChanged (int, float) override    { triggerAsyncUpdate(); }
    void parameterGestureChanged (int, bool) override   {}

    void handleAsyncUpdate() override
    {
        {
            const juce::ScopedValueSetter<bool> svs (updatingComboBox, true);
            comboBox.setSelectedId (parameter.getIndex() + 1, juce::dontSendNotification);
        }

        if (onParameterChanged != nullptr)
            onParameterChanged();
    }

    InternalParameter& parameter;
    juce::ComboBox& comboBox;
    bool updatingComboBox = false;
};
