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

#include "PlayerNode.h"
#include "../UI/CurveColours.h"
#include "../UI/CurveFolders.h"

namespace
{
    juce::String formatTime (double seconds)
    {
        const auto total = juce::jmax (0, (int) seconds);
        return juce::String (total / 60) + ":" + juce::String (total % 60).paddedLeft ('0', 2);
    }

    // Transport glyphs, drawn rather than typed, so they're the same size and weight
    enum class Glyph { previous, play, pause, stop, next };

    void drawGlyph (juce::Graphics& g, Glyph glyph, juce::Rectangle<float> r, juce::Colour c)
    {
        g.setColour (c);
        juce::Path p;

        const auto triangle = [&p] (juce::Rectangle<float> t)
        {
            p.addTriangle (t.getX(), t.getY(), t.getX(), t.getBottom(), t.getRight(), t.getCentreY());
        };

        switch (glyph)
        {
            case Glyph::play:  triangle (r.reduced (r.getWidth() * 0.18f, 0.0f).translated (r.getWidth() * 0.06f, 0.0f)); break;
            case Glyph::pause: p.addRectangle (r.getX() + r.getWidth() * 0.15f, r.getY(), r.getWidth() * 0.25f, r.getHeight());
                               p.addRectangle (r.getX() + r.getWidth() * 0.6f, r.getY(), r.getWidth() * 0.25f, r.getHeight()); break;
            case Glyph::stop:  p.addRectangle (r.reduced (r.getWidth() * 0.08f)); break;
            case Glyph::next:  triangle (r.withWidth (r.getWidth() * 0.75f));
                               p.addRectangle (r.getRight() - r.getWidth() * 0.18f, r.getY(), r.getWidth() * 0.18f, r.getHeight()); break;
            case Glyph::previous:
            {
                juce::Path back;
                back.addTriangle (r.getRight(), r.getY(), r.getRight(), r.getBottom(), r.getX() + r.getWidth() * 0.25f, r.getCentreY());
                p.addPath (back);
                p.addRectangle (r.getX(), r.getY(), r.getWidth() * 0.18f, r.getHeight());
                break;
            }
        }

        g.fillPath (p);
    }
}

