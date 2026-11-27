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

#include "LooperNode.h"
#include "../UI/CurveColours.h"
#include "../UI/CurveFolders.h"

namespace
{
    using State = LooperNode::State;

    juce::Colour getStateColour (State s)
    {
        switch (s)
        {
            case State::waiting:
            case State::countIn:
            case State::recording:   return Curve::Colours::danger;
            case State::playing:     return Curve::Colours::success;
            case State::overdubbing: return Curve::Colours::warning;
            case State::stopped:     return Curve::Colours::link;
            case State::empty:       break;
        }

        return Curve::Colours::dimText;
    }

    const juce::String dot = juce::String::fromUTF8 ("  \xc2\xb7  ");
}

//==============================================================================
/*  The looper's window: a light for what it's doing, the loop with where it's playing, the main
    button, labelled with what it does next, Stop, Undo and Clear, and the options as a card.
*/
class LooperEditor final : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit LooperEditor (LooperNode& n) : AudioProcessorEditor (n), looper (n)
    {
        // Through the parameters, with a gesture, so MIDI Learn picks them up when they're clicked
        const auto press = [] (InternalParameter& p)
        {
            return [&p]
            {
                p.beginChangeGesture();
                p.setValueNotifyingHost (1.0f);
                p.endChangeGesture();
            };
        };

        mainButton.onClick = press (looper.getLoopParameter());
        mainButton.setTooltip ("The looper's main control: records, closes the loop and plays it, overdubs onto it, and plays it "
                               "again, in turn. In MIDI Learn, click to map a footswitch to it");
        addAndMakeVisible (mainButton);

        const auto setUp = [&] (juce::TextButton& b, InternalParameter& p, const char* tip)
        {
            b.setTooltip (juce::String (tip) + ". In MIDI Learn, click to map a footswitch to it");
            b.onClick = press (p);
            addAndMakeVisible (b);
        };

        setUp (stopButton,  looper.getStopParameter(),  "Stops the loop. Its footswitch plays it again when it's stopped, and "
                                                        "pressed while it fades out, stops it at once");
        setUp (undoButton,  looper.getUndoParameter(),  "Undoes the last overdub, or redoes it");
        setUp (clearButton, looper.getClearParameter(), "Clears the loop, to start again");

        stopButton.setButtonText ("Stop");
        clearButton.setButtonText ("Clear");

        // The options
        const auto setUpSlider = [this] (juce::Slider& s, juce::Label& l, const char* text, const char* tip)
        {
            s.setSliderStyle (juce::Slider::LinearHorizontal);
            s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 22);
            s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            s.setTooltip (tip);
            addAndMakeVisible (s);
            setUpLabel (l, text);
        };

        setUpSlider (levelSlider, levelLabel, "Loop Volume", "How loud the loop is, over what you're playing");
        setUpSlider (feedbackSlider, feedbackLabel, "Older Layers",
                     "While overdubbing, how much of what's already there it keeps each time round. Kept keeps every layer; "
                     "less lets older layers fade away, so the loop keeps changing");
        levelBinding = std::make_unique<InternalSliderBinding> (looper.getLevelParameter(), levelSlider);
        feedbackBinding = std::make_unique<InternalSliderBinding> (looper.getFeedbackParameter(), feedbackSlider);

        setUpLabel (startLabel, "Start");
        startBox.addItemList ({ "Recording at once", "After a bar's count-in", "When you start playing" }, 1);
        startBox.setSelectedItemIndex ((int) looper.getStart(), juce::dontSendNotification);
        startBox.setTooltip ("How the first recording starts after you press Record: at once; after a bar of clicks at the "
                             "preset's tempo, to play along to; or when you start playing or singing, so your hands are free");
        startBox.onChange = [this] { looper.setStart ((LooperNode::Start) startBox.getSelectedItemIndex()); update(); };
        addAndMakeVisible (startBox);

        syncButton.setButtonText ("Sync to Tempo");
        syncButton.setTooltip ("Makes the first loop a whole number of bars at the preset's tempo, set in the editor's top bar");
        syncButton.setToggleState (looper.isSyncedToTempo(), juce::dontSendNotification);
        syncButton.onClick = [this] { looper.setSyncedToTempo (syncButton.getToggleState()); };

        fadeButton.setButtonText ("Fade Out on Stop");
        fadeButton.setTooltip ("Stop lets the loop die away over three seconds, rather than stopping dead. Press Stop again to "
                               "stop it at once");
        fadeButton.setToggleState (looper.isFadingOnStop(), juce::dontSendNotification);
        fadeButton.onClick = [this] { looper.setFadingOnStop (fadeButton.getToggleState()); };

        reverseButton.setButtonText ("Reverse");
        reverseButton.setTooltip ("Plays the loop backwards. In MIDI Learn, click to map a footswitch to it");
        halfSpeedButton.setButtonText ("Half Speed");
        halfSpeedButton.setTooltip ("Plays the loop at half speed, an octave down, e.g. to learn a fast part. In MIDI Learn, "
                                    "click to map a footswitch to it");
        reverseBinding = std::make_unique<InternalButtonBinding> (looper.getReverseParameter(), reverseButton);
        halfSpeedBinding = std::make_unique<InternalButtonBinding> (looper.getHalfSpeedParameter(), halfSpeedButton);

        for (auto* b : { &syncButton, &fadeButton })
            b->setClickingTogglesState (true);

        for (auto* b : { &syncButton, &fadeButton, &reverseButton, &halfSpeedButton })
            addAndMakeVisible (b);

        saveButton.setButtonText ("Save as File...");
        saveButton.setTooltip ("Saves the loop as a WAV file, e.g. to use in a DAW, or to play with the Player. Loops aren't kept "
                               "in presets, as they're too big");
        saveButton.onClick = [this] { save(); };
        addAndMakeVisible (saveButton);

        setResizable (true, false);
        setResizeLimits (460, 470, 1600, 1100);
        setSize (520, 500);
        startTimerHz (30);
        update();
    }

    ~LooperEditor() override
    {
        stopTimer();
        levelBinding.reset();
        feedbackBinding.reset();
        reverseBinding.reset();
        halfSpeedBinding.reset();
        looper.editorBeingDeleted (this);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Curve::Colours::canvas);

        const auto s = looper.getState();
        const auto colour = getStateColour (s);
        const bool aboutToRecord = s == State::waiting || s == State::countIn;
        const auto pulse = 0.5f + 0.5f * std::sin ((float) juce::Time::getMillisecondCounter() * 0.008f);

        // The state, as a looper pedal's light, pulsing while it waits to record
        const auto light = lightArea.toFloat().reduced (2.0f);
        g.setColour (colour.withAlpha (s == State::empty ? 0.12f : aboutToRecord ? 0.1f + 0.25f * pulse : 0.22f));
        g.fillEllipse (light);
        g.setColour (colour.withAlpha (aboutToRecord ? 0.5f + 0.5f * pulse : 1.0f));
        g.drawEllipse (light.reduced (1.5f), 3.0f);

        if (s == State::countIn)
        {
            g.setFont (juce::FontOptions (34.0f, juce::Font::bold));
            g.drawText (juce::String (looper.getCountInBeat()), light, juce::Justification::centred);
        }
        else
        {
            static const char* names[] = { "EMPTY", "WAIT", "COUNT", "REC", "PLAY", "DUB", "STOP" };
            g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
            g.drawText (names[(int) s], light, juce::Justification::centred);
        }

        // What it's doing, and the loop's length, beside the light
        auto info = infoArea;
        g.setColour (s == State::empty ? Curve::Colours::text : colour);
        g.setFont (juce::FontOptions (19.0f, juce::Font::bold));
        g.drawText (getTitle(), info.removeFromTop (info.getHeight() / 2), juce::Justification::bottomLeft, true);

        g.setColour (Curve::Colours::dimText);
        g.setFont (juce::FontOptions (13.0f).withFeatureEnabled ("tnum"));
        g.drawText (getDetail(), info.withTrimmedTop (5), juce::Justification::topLeft, true);

        // The loop, with where it's playing
        const auto wave = waveArea.toFloat();
        g.setColour (Curve::Colours::surface);
        g.fillRoundedRectangle (wave, 8.0f);

        const auto inner = wave.reduced (6.0f);

        if (! overview.empty())
        {
            const auto mid = inner.getCentreY();
            const auto w = inner.getWidth() / (float) overview.size();
            g.setColour (colour.withAlpha (0.75f));

            for (size_t i = 0; i < overview.size(); ++i)
                if (overview[i] > 0.0f)
                {
                    const auto h = juce::jmax (1.0f, overview[i] * inner.getHeight() * 0.5f);
                    g.fillRoundedRectangle (inner.getX() + (float) i * w, mid - h, juce::jmax (1.0f, w - 1.0f), h * 2.0f, 0.75f);
                }
        }

        if (const auto length = looper.getLoopLength(); length > 0 && s != State::recording)
        {
            const auto x = inner.getX() + inner.getWidth() * (float) looper.getPosition() / (float) length;
            g.setColour (juce::Colours::white.withAlpha (s == State::stopped ? 0.4f : 1.0f));
            g.fillRoundedRectangle (x - 1.0f, wave.getY() + 3.0f, 2.0f, wave.getHeight() - 6.0f, 1.0f);
        }
        else if (s == State::empty || aboutToRecord)
        {
            g.setColour (Curve::Colours::dimText.withAlpha (0.8f));
            g.setFont (juce::FontOptions (13.0f));
            g.drawText (s == State::waiting ? "Start playing to record" : s == State::countIn ? "Recording after the count-in"
                                            : "The loop appears here as you record it",
                        wave.toNearestInt(), juce::Justification::centred);
        }

        // The options, as a card
        g.setColour (Curve::Colours::surface);
        g.fillRoundedRectangle (optionsArea.toFloat(), 8.0f);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (18, 16);

        auto top = r.removeFromTop (72);
        lightArea = top.removeFromLeft (72);
        top.removeFromLeft (16);
        infoArea = top;

        r.removeFromTop (14);
        waveArea = r.removeFromTop (76);

        r.removeFromTop (14);
        auto buttons = r.removeFromTop (52);
        const auto small = (buttons.getWidth() - 12 * 3) / 6;
        clearButton.setBounds (buttons.removeFromRight (small));
        buttons.removeFromRight (12);
        undoButton.setBounds (buttons.removeFromRight (small));
        buttons.removeFromRight (12);
        stopButton.setBounds (buttons.removeFromRight (small));
        buttons.removeFromRight (12);
        mainButton.setBounds (buttons);

        r.removeFromTop (16);
        optionsArea = r.removeFromTop (5 * 32 + 20);
        auto options = optionsArea.reduced (14, 10);

        const auto row = [&options] (juce::Label& label)
        {
            auto line = options.removeFromTop (32);
            label.setBounds (line.removeFromLeft (labelWidth));
            line.removeFromLeft (10);
            return line.withSizeKeepingCentre (line.getWidth(), 26);
        };

        levelSlider.setBounds (row (levelLabel));
        feedbackSlider.setBounds (row (feedbackLabel));
        startBox.setBounds (row (startLabel).withWidth (230));

        for (auto pair : { std::pair { &syncButton, &fadeButton }, { &reverseButton, &halfSpeedButton } })
        {
            auto line = options.removeFromTop (32).withTrimmedLeft (labelWidth + 10);
            // Half the row each: taken before it's used, as the call after it would see the row halved
            const auto left = line.removeFromLeft (line.getWidth() / 2);
            pair.first->setBounds (left.withSizeKeepingCentre (left.getWidth(), 26));
            pair.second->setBounds (line.withSizeKeepingCentre (line.getWidth(), 26));
        }

        r.removeFromTop (12);
        saveButton.setBounds (r.removeFromTop (28).removeFromRight (130));
    }

