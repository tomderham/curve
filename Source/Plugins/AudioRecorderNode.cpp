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
#include "../UI/CurveFolders.h"
#include "../UI/CurveColours.h"

namespace
{
    // Where the recording is, as the audio thread sees it
    enum RecordState : int { idle = 0, startRequested = 1, recording = 2 };

    constexpr const char* recentRecordingsKey = "recentRecordings";
    constexpr int maxRecentRecordings = 30;

    juce::String formatTime (double seconds)
    {
        const auto total = juce::jmax (0.0, seconds);
        const auto mins = (int) (total / 60.0);
        const auto secs = total - mins * 60.0;
        return juce::String::formatted ("%02d:%04.1f", mins, secs);
    }
}

//==============================================================================
/*  The recorder's window: a record button, what it's doing and for how long, a scrolling waveform of
    what it hears, its options as a form, and the last recording, to open, drag out, rename, show or
    move to the Trash.
*/
class AudioRecorderEditor final : public juce::AudioProcessorEditor,
                                  public juce::DragAndDropContainer,
                                  private juce::Timer
{
public:
    explicit AudioRecorderEditor (AudioRecorderNode& owner)
        : AudioProcessorEditor (owner), recorder (owner)
    {
        recordButton.onClick = [this]
        {
            // Through the parameter, so the button can be MIDI learned
            auto& record = recorder.getRecordParameter();
            record.beginChangeGesture();
            record.set (recorder.isRecording() ? 0.0f : 1.0f);
            record.endChangeGesture();
        };
        recordButton.isRecording = [this] { return recorder.isRecording(); };
        addAndMakeVisible (recordButton);

        statusLabel.setFont (juce::FontOptions (16.0f, juce::Font::bold));
        detailLabel.setFont (juce::FontOptions (12.5f));
        detailLabel.setColour (juce::Label::textColourId, Curve::Colours::dimText);

        // In figures of one width, so it doesn't jitter as it counts
        timeLabel.setFont (juce::FontOptions (30.0f).withStyle ("Light").withFeatureEnabled ("tnum"));
        timeLabel.setJustificationType (juce::Justification::centredRight);

        for (auto* l : { &statusLabel, &timeLabel, &detailLabel })
        {
            l->setBorderSize ({});
            addAndMakeVisible (l);
        }

        // The form: a label down the left of each row
        for (auto [label, text] : { std::pair { &nameLabel, "Name" }, { &formatLabel, "Format" },
                                    { &includeLabel, "Include" }, { &autoLabel, "Auto" } })
        {
            label->setText (text, juce::dontSendNotification);
            label->setFont (juce::FontOptions (13.0f));
            label->setColour (juce::Label::textColourId, Curve::Colours::dimText);
            label->setJustificationType (juce::Justification::centredRight);
            addAndMakeVisible (label);
        }

        nameEditor.setTextToShowWhenEmpty ("Curve Recording, then the date and time", Curve::Colours::dimText.withAlpha (0.7f));
        nameEditor.setFont (juce::FontOptions (13.5f));
        nameEditor.setIndents (8, 5);
        nameEditor.setTooltip ("What recordings are called. With a name, e.g. Verse, they're numbered: Verse 1, Verse 2...");
        nameEditor.setText (recorder.getFileName(), juce::dontSendNotification);
        nameEditor.onTextChange = [this] { recorder.setFileName (nameEditor.getText()); };
        nameEditor.onReturnKey = [this] { unfocusAllComponents(); };
        addAndMakeVisible (nameEditor);

        formatBox.addItemList (AudioRecorderNode::getFormatNames(), 1);
        formatBox.setTooltip ("WAV and AIFF open anywhere. FLAC is lossless, and about half the size");
        formatBox.onChange = [this]
        {
            recorder.setFormat ((AudioRecorderNode::Format) formatBox.getSelectedItemIndex());
        };
        addAndMakeVisible (formatBox);

        includeBox.addItemList ({ "Only what's played after Record",
                                  "Also the 10 seconds before Record",
                                  "Also the 30 seconds before Record",
                                  "Also the 60 seconds before Record" }, 1);
        includeBox.setTooltip ("It can listen all the time, keeping the last few seconds, so when you press Record after "
                               "playing something you liked, the recording starts from before you pressed it");
        includeBox.onChange = [this]
        {
            static const int seconds[] = { 0, 10, 30, 60 };
            recorder.setHistorySeconds (seconds[juce::jlimit (0, 3, includeBox.getSelectedItemIndex())]);
        };
        addAndMakeVisible (includeBox);

        autoSwitch.setButtonText ("Start when there's sound, stop after");
        autoSwitch.setClickingTogglesState (false);
        autoSwitch.setTooltip ("Records by itself: starts when sound begins, with what was kept before it, and stops after "
                               "a silence. In MIDI Learn, click to map a control to it");
        autoSwitch.onClick = [this]
        {
            auto& p = recorder.getAutoParameter();
            p.beginChangeGesture();
            p.set (p.isOn() ? 0.0f : 1.0f);
            p.endChangeGesture();
        };
        addAndMakeVisible (autoSwitch);

        silenceBox.addItemList ({ "2 s", "5 s", "10 s", "30 s", "60 s" }, 1);
        silenceBox.setTooltip ("In Auto, how long a silence ends a recording");
        silenceBox.onChange = [this]
        {
            static const int seconds[] = { 2, 5, 10, 30, 60 };
            recorder.setSilenceSeconds (seconds[juce::jlimit (0, 4, silenceBox.getSelectedItemIndex())]);
        };
        addAndMakeVisible (silenceBox);

        silenceLabel.setText ("of silence", juce::dontSendNotification);
        silenceLabel.setFont (juce::FontOptions (13.0f));
        addAndMakeVisible (silenceLabel);

        recentHeader.setText ("LAST RECORDING", juce::dontSendNotification);
        recentHeader.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        recentHeader.setColour (juce::Label::textColourId, Curve::Colours::dimText);
        recentHeader.setBorderSize ({});
        addAndMakeVisible (recentHeader);

        revealFolder.setButtonText ("Show in Finder");
        revealFolder.setTooltip ("The last recording in the Finder, or the recordings folder, chosen in Preferences > Files");
        revealFolder.onClick = [this]
        {
            if (lastRow.file.existsAsFile())
                lastRow.file.revealToUser();
            else
                CurveFolders::get (CurveFolders::Kind::recordings).revealToUser();
        };
        addAndMakeVisible (revealFolder);

        lastRow.setTooltip ("Double-click to open it, drag it into another app, or right-click for more");
        addAndMakeVisible (lastRow);

        refreshOptions();
        refreshRecent();
        update();

        setResizable (true, false);
        setResizeLimits (440, 440, 1200, 900);
        setSize (500, 440);
        startTimerHz (30);
    }

    ~AudioRecorderEditor() override
    {
        stopTimer();
        recorder.editorBeingDeleted (this);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Curve::Colours::canvas);

        // The waveform: what it's heard lately, newest at the right, red while recording
        const auto area = waveformArea.toFloat();
        g.setColour (Curve::Colours::surface);
        g.fillRoundedRectangle (area, 8.0f);

        const auto inner = area.reduced (6.0f, 6.0f);
        const auto mid = inner.getCentreY();
        const auto columnWidth = inner.getWidth() / (float) peaks.size();

        g.setColour (Curve::Colours::edge.withAlpha (0.6f));
        g.fillRect (inner.getX(), mid - 0.5f, inner.getWidth(), 1.0f);

        for (size_t i = 0; i < peaks.size(); ++i)
        {
            const auto& column = peaks[(peakIndex + i) % peaks.size()];

            if (column.level <= 0.0f)
                continue;

            // Louder is taller, on a scale like the ear's, so quiet playing still shows
            const auto db = juce::Decibels::gainToDecibels (column.level, -60.0f);
            const auto height = juce::jmax (1.0f, (db + 60.0f) / 60.0f * inner.getHeight() * 0.5f);

            g.setColour (column.recorded ? Curve::Colours::danger.withAlpha (0.9f) : Curve::Colours::dimText.withAlpha (0.5f));
            g.fillRoundedRectangle (inner.getX() + (float) i * columnWidth, mid - height,
                                    juce::jmax (1.0f, columnWidth - 1.0f), height * 2.0f, 0.75f);
        }

        // The options, as a card
        g.setColour (Curve::Colours::surface);
        g.fillRoundedRectangle (formArea.toFloat(), 8.0f);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (18, 16);

        auto top = r.removeFromTop (60);
        recordButton.setBounds (top.removeFromLeft (60));
        top.removeFromLeft (14);
        timeLabel.setBounds (top.removeFromRight (150));
        top.removeFromRight (8);
        statusLabel.setBounds (top.removeFromTop (30).withTrimmedTop (6));
        detailLabel.setBounds (top.removeFromTop (22));

        r.removeFromTop (14);
        waveformArea = r.removeFromTop (60);

        r.removeFromTop (14);
        formArea = r.removeFromTop (4 * rowHeight + 20);
        auto form = formArea.reduced (14, 10);

        const auto row = [&form] (juce::Label& label)
        {
            auto line = form.removeFromTop (rowHeight);
            label.setBounds (line.removeFromLeft (labelWidth));
            line.removeFromLeft (10);
            return line.withSizeKeepingCentre (line.getWidth(), 26);
        };

        nameEditor.setBounds (row (nameLabel));
        formatBox.setBounds (row (formatLabel).withWidth (180));
        includeBox.setBounds (row (includeLabel).withWidth (250));

        auto autoRow = row (autoLabel);
        autoSwitch.setBounds (autoRow.removeFromLeft (juce::jmin (autoRow.getWidth() - 150, 250)));
        autoRow.removeFromLeft (6);
        silenceBox.setBounds (autoRow.removeFromLeft (70));
        autoRow.removeFromLeft (6);
        silenceLabel.setBounds (autoRow);

        r.removeFromTop (16);
        auto header = r.removeFromTop (26);
        revealFolder.setBounds (header.removeFromRight (116));
        recentHeader.setBounds (header);
        r.removeFromTop (6);
        lastRow.setBounds (r.removeFromTop (40));
    }