//==============================================================================
class PlayerEditor final : public juce::AudioProcessorEditor,
                           public juce::FileDragAndDropTarget,
                           public juce::DragAndDropContainer,
                           public juce::DragAndDropTarget,
                           private juce::Timer,
                           private juce::ChangeListener,
                           private juce::ListBoxModel
{
public:
    explicit PlayerEditor (PlayerNode& n)
        : AudioProcessorEditor (n), player (n),
          thumbnail (512, n.getFormatManager(), thumbnailCache)
    {
        const auto setUp = [this] (GlyphButton& b, InternalParameter& p, Glyph glyph, const char* tip)
        {
            b.glyph = glyph;
            b.setTooltip (juce::String (tip) + ". In MIDI Learn, click to map a control to it");
            b.onClick = [&p]
            {
                p.beginChangeGesture();
                p.setValueNotifyingHost (1.0f);
                p.endChangeGesture();
            };
            addAndMakeVisible (b);
        };

        setUp (previousButton, player.getPreviousParameter(), Glyph::previous, "Back to the start, or the previous track");
        setUp (playButton,     player.getPlayParameter(),     Glyph::play,     "Play or pause");
        setUp (stopButton,     player.getStopParameter(),     Glyph::stop,     "Stop, back to the start");
        setUp (nextButton,     player.getNextParameter(),     Glyph::next,     "The next track");
        playButton.large = true;

        atEndBox.addItemList ({ "Then Stop", "Then Next Track", "Repeat Track" }, 1);
        atEndBox.setSelectedItemIndex ((int) player.getAtEnd(), juce::dontSendNotification);
        atEndBox.setTooltip ("What happens when a track ends");
        atEndBox.onChange = [this] { player.setAtEnd ((PlayerNode::AtEnd) atEndBox.getSelectedItemIndex()); };
        addAndMakeVisible (atEndBox);

        levelSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        levelSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        levelSlider.setRange (0.0, 1.0);
        levelSlider.setValue (player.getLevelParameter().get(), juce::dontSendNotification);
        levelSlider.onDragStart = [this] { player.getLevelParameter().beginChangeGesture(); };
        levelSlider.onValueChange = [this] { player.getLevelParameter().set ((float) levelSlider.getValue()); };
        levelSlider.onDragEnd = [this] { player.getLevelParameter().endChangeGesture(); };
        levelSlider.setTooltip ("How loud the tracks are");
        addAndMakeVisible (levelSlider);

        volumeLabel.setText ("Volume", juce::dontSendNotification);
        volumeLabel.setFont (juce::FontOptions (12.5f));
        volumeLabel.setColour (juce::Label::textColourId, Curve::Colours::dimText);
        addAndMakeVisible (volumeLabel);

        addButton.setButtonText ("Add...");
        addButton.onClick = [this] { chooseFiles(); };
        addAndMakeVisible (addButton);

        listHeader.setText ("PLAYLIST", juce::dontSendNotification);
        listHeader.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        listHeader.setColour (juce::Label::textColourId, Curve::Colours::dimText);
        addAndMakeVisible (listHeader);

        list.setModel (this);
        list.setRowHeight (30);
        list.setOutlineThickness (0);
        list.setColour (juce::ListBox::backgroundColourId, Curve::Colours::surface);
        addAndMakeVisible (list);

        thumbnail.addChangeListener (this);
        player.addChangeListener (this);
        loadThumbnail();

        setResizable (true, false);
        setResizeLimits (380, 360, 1600, 1400);
        setSize (480, 460);
        startTimerHz (30);
    }

    ~PlayerEditor() override
    {
        stopTimer();
        player.removeChangeListener (this);
        thumbnail.removeChangeListener (this);
        list.setModel (nullptr);
        player.editorBeingDeleted (this);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Curve::Colours::canvas);

        // The track, and where it is
        auto title = titleArea;
        const auto file = player.getCurrentFile();
        g.setColour (Curve::Colours::text);
        g.setFont (juce::FontOptions (17.0f, juce::Font::bold));
        g.drawFittedText (file == juce::File() ? juce::String ("No tracks yet") : file.getFileNameWithoutExtension(),
                          title.removeFromTop (24), juce::Justification::centredLeft, 1);

        const auto count = player.getPlaylist().size();

        if (count > 0)
        {
            // The time, in figures of one width, so it doesn't jitter as it counts
            g.setFont (juce::FontOptions (13.0f).withFeatureEnabled ("tnum"));
            g.setColour (Curve::Colours::text);
            g.drawText (formatTime (player.getPosition()) + " / " + formatTime (player.getLength()),
                        title.removeFromRight (120), juce::Justification::centredRight);
        }

        g.setColour (Curve::Colours::dimText);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText (count == 0 ? juce::String ("Drop audio files here, or Add...")
                               : "Track " + juce::String (player.getCurrentIndex() + 1) + " of " + juce::String (count),
                    title, juce::Justification::centredLeft);

        // The waveform, played part brighter, and where it's playing
        const auto wave = waveArea.toFloat();
        g.setColour (Curve::Colours::surface);
        g.fillRoundedRectangle (wave, 6.0f);

        const auto length = thumbnail.getTotalLength();

        if (length > 0.0)
        {
            const auto inner = wave.reduced (2.0f, 6.0f);
            const auto played = juce::jlimit (0.0f, 1.0f, (float) (player.getPosition() / length));
            const auto split = inner.getX() + inner.getWidth() * played;

            g.setColour (Curve::Colours::accent);
            thumbnail.drawChannels (g, inner.withRight (split).toNearestInt(), 0.0, length * played, 1.0f);
            g.setColour (Curve::Colours::dimText.withAlpha (0.6f));
            thumbnail.drawChannels (g, inner.withLeft (split).toNearestInt(), length * played, length, 1.0f);

            g.setColour (juce::Colours::white);
            g.fillRect (split - 1.0f, wave.getY(), 2.0f, wave.getHeight());
        }

        if (isDraggingFiles)
        {
            g.setColour (Curve::Colours::accent);
            g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (3.0f), 8.0f, 2.0f);
        }
    }

    // Where a track dragged in the playlist would go
    void paintOverChildren (juce::Graphics& g) override
    {
        if (dropIndex < 0 || getNumRows() == 0)
            return;

        const auto row = list.getRowPosition (juce::jmin (dropIndex, getNumRows() - 1), true);
        const auto y = list.getY() + (dropIndex >= getNumRows() ? row.getBottom() : row.getY());

        g.setColour (Curve::Colours::accent);
        g.fillRoundedRectangle ((float) list.getX() + 6.0f, (float) juce::jlimit (list.getY(), list.getBottom() - 3, y - 1),
                                (float) list.getWidth() - 12.0f, 2.5f, 1.25f);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (16, 14);

        titleArea = r.removeFromTop (44);
        r.removeFromTop (10);
        waveArea = r.removeFromTop (72);

        r.removeFromTop (12);
        auto transport = r.removeFromTop (44);
        previousButton.setBounds (transport.removeFromLeft (40));
        transport.removeFromLeft (6);
        playButton.setBounds (transport.removeFromLeft (44));
        transport.removeFromLeft (6);
        stopButton.setBounds (transport.removeFromLeft (40));
        transport.removeFromLeft (6);
        nextButton.setBounds (transport.removeFromLeft (40));
        transport.removeFromLeft (16);
        atEndBox.setBounds (transport.removeFromRight (150).withSizeKeepingCentre (150, 26));
        transport.removeFromRight (12);
        volumeLabel.setBounds (transport.removeFromLeft (50));
        levelSlider.setBounds (transport);

        r.removeFromTop (14);
        auto header = r.removeFromTop (24);
        addButton.setBounds (header.removeFromRight (70));
        listHeader.setBounds (header);
        r.removeFromTop (4);
        list.setBounds (r);
    }

    //==============================================================================
    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        for (const auto& f : files)
            if (PlayerNode::canPlay (juce::File (f)))
                return true;

        return false;
    }

    void fileDragEnter (const juce::StringArray&, int, int) override    { isDraggingFiles = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override               { isDraggingFiles = false; repaint(); }

    void filesDropped (const juce::StringArray& files, int, int) override
    {
        isDraggingFiles = false;
        juce::Array<juce::File> toAdd;

        for (const auto& f : files)
            toAdd.add (juce::File (f));

        player.addFiles (toAdd);
    }

    //==============================================================================
    // Tracks dragged up and down the playlist
    bool isInterestedInDragSource (const SourceDetails& d) override
    {
        return d.description.toString().startsWith ("playlist:");
    }

    void itemDragMove (const SourceDetails& d) override
    {
        const auto inList = list.getLocalPoint (this, d.localPosition);
        const auto index = list.getInsertionIndexForPosition (inList.x, inList.y);

        if (index != dropIndex)
        {
            dropIndex = index;
            repaint (list.getBounds().expanded (0, 4));
        }
    }

    void itemDragExit (const SourceDetails&) override
    {
        dropIndex = -1;
        repaint();
    }

    void itemDropped (const SourceDetails& d) override
    {
        const auto from = d.description.toString().fromFirstOccurrenceOf (":", false, false).getIntValue();
        const auto to = dropIndex;
        dropIndex = -1;

        if (to >= 0)
        {
            player.moveFile (from, to);
            list.selectRow (juce::jlimit (0, getNumRows() - 1, to > from ? to - 1 : to));
        }

        repaint();
    }

    // Click or drag along the waveform to go there
    void mouseDown (const juce::MouseEvent& e) override { seekTo (e); }
    void mouseDrag (const juce::MouseEvent& e) override { seekTo (e); }

private:
    struct GlyphButton final : public juce::Button
    {
        GlyphButton() : Button ({}) { setWantsKeyboardFocus (false); }
        Glyph glyph = Glyph::play;
        bool large = false;

        void paintButton (juce::Graphics& g, bool highlighted, bool down) override
        {
            const auto bounds = getLocalBounds().toFloat().reduced (1.0f);

            g.setColour (large ? Curve::Colours::accent.withMultipliedBrightness (down ? 0.8f : highlighted ? 1.15f : 1.0f)
                               : juce::Colours::white.withAlpha (down ? 0.22f : highlighted ? 0.14f : 0.08f));
            g.fillEllipse (bounds);

            const auto size = large ? bounds.getWidth() * 0.36f : bounds.getWidth() * 0.32f;
            drawGlyph (g, glyph, bounds.withSizeKeepingCentre (size, size), juce::Colours::white.withAlpha (isEnabled() ? 1.0f : 0.4f));
        }
    };

    void seekTo (const juce::MouseEvent& e)
    {
        if (waveArea.contains (e.getMouseDownPosition()) && player.getLength() > 0.0)
            player.setPosition (player.getLength() * juce::jlimit (0.0, 1.0, (double) (e.x - waveArea.getX()) / waveArea.getWidth()));
    }

    void chooseFiles()
    {
        chooser = std::make_unique<juce::FileChooser> ("Add Backing Tracks", CurveFolders::get (CurveFolders::Kind::media),
                                                       player.getFormatManager().getWildcardForAllFormats());

        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                                | juce::FileBrowserComponent::canSelectMultipleItems,
                              [safeThis = SafePointer<PlayerEditor> (this)] (const juce::FileChooser& fc)
                              {
                                  if (safeThis != nullptr)
                                      safeThis->player.addFiles (fc.getResults());
                              });
    }

    void loadThumbnail()
    {
        shownFile = player.getCurrentFile();

        if (shownFile.existsAsFile())
            thumbnail.setSource (new juce::FileInputSource (shownFile));
        else
            thumbnail.clear();
    }

    void changeListenerCallback (juce::ChangeBroadcaster* source) override
    {
        // The playlist or the track changed
        if (source == &player)
        {
            if (player.getCurrentFile() != shownFile)
                loadThumbnail();

            // e.g. a preset, or a scene, set another
            atEndBox.setSelectedItemIndex ((int) player.getAtEnd(), juce::dontSendNotification);
            list.updateContent();
        }

        repaint();
    }

    void timerCallback() override
    {
        playButton.glyph = player.isPlaying() ? Glyph::pause : Glyph::play;
        playButton.repaint();

        if (player.isPlaying() != wasPlaying)
        {
            wasPlaying = player.isPlaying();
            list.repaint();
        }

        if (! levelSlider.isMouseButtonDown())
            levelSlider.setValue (player.getLevelParameter().get(), juce::dontSendNotification);

        repaint (titleArea);
        repaint (waveArea);
    }

    //==============================================================================
    int getNumRows() override   { return player.getPlaylist().size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        const bool current = row == player.getCurrentIndex();
        const auto file = player.getPlaylist()[row];
        const bool exists = file.existsAsFile();

        if (selected)
            g.fillAll (Curve::Colours::accent.withAlpha (0.22f));

        // Its number, or for the one it's on, a play mark
        auto r = juce::Rectangle<int> (0, 0, width, height).reduced (10, 0);
        const auto numberArea = r.removeFromLeft (24);

        if (current && player.isPlaying())
            drawGlyph (g, Glyph::play, numberArea.toFloat().withSizeKeepingCentre (9.0f, 10.0f).withX ((float) numberArea.getX() + 2.0f),
                       Curve::Colours::link);
        else
        {
            g.setColour (current ? Curve::Colours::link : Curve::Colours::dimText);
            g.setFont (juce::FontOptions (12.0f).withFeatureEnabled ("tnum"));
            g.drawText (juce::String (row + 1), numberArea, juce::Justification::centredLeft);
        }

        g.setColour (Curve::Colours::dimText);
        g.setFont (juce::FontOptions (12.0f).withFeatureEnabled ("tnum"));
        g.drawText (exists ? getDuration (file) : juce::String ("Missing"), r.removeFromRight (64), juce::Justification::centredRight);
        r.removeFromRight (8);

        g.setColour (! exists ? Curve::Colours::dimText : current ? Curve::Colours::link : Curve::Colours::text);
        g.setFont (juce::FontOptions (13.5f, current ? juce::Font::bold : juce::Font::plain));
        g.drawText (file.getFileNameWithoutExtension(), r, juce::Justification::centredLeft, true);
    }

    // Dragged up or down to reorder it
    juce::var getDragSourceDescription (const juce::SparseSet<int>& rows) override
    {
        return rows.size() == 1 ? juce::var ("playlist:" + juce::String (rows[0])) : juce::var();
    }

    // A track's length, read once
    juce::String getDuration (const juce::File& file)
    {
        const auto key = file.getFullPathName();

        if (auto it = durations.find (key); it != durations.end())
            return it->second;

        juce::String text;

        if (std::unique_ptr<juce::AudioFormatReader> reader { player.getFormatManager().createReaderFor (file) })
            if (reader->sampleRate > 0.0)
                text = formatTime ((double) reader->lengthInSamples / reader->sampleRate);

        return durations[key] = text;
    }

    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override  { player.selectTrack (row, true); }

    void listBoxItemClicked (int row, const juce::MouseEvent& e) override
    {
        if (! e.mods.isPopupMenu())
            return;

        juce::PopupMenu menu;
        menu.addItem ("Play", [this, row] { player.selectTrack (row, true); });
        menu.addItem ("Show in Finder", [this, row] { player.getPlaylist()[row].revealToUser(); });
        menu.addSeparator();
        menu.addItem ("Move Up", row > 0, false, [this, row] { player.moveFile (row, row - 1); list.selectRow (row - 1); });
        menu.addItem ("Move Down", row + 1 < getNumRows(), false, [this, row] { player.moveFile (row, row + 2); list.selectRow (row + 1); });
        menu.addSeparator();
        menu.addItem ("Remove from Playlist", [this, row] { player.removeFile (row); });
        menu.showMenuAsync (juce::PopupMenu::Options().withDeletionCheck (*this));
    }

    void deleteKeyPressed (int row) override    { player.removeFile (row); }

    PlayerNode& player;
    juce::AudioThumbnailCache thumbnailCache { 4 };
    juce::AudioThumbnail thumbnail;
    juce::File shownFile;

    GlyphButton previousButton, playButton, stopButton, nextButton;
    juce::ComboBox atEndBox;
    juce::Slider levelSlider;
    juce::TextButton addButton;
    juce::Label listHeader, volumeLabel;
    std::map<juce::String, juce::String> durations;
    int dropIndex = -1;
    bool wasPlaying = false;
    juce::ListBox list;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> titleArea, waveArea;
    bool isDraggingFiles = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayerEditor)
};

