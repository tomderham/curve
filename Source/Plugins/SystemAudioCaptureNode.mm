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

#if JUCE_MAC
#import <Foundation/Foundation.h>
#import <CoreAudio/CoreAudio.h>
#import <CoreAudio/AudioHardwareTapping.h>
#import <CoreAudio/CATapDescription.h>
#endif

#define DONT_SET_USING_JUCE_NAMESPACE 1
#include "SystemAudioCaptureNode.h"

using namespace juce;

#if JUCE_MAC
//==============================================================================
struct SystemAudioCaptureNode::TapWrapper {
    AudioObjectID tapID = 0;
    AudioObjectID aggregateDeviceID = 0;
    AudioDeviceIOProcID ioProcID = nullptr;
    AudioStreamBasicDescription asbd = {};
};
#endif // JUCE_MAC

//==============================================================================
SystemAudioCaptureNode::SystemAudioCaptureNode()
    : AudioPluginInstance (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      ringBuffer (2, ringBufferCapacity),
      resampleBuffer (2, 96000)
{
    ringBuffer.clear();
    resampleBuffer.clear();
    
#if JUCE_MAC
    tapWrapper = std::make_unique<TapWrapper>();
#endif
}

SystemAudioCaptureNode::~SystemAudioCaptureNode()
{
    releaseResources();
}

bool SystemAudioCaptureNode::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return (layouts.getMainInputChannels() == 0 && layouts.getMainOutputChannels() <= 2);
}

void SystemAudioCaptureNode::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    outputSampleRate = sampleRate;
    fifo.reset();
    ringBuffer.clear();
    isBuffering = true;
    resamplers[0].reset();
    resamplers[1].reset();
    resampleBuffer.setSize(2, samplesPerBlock * 8); // Extra large cushion for high-ratio resampling
    
#if JUCE_MAC
    if (!isCapturing.exchange(true)) {
        if (@available(macOS 14.2, *)) {
            @autoreleasepool 
            {
                pid_t myPid = getpid();
                AudioObjectID myProcessObjectID = 0;
                UInt32 size = sizeof(myProcessObjectID);
                AudioObjectPropertyAddress prop = { kAudioHardwarePropertyTranslatePIDToProcessObject, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
                if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &prop, sizeof(myPid), &myPid, &size, &myProcessObjectID) != noErr) return;

                // Autorelease to prevent memory leaks in non-ARC compilation environments
                CATapDescription *tapDesc = [[[CATapDescription alloc] initStereoGlobalTapButExcludeProcesses:@[ @(myProcessObjectID) ]] autorelease];
                if (!tapDesc) return;
                tapDesc.UUID = [NSUUID UUID];

                // Mute the original system audio so we don't hear a mix of unprocessed and processed audio
                tapDesc.muteBehavior = CATapMutedWhenTapped;

                if (AudioHardwareCreateProcessTap(tapDesc, &tapWrapper->tapID) != noErr) return;

                NSDictionary *aggDict = @{
                    @(kAudioAggregateDeviceNameKey): @"Curve System Tap",
                    @(kAudioAggregateDeviceUIDKey): [[NSUUID UUID] UUIDString],
                    @(kAudioAggregateDeviceTapListKey): @[ @{ @(kAudioSubTapUIDKey): tapDesc.UUID.UUIDString } ],
                    @(kAudioAggregateDeviceIsPrivateKey): @YES
                };

                if (AudioHardwareCreateAggregateDevice((__bridge CFDictionaryRef)aggDict, &tapWrapper->aggregateDeviceID) != noErr) return;

                Float64 targetSampleRate = sampleRate;
                AudioObjectPropertyAddress srProp = { kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
                AudioObjectSetPropertyData(tapWrapper->aggregateDeviceID, &srProp, 0, NULL, sizeof(Float64), &targetSampleRate);

                UInt32 targetBufferSize = static_cast<UInt32> (samplesPerBlock);
                AudioObjectPropertyAddress bsProp = { kAudioDevicePropertyBufferFrameSize, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
                AudioObjectSetPropertyData(tapWrapper->aggregateDeviceID, &bsProp, 0, NULL, sizeof(UInt32), &targetBufferSize);

                UInt32 asbdSize = sizeof(tapWrapper->asbd);
                AudioObjectPropertyAddress formatProp = { kAudioDevicePropertyStreamFormat, kAudioDevicePropertyScopeInput, kAudioObjectPropertyElementMain };
                if (AudioObjectGetPropertyData(tapWrapper->aggregateDeviceID, &formatProp, 0, NULL, &asbdSize, &tapWrapper->asbd) != noErr) return;

                actualSampleRate.store(tapWrapper->asbd.mSampleRate);

                SystemAudioCaptureNode* node = this;
                AudioDeviceIOBlock ioBlock = ^(const AudioTimeStamp *inNow, const AudioBufferList *inInputData, const AudioTimeStamp *inInputTime, AudioBufferList *outOutputData, const AudioTimeStamp *inOutputTime) {
                    juce::ignoreUnused(inNow, inInputTime, outOutputData, inOutputTime);
                    if (!node) return;

                    if (!(node->tapWrapper->asbd.mFormatFlags & kAudioFormatFlagIsFloat)) return;
                    
                    size_t numChannels = static_cast<size_t>(node->tapWrapper->asbd.mChannelsPerFrame);
                    if (numChannels == 0) return; // Prevent divide by zero / out-of-bounds indexing

                    size_t numFrames = inInputData->mBuffers[0].mDataByteSize / node->tapWrapper->asbd.mBytesPerFrame;
                    size_t targetChannels = static_cast<size_t>(juce::jmin(static_cast<int>(numChannels), 2));

                    if (node->tapWrapper->asbd.mFormatFlags & kAudioFormatFlagIsNonInterleaved) {
                        const float* channelPointers[2];
                        for (size_t c = 0; c < targetChannels; ++c)
                            channelPointers[c] = static_cast<const float*>(inInputData->mBuffers[c < inInputData->mNumberBuffers ? c : 0].mData);
                        node->pushAudio(channelPointers, static_cast<int>(targetChannels), static_cast<int>(numFrames));
                    } else {
                        const float* floatData = static_cast<const float*>(inInputData->mBuffers[0].mData);
                        node->pushAudioInterleaved(floatData, static_cast<int>(numChannels), static_cast<int>(numFrames));
                    }
                };

                if (AudioDeviceCreateIOProcIDWithBlock(&tapWrapper->ioProcID, tapWrapper->aggregateDeviceID, NULL, ioBlock) == noErr) {
                    AudioDeviceStart(tapWrapper->aggregateDeviceID, tapWrapper->ioProcID);
                }
            }
        }
    }
#endif
}

