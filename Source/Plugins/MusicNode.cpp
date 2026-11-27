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

#include "MusicNode.h"
#include "../UI/CurveColours.h"

namespace
{
    // Inside an AppleScript string
    juce::String quoted (const juce::String& s)
    {
        return "\"" + s.replace ("\\", "\\\\").replace ("\"", "\\\"") + "\"";
    }

    juce::String formatTime (double seconds)
    {
        const auto total = juce::jmax (0, (int) seconds);
        return juce::String (total / 60) + ":" + juce::String (total % 60).paddedLeft ('0', 2);
    }

    // Durations and positions as whole tenths of a second, as AppleScript writes decimals as the
    // Mac's region does, e.g. with a comma
    const char* musicNowPlayingScript = R"(
if application "Music" is running then
    tell application "Music"
        set s to player state as text
        if s is "stopped" then return "stopped"
        set t to current track
        return s & linefeed & (name of t) & linefeed & (artist of t) & linefeed & (album of t) & linefeed & (round ((duration of t) * 10)) & linefeed & (round (player position * 10)) & linefeed & (persistent ID of t)
    end tell
end if
return ""
)";

    const char* spotifyNowPlayingScript = R"(
if application "Spotify" is running then
    tell application "Spotify"
        set s to player state as text
        if s is "stopped" then return "stopped"
        set t to current track
        return s & linefeed & (name of t) & linefeed & (artist of t) & linefeed & (album of t) & linefeed & ((duration of t) div 100) & linefeed & (round (player position * 10)) & linefeed & (id of t) & linefeed & (artwork url of t)
    end tell
end if
return ""
)";
}

//==============================================================================
/*  Runs the node's AppleScripts, and other slow work, one at a time on a thread of its own, and
    while watching, asks the apps what's playing once a second.
*/
class MusicNode::Worker final : public juce::Thread
{
public:
    using Job = std::function<juce::String()>;
    using Done = std::function<void (const juce::String&)>;

    // onWatchToUse is called on this thread, once a second while watching. Given here, before the
    // thread starts, as it's never changed while it runs
    explicit Worker (std::function<void()> onWatchToUse) : Thread ("Curve Music"), onWatch (std::move (onWatchToUse))
    {
        startThread();
    }

    ~Worker() override { stopThread (6000); }

    void add (Job job, Done done)
    {
        {
            const juce::ScopedLock sl (queueLock);
            queue.push_back ({ std::move (job), std::move (done) });
        }

        notify();
    }

    std::atomic<bool> watching { false };

