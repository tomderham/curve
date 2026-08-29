#define DONT_SET_USING_JUCE_NAMESPACE 1
#include "AUNBandEQConverter.h"
#include <cmath>

#if JUCE_MAC
 #import <AudioToolbox/AudioToolbox.h>
 #import <AudioUnit/AudioUnit.h>
#endif

namespace AUNBandEQConverter
{
    // Constants for AUNBandEQ from AudioUnitParameters.h
    constexpr AudioUnitParameterID kParam_GlobalGain = 0;
    constexpr AudioUnitParameterID kParam_BypassBand = 1000;
    constexpr AudioUnitParameterID kParam_FilterType = 2000;
    constexpr AudioUnitParameterID kParam_Frequency  = 3000;
    constexpr AudioUnitParameterID kParam_Gain       = 4000;
    constexpr AudioUnitParameterID kParam_Bandwidth  = 5000;
    constexpr AudioUnitPropertyID  kProp_NumberOfBands    = 2200;
    constexpr AudioUnitPropertyID  kProp_MaxNumberOfBands = 2201;
    constexpr UInt32               kDefaultMaxBands       = 16;

    // Filter types from Apple's AudioUnitParameters.h
    [[maybe_unused]] constexpr int kFilter_Parametric                  = 0;
    [[maybe_unused]] constexpr int kFilter_2ndOrderButterworthLowPass  = 1;
    [[maybe_unused]] constexpr int kFilter_2ndOrderButterworthHighPass = 2;
    [[maybe_unused]] constexpr int kFilter_ResonantLowPass             = 3;
    [[maybe_unused]] constexpr int kFilter_ResonantHighPass            = 4;
    [[maybe_unused]] constexpr int kFilter_BandPass                    = 5;
    [[maybe_unused]] constexpr int kFilter_BandStop                    = 6;
    [[maybe_unused]] constexpr int kFilter_LowShelf                    = 7;
    [[maybe_unused]] constexpr int kFilter_HighShelf                   = 8;
    [[maybe_unused]] constexpr int kFilter_ResonantLowShelf            = 9;
    [[maybe_unused]] constexpr int kFilter_ResonantHighShelf           = 10;

    double qFactorToOctaveBandwidth (double q)
    {
        if (q <= 0.001)
            q = 0.001;

        // Formula: N = (2 / ln(2)) * asinh(1 / (2*Q)) = (2 / ln(2)) * ln( 1/(2*Q) + sqrt(1/(4*Q^2) + 1) )
        const double oneOverTwoQ = 1.0 / (2.0 * q);
        const double arg = oneOverTwoQ + std::sqrt (oneOverTwoQ * oneOverTwoQ + 1.0);
        return (2.0 / std::log (2.0)) * std::log (arg);
    }

    double octaveBandwidthToQFactor (double octaves)
    {
        if (octaves <= 0.001)
            octaves = 0.001;

        // Formula: Q = 1 / (2 * sinh((ln(2) / 2) * N))
        const double x = (std::log (2.0) / 2.0) * octaves;
        const double sinhX = std::sinh (x);
        if (sinhX == 0.0)
            return 1.0;
        return 1.0 / (2.0 * sinhX);
    }

    bool isAUNBandEQ (const juce::PluginDescription& desc)
    {
        return desc.fileOrIdentifier.containsIgnoreCase (",nbeq,appl")
            || (desc.name.containsIgnoreCase ("AUNBandEQ") || (desc.name.containsIgnoreCase ("Parametric EQ") && desc.manufacturerName.containsIgnoreCase ("Apple")));
    }

    bool isAUNBandEQ (juce::AudioProcessor* proc)
    {
        if (auto* plugin = dynamic_cast<juce::AudioPluginInstance*> (proc))
            return isAUNBandEQ (plugin->getPluginDescription());

        return false;
    }

    std::optional<juce::PluginDescription> findAUNBandEQ (const juce::KnownPluginList& knownPlugins)
    {
        for (const auto& desc : knownPlugins.getTypes())
        {
            if (isAUNBandEQ (desc))
                return desc;
        }

        return std::nullopt;
    }

