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

//==============================================================================
/** A MIDI message that a mapping responds to: a CC, a note or a program change. */
struct MidiTrigger
{
    enum class Type { controller, note, programChange };

    Type type = Type::controller;
    int channel = 1;    // 1..16, or 0 for any channel
    int number = 0;     // CC number, note number or program number

    /** Builds a trigger from a CC, note on/off or program change; nullopt for anything else. */
    static std::optional<MidiTrigger> fromMessage (const MidiMessage&);

    bool matches (const MidiMessage&) const;

    /** e.g. "CC 21 · Ch 1" */
    String describe() const;

    bool operator== (const MidiTrigger& other) const
    {
        return type == other.type && channel == other.channel && number == other.number;
    }

    bool operator!= (const MidiTrigger& other) const { return ! operator== (other); }
};

//==============================================================================
/** What a mapping controls. */
struct MidiMappingTarget
{
    enum class Kind { parameter, bypass, nextPreset, previousPreset };

    Kind kind = Kind::parameter;
    AudioProcessorGraph::NodeID nodeID;
    String paramID;         // preferred: stable across plugin versions
    int paramIndex = -1;    // fallback when the parameter has no ID

    static MidiMappingTarget forParameter (AudioProcessorGraph::NodeID, const AudioProcessor&, int paramIndex);
    static MidiMappingTarget forBypass (AudioProcessorGraph::NodeID);
    static MidiMappingTarget forPresetStep (bool next);

    bool isNodeTarget() const { return kind == Kind::parameter || kind == Kind::bypass; }

    /** Finds the target parameter on the given processor, by ID then by index. */
    AudioProcessorParameter* findParameter (AudioProcessor&) const;

    bool operator== (const MidiMappingTarget& other) const;
    bool operator!= (const MidiMappingTarget& other) const { return ! operator== (other); }
};

/** The ID a hosted plugin reports for a parameter, or empty if it has none. */
String getParameterIdentifier (AudioProcessorParameter&);

//==============================================================================
struct MidiMapping
{
    enum class Mode
    {
        absolute,   // CC 0..127 scales into min..max
        toggle,     // each press (any non-zero CC, note on, program change) flips the target
        follow      // the target is on while the control is (non-zero CC or note held)
    };

    MidiTrigger trigger;
    MidiMappingTarget target;
    Mode mode = Mode::absolute;
    float minValue = 0.0f;
    float maxValue = 1.0f;
    bool invert = false;

    /** The default mode for a newly learned trigger/target pair. */
    static Mode getDefaultMode (const MidiTrigger&, const MidiMappingTarget&);

    /** CC value, 127 for note on and program change, 0 for note off. */
    static int getControlValue (const MidiMessage&);
    static bool isPress (int controlValue)  { return controlValue > 0; }

    /** The normalised parameter value for an absolute control value. */
    float scale (int controlValue) const;

    std::unique_ptr<XmlElement> toXml() const;
    static std::optional<MidiMapping> fromXml (const XmlElement&);
};

//==============================================================================
/** A list of mappings in which each trigger and each target appear at most once. */
class MidiMappingSet
{
public:
    /** Adds the mapping, replacing any mapping with the same trigger or the same target. */
    void assign (const MidiMapping&);

    bool removeFor (const MidiMappingTarget&);
    bool removeForNode (AudioProcessorGraph::NodeID);
    void clear()                                    { mappings.clear(); }

    const MidiMapping* findFor (const MidiMappingTarget&) const;
    bool hasMappingsForNode (AudioProcessorGraph::NodeID) const;

    bool isEmpty() const                            { return mappings.empty(); }
    const std::vector<MidiMapping>& getMappings() const { return mappings; }

    static constexpr const char* xmlTag = "MIDIMAPPINGS";

    std::unique_ptr<XmlElement> toXml() const;
    void restoreFromXml (const XmlElement&);

private:
    std::vector<MidiMapping> mappings;
};
