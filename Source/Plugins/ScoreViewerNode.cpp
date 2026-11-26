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

#include "ScoreViewerNode.h"
#include "PdfView.h"
#include "../UI/CurveColours.h"
#include "../UI/WindowLock.h"
#include "../UI/CurveFolders.h"

juce::PropertiesFile* getUserSettings();

namespace
{
    // Every viewer open, for the page-turn keys
    juce::Array<juce::Component::SafePointer<juce::Component>>& getOpenViewers()
    {
        static juce::Array<juce::Component::SafePointer<juce::Component>> viewers;
        return viewers;
    }

    // As PluginWindow has it: while the plug-in windows are locked with their title bars hidden,
    // the viewer shows only the score
    bool isStageLayout()
    {
        if (auto* settings = getUserSettings())
            return WindowLock::locked.load()
                && settings->getBoolValue ("pluginWindowTitleBarsHiddenWhenLocked", true);

        return false;
    }
}

//==============================================================================
class ScoreViewerEditor final : public juce::AudioProcessorEditor,
                                public juce::FileDragAndDropTarget,
                                private juce::ChangeListener,
                                private juce::Timer
{
public:
    explicit ScoreViewerEditor (ScoreViewerNode& n) : AudioProcessorEditor (n), node (n)
    {
        pdfView.setBackground (Curve::Colours::canvas);
        pdfView.onSwipe = [this] (int direction) { node.turn (direction); };
        host.setView (pdfView.getNSView());
        addAndMakeVisible (host);

        openButton.setButtonText ("Open...");
        openButton.setTooltip ("Open a PDF or an image of a score, chart or lyrics");
        openButton.onClick = [this] { chooseFile(); };

        // As parameters, with a gesture, so MIDI Learn picks them up when they're clicked
        const auto pressButton = [] (InternalParameter& p)
        {
            p.beginChangeGesture();
            p.setValueNotifyingHost (1.0f);
            p.endChangeGesture();
        };

        previousButton.setButtonText (juce::String::fromUTF8 ("\xe2\x97\x80")); // ◀
        previousButton.setTooltip ("Previous page. In MIDI Learn, click to map a control to it");
        previousButton.onClick = [this, pressButton] { pressButton (node.getPreviousPageParameter()); };

        nextButton.setButtonText (juce::String::fromUTF8 ("\xe2\x96\xb6")); // ▶
        nextButton.setTooltip ("Next page. In MIDI Learn, click to map a control to it");
        nextButton.onClick = [this, pressButton] { pressButton (node.getNextPageParameter()); };

        twoUpButton.setButtonText ("Two Pages");
        twoUpButton.setClickingTogglesState (true);
        twoUpButton.setColour (juce::TextButton::buttonOnColourId, Curve::Colours::accent);
        twoUpButton.onClick = [this] { node.setTwoUp (twoUpButton.getToggleState()); };

        pageLabel.setJustificationType (juce::Justification::centred);
        pageLabel.setColour (juce::Label::textColourId, Curve::Colours::dimText);
        fileLabel.setColour (juce::Label::textColourId, Curve::Colours::dimText);
        fileLabel.setMinimumHorizontalScale (1.0f);

        for (auto* c : std::initializer_list<juce::Component*> { &openButton, &fileLabel, &previousButton, &pageLabel, &nextButton, &twoUpButton })
            addAndMakeVisible (c);

        node.addChangeListener (this);
        loadFile();
        timerCallback(); // In the stage layout from the start, if the windows are locked

        getOpenViewers().add (this);

        if (getOpenViewers().size() == 1)
            keyMonitor = PdfView::addPageKeyMonitor ([] (int direction) { return turnFrontViewer (direction); });

        setResizable (true, false);
        setResizeLimits (240, 200, 4000, 4000);
        setSize (520, 700);
        startTimer (500);
    }

    ~ScoreViewerEditor() override
    {
        // As Curve's other editors do: JUCE leaves it to the plug-in's editor to say it's going.
        // Otherwise the plug-in still has it, and the next window gets JUCE's generic controls
        node.editorBeingDeleted (this);
        node.removeChangeListener (this);
        getOpenViewers().removeAllInstancesOf (this);

        // The monitor goes with the first viewer's editor, so hand it on if others are open
        if (keyMonitor != nullptr)
        {
            PdfView::removePageKeyMonitor (keyMonitor);

            for (auto& viewer : getOpenViewers())
                if (auto* other = dynamic_cast<ScoreViewerEditor*> (viewer.getComponent()))
                {
                    other->keyMonitor = PdfView::addPageKeyMonitor ([] (int direction) { return turnFrontViewer (direction); });
                    break;
                }
        }

        host.setView (nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Curve::Colours::canvas);

        if (! isLoaded)
        {
            const auto& f = node.getFile();
            const auto message = f == juce::File()  ? juce::String ("Open a PDF or an image of a score, or drop one here")
                               : ! f.existsAsFile() ? "Can't find " + f.getFullPathName()
                                                    : "Can't read " + f.getFileName() + " as a PDF or an image";

            g.setColour (Curve::Colours::dimText);
            g.setFont (juce::FontOptions (14.0f));
            g.drawFittedText (message, getLocalBounds().withTrimmedTop (toolbarHeight()).reduced (20), juce::Justification::centred, 4);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto bar = r.removeFromTop (toolbarHeight()).reduced (8, 5);

        openButton.setBounds (bar.removeFromLeft (70));
        bar.removeFromLeft (8);
        twoUpButton.setBounds (bar.removeFromRight (84));
        bar.removeFromRight (8);
        nextButton.setBounds (bar.removeFromRight (30));
        pageLabel.setBounds (bar.removeFromRight (64));
        previousButton.setBounds (bar.removeFromRight (30));
        bar.removeFromRight (8);
        fileLabel.setBounds (bar);

        host.setBounds (r);
    }

    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        return files.size() == 1 && PdfView::canOpen (juce::File (files[0]));
    }

    void filesDropped (const juce::StringArray& files, int, int) override
    {
        node.setFile (juce::File (files[0]));
    }

private:
    int toolbarHeight() const   { return stageLayout ? 0 : 36; }

    // The viewer whose window was in front most recently: JUCE keeps its windows in that order
    static bool turnFrontViewer (int direction)
    {
        auto& desktop = juce::Desktop::getInstance();

        for (int i = desktop.getNumComponents(); --i >= 0;)
            for (auto& viewer : getOpenViewers())
                if (auto* editor = dynamic_cast<ScoreViewerEditor*> (viewer.getComponent()))
                    if (editor->getTopLevelComponent() == desktop.getComponent (i) && editor->isShowing())
                    {
                        editor->node.turn (direction);
                        return true;
                    }

        return false;
    }

    void chooseFile()
    {
        // Where the last one was, or the folder chosen in Preferences > Files
        const auto start = node.getFile().getParentDirectory().isDirectory() ? node.getFile().getParentDirectory()
                                                                             : CurveFolders::get (CurveFolders::Kind::media);

        chooser = std::make_unique<juce::FileChooser> ("Open a Score", start,
                                                       "*.pdf;*.png;*.jpg;*.jpeg;*.tif;*.tiff;*.gif;*.heic");

        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safeThis = SafePointer<ScoreViewerEditor> (this)] (const juce::FileChooser& fc)
                              {
                                  if (safeThis != nullptr && fc.getResult() != juce::File())
                                      safeThis->node.setFile (fc.getResult());
                              });
    }

    void loadFile()
    {
        loadedFile = node.getFile();

        isLoaded = loadedFile.existsAsFile() && pdfView.load (loadedFile);

        if (! isLoaded)
            pdfView.clear();

        host.setVisible (isLoaded);

        fileLabel.setText (loadedFile.getFileName(), juce::dontSendNotification);
        update();
        repaint();
    }

    void update()
    {
        if (node.getFile() != loadedFile)
        {
            loadFile();
            return;
        }

        pdfView.setTwoUp (node.isTwoUp());
        pdfView.showPage (node.getPage());
        twoUpButton.setToggleState (node.isTwoUp(), juce::dontSendNotification);

        const auto numPages = node.getNumPages();
        const auto first = node.getPage() + 1;
        const auto last = node.isTwoUp() ? juce::jmin (numPages, first + 1) : first;

        pageLabel.setText (numPages == 0 ? juce::String()
                                         : (last > first ? juce::String (first) + "-" + juce::String (last) : juce::String (first))
                                               + " / " + juce::String (numPages),
                           juce::dontSendNotification);

        previousButton.setEnabled (node.getPage() > 0);
        nextButton.setEnabled (node.getPage() + (node.isTwoUp() ? 2 : 1) < numPages);
    }

    void changeListenerCallback (juce::ChangeBroadcaster*) override  { update(); }

    // The toolbar hides while the windows are locked for the stage
    void timerCallback() override
    {
        if (const auto stage = isStageLayout(); stage != stageLayout)
        {
            stageLayout = stage;

            for (auto* c : std::initializer_list<juce::Component*> { &openButton, &fileLabel, &previousButton, &pageLabel, &nextButton, &twoUpButton })
                c->setVisible (! stage);

            resized();
            repaint();
        }
    }

    ScoreViewerNode& node;
    PdfView pdfView;
    juce::NSViewComponent host;
    juce::TextButton openButton, previousButton, nextButton, twoUpButton;
    juce::Label fileLabel, pageLabel;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::File loadedFile;
    bool stageLayout = false;
    bool isLoaded = false;
    void* keyMonitor = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScoreViewerEditor)
};