private:
    static constexpr int rowHeight = 34, labelWidth = 58;

    //==============================================================================
    struct RecordButton final : public juce::Button
    {
        RecordButton() : Button ("Record") { setTooltip ("Start or stop recording. In MIDI Learn, click to map a control to it"); }
        std::function<bool()> isRecording;
        float pulse = 0.0f;

        void paintButton (juce::Graphics& g, bool highlighted, bool down) override
        {
            const auto bounds = getLocalBounds().toFloat().reduced (2.0f);
            const bool recording = isRecording != nullptr && isRecording();

            // A ring, then a red dot to record, or a square to stop, that pulses while recording
            g.setColour (juce::Colours::white.withAlpha (highlighted ? 0.35f : 0.22f));
            g.drawEllipse (bounds.reduced (1.5f), 3.0f);

            auto red = Curve::Colours::danger.withMultipliedBrightness (down ? 0.8f : 1.0f);

            if (recording)
            {
                g.setColour (red.withAlpha (0.25f + 0.25f * pulse));
                g.fillEllipse (bounds.reduced (6.0f));
                g.setColour (red);
                g.fillRoundedRectangle (bounds.withSizeKeepingCentre (bounds.getWidth() * 0.36f, bounds.getHeight() * 0.36f), 4.0f);
            }
            else
            {
                g.setColour (red);
                g.fillEllipse (bounds.reduced (bounds.getWidth() * 0.22f));
            }
        }
    };

    struct Peak { float level = 0.0f; bool recorded = false; };

    void timerCallback() override
    {
        // One column of the waveform per tick
        peaks[peakIndex] = { recorder.readPeak(), recorder.isRecording() };
        peakIndex = (peakIndex + 1) % peaks.size();

        recordButton.pulse = 0.5f + 0.5f * std::sin ((float) juce::Time::getMillisecondCounter() * 0.006f);
        recordButton.repaint();

        update();
        repaint (waveformArea);

        // A recording just finished: it's the last one
        if (wasRecording != recorder.isRecording())
            refreshRecent();

        wasRecording = recorder.isRecording();
    }

    void update()
    {
        const auto sampleRate = recorder.getRecorderSampleRate();
        const bool rec = recorder.isRecording();
        const bool autoOn = recorder.getAutoParameter().isOn();

        const auto error = rec ? juce::String() : recorder.getWriteError();
        statusLabel.setText (rec ? "Recording" : error.isNotEmpty() ? "Recording stopped" : autoOn ? "Waiting for sound" : "Ready to record",
                             juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, rec || error.isNotEmpty() ? Curve::Colours::danger
                                                          : autoOn ? Curve::Colours::warning : Curve::Colours::text);
        statusLabel.setTooltip (error);

        timeLabel.setText (formatTime (rec ? (double) recorder.getRecordedSampleCount() / sampleRate : 0.0), juce::dontSendNotification);
        timeLabel.setColour (juce::Label::textColourId, rec ? Curve::Colours::text : Curve::Colours::dimText);

        const auto numChannels = recorder.getMainBusNumInputChannels();
        const auto channels = numChannels == 1 ? juce::String ("Mono") : numChannels == 2 ? juce::String ("Stereo") : juce::String (numChannels) + " channels";
        detailLabel.setText (rec ? recorder.getLastRecordedFile().getFileName()
                                 : error.isNotEmpty() ? error
                                 : channels + "  " + juce::String::fromUTF8 ("\xc2\xb7") + "  " + juce::String (sampleRate / 1000.0, 1) + " kHz",
                             juce::dontSendNotification);

        autoSwitch.setToggleState (autoOn, juce::dontSendNotification);

        // Not while recording: the file's name and format are set, and what's kept is being written
        for (auto* c : std::initializer_list<juce::Component*> { &nameEditor, &formatBox, &includeBox })
            c->setEnabled (! rec);
    }

    void refreshOptions()
    {
        static const int historyChoices[] = { 0, 10, 30, 60 };
        static const int silenceChoices[] = { 2, 5, 10, 30, 60 };

        for (int i = 0; i < 4; ++i)
            if (historyChoices[i] == recorder.getHistorySeconds())
                includeBox.setSelectedItemIndex (i, juce::dontSendNotification);

        for (int i = 0; i < 5; ++i)
            if (silenceChoices[i] == recorder.getSilenceSeconds())
                silenceBox.setSelectedItemIndex (i, juce::dontSendNotification);

        formatBox.setSelectedItemIndex ((int) recorder.getFormat(), juce::dontSendNotification);
    }

    // The last recording made, by any recorder, even before Curve was last opened
    void refreshRecent()
    {
        lastRow.file = AudioRecorderNode::getRecentRecordings()[0];
        lastRow.repaint();
    }

    //==============================================================================
    // The last recording, to open, drag into another app, rename, show, or move to the Trash
    struct RecordingRow final : public juce::Component,
                                public juce::SettableTooltipClient
    {
        explicit RecordingRow (AudioRecorderEditor& e) : editor (e) {}

        AudioRecorderEditor& editor;
        juce::File file;

        void paint (juce::Graphics& g) override
        {
            g.setColour (Curve::Colours::surface);
            g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);

            auto r = getLocalBounds().reduced (12, 0);

            if (! file.existsAsFile())
            {
                g.setFont (juce::FontOptions (13.0f));
                g.setColour (Curve::Colours::dimText);
                g.drawText ("None yet", r, juce::Justification::centredLeft);
                return;
            }

            const auto modified = file.getLastModificationTime();
            const auto isToday = modified.toString (true, false) == juce::Time::getCurrentTime().toString (true, false);

            g.setFont (juce::FontOptions (12.0f).withFeatureEnabled ("tnum"));
            g.setColour (Curve::Colours::dimText);
            g.drawText (modified.formatted (isToday ? "Today %H:%M" : "%d %b %H:%M"), r.removeFromRight (90), juce::Justification::centredRight);
            g.drawText (juce::File::descriptionOfSizeInBytes (file.getSize()), r.removeFromRight (70), juce::Justification::centredRight);
            r.removeFromRight (10);

            g.setFont (juce::FontOptions (13.5f));
            g.setColour (file.existsAsFile() ? Curve::Colours::text : Curve::Colours::dimText);
            g.drawText (file.getFileName(), r, juce::Justification::centredLeft, true);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu() && file.existsAsFile())
                editor.showRecordingMenu (file);
        }

        void mouseDoubleClick (const juce::MouseEvent&) override
        {
            if (file.existsAsFile())
                file.startAsProcess();
        }

        // Into a DAW, the Finder, or the Player
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (! dragging && e.getDistanceFromDragStart() > 4 && file.existsAsFile())
            {
                dragging = true;
                // The row may be gone when the drag ends, e.g. the window closed meanwhile
                juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this,
                                                                            [safeThis = SafePointer<RecordingRow> (this)]
                                                                            {
                                                                                if (safeThis != nullptr)
                                                                                    safeThis->dragging = false;
                                                                            });
            }
        }

        void mouseUp (const juce::MouseEvent&) override { dragging = false; }

        bool dragging = false;
    };

    void showRecordingMenu (const juce::File& file)
    {
        juce::PopupMenu menu;
        menu.addItem ("Open", file.existsAsFile(), false, [file] { file.startAsProcess(); });
        menu.addItem ("Show in Finder", file.existsAsFile(), false, [file] { file.revealToUser(); });
        menu.addItem ("Rename...", file.existsAsFile(), false, [this, file] { rename (file); });
        menu.addSeparator();
        menu.addItem ("Move to Trash", file.existsAsFile(), false, [this, file]
        {
            file.moveToTrash();
            AudioRecorderNode::forgetRecording (file);
            refreshRecent();
        });

        menu.showMenuAsync (juce::PopupMenu::Options().withDeletionCheck (*this));
    }

    void rename (const juce::File& file)
    {
        renameWindow = std::make_unique<juce::AlertWindow> ("Rename Recording", juce::String(), juce::MessageBoxIconType::NoIcon, this);
        renameWindow->addTextEditor ("name", file.getFileNameWithoutExtension());
        renameWindow->addButton ("Rename", 1, juce::KeyPress (juce::KeyPress::returnKey));
        renameWindow->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        renameWindow->enterModalState (true, juce::ModalCallbackFunction::create ([safeThis = SafePointer<AudioRecorderEditor> (this), file] (int result)
        {
            if (safeThis == nullptr || safeThis->renameWindow == nullptr)
                return;

            const auto name = safeThis->renameWindow->getTextEditorContents ("name");
            safeThis->renameWindow.reset();

            if (result != 1)
                return;

            if (AudioRecorderNode::renameRecording (file, name))
                safeThis->refreshRecent();
            else
            {
                juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't Rename the Recording",
                                                             "There may already be a file called that in its folder.");
            }
        }), false);
    }

    AudioRecorderNode& recorder;
    RecordButton recordButton;
    juce::Label statusLabel, timeLabel, detailLabel, recentHeader;
    juce::Label nameLabel, formatLabel, includeLabel, autoLabel, silenceLabel;
    juce::TextEditor nameEditor;
    juce::ComboBox includeBox, silenceBox, formatBox;
    juce::ToggleButton autoSwitch;
    juce::TextButton revealFolder;
    RecordingRow lastRow { *this };
    std::unique_ptr<juce::AlertWindow> renameWindow;

    juce::Rectangle<int> waveformArea, formArea;
    std::array<Peak, 240> peaks {}; // 8 seconds at 30 a second
    size_t peakIndex = 0;
    bool wasRecording = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioRecorderEditor)
};

