/*
  ==============================================================================

   This file is part of the JUCE framework.
   Copyright (c) Raw Material Software Limited

   JUCE is an open source framework subject to commercial or open source
   licensing.

   By downloading, installing, or using the JUCE framework, or combining the
   JUCE framework with any other source code, object code, content or any other
   copyrightable work, you agree to the terms of the JUCE End User Licence
   Agreement, and all incorporated terms including the JUCE Privacy Policy and
   the JUCE Website Terms of Service, as applicable, which will bind you. If you
   do not agree to the terms of these agreements, we will not license the JUCE
   framework to you, and you must discontinue the installation or download
   process and cease use of the JUCE framework.

   JUCE End User Licence Agreement: https://juce.com/legal/juce-8-licence/
   JUCE Privacy Policy: https://juce.com/juce-privacy-policy
   JUCE Website Terms of Service: https://juce.com/juce-website-terms-of-service/

   Or:

   You may also use this code under the terms of the AGPLv3:
   https://www.gnu.org/licenses/agpl-3.0.en.html

   THE JUCE FRAMEWORK IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
   WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING WARRANTY OF
   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, ARE DISCLAIMED.

  ==============================================================================

    Curve
   Copyright (c) Thomas Derham

   The modifications for Curve are licensed to you solely under the terms of the 
   AGPLv3 license terms as described above.

   CURVE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
   WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING WARRANTY OF
   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, ARE DISCLAIMED.

  ==============================================================================
*/

#include <JuceHeader.h>

#include <juce_audio_plugin_client/juce_audio_plugin_client.h>

#include "AudioRecorderNode.h"
#include "CrossfeedNode.h"
#include "GainNode.h"
#include "InternalPlugins.h"
#include "InvertPhaseNode.h"
#include "OutputInterfaceLoopbackNode.h"
#include "PluginGraph.h"
#include "SpeakerEmulationNode.h"

//==============================================================================


static const std::vector<PluginDescription>& getStaticInternalDescriptions()
{
    static const std::vector<PluginDescription> descriptions = []
    {
        std::vector<PluginDescription> result;

        // Audio I/O: Audio Input, Output Interface Loopback, Audio Output
        result.push_back (AudioProcessorGraph::AudioGraphIOProcessor (AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode).getPluginDescription());

        PluginDescription loopback;
        loopback.name = "Interface Loopback (In)";
        loopback.descriptiveName = "Interface Loopback (In)";
        loopback.pluginFormatName = "Internal";
        loopback.category = "Audio I/O";
        loopback.fileOrIdentifier = "OutputInterfaceLoopback";
        loopback.uniqueId = 0x53415450; // "SATP"
        loopback.isInstrument = false;
        loopback.numInputChannels = 0;
        loopback.numOutputChannels = 2;
        result.push_back (loopback);

        result.push_back (AudioProcessorGraph::AudioGraphIOProcessor (AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode).getPluginDescription());

        // Midi I/O
        result.push_back (AudioProcessorGraph::AudioGraphIOProcessor (AudioProcessorGraph::AudioGraphIOProcessor::midiInputNode).getPluginDescription());
        result.push_back (AudioProcessorGraph::AudioGraphIOProcessor (AudioProcessorGraph::AudioGraphIOProcessor::midiOutputNode).getPluginDescription());

        // Plugins
        PluginDescription xfed;
        xfed.name = "Headphone Crossfeed";
        xfed.descriptiveName = "Headphone Crossfeed";
        xfed.pluginFormatName = "Internal";
        xfed.category = "Plugins";
        xfed.fileOrIdentifier = "Crossfeed";
        xfed.uniqueId = 0x58464544; // "XFED"
        xfed.isInstrument = false;
        xfed.numInputChannels = 2;
        xfed.numOutputChannels = 2;
        result.push_back (xfed);

        PluginDescription spkr;
        spkr.name = "Headphone Speaker Emulation";
        spkr.descriptiveName = "Headphone Speaker Emulation";
        spkr.pluginFormatName = "Internal";
        spkr.category = "Plugins";
        spkr.fileOrIdentifier = "SpeakerEmulation";
        spkr.uniqueId = 0x53504B52; // "SPKR"
        spkr.isInstrument = false;
        spkr.numInputChannels = 2;
        spkr.numOutputChannels = 2;
        result.push_back (spkr);

        PluginDescription gain;
        gain.name = "Gain";
        gain.descriptiveName = "Gain";
        gain.pluginFormatName = "Internal";
        gain.category = "Plugins";
        gain.fileOrIdentifier = "Gain";
        gain.uniqueId = 0x4741494E; // "GAIN"
        gain.isInstrument = false;
        gain.numInputChannels = 2;
        gain.numOutputChannels = 2;
        result.push_back (gain);

        PluginDescription invp;
        invp.name = "Invert Phase";
        invp.descriptiveName = "Invert Phase";
        invp.pluginFormatName = "Internal";
        invp.category = "Plugins";
        invp.fileOrIdentifier = "InvertPhase";
        invp.uniqueId = 0x494E5650; // "INVP"
        invp.isInstrument = false;
        invp.numInputChannels = 2;
        invp.numOutputChannels = 2;
        result.push_back (invp);

        PluginDescription recd;
        recd.name = "Audio Recorder";
        recd.descriptiveName = "Audio Recorder";
        recd.pluginFormatName = "Internal";
        recd.category = "Plugins";
        recd.fileOrIdentifier = "AudioRecorder";
        recd.uniqueId = 0x52454344; // "RECD"
        recd.isInstrument = false;
        recd.numInputChannels = 2;
        recd.numOutputChannels = 0;
        result.push_back (recd);

        return result;
    }();

    return descriptions;
}

