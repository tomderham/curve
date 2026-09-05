/*
==============================================================================
   Curve - Audio Engine Diagnostics
   Real-time safe telemetry and event logger for detecting buffer underruns,
   DSP deadline overruns, FIFO cushion drops, and CoreAudio tap health resets.
   Works in Release builds and across both C++ and Objective-C++ translation units.
==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <array>
#include <cstdio>
#include <cstdarg>
#include <cstring>

#if JUCE_MAC
#include <os/log.h>
#endif

#ifndef ENABLE_AUDIO_DIAGNOSTICS
#define ENABLE_AUDIO_DIAGNOSTICS 0
#endif

#if ENABLE_AUDIO_DIAGNOSTICS
#define CURVE_AUDIO_LOG(...) AudioDiagnostics::log(__VA_ARGS__)
class AudioDiagnostics final : private juce::Timer
{
public:
    static AudioDiagnostics& getInstance()
    {
        static AudioDiagnostics instance;
        return instance;
    }

    // Early initialization from the message thread during app startup.
    // Guarantees singleton construction, log file creation, and timer setup
    // occur off the real-time audio thread before any audio device starts.
    static void initializeEarly()
    {
        getInstance().start();
    }

    void start()
    {
        if (! isTimerRunning())
            startTimer (1000); // 1-second telemetry interval
    }

    // Audio-thread safe: no memory allocation, no mutex locks
    void recordCallbackTiming (uint32_t durationUs, uint32_t deadlineUs)
    {
        totalBlocks.fetch_add (1, std::memory_order_relaxed);
        totalDurationUs.fetch_add (durationUs, std::memory_order_relaxed);

        uint32_t curMax = maxDurationUs.load (std::memory_order_relaxed);
        while (durationUs > curMax && ! maxDurationUs.compare_exchange_weak (curMax, durationUs, std::memory_order_relaxed))
        {}

        if (deadlineUs > 0 && durationUs > deadlineUs)
        {
            overrunBlocks.fetch_add (1, std::memory_order_relaxed);
            char msg[128];
            std::snprintf (msg, sizeof (msg), "[OVERRUN] Audio callback took %u us (deadline: %u us, over by %d us)",
                           durationUs, deadlineUs, (int)(durationUs - deadlineUs));
            postEvent (msg);
        }
    }

    // Audio-thread safe
    void recordFifoUnderrun (int requested, int available)
    {
        fifoUnderruns.fetch_add (1, std::memory_order_relaxed);
        char msg[128];
        std::snprintf (msg, sizeof (msg), "[FIFO UNDERRUN] Starvation! Needed %d samples, only %d available",
                       requested, available);
        postEvent (msg);
    }

    // Audio-thread safe
    void recordFifoCushionDrop (int ready, int maxCushion, int dropped)
    {
        fifoDrops.fetch_add (1, std::memory_order_relaxed);
        char msg[128];
        std::snprintf (msg, sizeof (msg), "[FIFO CUSHION DROP] Ready %d > max %d -> dropped %d samples",
                       ready, maxCushion, dropped);
        postEvent (msg);
    }

    // Audio-thread safe
    void recordConvolutionBlock (bool isSilent)
    {
        if (isSilent)
            convSilentBlocks.fetch_add (1, std::memory_order_relaxed);
        else
            convActiveBlocks.fetch_add (1, std::memory_order_relaxed);
    }

    // Audio-thread safe
    void recordTapDelivery (int numSamples, int channels)
    {
        juce::ignoreUnused (numSamples, channels);
        tapDeliveries.fetch_add (1, std::memory_order_relaxed);
        lastTapDeliveryTimeMs.store (juce::Time::getMillisecondCounter(), std::memory_order_relaxed);
    }

    // Audio-thread safe
    void recordTapEmpty()
    {
        tapEmptyDeliveries.fetch_add (1, std::memory_order_relaxed);
    }

    // Audio-thread safe event post (lock-free ring buffer)
    void postEvent (const char* text)
    {
        uint32_t slot = eventWriteIdx.fetch_add (1, std::memory_order_relaxed) % maxEvents;
        auto& entry = events[slot];
        std::strncpy (entry.message, text, sizeof (entry.message) - 1);
        entry.message[sizeof (entry.message) - 1] = '\0';
        entry.timestampMs = juce::Time::getMillisecondCounter();
        entry.ready.store (true, std::memory_order_release);
    }

    // Synchronous direct logging from non-audio threads (e.g. ResilienceManager, Tap init)
    static void log (const char* format, ...)
    {
        char buffer[256];
        va_list args;
        va_start (args, format);
        std::vsnprintf (buffer, sizeof (buffer), format, args);
        va_end (args);

        getInstance().writeOut (buffer);
    }

private:
    AudioDiagnostics()
    {
        logFile = std::fopen ("/tmp/curve_audio_debug.log", "a");
        if (logFile != nullptr)
        {
            auto now = juce::Time::getCurrentTime();
            auto timeStr = now.formatted ("%Y-%m-%d %H:%M:%S");
            std::fprintf (logFile, "\n========================================================\n");
            std::fprintf (logFile, "=== Curve Audio Diagnostics Started at %s ===\n", timeStr.toRawUTF8());
            std::fprintf (logFile, "========================================================\n");
            std::fflush (logFile);
        }
        std::fprintf (stderr, "=== Curve Audio Diagnostics Started ===\n");
       #if JUCE_MAC
        static os_log_t curveLog = os_log_create ("com.thomasderham.curve", "audio");
        os_log_with_type (curveLog, OS_LOG_TYPE_DEFAULT, "=== Curve Audio Diagnostics Started ===");
       #endif
    }

    ~AudioDiagnostics() override
    {
        stopTimer();
        drainEvents();
        if (logFile != nullptr)
        {
            std::fprintf (logFile, "=== Curve Audio Diagnostics Stopped ===\n\n");
            std::fclose (logFile);
            logFile = nullptr;
        }
    }

    void timerCallback() override
    {
        drainEvents();

        uint32_t blocks = totalBlocks.exchange (0, std::memory_order_relaxed);
        uint32_t overruns = overrunBlocks.exchange (0, std::memory_order_relaxed);
        uint32_t maxUs = maxDurationUs.exchange (0, std::memory_order_relaxed);
        uint64_t totalUs = totalDurationUs.exchange (0, std::memory_order_relaxed);
        uint32_t avgUs = (blocks > 0) ? static_cast<uint32_t> (totalUs / blocks) : 0;

        uint32_t fUnder = fifoUnderruns.exchange (0, std::memory_order_relaxed);
        uint32_t fDrop = fifoDrops.exchange (0, std::memory_order_relaxed);
        uint32_t tapRx = tapDeliveries.exchange (0, std::memory_order_relaxed);
        uint32_t tapEmpty = tapEmptyDeliveries.exchange (0, std::memory_order_relaxed);
        uint32_t cSilent = convSilentBlocks.exchange (0, std::memory_order_relaxed);
        uint32_t cActive = convActiveBlocks.exchange (0, std::memory_order_relaxed);

        if (blocks > 0 || fUnder > 0 || fDrop > 0 || overruns > 0 || tapRx > 0)
        {
            char summary[256];
            std::snprintf (summary, sizeof (summary),
                           "[AUDIO STATS 1s] Blocks: %u | Avg: %u us | Max: %u us | Overruns: %u | FIFO Under: %u | FIFO Drops: %u | TapRx: %u (empty: %u) | Conv(act/sil): %u/%u",
                           blocks, avgUs, maxUs, overruns, fUnder, fDrop, tapRx, tapEmpty, cActive, cSilent);
            writeOut (summary);
        }
    }

    void drainEvents()
    {
        uint32_t targetWrite = eventWriteIdx.load (std::memory_order_relaxed);
        while (eventReadIdx != targetWrite)
        {
            uint32_t slot = eventReadIdx % maxEvents;
            auto& entry = events[slot];
            if (entry.ready.load (std::memory_order_acquire))
            {
                writeOut (entry.message);
                entry.ready.store (false, std::memory_order_relaxed);
                eventReadIdx++;
            }
            else
            {
                break;
            }
        }
    }

    void writeOut (const char* msg)
    {
        auto now = juce::Time::getCurrentTime();
        auto ms = now.getMilliseconds();
        auto timeStr = now.formatted ("%H:%M:%S") + "." + juce::String (ms).paddedLeft ('0', 3);

        if (logFile != nullptr)
        {
            std::fprintf (logFile, "[%s] %s\n", timeStr.toRawUTF8(), msg);
            std::fflush (logFile);
        }
        std::fprintf (stderr, "[CurveAudio] [%s] %s\n", timeStr.toRawUTF8(), msg);
       #if JUCE_MAC
        static os_log_t curveLog = os_log_create ("com.thomasderham.curve", "audio");
        os_log_with_type (curveLog, OS_LOG_TYPE_DEFAULT, "[%{public}s] %{public}s", timeStr.toRawUTF8(), msg);
       #endif
    }

    std::FILE* logFile = nullptr;

    std::atomic<uint32_t> totalBlocks { 0 };
    std::atomic<uint32_t> overrunBlocks { 0 };
    std::atomic<uint32_t> maxDurationUs { 0 };
    std::atomic<uint64_t> totalDurationUs { 0 };
    std::atomic<uint32_t> fifoUnderruns { 0 };
    std::atomic<uint32_t> fifoDrops { 0 };
    std::atomic<uint32_t> tapDeliveries { 0 };
    std::atomic<uint32_t> tapEmptyDeliveries { 0 };
    std::atomic<uint32_t> convSilentBlocks { 0 };
    std::atomic<uint32_t> convActiveBlocks { 0 };
    std::atomic<juce::uint32> lastTapDeliveryTimeMs { 0 };

    static constexpr size_t maxEvents = 256;
    struct EventEntry
    {
        std::atomic<bool> ready { false };
        juce::uint32 timestampMs { 0 };
        char message[128] {};
    };
    std::array<EventEntry, maxEvents> events {};
    std::atomic<uint32_t> eventWriteIdx { 0 };
    uint32_t eventReadIdx = 0;
};
#else
#define CURVE_AUDIO_LOG(...) do {} while (0)
class AudioDiagnostics final
{
public:
    static AudioDiagnostics& getInstance()
    {
        static AudioDiagnostics instance;
        return instance;
    }

    static void initializeEarly() {}
    void start() {}
    void recordCallbackTiming (uint32_t, uint32_t) {}
    void recordFifoUnderrun (int, int) {}
    void recordFifoCushionDrop (int, int, int) {}
    void recordConvolutionBlock (bool) {}
    void recordTapDelivery (int, int) {}
    void recordTapEmpty() {}
    void postEvent (const char*) {}
    static void log (const char*, ...) {}
};
#endif