private:
    static constexpr int labelWidth = 92;

    //==============================================================================
    // The main control, as big as a footswitch, labelled with what it does next, in the colour of
    // what it'll be doing
    struct MainButton final : public juce::Button
    {
        MainButton() : Button ("Loop") {}
        juce::String title, hint;
        juce::Colour colour;

        void paintButton (juce::Graphics& g, bool highlighted, bool down) override
        {
            const auto r = getLocalBounds().toFloat().reduced (0.5f);
            g.setColour (colour.withMultipliedSaturation (0.85f).withMultipliedBrightness (down ? 0.75f : highlighted ? 0.95f : 0.85f));
            g.fillRoundedRectangle (r, 8.0f);

            auto text = getLocalBounds().reduced (14, 6);
            g.setColour (juce::Colours::white);
            g.setFont (juce::FontOptions (17.0f, juce::Font::bold));
            g.drawText (title, text.removeFromTop (text.getHeight() * 11 / 20), juce::Justification::bottomLeft, true);
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.setFont (juce::FontOptions (12.0f));
            g.drawText (hint, text, juce::Justification::topLeft, true);
        }
    };

    static void setUpLabelStyle (juce::Label& l)
    {
        l.setFont (juce::FontOptions (13.0f));
        l.setColour (juce::Label::textColourId, Curve::Colours::dimText);
        l.setJustificationType (juce::Justification::centredRight);
    }

    void setUpLabel (juce::Label& l, const char* text)
    {
        l.setText (text, juce::dontSendNotification);
        setUpLabelStyle (l);
        addAndMakeVisible (l);
    }

    juce::String getTitle() const
    {
        switch (looper.getState())
        {
            case State::empty:       return "Ready to record";
            case State::waiting:     return "Waiting for you to play";
            case State::countIn:     return "Counting in";
            case State::recording:   return "Recording";
            case State::playing:     return looper.isFadingOut() && looper.isFadingOnStop() ? "Fading out" : "Playing";
            case State::overdubbing: return "Overdubbing";
            case State::stopped:     return "Stopped";
        }

        return {};
    }

    juce::String getDetail() const
    {
        const auto s = looper.getState();
        const auto rate = looper.getLooperSampleRate();
        const auto tempo = juce::String (juce::roundToInt (looper.getTempo())) + " BPM";

        if (s == State::empty || s == State::waiting || s == State::countIn)
        {
            juce::String detail = s == State::empty ? "Press Record, or its footswitch" : "Press Cancel to stop waiting";

            if (looper.isSyncedToTempo() || s == State::countIn)
                detail << dot << tempo;

            return detail;
        }

        const auto length = s == State::recording ? looper.getRecordedLength() : looper.getLoopLength();
        juce::String detail = juce::String ((double) length / rate, 1) + " s";

        if (looper.getBars() > 0)
            detail << dot << looper.getBars() << (looper.getBars() == 1 ? " bar at " : " bars at ") << tempo;

        if (looper.getLayers() > 0)
            detail << dot << looper.getLayers() << (looper.getLayers() == 1 ? " layer" : " layers");

        if (looper.getReverseParameter().isOn())   detail << dot << "Reverse";
        if (looper.getHalfSpeedParameter().isOn()) detail << dot << "Half Speed";

        return detail;
    }

    void timerCallback() override
    {
        looper.getOverview (overview, juce::jmax (1, (waveArea.getWidth() - 12) / 3));
        update();
        repaint();
    }

    void update()
    {
        const auto s = looper.getState();
        const bool hasLoop = looper.getLoopLength() > 0;
        const auto start = looper.getStart();

        // What the main button does next, and the colour of what it'll be doing
        switch (s)
        {
            case State::empty:
                mainButton.title = "Record";
                mainButton.hint = start == LooperNode::Start::countIn ? "after a bar's count-in"
                                : start == LooperNode::Start::onSound ? "when you start playing" : "the first loop";
                mainButton.colour = Curve::Colours::danger;
                break;
            case State::waiting:
            case State::countIn:     mainButton.title = "Cancel";  mainButton.hint = "not recording after all"; mainButton.colour = Curve::Colours::control.brighter (0.3f); break;
            case State::recording:   mainButton.title = "Play";    mainButton.hint = "closes the loop and plays it"; mainButton.colour = Curve::Colours::success.darker (0.3f); break;
            case State::playing:     mainButton.title = "Overdub"; mainButton.hint = "records over the loop"; mainButton.colour = Curve::Colours::warning.darker (0.35f); break;
            case State::overdubbing: mainButton.title = "Play";    mainButton.hint = "ends the overdub"; mainButton.colour = Curve::Colours::success.darker (0.3f); break;
            case State::stopped:     mainButton.title = "Play";    mainButton.hint = "plays the loop from the start"; mainButton.colour = Curve::Colours::success.darker (0.3f); break;
        }

        mainButton.repaint();

        stopButton.setEnabled (s == State::playing || s == State::overdubbing || s == State::recording);
        undoButton.setButtonText (looper.canRedo() ? "Redo" : "Undo");
        undoButton.setEnabled (looper.canUndo() || looper.canRedo() || s == State::overdubbing);
        clearButton.setEnabled (s != State::empty);
        saveButton.setEnabled (hasLoop);
        startBox.setEnabled (s == State::empty);

        if (syncButton.getToggleState() != looper.isSyncedToTempo())
            syncButton.setToggleState (looper.isSyncedToTempo(), juce::dontSendNotification);

        if (fadeButton.getToggleState() != looper.isFadingOnStop())
            fadeButton.setToggleState (looper.isFadingOnStop(), juce::dontSendNotification);

        if (startBox.getSelectedItemIndex() != (int) start)
            startBox.setSelectedItemIndex ((int) start, juce::dontSendNotification);
    }

    void save()
    {
        const auto suggested = CurveFolders::get (CurveFolders::Kind::recordings)
                                 .getChildFile ("Curve Loop " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H.%M.%S") + ".wav");

        chooser = std::make_unique<juce::FileChooser> ("Save the Loop", suggested, "*.wav");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                | juce::FileBrowserComponent::warnAboutOverwriting,
                              [safeThis = SafePointer<LooperEditor> (this)] (const juce::FileChooser& fc)
                              {
                                  const auto file = fc.getResult();

                                  if (safeThis == nullptr || file == juce::File())
                                      return;

                                  if (! safeThis->looper.saveLoop (file.withFileExtension (".wav")))
                                      juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't Save the Loop",
                                                                                   "Check that the folder can be written to.");
                              });
    }

    LooperNode& looper;
    MainButton mainButton;
    juce::TextButton stopButton, undoButton, clearButton, saveButton;
    juce::Slider levelSlider, feedbackSlider;
    juce::Label levelLabel, feedbackLabel, startLabel;
    juce::ComboBox startBox;
    juce::ToggleButton syncButton, fadeButton, reverseButton, halfSpeedButton;
    std::unique_ptr<InternalSliderBinding> levelBinding, feedbackBinding;
    std::unique_ptr<InternalButtonBinding> reverseBinding, halfSpeedBinding;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> lightArea, infoArea, waveArea, optionsArea;
    std::vector<float> overview;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LooperEditor)
};