//==============================================================================
AudioRecorderNode::AudioRecorderNode()
    : AudioPluginInstance (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)),
      juce::Thread ("CurveAudioRecorderThread")
{
    // Not automatable: they're transport controls, not part of the sound
    auto record = InternalParameter::makeBool ("record", "Record", false, false);
    recordParam = record.get();
    addHostedParameter (std::move (record));
    recordParam->addListener (this);

    auto autoRecord = InternalParameter::makeBool ("autoRecord", "Auto", false, false);
    autoParam = autoRecord.get();
    addHostedParameter (std::move (autoRecord));

    startTimerHz (10);
}

AudioRecorderNode::~AudioRecorderNode()
{
    stopTimer();
    recordParam->removeListener (this);
    cancelPendingUpdate();
    stopRecording();
}

juce::StringArray AudioRecorderNode::getFormatNames()
{
    return { "WAV 16-bit", "WAV 24-bit", "WAV 32-bit Float", "FLAC 16-bit", "FLAC 24-bit", "AIFF 24-bit" };
}

void AudioRecorderNode::handleAsyncUpdate()
{
    if (recordParam->isOn() && ! isRecording())
        startRecording();
    else if (! recordParam->isOn() && isRecording())
        stopRecording();

    // e.g. the file couldn't be created
    if (recordParam->isOn() != isRecording())
        recordParam->set (isRecording() ? 1.0f : 0.0f);
}