//==============================================================================
PlayerNode::PlayerNode()
    : AudioPluginInstance (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    formatManager.registerBasicFormats();

    const auto add = [this] (const char* id, const char* name, InternalParameter*& p)
    {
        auto param = InternalParameter::makeBool (id, name, false, false);
        p = param.get();
        addHostedParameter (std::move (param));
        p->addListener (this);
    };

    add ("playPause", "Play/Pause", playParam);
    add ("stop",      "Stop",       stopParam);
    add ("next",      "Next Track", nextParam);
    add ("previous",  "Previous",   previousParam);

    InternalParameter::Options levelOptions;
    levelOptions.valueToText = [] (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; };
    auto levelP = std::make_unique<InternalParameter> ("level", "Level", juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f, levelOptions);
    levelParam = levelP.get();
    addHostedParameter (std::move (levelP));

    readAhead.startThread (juce::Thread::Priority::normal);
    transport.addChangeListener (this);
}

PlayerNode::~PlayerNode()
{
    cancelPendingUpdate();
    transport.removeChangeListener (this);
    transport.setSource (nullptr);
    readerSource = nullptr;
    readAhead.stopThread (1000);

    for (auto* p : { playParam, stopParam, nextParam, previousParam })
        p->removeListener (this);
}

bool PlayerNode::canPlay (const juce::File& file)
{
    return file.hasFileExtension ("wav;aif;aiff;flac;mp3;m4a;aac;caf;ogg");
}