    std::optional<juce::PluginDescription> findAUNBandEQ (juce::KnownPluginList& knownPlugins)
    {
        if (auto found = findAUNBandEQ (const_cast<const juce::KnownPluginList&> (knownPlugins)))
            return found;

        // The AudioComponent scan below is expensive (it enumerates every AudioUnit registered
        // on the system) and this overload can be invoked from the UI thread (e.g. every time
        // the "Add Plugin" context menu is built, before AUNBandEQ has been found once). Cache
        // the outcome for the lifetime of the process so the scan only ever runs once instead
        // of stalling the UI on every call. If it's found, it's also cached into knownPlugins
        // (below), so subsequent calls resolve via the fast const overload above regardless.
        static bool hasScanned = false;
        static std::optional<juce::PluginDescription> scanResult;

        if (hasScanned)
            return scanResult;

        hasScanned = true;

       #if JUCE_MAC
        juce::AudioUnitPluginFormat auFormat;
        auto auIdentifiers = auFormat.searchPathsForPlugins (juce::FileSearchPath(), false, false);
        for (const auto& id : auIdentifiers)
        {
            if (id.containsIgnoreCase ("nbeq") || id.containsIgnoreCase ("AUNBandEQ"))
            {
                juce::OwnedArray<juce::PluginDescription> found;
                auFormat.findAllTypesForFile (found, id);
                if (! found.isEmpty() && found[0] != nullptr)
                {
                    knownPlugins.addType (*found[0]);
                    scanResult = *found[0];
                    break;
                }
            }
        }
       #endif

        return scanResult;
    }

    static int toAUNBandEQFilterType (CalibrationFilterType type)
    {
        switch (type)
        {
            case CalibrationFilterType::peaking:   return kFilter_Parametric;
            case CalibrationFilterType::lowShelf:  return kFilter_LowShelf;
            case CalibrationFilterType::highShelf: return kFilter_HighShelf;
            case CalibrationFilterType::lowPass:   return kFilter_2ndOrderButterworthLowPass;
            case CalibrationFilterType::highPass:  return kFilter_2ndOrderButterworthHighPass;
            case CalibrationFilterType::bandPass:  return kFilter_BandPass;
            case CalibrationFilterType::notch:     return kFilter_BandStop;
            case CalibrationFilterType::unknown:
            default:                               return kFilter_Parametric;
        }
    }