// Room for ten seconds of what comes in while the writer catches up, e.g. writing what was kept from
// before Record, or a slow disk: no less than before, and no more than 64 MB, however many channels
void AudioRecorderNode::allocateFifo()
{
    const auto numChannels = juce::jlimit (1, maxRecorderChannels, getMainBusNumInputChannels());
    const auto most = (64 * 1024 * 1024 / (int) sizeof (float)) / numChannels;
    const auto samples = juce::jlimit (fifoCapacity, juce::jmax (fifoCapacity, most), (int) (currentSampleRate * 10.0));

    // Made off the audio thread, then swapped in under its lock, as the history is
    juce::AudioBuffer<float> newBuffer (numChannels, samples);
    newBuffer.clear();

    const juce::ScopedLock sl (getCallbackLock());
    std::swap (fifoBuffer, newBuffer);
    fifo.setTotalSize (samples);
    fifo.reset();
}

void AudioRecorderNode::setHistorySeconds (int seconds)
{
    seconds = juce::jlimit (0, 60, seconds);

    if (seconds == historySeconds || isRecording())
        return;

    historySeconds = seconds;
    allocateHistory();
}

// Made off the audio thread, then swapped in under its lock, so it only waits for the swap
void AudioRecorderNode::allocateHistory()
{
    juce::AudioBuffer<float> newBuffer;

    if (historySeconds > 0)
        newBuffer.setSize (juce::jmax (1, getMainBusNumInputChannels()),
                           (int) (currentSampleRate * (historySeconds + historyMarginSeconds)));

    newBuffer.clear();

    {
        const juce::ScopedLock sl (getCallbackLock());
        std::swap (historyBuffer, newBuffer);
        historyWritten.store (0);
    }
}