//==============================================================================
LooperNode::LooperNode()
    : AudioPluginInstance (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                            .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    // Each acts when set on, then goes back off, so a footswitch's toggles and presses both work
    const auto add = [this] (const char* id, const char* name, InternalParameter*& p)
    {
        auto param = InternalParameter::makeBool (id, name, false, false);
        p = param.get();
        addHostedParameter (std::move (param));
        p->addListener (this);
    };

    add ("loop",  "Loop",       loopParam);
    add ("stop",  "Stop/Play",  stopParam);
    add ("undo",  "Undo/Redo",  undoParam);
    add ("clear", "Clear",      clearParam);

    InternalParameter::Options levelOptions;
    levelOptions.valueToText = [] (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; };
    levelOptions.textToValue = [] (const juce::String& t) { return t.getFloatValue() / 100.0f; };
    auto levelP = std::make_unique<InternalParameter> ("level", "Loop Volume", juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f, levelOptions);
    levelParam = levelP.get();
    addHostedParameter (std::move (levelP));

    InternalParameter::Options feedbackOptions;
    feedbackOptions.valueToText = [] (float v) { return v >= 0.995f ? juce::String ("Kept") : juce::String (juce::roundToInt (v * 100.0f)) + "%"; };
    feedbackOptions.textToValue = [] (const juce::String& t) { return t.equalsIgnoreCase ("kept") ? 1.0f : t.getFloatValue() / 100.0f; };
    auto feedbackP = std::make_unique<InternalParameter> ("feedback", "Older Layers", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f, feedbackOptions);
    feedbackParam = feedbackP.get();
    addHostedParameter (std::move (feedbackP));

    auto reverseP = InternalParameter::makeBool ("reverse", "Reverse", false, false);
    reverseParam = reverseP.get();
    addHostedParameter (std::move (reverseP));

    auto halfSpeedP = InternalParameter::makeBool ("halfSpeed", "Half Speed", false, false);
    halfSpeedParam = halfSpeedP.get();
    addHostedParameter (std::move (halfSpeedP));
}

