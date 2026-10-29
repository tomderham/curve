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

#include "MidiMapping.h"

//==============================================================================
std::optional<MidiTrigger> MidiTrigger::fromMessage (const MidiMessage& m)
{
    if (m.isController())
        return MidiTrigger { Type::controller, m.getChannel(), m.getControllerNumber() };

    if (m.isNoteOnOrOff())
        return MidiTrigger { Type::note, m.getChannel(), m.getNoteNumber() };

    if (m.isProgramChange())
        return MidiTrigger { Type::programChange, m.getChannel(), m.getProgramChangeNumber() };

    return std::nullopt;
}

bool MidiTrigger::matches (const MidiMessage& m) const
{
    if (channel != 0 && ! m.isForChannel (channel))
        return false;

    switch (type)
    {
        case Type::controller:      return m.isController()     && m.getControllerNumber() == number;
        case Type::note:            return m.isNoteOnOrOff()    && m.getNoteNumber() == number;
        case Type::programChange:   return m.isProgramChange()  && m.getProgramChangeNumber() == number;
    }

    return false;
}

String MidiTrigger::describe() const
{
    String s;

    switch (type)
    {
        case Type::controller:      s << "CC " << number; break;
        case Type::note:            s << "Note " << MidiMessage::getMidiNoteName (number, true, true, 3); break;
        case Type::programChange:   s << "PC " << number; break;
    }

    s << String::fromUTF8 (" \xc2\xb7 ") << (channel == 0 ? String ("Any Ch") : "Ch " + String (channel));
    return s;
}

//==============================================================================
String getParameterIdentifier (AudioProcessorParameter& param)
{
    if (auto* hosted = dynamic_cast<HostedAudioProcessorParameter*> (&param))
        return hosted->getParameterID();

    if (auto* withID = dynamic_cast<AudioProcessorParameterWithID*> (&param))
        return withID->paramID;

    return {};
}

MidiMappingTarget MidiMappingTarget::forParameter (AudioProcessorGraph::NodeID nodeID, const AudioProcessor& processor, int paramIndex)
{
    MidiMappingTarget t;
    t.kind = Kind::parameter;
    t.nodeID = nodeID;
    t.paramIndex = paramIndex;

    if (auto* param = processor.getParameters()[paramIndex])
        t.paramID = getParameterIdentifier (*param);

    return t;
}

MidiMappingTarget MidiMappingTarget::forBypass (AudioProcessorGraph::NodeID nodeID)
{
    MidiMappingTarget t;
    t.kind = Kind::bypass;
    t.nodeID = nodeID;
    return t;
}

MidiMappingTarget MidiMappingTarget::forPresetStep (bool next)
{
    MidiMappingTarget t;
    t.kind = next ? Kind::nextPreset : Kind::previousPreset;
    return t;
}

AudioProcessorParameter* MidiMappingTarget::findParameter (AudioProcessor& processor) const
{
    const auto& params = processor.getParameters();

    if (paramID.isNotEmpty())
        for (auto* p : params)
            if (getParameterIdentifier (*p) == paramID)
                return p;

    return params[paramIndex];
}

bool MidiMappingTarget::operator== (const MidiMappingTarget& other) const
{
    if (kind != other.kind || nodeID != other.nodeID)
        return false;

    if (kind != Kind::parameter)
        return true;

    if (paramID.isNotEmpty() || other.paramID.isNotEmpty())
        return paramID == other.paramID;

    return paramIndex == other.paramIndex;
}

//==============================================================================
MidiMapping::Mode MidiMapping::getDefaultMode (const MidiTrigger& trigger, const MidiMappingTarget& target)
{
    if (target.kind == MidiMappingTarget::Kind::parameter && trigger.type == MidiTrigger::Type::controller)
        return Mode::absolute;

    // A CC driving bypass starts as a latching switch; learning switches it to
    // toggle if the button turns out to be momentary.
    if (target.kind == MidiMappingTarget::Kind::bypass && trigger.type == MidiTrigger::Type::controller)
        return Mode::follow;

    return Mode::toggle;
}

int MidiMapping::getControlValue (const MidiMessage& m)
{
    if (m.isController())
        return m.getControllerValue();

    if (m.isNoteOn())
        return 127;

    if (m.isNoteOff())
        return 0;

    return 127;
}

float MidiMapping::scale (int controlValue) const
{
    auto proportion = jlimit (0.0f, 1.0f, (float) controlValue / 127.0f);

    if (invert)
        proportion = 1.0f - proportion;

    return jlimit (0.0f, 1.0f, minValue + (maxValue - minValue) * proportion);
}

//==============================================================================
static const char* toString (MidiTrigger::Type t)
{
    switch (t)
    {
        case MidiTrigger::Type::controller:     return "cc";
        case MidiTrigger::Type::note:           return "note";
        case MidiTrigger::Type::programChange:  return "pc";
    }

    return "cc";
}

static const char* toString (MidiMappingTarget::Kind k)
{
    switch (k)
    {
        case MidiMappingTarget::Kind::parameter:        return "parameter";
        case MidiMappingTarget::Kind::bypass:           return "bypass";
        case MidiMappingTarget::Kind::nextPreset:       return "nextPreset";
        case MidiMappingTarget::Kind::previousPreset:   return "previousPreset";
    }

    return "parameter";
}