void AudioRecorderNode::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    const auto rateChanged = sampleRate > 0.0 && sampleRate != currentSampleRate;
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 96000.0;

    if (isRecording())
        stopRecording();

    allocateFifo();

    if (rateChanged || historyBuffer.getNumChannels() != juce::jmax (1, getMainBusNumInputChannels()) || (historySeconds > 0) != (historyBuffer.getNumSamples() > 0))
        allocateHistory();
}

void AudioRecorderNode::releaseResources()
{
    stopRecording();
}

void AudioRecorderNode::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = juce::jmin (buffer.getNumChannels(), fifoBuffer.getNumChannels());

    // What it hears, for Auto and the waveform
    float peak = 0.0f;

    for (int ch = 0; ch < numChannels; ++ch)
        peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, numSamples));

    if (std::isfinite (peak))
    {
        auto held = peakSinceRead.load (std::memory_order_relaxed);
        while (peak > held && ! peakSinceRead.compare_exchange_weak (held, peak, std::memory_order_relaxed)) {}

        if (peak > juce::Decibels::decibelsToGain (soundThresholdDb))
        {
            samplesSinceSound.store (0, std::memory_order_relaxed);
            heardSound.store (true, std::memory_order_relaxed);
        }
        else
        {
            samplesSinceSound.fetch_add (numSamples, std::memory_order_relaxed);
        }
    }

    // A recording starts here: what's kept so far is what came before it
    if (markRequested.exchange (false, std::memory_order_acq_rel))
        historyMark.store (historyWritten.load (std::memory_order_relaxed), std::memory_order_release);

    // What it hears, round and round
    if (const auto size = historyBuffer.getNumSamples(); size > 0)
    {
        const auto written = historyWritten.load (std::memory_order_relaxed);
        auto pos = (int) (written % size);
        int done = 0;

        while (done < numSamples)
        {
            const auto chunk = juce::jmin (numSamples - done, size - pos);

            for (int ch = 0; ch < juce::jmin (numChannels, historyBuffer.getNumChannels()); ++ch)
                historyBuffer.copyFrom (ch, pos, buffer, ch, done, chunk);

            done += chunk;
            pos = 0;
        }

        historyWritten.store (written + numSamples, std::memory_order_release);
    }

    // Acquired, so the reset of the FIFO and the mark, made before it was set, are seen with it
    if (isRecordingFlag.load (std::memory_order_acquire) && historyMark.load (std::memory_order_acquire) >= 0)
    {
        int start1, size1, start2, size2;
        fifo.prepareToWrite (numSamples, start1, size1, start2, size2);

        if (size1 > 0)
            for (int ch = 0; ch < numChannels; ++ch)
                fifoBuffer.copyFrom (ch, start1, buffer.getReadPointer (ch), size1);

        if (size2 > 0)
            for (int ch = 0; ch < numChannels; ++ch)
                fifoBuffer.copyFrom (ch, start2, buffer.getReadPointer (ch, size1), size2);

        fifo.finishedWrite (size1 + size2);
        samplesRecorded.fetch_add (size1 + size2, std::memory_order_relaxed);
    }
}

