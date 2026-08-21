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

#include "AudioRecorderNode.h"

//==============================================================================
class AudioRecorderEditor final : public juce::AudioProcessorEditor,
                                  private juce::Timer
{
public:
    explicit AudioRecorderEditor (AudioRecorderNode& owner)
        : AudioProcessorEditor (owner),
          recorderNode (owner)
    {
        setSize (360, 200);

        recordButton.setButtonText (recorderNode.isRecording() ? "Stop Recording" : "Start Recording");
        recordButton.onClick = [this]
        {
            if (recorderNode.isRecording())
                recorderNode.stopRecording();
            else
                recorderNode.startRecording();

            updateUI();
        };
        addAndMakeVisible (recordButton);

        revealButton.setButtonText ("Reveal in Finder");
        revealButton.onClick = [this]
        {
            auto file = recorderNode.getLastRecordedFile();
            if (file.existsAsFile())
                file.revealToUser();
        };
        addAndMakeVisible (revealButton);

        statusLabel.setJustificationType (juce::Justification::centred);
        statusLabel.setFont (juce::FontOptions (13.0f).withStyle ("Bold"));
        addAndMakeVisible (statusLabel);

        fileLabel.setJustificationType (juce::Justification::centred);
        fileLabel.setFont (juce::FontOptions (11.0f));
        fileLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
        addAndMakeVisible (fileLabel);

        startTimerHz (20);
        updateUI();
    }

    ~AudioRecorderEditor() override
    {
        stopTimer();
        recorderNode.editorBeingDeleted (this);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff18181c));

        g.setColour (juce::Colour (0xff2a2a32));
        g.drawRect (getLocalBounds().toFloat(), 1.0f);

        if (recorderNode.isRecording())
        {
            g.setColour (juce::Colours::red);
            g.fillEllipse ((float) getWidth() * 0.5f - 85.0f, 22.0f, 10.0f, 10.0f);
        }
    }

    void resized() override
    {
        statusLabel.setBounds (10, 16, getWidth() - 20, 22);
        recordButton.setBounds (getWidth() / 2 - 80, 48, 160, 40);
        fileLabel.setBounds (10, 100, getWidth() - 20, 36);
        revealButton.setBounds (getWidth() / 2 - 70, 148, 140, 28);
    }

private:
    void timerCallback() override
    {
        updateUI();
    }

    void updateUI()
    {
        bool rec = recorderNode.isRecording();
        recordButton.setButtonText (rec ? "Stop Recording" : "Start Recording");

        double sr = recorderNode.getSampleRate();
        if (sr <= 0.0)
            sr = 96000.0;

        int numChannels = recorderNode.getMainBusNumInputChannels();
        juce::String chStr = (numChannels == 1) ? "Mono" : (numChannels == 2) ? "Stereo" : (juce::String (numChannels) + "ch");
        
        if (rec)
        {
            auto samples = recorderNode.getRecordedSampleCount();
            double seconds = (double) samples / sr;
            int mins = (int) seconds / 60;
            int secs = (int) seconds % 60;
            int ms = (int) ((seconds - (int) seconds) * 100);

            statusLabel.setText (juce::String::formatted ("RECORDING [%02d:%02d.%02d]  (%s)", mins, secs, ms, chStr.toRawUTF8()), juce::dontSendNotification);
            statusLabel.setColour (juce::Label::textColourId, juce::Colours::red);
        }
        else
        {
            statusLabel.setText ("IDLE  (" + chStr + " @ " + juce::String ((int) sr) + " Hz)", juce::dontSendNotification);
            statusLabel.setColour (juce::Label::textColourId, juce::Colours::white);
        }

        auto file = recorderNode.getLastRecordedFile();
        if (file.existsAsFile())
        {
            fileLabel.setText (file.getFullPathName(), juce::dontSendNotification);
            revealButton.setEnabled (true);
        }
        else
        {
            fileLabel.setText ("Audio will save to Desktop as 32-bit Float WAV", juce::dontSendNotification);
            revealButton.setEnabled (false);
        }

        repaint();
    }

    AudioRecorderNode& recorderNode;
    juce::TextButton recordButton;
    juce::TextButton revealButton;
    juce::Label statusLabel;
    juce::Label fileLabel;
};

//==============================================================================
AudioRecorderNode::AudioRecorderNode()
    : AudioPluginInstance (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)),
      juce::Thread ("CurveAudioRecorderThread")
{
}

AudioRecorderNode::~AudioRecorderNode()
{
    stopRecording();
}

void AudioRecorderNode::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 96000.0;
    int numChans = juce::jmax (1, getMainBusNumInputChannels());
    fifo.reset();
    fifoBuffer.setSize (numChans, fifoCapacity);
    fifoBuffer.clear();
}

void AudioRecorderNode::releaseResources()
{
    stopRecording();
}