    bool applyProfileToAUNBandEQ (juce::AudioProcessor* proc, const CalibrationProfile& profile)
    {
        if (proc == nullptr)
            return false;

       #if JUCE_MAC
        AudioUnit audioUnit = nullptr;
        if (auto* plugin = dynamic_cast<juce::AudioPluginInstance*> (proc))
        {
            if (auto* client = plugin->getAudioUnitClient())
                audioUnit = client->getAudioUnitHandle();
        }

        if (audioUnit != nullptr)
        {
            // Query maximum supported bands from AudioUnit (kAUNBandEQProperty_MaxNumberOfBands)
            UInt32 maxBands = kDefaultMaxBands;
            UInt32 maxBandsSize = sizeof (maxBands);
            if (AudioUnitGetProperty (audioUnit, kProp_MaxNumberOfBands, kAudioUnitScope_Global, 0, &maxBands, &maxBandsSize) != noErr || maxBands == 0)
            {
                maxBands = kDefaultMaxBands;
            }

            // Query current number of bands
            UInt32 currentBands = 8;
            UInt32 currentBandsSize = sizeof (currentBands);
            if (AudioUnitGetProperty (audioUnit, kProp_NumberOfBands, kAudioUnitScope_Global, 0, &currentBands, &currentBandsSize) != noErr || currentBands == 0)
            {
                currentBands = 8;
            }

            // Requested bands clamped strictly to [1, maxBands]
            const UInt32 requestedBands = juce::jlimit ((UInt32) 1, maxBands, (UInt32) profile.bands.size());

            // Attempt to update number of bands if needed
            if (currentBands < requestedBands)
            {
                UInt32 newBands = requestedBands;
                if (AudioUnitSetProperty (audioUnit, kProp_NumberOfBands, kAudioUnitScope_Global, 0, &newBands, sizeof (newBands)) == noErr)
                {
                    currentBands = newBands;
                }
                else
                {
                    // Re-query to determine active bands
                    AudioUnitGetProperty (audioUnit, kProp_NumberOfBands, kAudioUnitScope_Global, 0, &currentBands, &currentBandsSize);
                }
            }

            // Number of bands we can safely configure
            const size_t numBandsToConfigure = std::min ({ profile.bands.size(), (size_t) currentBands, (size_t) maxBands });

            // Set global gain (Preamp)
            const float globalGain = (float) juce::jlimit (-96.0, 24.0, profile.preampGainDb);
            AudioUnitSetParameter (audioUnit, kParam_GlobalGain, kAudioUnitScope_Global, 0, globalGain, 0);

            auto notifyParam = [audioUnit] (AudioUnitParameterID paramID)
            {
                AudioUnitEvent event;
                event.mEventType = kAudioUnitEvent_ParameterValueChange;
                event.mArgument.mParameter.mAudioUnit = audioUnit;
                event.mArgument.mParameter.mParameterID = paramID;
                event.mArgument.mParameter.mScope = kAudioUnitScope_Global;
                event.mArgument.mParameter.mElement = 0;
                AUEventListenerNotify (nullptr, nullptr, &event);
            };

            auto notifyProp = [audioUnit] (AudioUnitPropertyID propID)
            {
                AudioUnitEvent event;
                event.mEventType = kAudioUnitEvent_PropertyChange;
                event.mArgument.mProperty.mAudioUnit = audioUnit;
                event.mArgument.mProperty.mPropertyID = propID;
                event.mArgument.mProperty.mScope = kAudioUnitScope_Global;
                event.mArgument.mProperty.mElement = 0;
                AUEventListenerNotify (nullptr, nullptr, &event);
            };

            notifyProp (kProp_NumberOfBands);
            notifyParam (kParam_GlobalGain);

            // Configure each band up to the safe limit
            for (size_t i = 0; i < numBandsToConfigure; ++i)
            {
                const auto& band = profile.bands[i];
                const auto bandIdx = (AudioUnitParameterID) i;

                const int auType = toAUNBandEQFilterType (band.type);
                const float freq = (float) juce::jlimit (20.0, 20000.0, band.frequencyHz);
                const float gain = (float) juce::jlimit (-96.0, 24.0, band.gainDb);
                const float bandwidth = (float) juce::jlimit (0.05, 5.0, band.bandwidthOctaves);
                const float bypass = band.enabled ? 0.0f : 1.0f;

                AudioUnitSetParameter (audioUnit, kParam_FilterType + bandIdx, kAudioUnitScope_Global, 0, (float) auType, 0);
                AudioUnitSetParameter (audioUnit, kParam_Frequency  + bandIdx, kAudioUnitScope_Global, 0, freq, 0);
                AudioUnitSetParameter (audioUnit, kParam_Gain       + bandIdx, kAudioUnitScope_Global, 0, gain, 0);
                AudioUnitSetParameter (audioUnit, kParam_Bandwidth  + bandIdx, kAudioUnitScope_Global, 0, bandwidth, 0);
                AudioUnitSetParameter (audioUnit, kParam_BypassBand + bandIdx, kAudioUnitScope_Global, 0, bypass, 0);

                notifyParam (kParam_FilterType + bandIdx);
                notifyParam (kParam_Frequency  + bandIdx);
                notifyParam (kParam_Gain       + bandIdx);
                notifyParam (kParam_Bandwidth  + bandIdx);
                notifyParam (kParam_BypassBand + bandIdx);
            }

            // Bypass and zero out any extra active bands beyond what the profile configured
            for (size_t i = numBandsToConfigure; i < (size_t) currentBands && i < (size_t) maxBands; ++i)
            {
                const auto bandIdx = (AudioUnitParameterID) i;
                AudioUnitSetParameter (audioUnit, kParam_Gain       + bandIdx, kAudioUnitScope_Global, 0, 0.0f, 0);
                AudioUnitSetParameter (audioUnit, kParam_BypassBand + bandIdx, kAudioUnitScope_Global, 0, 1.0f, 0);

                notifyParam (kParam_Gain       + bandIdx);
                notifyParam (kParam_BypassBand + bandIdx);
            }

            proc->updateHostDisplay (juce::AudioProcessor::ChangeDetails().withParameterInfoChanged (true).withNonParameterStateChanged (true));
            return true;
        }
       #endif

        return false;
    }

    bool restoreAUNBandEQState (juce::AudioProcessor* proc, const juce::MemoryBlock& state)
    {
        if (proc == nullptr || state.getSize() == 0)
            return false;

        proc->setStateInformation (state.getData(), (int) state.getSize());

       #if JUCE_MAC
        AudioUnit audioUnit = nullptr;
        if (auto* plugin = dynamic_cast<juce::AudioPluginInstance*> (proc))
        {
            if (auto* client = plugin->getAudioUnitClient())
                audioUnit = client->getAudioUnitHandle();
        }

        if (audioUnit != nullptr)
        {
            AudioUnitEvent propEvent;
            propEvent.mEventType = kAudioUnitEvent_PropertyChange;
            propEvent.mArgument.mProperty.mAudioUnit = audioUnit;
            propEvent.mArgument.mProperty.mPropertyID = kAudioUnitProperty_ClassInfo;
            propEvent.mArgument.mProperty.mScope = kAudioUnitScope_Global;
            propEvent.mArgument.mProperty.mElement = 0;
            AUEventListenerNotify (nullptr, nullptr, &propEvent);

            propEvent.mArgument.mProperty.mPropertyID = kProp_NumberOfBands;
            AUEventListenerNotify (nullptr, nullptr, &propEvent);
        }
       #endif

        proc->updateHostDisplay (juce::AudioProcessor::ChangeDetails().withParameterInfoChanged (true).withNonParameterStateChanged (true));
        return true;
    }
}