InternalPluginFormat::InternalPluginFactory::InternalPluginFactory (const std::initializer_list<Constructor>& constructorsIn)
    : constructors (constructorsIn),
      descriptions (getStaticInternalDescriptions())
{}

std::unique_ptr<AudioPluginInstance> InternalPluginFormat::InternalPluginFactory::createInstance (const String& name) const
{
    const auto begin = descriptions.begin();
    const auto it = std::find_if (begin,
                                  descriptions.end(),
                                  [&] (const PluginDescription& desc)
                                  {
                                      return name.equalsIgnoreCase (desc.name)
                                             || name.equalsIgnoreCase (desc.fileOrIdentifier);
                                  });

    if (it != descriptions.end())
    {
        const auto index = (size_t) std::distance (begin, it);
        return constructors[index]();
    }

    // Legacy node name compatibility
    if (name.equalsIgnoreCase ("System Audio Input") || name.equalsIgnoreCase ("SystemAudio")
        || name.equalsIgnoreCase ("Output Interface Loopback") || name.equalsIgnoreCase ("OutputInterfaceLoopback"))
    {
        const auto loopbackIt = std::find_if (begin, descriptions.end(), [] (const PluginDescription& desc) {
            return desc.fileOrIdentifier.equalsIgnoreCase ("OutputInterfaceLoopback")
                || desc.uniqueId == 0x53415450;
        });

        if (loopbackIt != descriptions.end())
        {
            const auto index = (size_t) std::distance (begin, loopbackIt);
            return constructors[index]();
        }
    }

    return nullptr;
}

InternalPluginFormat::InternalPluginFormat()
    : factory {
        [] { return std::make_unique<AudioProcessorGraph::AudioGraphIOProcessor> (AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode); },
        [] { return std::make_unique<OutputInterfaceLoopbackNode>(); },
        [] { return std::make_unique<AudioProcessorGraph::AudioGraphIOProcessor> (AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode); },
        [] { return std::make_unique<AudioProcessorGraph::AudioGraphIOProcessor> (AudioProcessorGraph::AudioGraphIOProcessor::midiInputNode); },
        [] { return std::make_unique<AudioProcessorGraph::AudioGraphIOProcessor> (AudioProcessorGraph::AudioGraphIOProcessor::midiOutputNode); },
        [] { return std::make_unique<CrossfeedNode>(); },
        [] { return std::make_unique<SpeakerEmulationNode>(); },
        [] { return std::make_unique<GainNode>(); },
        [] { return std::make_unique<InvertPhaseNode>(); },
        [] { return std::make_unique<AudioRecorderNode>(); }
    }
{
}

std::unique_ptr<AudioPluginInstance> InternalPluginFormat::createInstance (const String& name)
{
    return factory.createInstance (name);
}

void InternalPluginFormat::createPluginInstance (const PluginDescription& desc,
                                                 double /*initialSampleRate*/, int /*initialBufferSize*/,
                                                 PluginCreationCallback callback)
{
    if (auto p = createInstance (desc.name))
        callback (std::move (p), {});
    else if (auto pById = createInstance (desc.fileOrIdentifier))
        callback (std::move (pById), {});
    else
        callback (nullptr, NEEDS_TRANS ("Invalid internal plugin name"));
}

bool InternalPluginFormat::requiresUnblockedMessageThreadDuringCreation (const PluginDescription&) const
{
    return false;
}

const std::vector<PluginDescription>& InternalPluginFormat::getAllTypes() const
{
    return factory.getDescriptions();
}