void AudioRecorderNode::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (isRecordingFlag.load (std::memory_order_relaxed))
    {
        int numSamples = buffer.getNumSamples();
        int numChannels = juce::jmin (buffer.getNumChannels(), fifoBuffer.getNumChannels());

        int start1, size1, start2, size2;
        fifo.prepareToWrite (numSamples, start1, size1, start2, size2);

        if (size1 > 0)
        {
            for (int ch = 0; ch < numChannels; ++ch)
                fifoBuffer.copyFrom (ch, start1, buffer.getReadPointer (ch), size1);
        }

        if (size2 > 0)
        {
            for (int ch = 0; ch < numChannels; ++ch)
                fifoBuffer.copyFrom (ch, start2, buffer.getReadPointer (ch, size1), size2);
        }

        fifo.finishedWrite (size1 + size2);
        samplesRecorded.fetch_add (size1 + size2, std::memory_order_relaxed);
    }
}

void AudioRecorderNode::startRecording()
{
    if (isRecordingFlag.load (std::memory_order_acquire))
        return;

    int numChans = juce::jmax (1, getMainBusNumInputChannels());
    fifo.reset();
    fifoBuffer.setSize (numChans, fifoCapacity);
    fifoBuffer.clear();
    samplesRecorded.store (0, std::memory_order_release);

    auto desktopDir = juce::File::getSpecialLocation (juce::File::userDesktopDirectory);
    auto timeString = juce::Time::getCurrentTime().formatted ("%Y-%m-%d_%H%M%S");
    lastRecordedFile = desktopDir.getChildFile ("Curve_Capture_" + juce::String ((int) currentSampleRate) + "Hz_" + timeString + ".wav");

    if (lastRecordedFile.existsAsFile())
        lastRecordedFile.deleteFile();

    auto outStream = lastRecordedFile.createOutputStream();
    if (outStream != nullptr)
    {
        juce::WavAudioFormat wavFormat;
        juce::AudioFormatWriterOptions options;
        options = options.withSampleRate (currentSampleRate)
                         .withNumChannels (numChans)
                         .withBitsPerSample (32);

        std::unique_ptr<juce::OutputStream> streamPtr (outStream.release());
        auto newWriter = wavFormat.createWriterFor (streamPtr, options);
        if (newWriter != nullptr)
        {
            const juce::ScopedLock sl (writerLock);
            writer = std::move (newWriter);
            isRecordingFlag.store (true, std::memory_order_release);
            startThread();
        }
    }
}

void AudioRecorderNode::stopRecording()
{
    if (! isRecordingFlag.load (std::memory_order_acquire))
        return;

    isRecordingFlag.store (false, std::memory_order_release);
    stopThread (2000);

    // Flush any remaining samples in FIFO
    int ready = fifo.getNumReady();
    if (ready > 0)
    {
        int start1, size1, start2, size2;
        fifo.prepareToRead (ready, start1, size1, start2, size2);

        const juce::ScopedLock sl (writerLock);
        if (writer != nullptr)
        {
            if (size1 > 0)
                writer->writeFromAudioSampleBuffer (fifoBuffer, start1, size1);
            if (size2 > 0)
                writer->writeFromAudioSampleBuffer (fifoBuffer, start2, size2);
        }
        fifo.finishedRead (size1 + size2);
    }

    {
        const juce::ScopedLock sl (writerLock);
        writer.reset();
    }
}

void AudioRecorderNode::run()
{
    while (! threadShouldExit())
    {
        int ready = fifo.getNumReady();
        if (ready >= 512)
        {
            int start1, size1, start2, size2;
            fifo.prepareToRead (ready, start1, size1, start2, size2);

            const juce::ScopedLock sl (writerLock);
            if (writer != nullptr)
            {
                if (size1 > 0)
                    writer->writeFromAudioSampleBuffer (fifoBuffer, start1, size1);
                if (size2 > 0)
                    writer->writeFromAudioSampleBuffer (fifoBuffer, start2, size2);
            }
            fifo.finishedRead (size1 + size2);
        }
        else
        {
            wait (5);
        }
    }
}

juce::AudioProcessorEditor* AudioRecorderNode::createEditor()
{
    return new AudioRecorderEditor (*this);
}

bool AudioRecorderNode::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto inSet  = layouts.getMainInputChannelSet();
    const auto outSet = layouts.getMainOutputChannelSet();

    if (inSet.isDisabled() || inSet.size() == 0)
        return false;

    return outSet.isDisabled() || outSet.size() == 0;
}

void AudioRecorderNode::processorLayoutsChanged()
{
    int numChans = juce::jmax (1, getMainBusNumInputChannels());
    fifo.reset();
    fifoBuffer.setSize (numChans, fifoCapacity);
    fifoBuffer.clear();
}

void AudioRecorderNode::fillInPluginDescription (juce::PluginDescription& description) const
{
    description.name = getName();
    description.descriptiveName = getName();
    description.pluginFormatName = "Internal";
    description.category = "Plugins";
    description.fileOrIdentifier = "AudioRecorder";
    description.uniqueId = 0x52454344; // "RECD"
    description.isInstrument = false;
    description.numInputChannels = getMainBusNumInputChannels();
    description.numOutputChannels = 0;
}