LooperNode::~LooperNode()
{
    stopTimer();
    cancelPendingUpdate();

    for (auto* p : { loopParam, stopParam, undoParam, clearParam })
        p->removeListener (this);
}

void LooperNode::prepareToPlay (double sampleRate, int)
{
    const auto rate = sampleRate > 0.0 ? sampleRate : 48000.0;
    const auto samples = (int) (rate * maxSeconds);

    // Kept through a re-prepare at the same rate, e.g. a change elsewhere in the graph
    if (rate != currentSampleRate || loops[0].getNumSamples() != samples)
    {
        currentSampleRate = rate;

        for (auto& loop : loops)
        {
            loop.setSize (2, samples);
            loop.clear();
        }

        state = (int) State::empty;
        loopLength = recordedLength = position = targetLength = layers = bars = countInBeat = 0;
        hasUndo = undone = false;
        readPosition = 0.0;
        fadeGain = 1.0f;
        fadeStep = 0.0f;
        fadingOut = false;
    }

    level.reset (rate, 0.05);
    level.setCurrentAndTargetValue (levelParam->get());
}

void LooperNode::parameterValueChanged (int index, float newValue)
{
    if (newValue < 0.5f)
        return;

    if (index == loopParam->getParameterIndex())        ++pressedLoop;
    else if (index == stopParam->getParameterIndex())   ++pressedStop;
    else if (index == undoParam->getParameterIndex())   ++pressedUndo;
    else if (index == clearParam->getParameterIndex())  ++pressedClear;
    else return;

    triggerAsyncUpdate();
}