void PlayerNode::addFiles (const juce::Array<juce::File>& files)
{
    const bool wasEmpty = playlist.isEmpty();

    for (const auto& f : files)
        if (canPlay (f) && ! playlist.contains (f))
            playlist.add (f);

    if (wasEmpty && ! playlist.isEmpty())
        load (0);

    sendChangeMessage();
}

void PlayerNode::removeFile (int index)
{
    if (! juce::isPositiveAndBelow (index, playlist.size()))
        return;

    const bool wasCurrent = index == currentIndex;
    playlist.remove (index);

    if (index < currentIndex)
        --currentIndex;

    if (wasCurrent)
        load (juce::jlimit (0, juce::jmax (0, playlist.size() - 1), currentIndex));

    sendChangeMessage();
}

void PlayerNode::moveFile (int index, int newIndex)
{
    if (! juce::isPositiveAndBelow (index, playlist.size()))
        return;

    newIndex = juce::jlimit (0, playlist.size(), newIndex);

    // Before the one at newIndex: once it's taken out, everything after it moves up one
    if (newIndex > index)
        --newIndex;

    if (newIndex == index)
        return;

    const auto current = playlist[currentIndex];
    playlist.move (index, newIndex);
    currentIndex = juce::jmax (0, playlist.indexOf (current));
    sendChangeMessage();
}