//==============================================================================
ScoreViewerNode::ScoreViewerNode()
    : AudioPluginInstance (BusesProperties())
{
    // Momentary: each time one is set on, it turns a page and goes back off
    auto next = InternalParameter::makeBool ("nextPage", "Next Page", false);
    nextParam = next.get();
    addHostedParameter (std::move (next));

    auto previous = InternalParameter::makeBool ("previousPage", "Previous Page", false);
    previousParam = previous.get();
    addHostedParameter (std::move (previous));

    nextParam->addListener (this);
    previousParam->addListener (this);
}

ScoreViewerNode::~ScoreViewerNode()
{
    cancelPendingUpdate();
    nextParam->removeListener (this);
    previousParam->removeListener (this);
}

void ScoreViewerNode::setFile (const juce::File& newFile)
{
    file = newFile;
    numPages = PdfView::countPages (file);
    page = 0;
    sendChangeMessage();
}

void ScoreViewerNode::turn (int direction)
{
    showPage (page + direction * (twoUp ? 2 : 1));
}

// PDFKit shows two pages side by side from an even page: 1-2, 3-4...
static int toShownPage (int index, int numPages, bool twoUp)
{
    const auto clamped = juce::jlimit (0, juce::jmax (0, numPages - 1), index);
    return twoUp ? (clamped & ~1) : clamped;
}