void AudioRecorderNode::startRecording()
{
    if (isRecordingFlag.load (std::memory_order_acquire))
        return;

    writeError.clear();
    writeFailed = false;

    const auto numChans = juce::jlimit (1, maxRecorderChannels, getMainBusNumInputChannels());
    fifo.reset();
    fifoBuffer.clear();
    samplesRecorded.store (0, std::memory_order_release);

    // In the folder chosen in Preferences > Files, by default the Desktop
    std::unique_ptr<juce::AudioFormat> audioFormat;
    int bits = 24;
    auto sampleFormat = juce::AudioFormatWriterOptions::SampleFormat::integral;

    switch (format)
    {
        case Format::wav16:      audioFormat = std::make_unique<juce::WavAudioFormat>();  bits = 16; break;
        case Format::wav24:      audioFormat = std::make_unique<juce::WavAudioFormat>();  bits = 24; break;
        case Format::wav32Float: audioFormat = std::make_unique<juce::WavAudioFormat>();  bits = 32;
                                 sampleFormat = juce::AudioFormatWriterOptions::SampleFormat::floatingPoint; break;
        case Format::flac16:     audioFormat = std::make_unique<juce::FlacAudioFormat>(); bits = 16; break;
        case Format::flac24:     audioFormat = std::make_unique<juce::FlacAudioFormat>(); bits = 24; break;
        case Format::aiff24:     audioFormat = std::make_unique<juce::AiffAudioFormat>(); bits = 24; break;
    }

    lastRecordedFile = getNextFile();

    auto outStream = lastRecordedFile.createOutputStream();

    if (outStream == nullptr)
        return;

    const auto options = juce::AudioFormatWriterOptions().withSampleRate (currentSampleRate)
                                                         .withNumChannels (numChans)
                                                         .withBitsPerSample (bits)
                                                         .withSampleFormat (sampleFormat);

    std::unique_ptr<juce::OutputStream> stream (outStream.release());
    auto newWriter = audioFormat->createWriterFor (stream, options);

    if (newWriter == nullptr)
    {
        lastRecordedFile.deleteFile();
        return;
    }

    {
        const juce::ScopedLock sl (writerLock);
        writer = std::move (newWriter);
    }

    // The audio thread marks where it starts, then records from there. Recording is on before the mark
    // is asked for, so the block that marks it is recorded: until then, unmarked, blocks aren't
    historyMark.store (-1, std::memory_order_release);
    startRequestTime = juce::Time::getMillisecondCounter();
    isRecordingFlag.store (true, std::memory_order_release);
    markRequested.store (true, std::memory_order_release);
    startThread();

    if (! recordParam->isOn())
        recordParam->set (1.0f);
}