// Opens the track, ready to play from its start
void PlayerNode::load (int index)
{
    playingToEnd = false;
    transport.stop();
    transport.setSource (nullptr);
    readerSource = nullptr;
    currentIndex = juce::jlimit (0, juce::jmax (0, playlist.size() - 1), index);

    // Not an empty file, which, repeated, would divide by its length. setSource() prepares it, if
    // the transport has been
    if (std::unique_ptr<juce::AudioFormatReader> reader { formatManager.createReaderFor (playlist[currentIndex]) };
        reader != nullptr && reader->lengthInSamples > 0 && reader->sampleRate > 0.0)
    {
        const auto rate = reader->sampleRate;
        readerSource = std::make_unique<juce::AudioFormatReaderSource> (reader.release(), true);
        readerSource->setLooping (atEnd == AtEnd::repeat);
        transport.setSource (readerSource.get(), 32768, &readAhead, rate, 2);
    }

    sendChangeMessage();
}

void PlayerNode::selectTrack (int index, bool andPlay)
{
    load (index);

    if (andPlay)
        play();
}

void PlayerNode::play()
{
    if (readerSource == nullptr && ! playlist.isEmpty())
        load (currentIndex);

    if (readerSource == nullptr)
        return;

    if (transport.hasStreamFinished())
        transport.setPosition (0.0);

    playingToEnd = true;
    transport.start();
    sendChangeMessage();
}