// On the message thread: what each press asks the audio thread to do
void LooperNode::handleAsyncUpdate()
{
    // Back off, without telling anyone, so the next press sets them on again
    for (auto* p : { loopParam, stopParam, undoParam, clearParam })
        p->setValue (0.0f);

    takePresses();
}

// On the message thread
void LooperNode::takePresses()
{
    // One press per request, so none overwrites another before the audio thread has it, and each is
    // taken in the state the last left it in. If the audio thread hasn't taken the last yet, e.g.
    // between blocks, it's tried again in a moment
    while (pendingRequest.load (std::memory_order_acquire) == none)
    {
        const auto take = [] (std::atomic<int>& presses)
        {
            if (presses.load() <= 0)
                return false;

            --presses;
            return true;
        };

        const auto s = getState();

        if (take (pressedClear))
        {
            request (clear);
        }
        else if (take (pressedLoop))
        {
            switch (s)
            {
                case State::empty:       request (record); break;
                case State::waiting:
                case State::countIn:     request (cancel); break;
                case State::recording:   request (closeLoop); break;
                case State::overdubbing: request (endOverdub); break;
                case State::stopped:     request (play); break;

                case State::playing:
                {
                    // What it was before this overdub, to go back to. Nothing's waiting for the audio
                    // thread, so it can't swap the copies meanwhile; it says there's an undo once it
                    // starts the overdub
                    const auto from = active.load(), to = 1 - from;
                    const auto length = getLoopLength();

                    for (int ch = 0; ch < loops[to].getNumChannels(); ++ch)
                        loops[to].copyFrom (ch, 0, loops[from], ch, 0, length);

                    request (overdub);
                    break;
                }
            }
        }
        else if (take (pressedStop))
        {
            request (s == State::stopped ? play : s == State::waiting || s == State::countIn ? cancel : stop);
        }
        else if (take (pressedUndo))
        {
            request (undo);
        }
        else
        {
            stopTimer();
            return;
        }
    }

    startTimer (5);
}

//==============================================================================
// Audio thread

void LooperNode::beginRecording()
{
    recordedLength = 0;
    targetLength = 0;
    position = 0;
    layers = 0;
    bars = 0;
    countInBeat = 0;
    hasUndo = false;
    state = (int) State::recording;
}

// Ends the first recording, as a whole number of bars if syncing: recording on to the end of the bar
// if it's short of it, or cut back to it if it's past it, with the loop then playing on from where
// that leaves it
void LooperNode::finishRecording (bool thenStop, double samplesPerBar)
{
    const auto maxLength = loops[0].getNumSamples();
    const auto recorded = recordedLength.load();

    if (recorded < (int) (currentSampleRate * 0.1))
    {
        state = (int) State::empty;
        recordedLength = 0;
        return;
    }

    int length = recorded;

    if (syncToTempo.load() && samplesPerBar > 0.0)
    {
        const auto numBars = juce::jmax (1, juce::roundToInt (recorded / samplesPerBar));
        length = juce::jmin (maxLength, (int) (numBars * samplesPerBar));
        bars = numBars;

        if (length > recorded && ! thenStop)
        {
            targetLength = length;
            return;
        }
    }
    else
    {
        bars = 0;
    }

    loopLength = juce::jmin (length, recorded);
    smoothSeam (loopLength.load());
    position = thenStop ? 0 : (recorded - loopLength.load()) % juce::jmax (1, loopLength.load());
    readPosition = position.load();
    layers = 1;
    targetLength = 0;
    fadeGain = 1.0f;
    fadeStep = 0.0f;
    fadingOut = false;
    state = (int) (thenStop ? State::stopped : State::playing);
}