    // On the worker's thread. Given up on after five seconds, e.g. an app that isn't answering, or
    // one waiting for macOS to ask whether Curve may control it, or as the worker stops. Its output
    // is read once it's finished: reading waits for it to close, so a hung script would never time
    // out. The scripts' output is small, well within what the pipe holds meanwhile
    static juce::String runScript (const juce::String& script)
    {
        juce::ChildProcess process;

        if (! process.start (juce::StringArray { "/usr/bin/osascript", "-e", script },
                             juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
            return "error: osascript";

        const auto deadline = juce::Time::getMillisecondCounter() + 5000;

        while (process.isRunning())
        {
            if (juce::Thread::currentThreadShouldExit() || juce::Time::getMillisecondCounter() > deadline)
            {
                process.kill();
                return "error: timed out";
            }

            juce::Thread::sleep (10);
        }

        const auto output = process.readAllProcessOutput();
        return process.getExitCode() == 0 ? output.trimEnd() : "error: " + output.trim();
    }

    // A download, given up on after a few seconds, or as the worker stops
    static juce::MemoryBlock download (const juce::String& url)
    {
        juce::MemoryBlock data;
        auto stream = juce::URL (url).createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                                             .withConnectionTimeoutMs (3000));

        if (stream == nullptr)
            return data;

        const auto deadline = juce::Time::getMillisecondCounter() + 8000;
        char chunk[8192];

        while (! stream->isExhausted() && data.getSize() < 10 * 1024 * 1024)
        {
            if (juce::Thread::currentThreadShouldExit() || juce::Time::getMillisecondCounter() > deadline)
                return {};

            const auto read = stream->read (chunk, (int) sizeof (chunk));

            if (read <= 0)
                break;

            data.append (chunk, (size_t) read);
        }

        return data;
    }

private:
    void run() override
    {
        auto lastWatch = juce::Time::getMillisecondCounter() - 1000;

        while (! threadShouldExit())
        {
            std::pair<Job, Done> next;

            {
                const juce::ScopedLock sl (queueLock);

                if (! queue.empty())
                {
                    next = std::move (queue.front());
                    queue.pop_front();
                }
            }

            if (next.first != nullptr)
            {
                const auto result = next.first();

                if (next.second != nullptr)
                    juce::MessageManager::callAsync ([done = std::move (next.second), result] { done (result); });

                continue;
            }

            if (watching.load() && juce::Time::getMillisecondCounter() - lastWatch >= 1000)
            {
                lastWatch = juce::Time::getMillisecondCounter();

                if (onWatch != nullptr)
                    onWatch();

                continue;
            }

            wait (100);
        }
    }

    const std::function<void()> onWatch;
    juce::CriticalSection queueLock;
    std::deque<std::pair<Job, Done>> queue;
};

//==============================================================================
class MusicEditor final : public juce::AudioProcessorEditor,
                          private juce::ChangeListener,
                          private juce::Timer,
                          private juce::ListBoxModel
{
public:
    explicit MusicEditor (MusicNode& n) : AudioProcessorEditor (n), music (n)
    {
        const auto setUp = [this] (GlyphButton& b, InternalParameter& p, int glyph, const char* tip)
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

        setUp (previousButton, music.getPreviousParameter(), 0, "Previous track");
        setUp (playButton, music.getPlayPauseParameter(), 1, "Play or pause");
        setUp (nextButton, music.getNextParameter(), 3, "Next track");
        playButton.large = true;

        searchBox.setTextToShowWhenEmpty ("Search the songs in your library", Curve::Colours::dimText);
        searchBox.setFont (juce::FontOptions (13.5f));
        searchBox.setIndents (8, 6);
        searchBox.onReturnKey = [this] { runSearch(); };
        searchBox.onTextChange = [this] { searchPending = juce::Time::getMillisecondCounter(); };
        addAndMakeVisible (searchBox);

        linkBox.setTextToShowWhenEmpty ("Paste a Spotify link, then press Return", Curve::Colours::dimText);
        linkBox.setFont (juce::FontOptions (13.5f));
        linkBox.setIndents (8, 6);
        linkBox.onReturnKey = [this]
        {
            music.playSpotifyLink (linkBox.getText());
            linkBox.clear();
        };
        addChildComponent (linkBox);

        for (auto* b : { &searchTab, &playlistsTab })
        {
            b->setClickingTogglesState (true);
            b->setRadioGroupId (1);
            b->setColour (juce::TextButton::buttonOnColourId, Curve::Colours::accent);
            addAndMakeVisible (b);
        }

        searchTab.setButtonText ("Library");
        searchTab.setTooltip ("Search the songs in your Music library");
        playlistsTab.setButtonText ("Playlists");
        playlistsTab.setTooltip ("Your playlists in Music");

        // AppleScript reaches only the library, so the catalogue is searched in Music itself
        catalogueButton.setButtonText ("Search Apple Music");
        catalogueButton.setTooltip ("Opens the search in the Music app, to find songs that aren't in your library. Songs you add "
                                    "to your library there can then be found and played here");
        catalogueButton.onClick = [this]
        {
            juce::URL ("music://music.apple.com/search").withParameter ("term", searchBox.getText().trim()).launchInDefaultBrowser();
        };
        addAndMakeVisible (catalogueButton);

        emptyLabel.setFont (juce::FontOptions (13.0f));
        emptyLabel.setColour (juce::Label::textColourId, Curve::Colours::dimText);
        emptyLabel.setJustificationType (juce::Justification::centred);
        emptyLabel.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (emptyLabel);
        searchTab.setToggleState (true, juce::dontSendNotification);
        searchTab.onClick = [this] { showResults (searchResults); };
        playlistsTab.onClick = [this]
        {
            music.getPlaylists ([safeThis = SafePointer<MusicEditor> (this)] (juce::Array<MusicNode::Item> items)
            {
                if (safeThis != nullptr)
                {
                    safeThis->playlists = items;

                    if (safeThis->playlistsTab.getToggleState())
                        safeThis->showResults (items);
                }
            });
        };

        list.setModel (this);
        list.setRowHeight (40);
        list.setOutlineThickness (0);
        list.setColour (juce::ListBox::backgroundColourId, Curve::Colours::surface);
        addAndMakeVisible (list);

        allowButton.setButtonText ("Open Automation Settings...");
        allowButton.onClick = []
        {
            juce::URL ("x-apple.systempreferences:com.apple.preference.security?Privacy_Automation").launchInDefaultBrowser();
        };
        addChildComponent (allowButton);

        music.addChangeListener (this);
        music.setWatching (true);

        setResizable (true, false);
        setResizeLimits (360, 420, 1200, 1600);
        setSize (440, 560);
        startTimerHz (10);
        update();
    }

    ~MusicEditor() override
    {
        stopTimer();
        music.setWatching (false);
        music.removeChangeListener (this);
        list.setModel (nullptr);
        music.editorBeingDeleted (this);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Curve::Colours::canvas);

        const auto now = music.getNowPlaying();

        // The artwork, or a note where there's none
        const auto art = artArea.toFloat();
        g.setColour (Curve::Colours::surface);
        g.fillRoundedRectangle (art, 8.0f);

        if (now.artwork.isValid())
        {
            juce::Graphics::ScopedSaveState state (g);
            juce::Path clip;
            clip.addRoundedRectangle (art, 8.0f);
            g.reduceClipRegion (clip);
            g.drawImage (now.artwork, art, juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);
        }
        else
        {
            g.setColour (Curve::Colours::dimText.withAlpha (0.4f));
            g.setFont (juce::FontOptions (42.0f));
            g.drawText (juce::String::fromUTF8 ("\xe2\x99\xaa"), art, juce::Justification::centred);
        }

        // What's playing
        auto info = infoArea;
        const auto problem = music.getProblem();

        g.setColour (Curve::Colours::text);
        g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
        g.drawFittedText (now.app == MusicNode::App::none ? juce::String ("Nothing playing") : now.title,
                          info.removeFromTop (24), juce::Justification::centredLeft, 1);

        g.setColour (Curve::Colours::dimText);
        g.setFont (juce::FontOptions (13.0f));

        if (problem.isNotEmpty())
        {
            g.setColour (Curve::Colours::warning);
            g.drawFittedText (problem, info.removeFromTop (54), juce::Justification::topLeft, 3);
        }
        else if (now.app == MusicNode::App::none)
        {
            g.drawFittedText ("Play something in Music" + juce::String (MusicNode::isSpotifyInstalled() ? " or Spotify" : "")
                                + " to control it here", info.removeFromTop (40), juce::Justification::topLeft, 2);
        }
        else
        {
            g.drawFittedText (now.artist, info.removeFromTop (20), juce::Justification::centredLeft, 1);
            g.drawFittedText (now.album, info.removeFromTop (20), juce::Justification::centredLeft, 1);

            g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
            g.setColour (now.app == MusicNode::App::spotify ? Curve::Colours::success : Curve::Colours::danger.brighter (0.3f));
            g.drawText (now.app == MusicNode::App::spotify ? "SPOTIFY" : "MUSIC", info.removeFromTop (18), juce::Justification::centredLeft);
        }

        // Where it is in the track, which a click moves
        if (now.duration > 0.0)
        {
            const auto bar = positionArea.toFloat().withSizeKeepingCentre ((float) positionArea.getWidth(), 4.0f);
            g.setColour (juce::Colours::white.withAlpha (0.15f));
            g.fillRoundedRectangle (bar, 2.0f);
            g.setColour (Curve::Colours::accent);
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * (float) juce::jlimit (0.0, 1.0, now.position / now.duration)), 2.0f);