void PlayerNode::pause()
{
    playingToEnd = false;
    transport.stop();
    sendChangeMessage();
}

void PlayerNode::stop()
{
    pause();
    transport.setPosition (0.0);
}

void PlayerNode::next()
{
    if (playlist.isEmpty())
        return;

    const bool wasPlaying = isPlaying();
    selectTrack ((currentIndex + 1) % playlist.size(), wasPlaying);
}

// As players do: back to the start, unless it's just started, then the track before
void PlayerNode::previous()
{
    if (playlist.isEmpty())
        return;

    const bool wasPlaying = isPlaying();

    if (getPosition() > 3.0)
        transport.setPosition (0.0);
    else
        selectTrack ((currentIndex + playlist.size() - 1) % playlist.size(), wasPlaying);
}

void PlayerNode::setAtEnd (AtEnd newAtEnd)
{
    atEnd = newAtEnd;

    if (readerSource != nullptr)
        readerSource->setLooping (atEnd == AtEnd::repeat);
}

// A track that was playing ended by itself: what's next. The transport says it's stopped after
// the stop, so whether it was stopped on purpose is kept until then, rather than only during it
void PlayerNode::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (playingToEnd && readerSource != nullptr && ! transport.isPlaying() && transport.hasStreamFinished())
    {
        playingToEnd = false;

        if (atEnd == AtEnd::next && currentIndex + 1 < playlist.size())
            selectTrack (currentIndex + 1, true);
        else
            transport.setPosition (0.0);
    }

    sendChangeMessage();
}

