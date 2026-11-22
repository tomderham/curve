/*
==============================================================================
   Copyright (c) Thomas Derham

   You may also use this code under the terms of the AGPLv3:
   https://www.gnu.org/licenses/agpl-3.0.en.html

   CURVE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
   WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING WARRANTY OF
   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, ARE DISCLAIMED.
==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Plugins/PluginGraph.h"

//==============================================================================
/** The MIDI Mappings window, from the editor menu's MIDI Mappings for Current Preset...: the loaded preset's mappings, with each
    one's mode, the range a control covers (in the parameter's own units, e.g. dB), and invert.

    Editing a mapping assigns it again, which marks the preset changed and updates the MIDI engine,
    as MIDI Learn does. It follows the loaded preset, and mappings made with MIDI Learn while it's
    open appear straight away.
*/
class MidiMappingsComponent final : public juce::Component,
                                    private juce::Timer
{
public:
    explicit MidiMappingsComponent (PluginGraph& g) : graph (g)
    {
        for (auto* header : { &controlHeader, &targetHeader, &modeHeader, &minHeader, &maxHeader, &invertHeader })
        {
            header->setFont (juce::FontOptions (12.0f).withStyle ("Bold"));
            header->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
            addAndMakeVisible (header);
        }

        controlHeader.setText ("Control", juce::dontSendNotification);
        targetHeader.setText ("Controls", juce::dontSendNotification);
        modeHeader.setText ("Mode", juce::dontSendNotification);
        minHeader.setText ("From", juce::dontSendNotification);
        maxHeader.setText ("To", juce::dontSendNotification);
        invertHeader.setText ("Invert", juce::dontSendNotification);

        emptyLabel.setText ("This preset has no MIDI mappings. To make one, click the menu icon at the top left of the editor and select MIDI Learn.",
                            juce::dontSendNotification);
        emptyLabel.setJustificationType (juce::Justification::centred);
        emptyLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
        addChildComponent (emptyLabel);

        footer.setText ("A knob or fader moves its parameter between From and To. For toggles and buttons, they're "
                        "the off and on values. Type a value in the parameter's units, or a percentage such as 25%. "
                        "Mappings are saved with the preset.",
                        juce::dontSendNotification);
        footer.setFont (juce::FontOptions (12.0f));
        footer.setJustificationType (juce::Justification::topLeft);
        footer.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
        addAndMakeVisible (footer);

        viewport.setViewedComponent (&rowsHolder, false);
        viewport.setScrollBarsShown (true, false);
        addAndMakeVisible (viewport);

        rebuild();
        setSize (760, 360);
        startTimer (250);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (16, 12);
        footer.setBounds (r.removeFromBottom (36));
        r.removeFromBottom (8);

        layOutColumns (r.removeFromTop (20), controlHeader, targetHeader, modeHeader, minHeader, maxHeader, invertHeader, nullptr);
        r.removeFromTop (4);

        viewport.setBounds (r);
        emptyLabel.setBounds (r);

        const int width = viewport.getMaximumVisibleWidth();
        rowsHolder.setSize (width, (int) rows.size() * rowHeight);

        for (size_t i = 0; i < rows.size(); ++i)
        {
            auto& row = *rows[i];
            layOutColumns ({ 0, (int) i * rowHeight, width, rowHeight }, row.control, row.target, row.mode,
                           row.minValue, row.maxValue, row.invert, &row.remove);
        }
    }

private:
    static constexpr int rowHeight = 30;
    static constexpr int rangeId = 1, toggleId = 2, followId = 3;

    struct Row
    {
        MidiMapping mapping;
        juce::Label control, target, minValue, maxValue;
        juce::ComboBox mode;
        juce::ToggleButton invert;
        juce::TextButton remove { juce::String::fromUTF8 ("\xc3\x97") };
    };

    PluginGraph& graph;
    juce::Label controlHeader, targetHeader, modeHeader, minHeader, maxHeader, invertHeader, emptyLabel, footer;
    juce::Viewport viewport;
    juce::Component rowsHolder;
    std::vector<std::unique_ptr<Row>> rows;
    juce::String shownMappings;

    static void layOutColumns (juce::Rectangle<int> r, juce::Component& control, juce::Component& target, juce::Component& mode,
                               juce::Component& minValue, juce::Component& maxValue, juce::Component& invert, juce::Component* remove)
    {
        r = r.reduced (0, 3);
        auto removeArea = r.removeFromRight (28);
        if (remove != nullptr)
            remove->setBounds (removeArea);

        control.setBounds (r.removeFromLeft (130));
        invert.setBounds (r.removeFromRight (56));
        maxValue.setBounds (r.removeFromRight (96).reduced (2, 0));
        minValue.setBounds (r.removeFromRight (96).reduced (2, 0));
        mode.setBounds (r.removeFromRight (100).reduced (2, 0));
        target.setBounds (r);
    }

    // Everything the rows show: also each node's name, and whether its parameter is found, which
    // it isn't until a plug-in that loads asynchronously (AUv3) has loaded
    juce::String describeMappings() const
    {
        auto text = graph.getMidiMappings().toXml()->toString (juce::XmlElement::TextFormat().singleLine().withoutHeader())
                  + graph.getFile().getFullPathName();

        for (const auto& m : graph.getMidiMappings().getMappings())
            if (m.target.isNodeTarget())
                text << "\n" << graph.getNodeDisplayName (m.target.nodeID) << (findParameter (m) != nullptr ? "+" : "-");

        return text;
    }

    // Follows the loaded preset, and mappings made or removed elsewhere, but never rebuilds under
    // a value being typed
    void timerCallback() override
    {
        // Catches up when its window is shown again
        if (! isShowing())
            return;

        const auto current = describeMappings();
        if (current == shownMappings)
            return;

        for (auto& row : rows)
            if (row->minValue.isBeingEdited() || row->maxValue.isBeingEdited())
                return;

        rebuild();
    }

    // Names the preset whose mappings these are. Also when first put in its window, after the first rebuild
    void parentHierarchyChanged() override  { updateWindowTitle(); }

    void updateWindowTitle()
    {
        if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
            window->setName ("MIDI Mappings: " + (graph.getFile() == juce::File() ? juce::String ("Untitled")
                                                                                 : graph.getFile().getFileNameWithoutExtension()));
    }

    juce::AudioProcessorParameter* findParameter (const MidiMapping& m) const
    {
        if (m.target.kind != MidiMappingTarget::Kind::parameter)
            return nullptr;

        auto* node = graph.graph.getNodeForId (m.target.nodeID);
        auto* processor = node != nullptr ? node->getProcessor() : nullptr;
        return processor != nullptr ? m.target.findParameter (*processor) : nullptr;
    }

    static juce::String describeValue (juce::AudioProcessorParameter& param, float normalised)
    {
        auto text = param.getText (normalised, 64).trim();
        const auto label = param.getLabel().trim();

        // Unless the text has it already, e.g. "100 % width"
        if (label.isNotEmpty() && ! text.endsWithIgnoreCase (label)
            && ! juce::StringArray::fromTokens (text, " ", {}).contains (label, true))
            text << " " << label;

        return text;
    }

    // A percentage is of the parameter's range, for plug-ins that can't read their values back from
    // text. Not for a parameter in %, such as a 0-200% width, which reads its own
    static float parseValue (juce::AudioProcessorParameter& param, const juce::String& text)
    {
        const auto trimmed = text.trim();

        if (trimmed.endsWithChar ('%') && ! param.getLabel().contains ("%"))
            return juce::jlimit (0.0f, 1.0f, trimmed.dropLastCharacters (1).trim().getFloatValue() / 100.0f);

        return juce::jlimit (0.0f, 1.0f, param.getValueForText (trimmed));
    }

    void rebuild()
    {
        shownMappings = describeMappings();
        rows.clear();

        for (const auto& m : graph.getMidiMappings().getMappings())
            if (m.target.isNodeTarget() || m.target.isHostTarget())
                addRow (m);

        updateWindowTitle();
        emptyLabel.setVisible (rows.empty());
        resized();
    }

    void addRow (const MidiMapping& m)
    {
        auto row = std::make_unique<Row>();
        auto* r = row.get();
        r->mapping = m;

        r->control.setText (m.trigger.describe(), juce::dontSendNotification);
        const auto target = m.target.isNodeTarget() ? graph.getNodeDisplayName (m.target.nodeID) + juce::String::fromUTF8 (" \xe2\x80\xba ")
                                                          + graph.describeMidiMappingTarget (m.target)
                                                    : graph.describeMidiMappingTarget (m.target);
        r->target.setText (target, juce::dontSendNotification);
        r->target.setTooltip (target);

        const bool isParameter = hasRange (m);

        // Bypass has no range: a press toggles it, or it follows the button
        if (isParameter)
            r->mode.addItem ("Range", rangeId);
        r->mode.addItem ("Toggle", toggleId);
        r->mode.addItem ("Follow", followId);
        r->mode.setTooltip (m.target.kind == MidiMappingTarget::Kind::tapTempo
                              ? "Toggle: taps on each press, for a button that springs back. "
                                "Follow: taps on each change, for a button that stays on or off until pressed again."
                              : "Range: a knob or fader sets the value. Toggle: each press switches between From and To. "
                                "Follow: To while the button is held or on, From when it's released or off.");
        r->mode.setSelectedId (m.mode == MidiMapping::Mode::absolute ? rangeId
                             : m.mode == MidiMapping::Mode::toggle   ? toggleId : followId,
                               juce::dontSendNotification);
        r->mode.onChange = [this, r]
        {
            auto updated = r->mapping;
            const auto id = r->mode.getSelectedId();
            updated.mode = id == rangeId ? MidiMapping::Mode::absolute
                         : id == toggleId ? MidiMapping::Mode::toggle : MidiMapping::Mode::follow;
            assign (*r, updated);
        };

        for (auto* valueLabel : { &r->minValue, &r->maxValue })
        {
            const bool isMin = valueLabel == &r->minValue;
            valueLabel->setEditable (isParameter);
            valueLabel->setColour (juce::Label::outlineColourId, juce::Colours::white.withAlpha (isParameter ? 0.2f : 0.0f));
            valueLabel->setText (isParameter ? describeRangeValue (m, isMin ? m.minValue : m.maxValue)
                                             : juce::String::fromUTF8 ("\xe2\x80\x94"),
                                 juce::dontSendNotification);

            valueLabel->onTextChange = [this, r, isMin]
            {
                if (! hasRange (r->mapping))
                    return;

                auto updated = r->mapping;
                const auto value = parseRangeValue (r->mapping, (isMin ? r->minValue : r->maxValue).getText());
                (isMin ? updated.minValue : updated.maxValue) = value;
                assign (*r, updated);

                // As the parameter shows it, e.g. snapped to its steps
                (isMin ? r->minValue : r->maxValue).setText (describeRangeValue (r->mapping, value), juce::dontSendNotification);
            };
        }

        r->invert.setToggleState (m.invert, juce::dontSendNotification);
        r->invert.setTooltip ("Reverses the control: From at the top of its travel, or when a button is pressed.");
        r->invert.onClick = [this, r]
        {
            auto updated = r->mapping;
            updated.invert = r->invert.getToggleState();
            assign (*r, updated);
        };

        r->remove.setTooltip ("Remove this mapping");
        r->remove.onClick = [this, r]
        {
            // Rebuilds from the timer, which deletes this row
            graph.removeMidiMappingFor (r->mapping.target);
        };

        updateEnablement (*r, isParameter);

        for (juce::Component* c : { (juce::Component*) &r->control, (juce::Component*) &r->target, (juce::Component*) &r->mode,
                                    (juce::Component*) &r->minValue, (juce::Component*) &r->maxValue,
                                    (juce::Component*) &r->invert, (juce::Component*) &r->remove })
            rowsHolder.addAndMakeVisible (c);

        rows.push_back (std::move (row));
    }

    // Invert has no effect on a toggle, or on a bypass toggled by each press
    static void updateEnablement (Row& r, bool isParameter)
    {
        r.invert.setEnabled (r.mapping.mode != MidiMapping::Mode::toggle && r.mapping.target.kind != MidiMappingTarget::Kind::tapTempo);
        r.minValue.setEnabled (isParameter);
        r.maxValue.setEnabled (isParameter);
    }

    void assign (Row& r, const MidiMapping& updated)
    {
        // Only absorb this change if nothing else changed since the last rebuild, such as a
        // mapping learned while a value was being typed, so the timer still rebuilds for that
        const bool wasCurrent = describeMappings() == shownMappings;

        r.mapping = updated;
        graph.assignMidiMapping (updated);

        if (wasCurrent)
            shownMappings = describeMappings(); // already showing it
        updateEnablement (r, hasRange (updated));
    }

    // A parameter's range, or the tempo's, which a knob sweeps between From and To
    bool hasRange (const MidiMapping& m) const
    {
        return findParameter (m) != nullptr || m.target.kind == MidiMappingTarget::Kind::tempo;
    }

    static double toTempo (float normalised)
    {
        return MidiMappingTarget::minMappedTempo + (MidiMappingTarget::maxMappedTempo - MidiMappingTarget::minMappedTempo) * normalised;
    }

    juce::String describeRangeValue (const MidiMapping& m, float normalised) const
    {
        if (m.target.kind == MidiMappingTarget::Kind::tempo)
            return juce::String (toTempo (normalised), 1).trimCharactersAtEnd ("0").trimCharactersAtEnd (".") + " BPM";

        if (auto* p = findParameter (m))
            return describeValue (*p, normalised);

        return {};
    }

    float parseRangeValue (const MidiMapping& m, const juce::String& text) const
    {
        if (m.target.kind == MidiMappingTarget::Kind::tempo)
        {
            // A percentage is of the range a knob sweeps, as for a parameter
            const auto trimmed = text.trim();

            if (trimmed.endsWithChar ('%'))
                return juce::jlimit (0.0f, 1.0f, trimmed.dropLastCharacters (1).trim().getFloatValue() / 100.0f);

            return (float) juce::jlimit (0.0, 1.0, (trimmed.getDoubleValue() - MidiMappingTarget::minMappedTempo)
                                                       / (MidiMappingTarget::maxMappedTempo - MidiMappingTarget::minMappedTempo));
        }

        if (auto* p = findParameter (m))
            return parseValue (*p, text);

        return 0.0f;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiMappingsComponent)
};

//==============================================================================
class MidiMappingsWindow final : public juce::DocumentWindow
{
public:
    explicit MidiMappingsWindow (PluginGraph& graph)
        : DocumentWindow ("MIDI Mappings",
                          juce::LookAndFeel::getDefaultLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId),
                          DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new MidiMappingsComponent (graph), true);
        setResizable (true, false);
        setResizeLimits (620, 240, 2000, 2000);
        centreWithSize (getWidth(), getHeight());
    }

    void closeButtonPressed() override { setVisible (false); }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiMappingsWindow)
};
