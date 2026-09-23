/*
==============================================================================
   Curve - Audio Engine Constants
   Centralized audio definitions, fallback parameters, and internal plugin identifiers.
==============================================================================
*/

#pragma once

#include <cstdint>

namespace Curve
{
namespace AudioConstants
{
    // Default fallback sample rate (Hz) when no audio device is open or available.
    constexpr double defaultSampleRate = 44100.0;

    // Default fallback buffer size (samples per block) when no audio device is open or available.
    constexpr int defaultBufferSize = 512;

    // Default tap warmup buffer size for system audio capture
    constexpr int defaultTapBufferSize = 128;

    // Default channel counts and bitmask
    constexpr int defaultNumChannels = 2; // Stereo
    constexpr const char* defaultStereoBitmask = "11"; // Radix-2 binary string: bits 0 & 1 enabled (L & R)

    // Internal I/O plugin format name
    constexpr const char* internalPluginFormat = "Internal";

    // Internal plugin names and identifiers
    constexpr const char* audioOutputName    = "Audio Output";
    constexpr const char* audioInputName     = "Audio Input";
    constexpr const char* loopbackIdentifier = "OutputInterfaceLoopback";
    constexpr const char* loopbackName       = "Interface Loopback (In)";

    // Internal plugin Unique IDs
    constexpr uint32_t audioOutputUniqueId = 0x724248cb;
    constexpr uint32_t audioInputUniqueId  = 0x246006c0;
    constexpr uint32_t loopbackUniqueId    = 0x53415450; // 'SATP'
}
}