void PlayerNode::parameterValueChanged (int index, float newValue)
{
    if (newValue < 0.5f)
        return;

    if (index == playParam->getParameterIndex())            ++pressedPlay;
    else if (index == stopParam->getParameterIndex())       ++pressedStop;
    else if (index == nextParam->getParameterIndex())       ++pressedNext;
    else if (index == previousParam->getParameterIndex())   ++pressedPrevious;
    else return;

    triggerAsyncUpdate();
}

void PlayerNode::handleAsyncUpdate()
{
    // Back off, without telling anyone, so the next press sets them on again
    for (auto* p : { playParam, stopParam, nextParam, previousParam })
        p->setValue (0.0f);

    for (auto n = pressedPlay.exchange (0); n > 0; --n)
    {
        if (isPlaying())
            pause();
        else
            play();
    }

    for (auto n = pressedStop.exchange (0); n > 0; --n)        stop();
    for (auto n = pressedNext.exchange (0); n > 0; --n)        next();
    for (auto n = pressedPrevious.exchange (0); n > 0; --n)    previous();
}

void PlayerNode::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    currentBlockSize = juce::jmax (1, samplesPerBlock);
    playBuffer.setSize (2, currentBlockSize);
    transport.prepareToPlay (currentBlockSize, currentSampleRate);
    level.reset (currentSampleRate, 0.05);
    level.setCurrentAndTargetValue (levelParam->get());
}

void PlayerNode::releaseResources()
{
    transport.releaseResources();
}

void PlayerNode::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const auto numSamples = buffer.getNumSamples();

    // Nothing comes in: what's in the buffer is left from elsewhere in the graph
    buffer.clear();

    // Not prepared yet, so nothing to play into
    if (playBuffer.getNumSamples() <= 0)
        return;

    // A bigger block than it was prepared for is played in parts
    for (int done = 0; done < numSamples;)
    {
        const auto chunk = juce::jmin (numSamples - done, playBuffer.getNumSamples());
        juce::AudioSourceChannelInfo info (&playBuffer, 0, chunk);
        transport.getNextAudioBlock (info);

        level.setTargetValue (levelParam->get());

        for (int i = 0; i < chunk; ++i)
        {
            const auto gain = level.getNextValue();

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.getWritePointer (ch)[done + i] += playBuffer.getSample (juce::jmin (ch, 1), i) * gain;
        }

        done += chunk;
    }
}

void PlayerNode::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement xml ("PLAYER");
    xml.setAttribute ("track", currentIndex);
    xml.setAttribute ("atEnd", (int) atEnd);
    xml.setAttribute ("level", (double) levelParam->get());

    for (const auto& f : playlist)
        xml.createNewChildElement ("FILE")->setAttribute ("path", f.getFullPathName());

    copyXmlToBinary (xml, destData);
}

void PlayerNode::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        juce::Array<juce::File> files;

        for (auto* e : xml->getChildWithTagNameIterator ("FILE"))
            if (const auto path = e->getStringAttribute ("path"); juce::File::isAbsolutePath (path))
                files.add (juce::File (path));

        atEnd = (AtEnd) juce::jlimit (0, 2, xml->getIntAttribute ("atEnd"));
        levelParam->setValueNotifyingHost ((float) xml->getDoubleAttribute ("level", 1.0));

        const auto track = xml->getIntAttribute ("track");

        // The same playlist and track, e.g. a scene switch that only changes the level: left playing
        if (files == playlist && track == currentIndex && readerSource != nullptr)
        {
            setAtEnd (atEnd);
            sendChangeMessage();
            return;
        }

        playlist = files;
        load (track);
    }
}

bool PlayerNode::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return layouts.getMainInputChannelSet().isDisabled()
        && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
}

juce::AudioProcessorEditor* PlayerNode::createEditor()
{
    return new PlayerEditor (*this);
}

void PlayerNode::describe (juce::PluginDescription& description)
{
    description.name = "Player";
    description.descriptiveName = "Plays backing tracks from a playlist";
    description.pluginFormatName = "Internal";
    description.category = "Plugins";
    description.fileOrIdentifier = "Player";
    description.uniqueId = 0x504C4159; // "PLAY"
    description.isInstrument = false;
    description.numInputChannels = 0;
    description.numOutputChannels = 2;
}