            g.setColour (Curve::Colours::dimText);
            g.setFont (juce::FontOptions (11.0f));
            g.drawText (formatTime (now.position), timesArea, juce::Justification::centredLeft);
            g.drawText (formatTime (now.duration), timesArea, juce::Justification::centredRight);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (16, 14);

        auto top = r.removeFromTop (128);
        artArea = top.removeFromLeft (128);
        top.removeFromLeft (16);
        infoArea = top.removeFromTop (84);

        // In the transport's place, as there's nothing to control until Curve's allowed to
        auto transport = top.removeFromBottom (40);
        allowButton.setBounds (transport.withWidth (juce::jmin (transport.getWidth(), 220)).withSizeKeepingCentre (juce::jmin (transport.getWidth(), 220), 26));

        previousButton.setBounds (transport.removeFromLeft (36).withSizeKeepingCentre (36, 36));
        transport.removeFromLeft (8);
        playButton.setBounds (transport.removeFromLeft (40));
        transport.removeFromLeft (8);
        nextButton.setBounds (transport.removeFromLeft (36).withSizeKeepingCentre (36, 36));

        r.removeFromTop (10);
        positionArea = r.removeFromTop (14);
        timesArea = r.removeFromTop (16);

        r.removeFromTop (16);
        auto tabs = r.removeFromTop (28);
        searchTab.setBounds (tabs.removeFromLeft (86));
        tabs.removeFromLeft (4);
        playlistsTab.setBounds (tabs.removeFromLeft (86));
        catalogueButton.setBounds (tabs.removeFromRight (140));