void LooperNode::startFade (double seconds)
{
    fadeStep = (float) (1.0 / juce::jmax (1.0, seconds * currentSampleRate));
    fadeInStep = 0.0f;
    fadingOut = true;
}

// Where the loop's end meets its start, a few milliseconds faded out and in, so a loop that ends
// mid-note doesn't click each time round
void LooperNode::smoothSeam (int length)
{
    auto& loop = loops[active.load (std::memory_order_relaxed)];
    const auto fadeLength = juce::jmin (length / 4, (int) (0.004 * currentSampleRate));

    if (fadeLength < 2)
        return;

    for (int ch = 0; ch < loop.getNumChannels(); ++ch)
    {
        loop.applyGainRamp (ch, 0, fadeLength, 0.0f, 1.0f);
        loop.applyGainRamp (ch, length - fadeLength, fadeLength, 1.0f, 0.0f);
    }
}

void LooperNode::handleRequest (Request r, double samplesPerBar)
{
    const auto s = getState();

    switch (r)
    {
        case record:
            if (s == State::empty)
            {
                switch (getStart())
                {
                    case Start::now:
                        beginRecording();
                        break;

                    case Start::countIn:
                        countInLength = juce::jmax (1, (int) samplesPerBar);
                        countInDone = 0;
                        countInBeat = 1;
                        hasUndo = false;
                        state = (int) State::countIn;
                        break;

                    case Start::onSound:
                        hasUndo = false;
                        state = (int) State::waiting;
                        break;
                }
            }
            break;

        case cancel:
            if (s == State::waiting || s == State::countIn)
            {
                countInBeat = 0;
                state = (int) State::empty;
            }
            break;

        case closeLoop:
            if (s == State::recording && targetLength.load() == 0)
                finishRecording (false, samplesPerBar);
            break;

        case overdub:
            if (s == State::playing)
            {
                // Overdubbing as it fades out brings it back
                fadeGain = 1.0f;
                fadeStep = 0.0f;
                fadingOut = false;
                hasUndo = true;     // The copy from before it, made as it was asked for
                undone = false;
                state = (int) State::overdubbing;
            }
            else
            {
                // Not started, e.g. it had stopped meanwhile: the copy made for it isn't an undo
                hasUndo = false;
                undone = false;
            }
            break;

        case endOverdub:
            if (s == State::overdubbing)
            {
                state = (int) State::playing;
                ++layers;
            }
            break;

        case stop:
            if (s == State::recording)
            {
                finishRecording (true, samplesPerBar);
            }
            else if (s == State::waiting || s == State::countIn)
            {
                state = (int) State::empty;
            }
            else if (s == State::playing || s == State::overdubbing)
            {
                if (s == State::overdubbing)
                {
                    ++layers;
                    state = (int) State::playing;
                }

                // A short fade, so it doesn't click; pressed again as it fades out, it stops at once
                startFade (fadeOnStop.load() && ! fadingOut.load() ? 3.0 : 0.01);
            }
            break;

        case play:
            if (s == State::stopped && loopLength.load() > 0)
            {
                position = 0;
                readPosition = reverseParam->isOn() ? loopLength.load() - 1 : 0;

                // Faded in over 5 ms, so starting it doesn't click
                fadeGain = 0.0f;
                fadeInStep = (float) (1.0 / juce::jmax (1.0, 0.005 * currentSampleRate));
                fadeStep = 0.0f;
                fadingOut = false;
                state = (int) State::playing;
            }
            break;

        case undo:
            if (s == State::overdubbing)
            {
                state = (int) State::playing;
                ++layers;
            }

            if (hasUndo.load())
            {
                active = 1 - active.load();
                undone = ! undone.load();
                layers += undone.load() ? -1 : 1;
            }
            break;

        case clear:
            state = (int) State::empty;
            loopLength = recordedLength = position = targetLength = layers = bars = countInBeat = 0;
            hasUndo = undone = false;
            readPosition = 0.0;
            fadeGain = 1.0f;
            fadeStep = 0.0f;
            fadingOut = false;
            break;

        case none:
            break;
    }
}

// The first take: what comes in passes through, and is kept. How much it took: less than asked if
// the loop closed, so the rest plays
int LooperNode::recordInto (juce::AudioBuffer<float>& buffer, int start, int num)
{
    auto& loop = loops[active.load (std::memory_order_relaxed)];
    const auto maxLength = loop.getNumSamples();
    const auto numChannels = juce::jmin (2, buffer.getNumChannels(), loop.getNumChannels());
    auto recorded = recordedLength.load();
    const auto target = targetLength.load();

    auto limit = maxLength - recorded;

    if (target > 0)
        limit = juce::jmin (limit, target - recorded);

    const auto n = juce::jlimit (0, num, limit);

    for (int ch = 0; ch < numChannels; ++ch)
        loop.copyFrom (ch, recorded, buffer, ch, start, n);

    // Mono in, both sides of the loop
    if (numChannels == 1 && loop.getNumChannels() > 1)
        loop.copyFrom (1, recorded, buffer, 0, start, n);

    recorded += n;
    recordedLength = recorded;

    if (target > 0 && recorded >= target)
    {
        // To the end of the bar: closed exactly there
        loopLength = target;
        smoothSeam (target);
        position = 0;
        readPosition = reverseParam->isOn() ? target - 1 : 0;
        layers = 1;
        targetLength = 0;
        state = (int) State::playing;
    }
    else if (recorded >= maxLength)
    {
        // As long as it can be: closed there
        loopLength = maxLength;
        smoothSeam (maxLength);
        position = 0;
        readPosition = 0.0;
        layers = 1;
        bars = 0;
        state = (int) State::playing;
    }

    return n > 0 ? n : num;
}

