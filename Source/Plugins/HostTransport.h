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
#include <atomic>
#include <vector>

//==============================================================================
/**
    The tempo and transport Curve gives plug-ins, as a host's play head: tempo-synced effects
    follow its tempo, and plug-ins with their own sequencer or loops (e.g. a drum machine) start
    and stop with its transport. 4/4. The position only advances while playing, from the start of
    bar 1 each time it starts.

    Tempo, playing and tapping are set on the message thread. The graph takes them once at the start
    of each block, so every plug-in in a block sees the same position, and advances the position
    after it (see TransportGraph).
*/
class HostTransport final : public juce::AudioPlayHead
{
public:
    static constexpr double defaultTempo = 120.0, minTempo = 20.0, maxTempo = 300.0;

    // Message thread
    void setTempo (double bpm)          { tempo.store (juce::jlimit (minTempo, maxTempo, bpm)); }
    double getTempo() const             { return tempo.load(); }

    void setPlaying (bool shouldPlay)
    {
        // Each start is from the start of bar 1
        if (shouldPlay && ! playing.load())
            restartRequested.store (true);

        playing.store (shouldPlay);
    }

    bool isPlaying() const              { return playing.load(); }

    /** One tap of tap tempo. From the second tap of a run, sets the tempo from the average of the
        last few, and returns true. A pause of over two seconds starts a new run. */
    bool tap()
    {
        const auto now = juce::Time::getMillisecondCounterHiRes();

        // A bouncing button, or two taps handled together: 300 BPM is 200 ms a beat
        if (! taps.empty() && now - taps.back() < 100.0)
            return false;

        if (! taps.empty() && now - taps.back() > 2000.0)
            taps.clear();

        taps.push_back (now);

        if (taps.size() > 5)
            taps.erase (taps.begin());

        if (taps.size() < 2)
            return false;

        const auto interval = (taps.back() - taps.front()) / (double) (taps.size() - 1);
        setTempo (60000.0 / interval);
        return true;
    }

    // Audio thread, around each block. A start or a tempo change made during a block takes effect
    // at the next, so a start is always from the start of bar 1
    void beginBlock (double rate)
    {
        if (rate > 0.0)
            sampleRate.store (rate);

        const bool isPlayingNow = playing.load();

        if (isPlayingNow && restartRequested.exchange (false))
        {
            ppqPosition.store (0.0);
            samplePosition.store (0);
        }

        blockTempo.store (tempo.load());
        blockPlaying.store (isPlayingNow);
    }

    void endBlock (int numSamples)
    {
        const auto rate = sampleRate.load();

        if (! blockPlaying.load() || rate <= 0.0)
            return;

        ppqPosition.store (ppqPosition.load() + (double) numSamples / rate * blockTempo.load() / 60.0);
        samplePosition.store (samplePosition.load() + numSamples);
    }

    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        const auto ppq = ppqPosition.load();
        const auto samples = samplePosition.load();

        info.setBpm (blockTempo.load());
        info.setTimeSignature (TimeSignature { 4, 4 });
        info.setIsPlaying (blockPlaying.load());
        info.setPpqPosition (ppq);
        info.setPpqPositionOfLastBarStart (std::floor (ppq / 4.0) * 4.0);
        info.setBarCount ((int64_t) std::floor (ppq / 4.0));
        info.setTimeInSamples (samples);

        if (const auto rate = sampleRate.load(); rate > 0.0)
            info.setTimeInSeconds ((double) samples / rate);

        return info;
    }

private:
    std::atomic<double> tempo { defaultTempo }, blockTempo { defaultTempo };
    std::atomic<bool> playing { false }, restartRequested { false }, blockPlaying { false };
    std::atomic<double> ppqPosition { 0.0 }, sampleRate { 0.0 };
    std::atomic<int64_t> samplePosition { 0 };

    std::vector<double> taps;   // message thread
};

//==============================================================================
/** The graph of a preset, giving its plug-ins the host transport as their play head. */
class TransportGraph final : public juce::AudioProcessorGraph
{
public:
    TransportGraph()            { setPlayHead (&transport); }
    ~TransportGraph() override  { setPlayHead (nullptr); }

    void processBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) override
    {
        transport.beginBlock (getSampleRate());
        juce::AudioProcessorGraph::processBlock (audio, midi);
        transport.endBlock (audio.getNumSamples());
    }

    void processBlock (juce::AudioBuffer<double>& audio, juce::MidiBuffer& midi) override
    {
        transport.beginBlock (getSampleRate());
        juce::AudioProcessorGraph::processBlock (audio, midi);
        transport.endBlock (audio.getNumSamples());
    }

    HostTransport transport;
};