        r.removeFromTop (10);
        const auto field = r.removeFromTop (30);
        searchBox.setBounds (field);
        linkBox.setBounds (field);
        r.removeFromTop (8);
        list.setBounds (r);
        emptyLabel.setBounds (r.reduced (20));
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto now = music.getNowPlaying();

        if (positionArea.expanded (0, 6).contains (e.getPosition()) && now.duration > 0.0)
            music.seek (now.duration * juce::jlimit (0.0, 1.0, (double) (e.x - positionArea.getX()) / positionArea.getWidth()));
    }

private:
    struct GlyphButton final : public juce::Button
    {
        GlyphButton() : Button ({}) { setWantsKeyboardFocus (false); }
        int glyph = 1; // previous, play, pause, next
        bool large = false;

        void paintButton (juce::Graphics& g, bool highlighted, bool down) override
        {
            const auto b = getLocalBounds().toFloat().reduced (1.0f);
            g.setColour (large ? Curve::Colours::accent.withMultipliedBrightness (down ? 0.8f : highlighted ? 1.15f : 1.0f)
                               : juce::Colours::white.withAlpha (down ? 0.22f : highlighted ? 0.14f : 0.08f));
            g.fillEllipse (b);

            const auto r = b.withSizeKeepingCentre (b.getWidth() * 0.36f, b.getWidth() * 0.36f);
            juce::Path p;

            switch (glyph)
            {
                case 1: p.addTriangle (r.getX() + r.getWidth() * 0.15f, r.getY(), r.getX() + r.getWidth() * 0.15f, r.getBottom(), r.getRight(), r.getCentreY()); break;
                case 2: p.addRectangle (r.getX() + r.getWidth() * 0.1f, r.getY(), r.getWidth() * 0.28f, r.getHeight());
                        p.addRectangle (r.getX() + r.getWidth() * 0.62f, r.getY(), r.getWidth() * 0.28f, r.getHeight()); break;
                case 3: p.addTriangle (r.getX(), r.getY(), r.getX(), r.getBottom(), r.getX() + r.getWidth() * 0.75f, r.getCentreY());
                        p.addRectangle (r.getRight() - r.getWidth() * 0.18f, r.getY(), r.getWidth() * 0.18f, r.getHeight()); break;
                default: p.addTriangle (r.getRight(), r.getY(), r.getRight(), r.getBottom(), r.getX() + r.getWidth() * 0.25f, r.getCentreY());
                         p.addRectangle (r.getX(), r.getY(), r.getWidth() * 0.18f, r.getHeight()); break;
            }

            g.setColour (juce::Colours::white);
            g.fillPath (p);
        }
    };

    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        update();
        repaint();
    }

    // A search once typing pauses
    void timerCallback() override
    {
        if (searchPending != 0 && juce::Time::getMillisecondCounter() - searchPending > 500)
            runSearch();
    }

    void update()
    {
        const auto now = music.getNowPlaying();
        const bool spotify = now.app == MusicNode::App::spotify;

        playButton.glyph = now.isPlaying ? 2 : 1;
        playButton.repaint();

        // Spotify can't be searched from here, but plays a link pasted in
        searchBox.setVisible (! spotify && searchTab.getToggleState());
        linkBox.setVisible (spotify);
        searchTab.setVisible (! spotify);
        playlistsTab.setVisible (! spotify);
        catalogueButton.setVisible (! spotify && searchTab.getToggleState());
        list.setVisible (! spotify);
        updateEmptyLabel();
        const bool notAllowed = music.getProblem().containsIgnoreCase ("allow");
        allowButton.setVisible (notAllowed);

        for (auto* b : { &previousButton, &playButton, &nextButton })
            b->setVisible (! notAllowed);
    }

    void runSearch()
    {
        searchPending = 0;
        const auto text = searchBox.getText().trim();

        if (text.isEmpty())
        {
            ++searchNumber; // One still running is out of date
            searchResults.clear();
            showResults (searchResults);
            return;
        }

        // Only the latest search's results, which may come back after an earlier one's
        const auto thisSearch = ++searchNumber;

        music.search (text, [safeThis = SafePointer<MusicEditor> (this), thisSearch] (juce::Array<MusicNode::Item> items)
        {
            if (safeThis != nullptr && safeThis->searchNumber == thisSearch)
            {
                safeThis->searchResults = items;

                if (safeThis->searchTab.getToggleState())
                    safeThis->showResults (items);
            }
        });
    }

    void showResults (const juce::Array<MusicNode::Item>& items)
    {
        shown = items;
        searchBox.setVisible (searchTab.getToggleState() && music.getNowPlaying().app != MusicNode::App::spotify);
        catalogueButton.setVisible (searchBox.isVisible());
        list.updateContent();
        list.repaint();
        updateEmptyLabel();
    }

    // What the list would show, while it's empty
    void updateEmptyLabel()
    {
        const auto spotify = music.getNowPlaying().app == MusicNode::App::spotify;
        juce::String text;

        if (spotify)
            text = "Spotify can't be searched from here: copy a song's, album's or playlist's link in Spotify (Share > Copy Link), "
                   "and paste it above";
        else if (shown.isEmpty() && playlistsTab.getToggleState())
            text = "No playlists of your own in Music";
        else if (shown.isEmpty() && searchBox.getText().trim().isEmpty())
            text = "Type to search the songs in your library, then double-click one to play it";
        else if (shown.isEmpty())
            text = "No songs in your library match. Search Apple Music to find others";

        emptyLabel.setText (text, juce::dontSendNotification);
        emptyLabel.setVisible (text.isNotEmpty());
    }

    int getNumRows() override   { return shown.size(); }

    // The name, with the artist, or the number of songs, beneath it
    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (selected)
            g.fillAll (Curve::Colours::accent.withAlpha (0.22f));

        const auto& item = shown.getReference (row);
        auto r = juce::Rectangle<int> (12, 4, width - 24, height - 8);

        g.setColour (Curve::Colours::text);
        g.setFont (juce::FontOptions (13.5f));
        g.drawText (item.name, r.removeFromTop (r.getHeight() / 2 + 1), juce::Justification::bottomLeft, true);

        g.setColour (Curve::Colours::dimText);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (item.detail, r.withTrimmedTop (1), juce::Justification::topLeft, true);
    }

    void playRow (int row)
    {
        if (! juce::isPositiveAndBelow (row, shown.size()))
            return;

        const auto& item = shown.getReference (row);

        if (playlistsTab.getToggleState())
            music.playPlaylist (item.id);
        else
            music.playTrack (item.id);
    }

    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override   { playRow (row); }
    void returnKeyPressed (int row) override                                    { playRow (row); }

    MusicNode& music;
    GlyphButton previousButton, playButton, nextButton;
    juce::TextEditor searchBox, linkBox;
    juce::TextButton searchTab, playlistsTab, allowButton, catalogueButton;
    juce::Label emptyLabel;
    juce::ListBox list;
    juce::Array<MusicNode::Item> searchResults, playlists, shown;
    juce::uint32 searchPending = 0;
    int searchNumber = 0;
    juce::Rectangle<int> artArea, infoArea, positionArea, timesArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MusicEditor)
};