void SystemAudioCaptureNode::releaseResources()
{
#if JUCE_MAC
    if (isCapturing.exchange(false)) {
        if (tapWrapper->ioProcID && tapWrapper->aggregateDeviceID) {
            AudioDeviceStop(tapWrapper->aggregateDeviceID, tapWrapper->ioProcID);
            AudioDeviceDestroyIOProcID(tapWrapper->aggregateDeviceID, tapWrapper->ioProcID);
            tapWrapper->ioProcID = nullptr;
        }
        if (tapWrapper->aggregateDeviceID) {
            AudioHardwareDestroyAggregateDevice(tapWrapper->aggregateDeviceID);
            tapWrapper->aggregateDeviceID = 0;
        }
        if (tapWrapper->tapID) {
            AudioHardwareDestroyProcessTap(tapWrapper->tapID);
            tapWrapper->tapID = 0;
        }
    }
#endif
}

void SystemAudioCaptureNode::pushAudio (const float* const* channelData, int numChannels, int numSamples)
{
    if (numChannels <= 0 || numSamples <= 0) return;

    int start1, size1, start2, size2;
    fifo.prepareToWrite (numSamples, start1, size1, start2, size2);
    
    int destChannels = ringBuffer.getNumChannels();
    
    if (size1 > 0)
        for (int ch = 0; ch < destChannels; ++ch)
        {
            int sourceCh = juce::jmin (ch, numChannels - 1);
            ringBuffer.copyFrom (ch, start1, channelData[sourceCh], size1);
        }
            
    if (size2 > 0)
        for (int ch = 0; ch < destChannels; ++ch)
        {
            int sourceCh = juce::jmin (ch, numChannels - 1);
            ringBuffer.copyFrom (ch, start2, channelData[sourceCh] + size1, size2);
        }
            
    fifo.finishedWrite (size1 + size2);
}

void SystemAudioCaptureNode::pushAudioInterleaved (const float* interleavedData, int numChannels, int numSamples)
{
    if (numChannels <= 0 || numSamples <= 0) return;

    int start1, size1, start2, size2;
    fifo.prepareToWrite (numSamples, start1, size1, start2, size2);
    
    int destChannels = ringBuffer.getNumChannels();
    
    if (size1 > 0)
        for (int ch = 0; ch < destChannels; ++ch)
        {
            int sourceCh = juce::jmin (ch, numChannels - 1);
            float* dest = ringBuffer.getWritePointer (ch, start1);
            for (int s = 0; s < size1; ++s)
                dest[s] = interleavedData[s * numChannels + sourceCh];
        }
            
    if (size2 > 0)
        for (int ch = 0; ch < destChannels; ++ch)
        {
            int sourceCh = juce::jmin (ch, numChannels - 1);
            float* dest = ringBuffer.getWritePointer (ch, start2);
            for (int s = 0; s < size2; ++s)
                dest[s] = interleavedData[(size1 + s) * numChannels + sourceCh];
        }
            
    fifo.finishedWrite (size1 + size2);
}