void ScoreViewerNode::showPage (int index)
{
    const auto clamped = toShownPage (index, numPages, twoUp);

    if (clamped != page)
    {
        page = clamped;
        sendChangeMessage();
    }
}

void ScoreViewerNode::setTwoUp (bool shouldShowTwo)
{
    if (twoUp != shouldShowTwo)
    {
        twoUp = shouldShowTwo;
        page = toShownPage (page, numPages, twoUp);
        sendChangeMessage();
    }
}

void ScoreViewerNode::parameterValueChanged (int index, float newValue)
{
    if (newValue < 0.5f)
        return;

    pendingTurns.fetch_add (index == nextParam->getParameterIndex() ? 1 : -1);
    triggerAsyncUpdate();
}

void ScoreViewerNode::handleAsyncUpdate()
{
    // Back off, without telling anyone, so the next press sets them on again
    nextParam->setValue (0.0f);
    previousParam->setValue (0.0f);

    for (auto turns = pendingTurns.exchange (0); turns != 0; turns += turns > 0 ? -1 : 1)
        turn (turns > 0 ? 1 : -1);
}

juce::AudioProcessorEditor* ScoreViewerNode::createEditor()
{
    return new ScoreViewerEditor (*this);
}

void ScoreViewerNode::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement xml ("SCOREVIEWER");
    xml.setAttribute ("file", file.getFullPathName());
    xml.setAttribute ("page", page);
    xml.setAttribute ("twoUp", twoUp);
    copyXmlToBinary (xml, destData);
}

void ScoreViewerNode::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        const auto path = xml->getStringAttribute ("file");
        const auto newFile = juce::File::isAbsolutePath (path) ? juce::File (path) : juce::File();

        if (newFile != file)
        {
            file = newFile;
            numPages = PdfView::countPages (file);
        }

        twoUp = xml->getBoolAttribute ("twoUp");
        page = toShownPage (xml->getIntAttribute ("page"), numPages, twoUp);
        sendChangeMessage();
    }
}

void ScoreViewerNode::fillInPluginDescription (juce::PluginDescription& description) const
{
    description.name = getName();
    description.descriptiveName = "Shows a score, chart or lyrics, with pages turned by MIDI or a page turner";
    description.pluginFormatName = "Internal";
    description.category = "Plugins";
    description.fileOrIdentifier = getName();
    description.uniqueId = (int) uniqueId;
    description.isInstrument = false;
    description.numInputChannels = 0;
    description.numOutputChannels = 0;
}