//==============================================================================
MusicNode::MusicNode()
    : AudioPluginInstance (BusesProperties())
{
    const auto add = [this] (const char* id, const char* name, InternalParameter*& p)
    {
        auto param = InternalParameter::makeBool (id, name, false, false);
        p = param.get();
        addHostedParameter (std::move (param));
        p->addListener (this);
    };

    add ("playPause", "Play/Pause", playPauseParam);
    add ("next",      "Next Track", nextParam);
    add ("previous",  "Previous Track", previousParam);
}

MusicNode::~MusicNode()
{
    *alive = false;
    cancelPendingUpdate();
    worker = nullptr;

    for (auto* p : { playPauseParam, nextParam, previousParam })
        p->removeListener (this);
}

bool MusicNode::isSpotifyInstalled()
{
    for (const auto& dir : { juce::File ("/Applications"), juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Applications") })
        if (dir.getChildFile ("Spotify.app").isDirectory())
            return true;

    return false;
}

MusicNode::NowPlaying MusicNode::getNowPlaying() const
{
    const juce::ScopedLock sl (lock);
    return nowPlaying;
}

juce::String MusicNode::getProblem() const
{
    const juce::ScopedLock sl (lock);
    return problem;
}

// The worker is made the first time it's needed, so a Music node that's never used starts no thread
MusicNode::Worker& MusicNode::getWorker()
{
    if (worker == nullptr)
    {
        // While watching: both apps asked, on the worker's thread, then the answer taken on the message thread
        worker = std::make_unique<Worker> ([this, weak = std::weak_ptr<bool> (alive)]
        {
            const auto musicOutput = Worker::runScript (musicNowPlayingScript);

            if (juce::Thread::currentThreadShouldExit())
                return;

            const auto spotifyOutput = isSpotifyInstalled() ? Worker::runScript (spotifyNowPlayingScript) : juce::String();

            juce::MessageManager::callAsync ([this, weak, musicOutput, spotifyOutput]
            {
                if (auto isAlive = weak.lock(); isAlive != nullptr && *isAlive)
                    updateNowPlaying (musicOutput, spotifyOutput);
            });
        });
    }

    return *worker;
}

void MusicNode::run (const juce::String& script, std::function<void (const juce::String&)> onDone)
{
    getWorker().add ([script] { return Worker::runScript (script); },
                 [weak = std::weak_ptr<bool> (alive), onDone] (const juce::String& result)
                 {
                     if (auto isAlive = weak.lock(); isAlive != nullptr && *isAlive && onDone != nullptr)
                         onDone (result);
                 });
}

// To whichever app is playing, or was last, or Music
juce::String MusicNode::controlScript (const juce::String& musicCommand, const juce::String& spotifyCommand) const
{
    const auto app = getNowPlaying().app;

    if (app == App::spotify)
        return "tell application \"Spotify\" to " + spotifyCommand;

    // Not known, as the window, which watches what's playing, hasn't been open, e.g. a footswitch
    // mapped to it: the app that's playing, or else that's open, rather than always Music
    if (app == App::none && isSpotifyInstalled())
        return R"(
set target to "Music"
if application "Spotify" is running then
    tell application "Spotify" to set spotifyPlaying to (player state is playing)
    if spotifyPlaying or application "Music" is not running then set target to "Spotify"
end if
if target is "Spotify" then
    tell application "Spotify" to )" + spotifyCommand + R"(
else
    tell application "Music" to )" + musicCommand + R"(
end if
)";

    return "tell application \"Music\" to " + musicCommand;
}

void MusicNode::playPause()         { run (controlScript ("playpause", "playpause")); }
void MusicNode::next()              { run (controlScript ("next track", "next track")); }
void MusicNode::previous()          { run (controlScript ("previous track", "previous track")); }

void MusicNode::seek (double seconds)
{
    const auto value = juce::String (seconds, 1).replaceCharacter (',', '.');
    run (controlScript ("set player position to " + value, "set player position to " + value));

    const juce::ScopedLock sl (lock);
    nowPlaying.position = seconds;
}

void MusicNode::playTrack (const juce::String& id)
{
    run ("tell application \"Music\" to play (first track of library playlist 1 whose persistent ID is " + quoted (id) + ")");
}

void MusicNode::playPlaylist (const juce::String& id)
{
    run ("tell application \"Music\" to play (first user playlist whose persistent ID is " + quoted (id) + ")");
}

void MusicNode::playSpotifyLink (const juce::String& link)
{
    // https://open.spotify.com/track/ID?si=... as spotify:track:ID, or a spotify: link as it is
    auto uri = link.trim();

    if (uri.containsIgnoreCase ("open.spotify.com/"))
    {
        auto path = uri.fromFirstOccurrenceOf ("open.spotify.com/", false, true).upToFirstOccurrenceOf ("?", false, false);
        auto parts = juce::StringArray::fromTokens (path, "/", {});
        parts.removeEmptyStrings();

        // Links may start with a region, e.g. /intl-en/track/...
        if (parts.size() >= 3 && parts[0].startsWith ("intl-"))
            parts.remove (0);

        if (parts.size() >= 2)
            uri = "spotify:" + parts[0] + ":" + parts[1];
    }

    if (uri.startsWith ("spotify:"))
        run ("tell application \"Spotify\" to play track " + quoted (uri));
}

static juce::Array<MusicNode::Item> parseItems (const juce::String& output, bool detailIsSecond)
{
    juce::Array<MusicNode::Item> items;

    if (output.startsWith ("error:"))
        return items;

    for (const auto& line : juce::StringArray::fromLines (output))
    {
        const auto fields = juce::StringArray::fromTokens (line, "\t", {});

        if (fields.size() >= 2)
            items.add ({ fields[1], detailIsSecond && fields.size() > 2 ? fields[2] : juce::String(), fields[0] });
    }

    return items;
}

void MusicNode::search (const juce::String& text, std::function<void (juce::Array<Item>)> onResults)
{
    const auto script = R"(
tell application "Music"
    set r to (search library playlist 1 for )" + quoted (text) + R"( only songs)
    set out to ""
    set n to 0
    repeat with t in r
        set n to n + 1
        if n > 50 then exit repeat
        set out to out & (persistent ID of t) & tab & (name of t) & tab & (artist of t) & linefeed
    end repeat
    return out
end tell
)";

    run (script, [onResults] (const juce::String& output) { onResults (parseItems (output, true)); });
}