namespace
{
    juce::String getExtension (AudioRecorderNode::Format f)
    {
        using F = AudioRecorderNode::Format;
        return f == F::flac16 || f == F::flac24 ? ".flac" : f == F::aiff24 ? ".aif" : ".wav";
    }
}

juce::File AudioRecorderNode::getNextFile() const
{
    const auto folder = CurveFolders::get (CurveFolders::Kind::recordings);
    const auto extension = getExtension (format);

    if (fileName.isEmpty())
        return folder.getChildFile ("Curve Recording " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H.%M.%S") + extension)
                     .getNonexistentSibling();

    // The number after the highest there is, whatever its format, so takes count up in order
    int highest = 0;

    for (const auto& f : folder.findChildFiles (juce::File::findFiles, false, fileName + " *"))
    {
        const auto rest = f.getFileNameWithoutExtension().substring (fileName.length() + 1);

        if (rest.isNotEmpty() && rest.containsOnly ("0123456789"))
            highest = juce::jmax (highest, rest.getIntValue());
    }

    return folder.getChildFile (fileName + " " + juce::String (highest + 1) + extension).getNonexistentSibling();
}

void AudioRecorderNode::stopRecording()
{
    // Recording can also stop for device or layout changes. Clear the parameter
    // first, so a pending update can't start it again.
    if (recordParam->isOn())
        recordParam->set (0.0f);

    if (! isRecordingFlag.load (std::memory_order_acquire))
        return;

    isRecordingFlag.store (false, std::memory_order_release);
    markRequested.store (false, std::memory_order_release);
    stopThread (4000);

    {
        const juce::ScopedLock sl (writerLock);
        writer.reset();
    }

    rememberRecording (lastRecordedFile);
}

void AudioRecorderNode::timerCallback()
{
    // A write that failed, e.g. the disk's full: stopped, and said, rather than recording nothing
    if (writeFailed.exchange (false) && isRecording())
    {
        stopRecording();
        writeError = "Couldn't write to " + lastRecordedFile.getFileName() + ". Is the disk full, or gone?";
        startedByAuto = false;
    }

    // Without audio running, nothing marks where the recording starts: soon, it starts anyway, without
    // what's kept
    if (isRecording() && historyMark.load() < 0 && juce::Time::getMillisecondCounter() - startRequestTime > 300)
    {
        markRequested.store (false);
        historyMark.store (historyWritten.load());
    }

    if (! autoParam->isOn())
    {
        heardSound.store (false);
        startedByAuto = false;
        return;
    }

    if (! isRecording() && heardSound.exchange (false))
    {
        startRecording();
        startedByAuto = isRecording();
    }
    else if (isRecording() && startedByAuto
             && samplesSinceSound.load() > (juce::int64) (silenceSeconds * currentSampleRate))
    {
        stopRecording();
        startedByAuto = false;
        heardSound.store (false);
    }
}

// What was kept from before the recording started, from the mark back
void AudioRecorderNode::writeHistory()
{
    const auto mark = historyMark.load (std::memory_order_acquire);
    const auto size = (juce::int64) historyBuffer.getNumSamples();

    if (historySeconds <= 0 || size == 0 || mark <= 0)
        return;

    const auto wanted = (juce::int64) (historySeconds * currentSampleRate);
    auto length = juce::jmin (wanted, mark, size - (juce::int64) (historyMarginSeconds * currentSampleRate));

    if (length <= 0)
        return;

    auto from = mark - length;

    const juce::ScopedLock sl (writerLock);

    if (writer == nullptr)
        return;

    // In chunks, so it stops soon if the recording does, and a minute of FLAC doesn't hold it for long
    while (length > 0 && ! threadShouldExit())
    {
        const auto pos = (int) (from % size);
        const auto chunk = (int) juce::jmin (length, size - pos, (juce::int64) 8192);

        if (! writer->writeFromAudioSampleBuffer (historyBuffer, pos, chunk))
        {
            writeFailed = true;
            return;
        }

        from += chunk;
        length -= chunk;
        samplesRecorded.fetch_add (chunk, std::memory_order_relaxed);
    }
}

void AudioRecorderNode::writeFromFifo (int numSamples)
{
    int start1, size1, start2, size2;
    fifo.prepareToRead (numSamples, start1, size1, start2, size2);

    {
        const juce::ScopedLock sl (writerLock);

        // A disk that's full, or gone: said, and the recording's stopped (see timerCallback())
        if (writer != nullptr)
        {
            if (size1 > 0 && ! writer->writeFromAudioSampleBuffer (fifoBuffer, start1, size1))
                writeFailed = true;
            if (size2 > 0 && ! writer->writeFromAudioSampleBuffer (fifoBuffer, start2, size2))
                writeFailed = true;
        }
    }

    fifo.finishedRead (size1 + size2);
}

void AudioRecorderNode::run()
{
    // What was kept comes first, once the audio thread has marked where the recording starts
    while (! threadShouldExit() && historyMark.load (std::memory_order_acquire) < 0)
        wait (2);

    writeHistory();

    while (! threadShouldExit())
    {
        if (const auto ready = fifo.getNumReady(); ready >= 512)
            writeFromFifo (ready);
        else
            wait (5);
    }

    // What's left as it stops
    if (const auto remaining = fifo.getNumReady(); remaining > 0)
        writeFromFifo (remaining);
}

//==============================================================================
juce::Array<juce::File> AudioRecorderNode::getRecentRecordings()
{
    juce::Array<juce::File> files;

    if (auto* settings = getUserSettings())
        for (const auto& path : juce::StringArray::fromLines (settings->getValue (recentRecordingsKey)))
            if (juce::File::isAbsolutePath (path) && juce::File (path).existsAsFile())
                files.add (juce::File (path));

    return files;
}

void AudioRecorderNode::rememberRecording (const juce::File& file)
{
    if (auto* settings = getUserSettings(); settings != nullptr && file.existsAsFile())
    {
        auto paths = juce::StringArray::fromLines (settings->getValue (recentRecordingsKey));
        paths.removeString (file.getFullPathName());
        paths.insert (0, file.getFullPathName());
        paths.removeEmptyStrings();

        while (paths.size() > maxRecentRecordings)
            paths.remove (paths.size() - 1);

        settings->setValue (recentRecordingsKey, paths.joinIntoString ("\n"));
        settings->saveIfNeeded();
    }
}

void AudioRecorderNode::forgetRecording (const juce::File& file)
{
    if (auto* settings = getUserSettings())
    {
        auto paths = juce::StringArray::fromLines (settings->getValue (recentRecordingsKey));
        paths.removeString (file.getFullPathName());
        settings->setValue (recentRecordingsKey, paths.joinIntoString ("\n"));
        settings->saveIfNeeded();
    }
}

bool AudioRecorderNode::renameRecording (const juce::File& file, const juce::String& newName)
{
    const auto name = juce::File::createLegalFileName (newName.trim());

    if (name.isEmpty() || ! file.existsAsFile())
        return false;

    const auto target = file.getSiblingFile (name + file.getFileExtension());

    if (target == file)
        return true;

    if (target.exists() || ! file.moveFileTo (target))
        return false;

    if (auto* settings = getUserSettings())
    {
        auto paths = juce::StringArray::fromLines (settings->getValue (recentRecordingsKey));

        if (const auto index = paths.indexOf (file.getFullPathName()); index >= 0)
            paths.set (index, target.getFullPathName());

        settings->setValue (recentRecordingsKey, paths.joinIntoString ("\n"));
        settings->saveIfNeeded();
    }

    return true;
}

//==============================================================================
void AudioRecorderNode::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement xml ("RECORDER");
    xml.setAttribute ("format", (int) format);
    xml.setAttribute ("name", fileName);
    xml.setAttribute ("keepSeconds", historySeconds);
    xml.setAttribute ("silenceSeconds", silenceSeconds);
    xml.setAttribute ("auto", autoParam->isOn());
    copyXmlToBinary (xml, destData);
}

void AudioRecorderNode::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        format = (Format) juce::jlimit (0, (int) Format::aiff24, xml->getIntAttribute ("format", (int) Format::wav32Float));
        setFileName (xml->getStringAttribute ("name"));
        setSilenceSeconds (xml->getIntAttribute ("silenceSeconds", 5));
        autoParam->setValueNotifyingHost (xml->getBoolAttribute ("auto") ? 1.0f : 0.0f);
        setHistorySeconds (xml->getIntAttribute ("keepSeconds", 0));
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

    if (inSet.isDisabled() || inSet.size() == 0 || inSet.size() > maxRecorderChannels)
        return false;

    return outSet.isDisabled() || outSet.size() == 0;
}

void AudioRecorderNode::processorLayoutsChanged()
{
    if (isRecording())
        stopRecording();

    allocateFifo();

    // As many channels kept as it records
    if (historyBuffer.getNumChannels() != juce::jmax (1, getMainBusNumInputChannels()) && historySeconds > 0)
        allocateHistory();
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