// The loop over what comes in, overdubbing onto it if it is. How much it took: less than asked if
// it faded out and stopped
int LooperNode::playLoop (juce::AudioBuffer<float>& buffer, int start, int num)
{
    auto& loop = loops[active.load (std::memory_order_relaxed)];
    const auto length = loopLength.load();

    if (length <= 0)
        return num;

    const bool dubbing = getState() == State::overdubbing;
    const auto rate = (halfSpeedParam->isOn() ? 0.5 : 1.0) * (reverseParam->isOn() ? -1.0 : 1.0);
    const auto speed = (float) std::abs (rate);

    // What's there is kept this much each time round; at half speed, it's passed twice
    const auto keep = dubbing ? std::pow (feedbackParam->get(), speed) : 1.0f;

    const auto numOut = juce::jmin (2, buffer.getNumChannels());

    if (numOut == 0)
        return num;
    const auto numLoop = loop.getNumChannels();

    for (int i = 0; i < num; ++i)
    {
        const auto gain = level.getNextValue() * fadeGain;
        const auto index = juce::jlimit (0, length - 1, (int) readPosition);
        const auto next = index + 1 < length ? index + 1 : 0;
        const auto frac = (float) (readPosition - (double) index);

        // What came in, before the loop's added to it
        float in[2] = {};

        for (int ch = 0; ch < numOut; ++ch)
            in[ch] = buffer.getSample (ch, start + i);

        for (int ch = 0; ch < juce::jmax (numOut, numLoop); ++ch)
        {
            auto* stored = loop.getWritePointer (juce::jmin (ch, numLoop - 1));
            const auto played = stored[index] + (stored[next] - stored[index]) * frac;
            const auto input = in[juce::jmin (ch, numOut - 1)];

            if (ch < numOut)
                buffer.getWritePointer (ch)[start + i] = input + played * gain;

            // Mono in, onto both sides of the loop. At half speed each sample is passed twice, so
            // each pass adds half
            if (dubbing && ch < numLoop)
                stored[index] = stored[index] * keep + input * speed;
        }

        if (fadeInStep > 0.0f)
        {
            fadeGain = juce::jmin (1.0f, fadeGain + fadeInStep);

            if (fadeGain >= 1.0f)
                fadeInStep = 0.0f;
        }

        if (fadeStep > 0.0f)
        {
            fadeGain -= fadeStep;

            if (fadeGain <= 0.0f)
            {
                fadeGain = 1.0f;
                fadeStep = 0.0f;
                fadingOut = false;
                position = 0;
                readPosition = 0.0;
                state = (int) State::stopped;
                return i + 1;
            }
        }

        readPosition += rate;

        if (readPosition >= (double) length)
            readPosition -= (double) length;
        else if (readPosition < 0.0)
            readPosition += (double) length;
    }

    position = (int) readPosition;
    return num;
}

// A bar of clicks, the first of it higher, as a metronome's
void LooperNode::addClicks (juce::AudioBuffer<float>& buffer, int start, int num)
{
    const auto beatLength = (double) countInLength / 4.0;
    const auto clickLength = 0.05 * currentSampleRate;
    int beat = 0;

    for (int i = 0; i < num; ++i)
    {
        const auto t = (double) (countInDone + i);
        beat = juce::jlimit (0, 3, (int) (t / beatLength));
        const auto within = t - beat * beatLength;

        if (within < clickLength)
        {
            const auto frequency = beat == 0 ? 1760.0 : 1320.0;
            const auto sample = (float) (0.35 * std::exp (-within / (0.01 * currentSampleRate))
                                          * std::sin (juce::MathConstants<double>::twoPi * frequency * within / currentSampleRate));

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.getWritePointer (ch)[start + i] += sample;
        }
    }

    if (num > 0)
        countInBeat = beat + 1;
}