void MusicNode::getPlaylists (std::function<void (juce::Array<Item>)> onResults)
{
    const auto script = R"(
tell application "Music"
    set out to ""
    repeat with p in user playlists
        if special kind of p is none then set out to out & (persistent ID of p) & tab & (name of p) & tab & (count of tracks of p) & " songs" & linefeed
    end repeat
    return out
end tell
)";

    run (script, [onResults] (const juce::String& output) { onResults (parseItems (output, true)); });
}

void MusicNode::setWatching (bool shouldWatch)
{
    getWorker().watching = shouldWatch;
}

void MusicNode::updateNowPlaying (const juce::String& musicOutput, const juce::String& spotifyOutput)
{
    const auto parse = [] (const juce::String& output, App app)
    {
        NowPlaying n;
        const auto lines = juce::StringArray::fromLines (output);

        if (lines.size() < 7 || lines[0] == "stopped")
            return n;

        n.app = app;
        n.isPlaying = lines[0] == "playing";
        n.title = lines[1];
        n.artist = lines[2];
        n.album = lines[3];
        n.duration = lines[4].getIntValue() / 10.0;
        n.position = lines[5].getIntValue() / 10.0;
        n.trackID = lines[6];
        n.artworkURL = lines[7];
        return n;
    };

    // Not allowed to control them: macOS says so as error -1743
    const auto denied = [] (const juce::String& output) { return output.contains ("-1743") || output.containsIgnoreCase ("not authori"); };

    juce::String newProblem;

    if (denied (musicOutput) || denied (spotifyOutput))
        newProblem = "Curve isn't allowed to control " + juce::String (denied (musicOutput) ? "Music" : "Spotify")
                   + ". Allow it in System Settings > Privacy & Security > Automation.";

    const auto fromMusic = parse (musicOutput, App::music);
    const auto fromSpotify = parse (spotifyOutput, App::spotify);

    // The one playing, or else the one paused, Music first
    auto chosen = fromMusic.isPlaying ? fromMusic : fromSpotify.isPlaying ? fromSpotify
                : fromMusic.app != App::none ? fromMusic : fromSpotify;

    bool newTrack = false;

    {
        const juce::ScopedLock sl (lock);
        newTrack = chosen.trackID != nowPlaying.trackID || chosen.app != nowPlaying.app;

        if (! newTrack)
            chosen.artwork = nowPlaying.artwork;

        nowPlaying = chosen;
        problem = newProblem;
    }

    // Its artwork, once per track
    if (newTrack && chosen.app != App::none)
    {
        const auto trackID = chosen.trackID;
        const auto setArtwork = [this, trackID] (juce::Image image)
        {
            const juce::ScopedLock sl (lock);

            if (nowPlaying.trackID == trackID)
                nowPlaying.artwork = image;
        };

        if (chosen.app == App::music)
        {
            // A file of its own, so two Music Remotes, or quick track changes, don't share one, and
            // deleted once read
            const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                .getChildFile ("Curve Music Artwork " + juce::Uuid().toString());
            const auto script = R"(
tell application "Music"
    try
        set d to raw data of artwork 1 of current track
    on error
        return "none"
    end try
end tell
set f to open for access (POSIX file )" + quoted (file.getFullPathName()) + R"() with write permission
set eof f to 0
write d to f
close access f
return "ok"
)";

            run (script, [this, file, setArtwork] (const juce::String& result)
            {
                setArtwork (result == "ok" ? juce::ImageFileFormat::loadFrom (file) : juce::Image());
                file.deleteFile();
                sendChangeMessage();
            });
        }
        else if (chosen.artworkURL.startsWith ("https://"))
        {
            const auto url = chosen.artworkURL;
            auto weak = std::weak_ptr<bool> (alive);

            getWorker().add ([url] { return Worker::download (url).toBase64Encoding(); },
            [weak, setArtwork, this] (const juce::String& encoded)
            {
                if (auto isAlive = weak.lock(); isAlive == nullptr || ! *isAlive)
                    return;

                juce::MemoryBlock data;
                data.fromBase64Encoding (encoded);
                setArtwork (juce::ImageFileFormat::loadFrom (data.getData(), data.getSize()));
                sendChangeMessage();
            });
        }
    }

    sendChangeMessage();
}