static const char* toString (MidiMapping::Mode m)
{
    switch (m)
    {
        case MidiMapping::Mode::absolute:   return "absolute";
        case MidiMapping::Mode::toggle:     return "toggle";
        case MidiMapping::Mode::follow:     return "follow";
    }

    return "absolute";
}

template <typename Enum, size_t N>
static std::optional<Enum> fromString (const String& s, const Enum (&values)[N])
{
    for (auto v : values)
        if (s == toString (v))
            return v;

    return std::nullopt;
}

std::unique_ptr<XmlElement> MidiMapping::toXml() const
{
    auto e = std::make_unique<XmlElement> ("MAPPING");

    e->setAttribute ("type",    toString (trigger.type));
    e->setAttribute ("channel", trigger.channel);
    e->setAttribute ("number",  trigger.number);
    e->setAttribute ("target",  toString (target.kind));

    if (target.isNodeTarget())
        e->setAttribute ("node", (int) target.nodeID.uid);

    if (target.kind == MidiMappingTarget::Kind::parameter)
    {
        e->setAttribute ("paramID",    target.paramID);
        e->setAttribute ("paramIndex", target.paramIndex);
    }

    e->setAttribute ("mode", toString (mode));

    if (minValue != 0.0f)   e->setAttribute ("min", minValue);
    if (maxValue != 1.0f)   e->setAttribute ("max", maxValue);
    if (invert)             e->setAttribute ("invert", true);

    return e;
}

std::optional<MidiMapping> MidiMapping::fromXml (const XmlElement& e)
{
    const auto type = fromString (e.getStringAttribute ("type"),
                                  { MidiTrigger::Type::controller, MidiTrigger::Type::note, MidiTrigger::Type::programChange });
    const auto kind = fromString (e.getStringAttribute ("target"),
                                  { MidiMappingTarget::Kind::parameter, MidiMappingTarget::Kind::bypass,
                                    MidiMappingTarget::Kind::nextPreset, MidiMappingTarget::Kind::previousPreset });

    if (! type.has_value() || ! kind.has_value())
        return std::nullopt;

    MidiMapping m;
    m.trigger = { *type, jlimit (0, 16, e.getIntAttribute ("channel", 1)), jlimit (0, 127, e.getIntAttribute ("number")) };

    m.target.kind = *kind;
    m.target.nodeID = AudioProcessorGraph::NodeID ((uint32) e.getIntAttribute ("node"));
    m.target.paramID = e.getStringAttribute ("paramID");
    m.target.paramIndex = e.getIntAttribute ("paramIndex", -1);

    m.mode = fromString (e.getStringAttribute ("mode"), { Mode::absolute, Mode::toggle, Mode::follow })
                 .value_or (getDefaultMode (m.trigger, m.target));
    m.minValue = (float) e.getDoubleAttribute ("min", 0.0);
    m.maxValue = (float) e.getDoubleAttribute ("max", 1.0);
    m.invert = e.getBoolAttribute ("invert");

    return m;
}

//==============================================================================
void MidiMappingSet::assign (const MidiMapping& mapping)
{
    mappings.erase (std::remove_if (mappings.begin(), mappings.end(),
                                    [&] (const MidiMapping& m) { return m.trigger == mapping.trigger || m.target == mapping.target; }),
                    mappings.end());
    mappings.push_back (mapping);
}

bool MidiMappingSet::removeFor (const MidiMappingTarget& target)
{
    const auto oldSize = mappings.size();
    mappings.erase (std::remove_if (mappings.begin(), mappings.end(),
                                    [&] (const MidiMapping& m) { return m.target == target; }),
                    mappings.end());
    return mappings.size() != oldSize;
}

bool MidiMappingSet::removeForNode (AudioProcessorGraph::NodeID nodeID)
{
    const auto oldSize = mappings.size();
    mappings.erase (std::remove_if (mappings.begin(), mappings.end(),
                                    [&] (const MidiMapping& m) { return m.target.isNodeTarget() && m.target.nodeID == nodeID; }),
                    mappings.end());
    return mappings.size() != oldSize;
}

const MidiMapping* MidiMappingSet::findFor (const MidiMappingTarget& target) const
{
    for (auto& m : mappings)
        if (m.target == target)
            return &m;

    return nullptr;
}

bool MidiMappingSet::hasMappingsForNode (AudioProcessorGraph::NodeID nodeID) const
{
    return std::any_of (mappings.begin(), mappings.end(),
                        [&] (const MidiMapping& m) { return m.target.isNodeTarget() && m.target.nodeID == nodeID; });
}

std::unique_ptr<XmlElement> MidiMappingSet::toXml() const
{
    auto xml = std::make_unique<XmlElement> (xmlTag);

    for (auto& m : mappings)
        xml->addChildElement (m.toXml().release());

    return xml;
}

void MidiMappingSet::restoreFromXml (const XmlElement& xml)
{
    mappings.clear();

    for (auto* e : xml.getChildWithTagNameIterator ("MAPPING"))
        if (auto m = MidiMapping::fromXml (*e))
            assign (*m);
}