void LooperNode::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // A bar at the preset's tempo, in 4/4
    double bpm = 120.0;

    if (auto* head = getPlayHead())
        if (const auto pos = head->getPosition())
            if (const auto b = pos->getBpm(); b.hasValue() && *b > 0.0)
                bpm = *b;

    tempo.store (bpm, std::memory_order_relaxed);
    const auto samplesPerBar = currentSampleRate * 60.0 / bpm * 4.0;

    handleRequest ((Request) pendingRequest.exchange (none, std::memory_order_acq_rel), samplesPerBar);
    level.setTargetValue (levelParam->get());

    const auto numSamples = buffer.getNumSamples();
    const auto threshold = juce::Decibels::decibelsToGain (soundThresholdDb);

    // In parts, as what it's doing can change within a block: a count-in ends, sound begins, or the
    // loop closes at the end of a bar
    for (int i = 0; i < numSamples;)
    {
        switch (getState())
        {
            case State::waiting:
            {
                int j = i;

                for (; j < numSamples; ++j)
                {
                    bool loud = false;

                    for (int ch = 0; ch < buffer.getNumChannels() && ! loud; ++ch)
                        loud = std::abs (buffer.getSample (ch, j)) > threshold;

                    if (loud)
                        break;
                }

                if (j < numSamples)
                    beginRecording();

                i = j;
                break;
            }

            case State::countIn:
            {
                const auto n = juce::jmin (numSamples - i, countInLength - countInDone);
                addClicks (buffer, i, n);
                countInDone += n;
                i += n;

                if (countInDone >= countInLength)
                    beginRecording();
                break;
            }

            case State::recording:
                i += recordInto (buffer, i, numSamples - i);
                break;

            case State::playing:
            case State::overdubbing:
                i += playLoop (buffer, i, numSamples - i);
                break;

            case State::empty:
            case State::stopped:
                i = numSamples;
                break;
        }
    }
}

//==============================================================================
void LooperNode::getOverview (std::vector<float>& peaks, int numColumns) const
{
    const auto s = getState();
    const auto length = s == State::recording ? getRecordedLength() : getLoopLength();
    peaks.assign ((size_t) numColumns, 0.0f);

    if (s == State::empty || s == State::waiting || s == State::countIn)
        return;

    // While recording, along the time it'll take, or four seconds, so it grows across
    const auto target = targetLength.load();
    const auto span = s == State::recording ? juce::jmax (length, target, (int) (currentSampleRate * 4.0)) : length;

    if (length <= 0 || span <= 0)
        return;

    const auto& loop = loops[active.load()];
    const auto perColumn = juce::jmax (1, span / numColumns);

    for (int c = 0; c < numColumns; ++c)
    {
        const auto first = c * perColumn;

        if (first >= length)
            break;

        const auto count = juce::jmin (perColumn, length - first);
        float peak = 0.0f;

        // A sample of each column is enough to draw it
        for (int i = 0; i < count; i += juce::jmax (1, count / 64))
            for (int ch = 0; ch < loop.getNumChannels(); ++ch)
                peak = juce::jmax (peak, std::abs (loop.getSample (ch, first + i)));

        peaks[(size_t) c] = juce::jmin (1.0f, peak);
    }
}

bool LooperNode::saveLoop (const juce::File& file)
{
    const auto length = getLoopLength();

    if (length <= 0)
        return false;

    // Written beside it, then put in its place, so a file that's there isn't lost if this fails
    const juce::TemporaryFile temp (file);
    std::unique_ptr<juce::OutputStream> stream (temp.getFile().createOutputStream().release());

    if (stream == nullptr)
        return false;

    const auto options = juce::AudioFormatWriterOptions().withSampleRate (currentSampleRate)
                                                         .withNumChannels (2)
                                                         .withBitsPerSample (24);

    {
        auto writer = juce::WavAudioFormat().createWriterFor (stream, options);

        if (writer == nullptr || ! writer->writeFromAudioSampleBuffer (loops[active.load()], 0, length))
            return false;
    }

    return temp.overwriteTargetFileWithTemporary();
}

void LooperNode::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement xml ("LOOPER");
    xml.setAttribute ("syncToTempo", syncToTempo.load());
    xml.setAttribute ("start", startMode.load());
    xml.setAttribute ("fadeOut", fadeOnStop.load());
    xml.setAttribute ("level", (double) levelParam->get());
    xml.setAttribute ("feedback", (double) feedbackParam->get());
    xml.setAttribute ("reverse", reverseParam->isOn());
    xml.setAttribute ("halfSpeed", halfSpeedParam->isOn());
    copyXmlToBinary (xml, destData);
}

void LooperNode::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        syncToTempo = xml->getBoolAttribute ("syncToTempo");
        startMode = juce::jlimit (0, (int) Start::onSound, xml->getIntAttribute ("start"));
        fadeOnStop = xml->getBoolAttribute ("fadeOut");
        levelParam->setValueNotifyingHost ((float) xml->getDoubleAttribute ("level", 1.0));
        feedbackParam->setValueNotifyingHost ((float) xml->getDoubleAttribute ("feedback", 1.0));
        reverseParam->setValueNotifyingHost (xml->getBoolAttribute ("reverse") ? 1.0f : 0.0f);
        halfSpeedParam->setValueNotifyingHost (xml->getBoolAttribute ("halfSpeed") ? 1.0f : 0.0f);
    }
}

bool LooperNode::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    return in == out && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

juce::AudioProcessorEditor* LooperNode::createEditor()
{
    return new LooperEditor (*this);
}

void LooperNode::fillInPluginDescription (juce::PluginDescription& description) const
{
    description.name = getName();
    description.descriptiveName = "Loops what comes in, as a looper pedal does";
    description.pluginFormatName = "Internal";
    description.category = "Plugins";
    description.fileOrIdentifier = getName();
    description.uniqueId = 0x4C4F4F50; // "LOOP"
    description.isInstrument = false;
    description.numInputChannels = 2;
    description.numOutputChannels = 2;
}