void MusicNode::parameterValueChanged (int index, float newValue)
{
    if (newValue < 0.5f)
        return;

    if (index == playPauseParam->getParameterIndex())       ++pressedPlayPause;
    else if (index == nextParam->getParameterIndex())       ++pressedNext;
    else if (index == previousParam->getParameterIndex())   ++pressedPrevious;
    else return;

    triggerAsyncUpdate();
}

void MusicNode::handleAsyncUpdate()
{
    for (auto* p : { playPauseParam, nextParam, previousParam })
        p->setValue (0.0f);

    for (auto n = pressedPlayPause.exchange (0); n > 0; --n)   playPause();
    for (auto n = pressedNext.exchange (0); n > 0; --n)        next();
    for (auto n = pressedPrevious.exchange (0); n > 0; --n)    previous();
}

juce::AudioProcessorEditor* MusicNode::createEditor()
{
    return new MusicEditor (*this);
}

void MusicNode::fillInPluginDescription (juce::PluginDescription& description) const
{
    description.name = getName();
    description.descriptiveName = "Controls Music or Spotify: what's playing, play and pause, search";
    description.pluginFormatName = "Internal";
    description.category = "Plugins";
    description.fileOrIdentifier = getName();
    description.uniqueId = 0x4D555349; // "MUSI"
    description.isInstrument = false;
    description.numInputChannels = 0;
    description.numOutputChannels = 0;
}