void SystemAudioCaptureNode::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    buffer.clear();
    double captureRate = actualSampleRate.load();
    
    // Dynamically tie latency to the user's chosen buffer size
    // E.g. at 256 samples, latency is ~5.3ms at 96kHz
    int targetCushion = buffer.getNumSamples() * 2;
    int maxCushion    = buffer.getNumSamples() * 8;
    
    int currentReady = fifo.getNumReady();
    
    // Hard reset only if we severely violate the 80ms ceiling (e.g. heavy system lag spike)
    if (currentReady > maxCushion)
    {
        int dropCount = currentReady - targetCushion;
        int start1, size1, start2, size2;
        fifo.prepareToRead (dropCount, start1, size1, start2, size2);
        fifo.finishedRead (size1 + size2);
        currentReady = targetCushion;
    }
    
    if (isBuffering)
    {
        if (currentReady >= targetCushion) {
            isBuffering = false;
        } else {
            return; // output silence while buffering
        }
    }
    
    bool needsResampling = std::abs (captureRate - outputSampleRate) > 1.0;
    
    if (needsResampling)
    {
        // ASRC mode used if base sample rates mismatch (e.g. 48kHz -> 96kHz)
        double nominalRatio = captureRate / outputSampleRate;
        
        // 0.000005 max shift per 100 samples off target. Max total shift pinned to 0.5%
        double driftCorrection = (currentReady - targetCushion) * 0.000005; 
        driftCorrection = juce::jlimit (-0.005, 0.005, driftCorrection);
        double ratio = nominalRatio + driftCorrection;
        
        int numSamplesNeededFromCapture = juce::roundToInt (buffer.getNumSamples() * ratio) + 8;
        
        jassert (resampleBuffer.getNumSamples() >= numSamplesNeededFromCapture);
        
        if (currentReady < numSamplesNeededFromCapture) {
            isBuffering = true;
            return;
        }
        
        int start1, size1, start2, size2;
        fifo.prepareToRead (numSamplesNeededFromCapture, start1, size1, start2, size2);
        
        if (size1 + size2 > 0)
        {
            int consumed = 0;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                auto* tempWrite = resampleBuffer.getWritePointer (ch);
                if (size1 > 0 && ch < ringBuffer.getNumChannels())
                    juce::FloatVectorOperations::copy (tempWrite, ringBuffer.getReadPointer(ch, start1), size1);
                if (size2 > 0 && ch < ringBuffer.getNumChannels())
                    juce::FloatVectorOperations::copy (tempWrite + size1, ringBuffer.getReadPointer(ch, start2), size2);
                
                if (ch < 2) {
                    int used = resamplers[ch].process (ratio, resampleBuffer.getReadPointer(ch), buffer.getWritePointer(ch), buffer.getNumSamples());
                    if (ch == 0) consumed = used;
                }
            }
            fifo.finishedRead (consumed);
        }
    }
    else
    {
        // Bit-perfect mode if sample rates match
        int numSamplesNeeded = buffer.getNumSamples();
        
        if (currentReady < numSamplesNeeded) {
            isBuffering = true;
            return;
        }
        
        int start1, size1, start2, size2;
        fifo.prepareToRead (numSamplesNeeded, start1, size1, start2, size2);
        
        if (size1 + size2 > 0)
        {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                if (size1 > 0 && ch < ringBuffer.getNumChannels())
                    buffer.copyFrom (ch, 0, ringBuffer.getReadPointer(ch, start1), size1);
                if (size2 > 0 && ch < ringBuffer.getNumChannels())
                    buffer.copyFrom (ch, size1, ringBuffer.getReadPointer(ch, start2), size2);
            }
            fifo.finishedRead (size1 + size2);
        }
    }
}

void SystemAudioCaptureNode::fillInPluginDescription (juce::PluginDescription& description) const
{
    description.name = getName();
    description.descriptiveName = getName();
    description.pluginFormatName = "Internal";
    description.category = "I/O";
    description.fileOrIdentifier = "SystemAudio";
    description.uniqueId = 0x53415450; // "SATP"
    description.isInstrument = false;
    description.numInputChannels = 0;
    description.numOutputChannels = 2;
}