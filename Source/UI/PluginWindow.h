/*
  ==============================================================================

   This file is part of the JUCE framework.
   Copyright (c) Raw Material Software Limited

   JUCE is an open source framework subject to commercial or open source
   licensing.

   By downloading, installing, or using the JUCE framework, or combining the
   JUCE framework with any other source code, object code, content or any other
   copyrightable work, you agree to the terms of the JUCE End User Licence
   Agreement, and all incorporated terms including the JUCE Privacy Policy and
   the JUCE Website Terms of Service, as applicable, which will bind you. If you
   do not agree to the terms of these agreements, we will not license the JUCE
   framework to you, and you must discontinue the installation or download
   process and cease use of the JUCE framework.

   JUCE End User Licence Agreement: https://juce.com/legal/juce-8-licence/
   JUCE Privacy Policy: https://juce.com/juce-privacy-policy
   JUCE Website Terms of Service: https://juce.com/juce-website-terms-of-service/

   Or:

   You may also use this code under the terms of the AGPLv3:
   https://www.gnu.org/licenses/agpl-3.0.en.html

   THE JUCE FRAMEWORK IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
   WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING WARRANTY OF
   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, ARE DISCLAIMED.

  ==============================================================================

    Curve
   Copyright (c) Thomas Derham

   The modifications for Curve are licensed to you solely under the terms of the 
   AGPLv3 license terms as described above.

   CURVE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
   WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING WARRANTY OF
   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, ARE DISCLAIMED.

  ==============================================================================
*/

#pragma once

#include "../Plugins/IOConfigurationWindow.h"
#include "../Plugins/ARAPlugin.h"
#include "WindowArrangement.h"

class PluginGraph;

ApplicationProperties& getAppProperties();

/**
    A window that shows a log of parameter change messages sent by the plugin.
*/
class PluginDebugWindow final : public AudioProcessorEditor,
                                public AudioProcessorParameter::Listener,
                                public ListBoxModel,
                                public AsyncUpdater
{
public:
    PluginDebugWindow (AudioProcessor& proc)
        : AudioProcessorEditor (proc), audioProc (proc)
    {
        setSize (500, 200);
        addAndMakeVisible (list);

        for (auto* p : audioProc.getParameters())
            p->addListener (this);

        log.add ("Parameter debug log started");
    }

    ~PluginDebugWindow() override
    {
        for (auto* p : audioProc.getParameters())
            p->removeListener (this);
    }

    void parameterValueChanged (int parameterIndex, float newValue) override
    {
        auto* param = audioProc.getParameters()[parameterIndex];
        auto value = param->getCurrentValueAsText().quoted() + " (" + String (newValue, 4) + ")";

        appendToLog ("parameter change", *param, value);
    }

    void parameterGestureChanged (int parameterIndex, bool gestureIsStarting) override
    {
        auto* param = audioProc.getParameters()[parameterIndex];
        appendToLog ("gesture", *param, gestureIsStarting ? "start" : "end");
    }

private:
    void appendToLog (StringRef action, AudioProcessorParameter& param, StringRef value)
    {
        String entry (action + " " + param.getName (30).quoted() + " [" + String (param.getParameterIndex()) + "]: " + value);

        {
            ScopedLock lock (pendingLogLock);
            pendingLogEntries.add (entry);
        }

        triggerAsyncUpdate();
    }

    void resized() override
    {
        list.setBounds (getLocalBounds());
    }

    int getNumRows() override
    {
        return log.size();
    }

    void paintListBoxItem (int rowNumber, Graphics& g, int width, int height, bool) override
    {
        g.setColour (getLookAndFeel().findColour (TextEditor::textColourId));

        if (isPositiveAndBelow (rowNumber, log.size()))
            g.drawText (log[rowNumber], Rectangle<int> { 0, 0, width, height }, Justification::left, true);
    }

    void handleAsyncUpdate() override
    {
        if (log.size() > logSizeTrimThreshold)
            log.removeRange (0, log.size() - maxLogSize);

        {
            ScopedLock lock (pendingLogLock);
            log.addArray (pendingLogEntries);
            pendingLogEntries.clear();
        }

        list.updateContent();
        list.scrollToEnsureRowIsOnscreen (log.size() - 1);
    }

    constexpr static const int maxLogSize = 300;
    constexpr static const int logSizeTrimThreshold = 400;

    ListBox list { "Log", this };

    StringArray log;
    StringArray pendingLogEntries;
    CriticalSection pendingLogLock;

    AudioProcessor& audioProc;
};

//==============================================================================
/**
    A desktop window containing a plugin's GUI.
*/
class PluginWindow final : public DocumentWindow
{
public:
    enum class Type
    {
        normal = 0,
        generic,
        programs,
        audioIO,
        debug,
        araHost,
        numTypes
    };

    PluginWindow (AudioProcessorGraph::Node* n, Type t, OwnedArray<PluginWindow>& windowList, KeyListener* keyListener)
        : DocumentWindow (n->getProcessor()->getName(),
                          LookAndFeel::getDefaultLookAndFeel().findColour (ResizableWindow::backgroundColourId),
                          DocumentWindow::minimiseButton | DocumentWindow::closeButton),
          activeWindowList (windowList),
          node (n), type (t)
    {
        // As Curve's other windows. A title bar JUCE draws, over the plug-in's own view, could be
        // left partly drawn when a plug-in sized itself as its window opened
        setUsingNativeTitleBar (true);

        setSize (400, 300);

        if (auto* ui = createProcessorEditor (*node->getProcessor(), type))
        {
            setContentOwned (ui, true);
            setResizable (ui->isResizable(), false);
        }

        setConstrainer (&constrainer);

        // A plug-in that can be resized reopens at the size it was
        if (getContentComponent() != nullptr && isResizable())
        {
            const auto savedWidth  = (int) node->properties.getWithDefault (getLastWidthProp (type), 0);
            const auto savedHeight = (int) node->properties.getWithDefault (getLastHeightProp (type), 0);

            if (savedWidth > 0 && savedHeight > 0)
                setBoundsConstrained (getBounds().withSize (savedWidth, savedHeight));
        }

        placeAsSaved();

        node->properties.set (getOpenProp (type), true);

        setVisible (true);

        if (isShownOnAllSpaces())
            WindowArrangement::setOnAllSpaces (*this, true);

        if (arePositionsLocked())
            WindowArrangement::setMovable (*this, false);

        isPlacing = false;

        // Where it opened, if nothing was saved, or where a preset saved before displays were
        // remembered has it. A displaced one keeps the display it was saved on
        if (! isDisplaced)
            rememberBounds();

        addKeyListener (keyListener);
    }

    ~PluginWindow() override
    {
        if (node != nullptr)
            node->properties.set (getOpenProp (type), false);
        clearContentComponent();
    }

    void moved() override
    {
        if (node == nullptr || isPlacing)
            return;

        // A move without a mouse button down isn't a drag. If the window's display has gone, it's
        // macOS moving it onto another, which mustn't replace the display it was saved on
        if (! WindowArrangement::isMouseButtonDown())
        {
            if (isDisplaced)
                return;

            const auto savedDisplay = node->properties[getDisplayProp (type)].toString().toStdString();

            if (! savedDisplay.empty()
                && Curve::WindowPlacement::findDisplay (WindowArrangement::getDisplays(), savedDisplay) == nullptr)
            {
                isDisplaced = true;
                return;
            }
        }

        isDisplaced = false;
        rememberBounds();
    }

    // JUCE's windows take the keys as they're shown (TopLevelWindow::visibilityChanged()), which
    // closed the menu bar panel when a window opened from it, and took them from the graph editor
    // as a preset's windows opened. A plug-in window comes to the front, and takes the keys only
    // when none of Curve's other windows has them; one opened from its node is given them anyway
    void visibilityChanged() override
    {
        if (isShowing())
            WindowArrangement::bringToFront ({ this });
    }

    void resized() override
    {
        DocumentWindow::resized();

        if (node != nullptr && ! isPlacing && isResizable())
        {
            node->properties.set (getLastWidthProp (type), getWidth());
            node->properties.set (getLastHeightProp (type), getHeight());
        }
    }

    // Clicked, or brought to the front otherwise: now in front of the others
    void broughtToFront() override
    {
        if (node != nullptr && ! isPlacing)
            node->properties.set (getStackProp (type), nextStackValue());
    }

    /** Moves a window that was put on the main display because its own was away back to its own,
        if that's connected again. */
    void returnToSavedDisplay()
    {
        if (! isDisplaced || node == nullptr)
            return;

        const auto result = Curve::WindowPlacement::place (getSaved(), getWidth(), getHeight(), WindowArrangement::getDisplays());

        if (result.outcome == Curve::WindowPlacement::Outcome::onSavedDisplay)
        {
            const ScopedValueSetter<bool> placingNow (isPlacing, true);
            setTopLeftPosition (result.x, result.y);
            isDisplaced = false;
        }
    }

    /** How far forward the window was last brought, to order a preset's windows as they were. */
    int getStackPosition() const
    {
        return node != nullptr ? (int) node->properties[getStackProp (type)] : 0;
    }

    void closeButtonPressed() override
    {
        // Before this is deleted with the window
        const auto notify = onClosed;
        activeWindowList.removeObject (this);

        if (notify != nullptr)
            notify();
    }

    /** Called after the window's own close button closes it, e.g. for the menu bar panel to follow */
    std::function<void()> onClosed;

    /** Whether plug-in windows show on every Space, rather than only the one each is on. Off by
        default: windows stay on the Space they're put on (see PluginGraph::openPresetPluginWindows()). */
    static bool isShownOnAllSpaces()
    {
        if (auto* settings = getAppProperties().getUserSettings())
            return settings->getBoolValue (onAllSpacesSetting, false);

        return false;
    }

    static void setShownOnAllSpaces (bool shouldBeOnAll)
    {
        if (auto* settings = getAppProperties().getUserSettings())
        {
            settings->setValue (onAllSpacesSetting, shouldBeOnAll);
            settings->saveIfNeeded();
        }
    }

    static constexpr const char* onAllSpacesSetting = "pluginWindowsOnAllSpaces";

    /** Whether plug-in windows can't be dragged, so a layout can't be knocked out of place on stage */
    static bool arePositionsLocked()
    {
        if (auto* settings = getAppProperties().getUserSettings())
            return settings->getBoolValue (positionsLockedSetting, false);

        return false;
    }

    static void setPositionsLocked (bool shouldLock)
    {
        if (auto* settings = getAppProperties().getUserSettings())
        {
            settings->setValue (positionsLockedSetting, shouldLock);
            settings->saveIfNeeded();
        }
    }

    static constexpr const char* positionsLockedSetting = "pluginWindowPositionsLocked";

    /** The node properties, saved in presets as node attributes, that say how its window of this
        type was arranged. Each window type's own, so a node's generic editor has its own place. */
    static String getOpenProp        (Type type)    { return "uiopen_"     + getTypeName (type); }
    static String getLastXProp       (Type type)    { return "uiLastX_"    + getTypeName (type); }
    static String getLastYProp       (Type type)    { return "uiLastY_"    + getTypeName (type); }
    static String getLastWidthProp   (Type type)    { return "uiLastW_"    + getTypeName (type); }
    static String getLastHeightProp  (Type type)    { return "uiLastH_"    + getTypeName (type); }
    static String getDisplayProp     (Type type)    { return "uiDisplay_"  + getTypeName (type); }
    static String getDisplayXProp    (Type type)    { return "uiDisplayX_" + getTypeName (type); }
    static String getDisplayYProp    (Type type)    { return "uiDisplayY_" + getTypeName (type); }
    static String getStackProp       (Type type)    { return "uiStack_"    + getTypeName (type); }
    static String getOnTopProp       (Type type)    { return "uiOnTop_"    + getTypeName (type); }

    /** Kept above other apps' windows, e.g. a score or a meter on a stage */
    bool isKeptOnTop() const        { return node != nullptr && (bool) node->properties[getOnTopProp (type)]; }

    static StringArray getArrangementProps (Type type)
    {
        return { getOpenProp (type), getLastXProp (type), getLastYProp (type),
                 getLastWidthProp (type), getLastHeightProp (type),
                 getDisplayProp (type), getDisplayXProp (type), getDisplayYProp (type),
                 getStackProp (type), getOnTopProp (type) };
    }

    /** Sets a node's window properties from its saved attributes, removing any it doesn't have
        if removeMissing, so a scene's windows are as that scene has them. */
    static void restoreArrangement (const XmlElement& xml, NamedValueSet& properties, Type type, bool removeMissing)
    {
        for (const auto& prop : getArrangementProps (type))
        {
            const Identifier id (prop);

            if (! xml.hasAttribute (prop))
            {
                if (removeMissing)
                    properties.remove (id);

                continue;
            }

            if (prop == getDisplayProp (type))
            {
                properties.set (id, xml.getStringAttribute (prop));
            }
            else
            {
                const auto value = xml.getIntAttribute (prop);
                properties.set (id, value);

                // Windows brought forward from now on go in front of those of the loaded preset
                if (prop == getStackProp (type))
                    stackCounter = jmax (stackCounter, value);
            }
        }
    }

    static void saveArrangement (const NamedValueSet& properties, XmlElement& xml, Type type)
    {
        for (const auto& prop : getArrangementProps (type))
            if (properties.contains (prop))
                xml.setAttribute (prop, properties[prop].toString());
    }

    OwnedArray<PluginWindow>& activeWindowList;
    const AudioProcessorGraph::Node::Ptr node;
    const Type type;

    BorderSize<int> getBorderThickness() const override
    {
        return DocumentWindow::getBorderThickness();
    }

    // A resize the plug-in's editor makes itself is taken as it is (see DecoratorConstrainer).
    // From JUCE's AudioPluginHost, upstream commit 5922150778
    void childBoundsChanged (Component* child) override
    {
        std::optional<ScopedValueSetter<Rectangle<int>>> scope;

        if (child == getContentComponent())
            scope.emplace (constrainer.pendingBoundsFromChild, child->getLocalBounds());

        DocumentWindow::childBoundsChanged (child);
    }

private:
    class DecoratorConstrainer final : public BorderedComponentBoundsConstrainer
    {
    public:
        explicit DecoratorConstrainer (DocumentWindow& windowIn)
            : window (windowIn) {}

        ComponentBoundsConstrainer* getWrappedConstrainer() const override
        {
            auto* editor = dynamic_cast<AudioProcessorEditor*> (window.getContentComponent());
            return editor != nullptr ? editor->getConstrainer() : nullptr;
        }

        void checkBounds (Rectangle<int>& bounds,
                          const Rectangle<int>& previousBounds,
                          const Rectangle<int>& limits,
                          bool isStretchingTop,
                          bool isStretchingLeft,
                          bool isStretchingBottom,
                          bool isStretchingRight) override
        {
            // Responding to the editor resizing itself, the only valid size is the editor's: its
            // constrainer could otherwise hold the window at the old size, e.g. a fixed-size one
            if (! pendingBoundsFromChild.isEmpty())
            {
                bounds = getAdditionalBorder().addedTo (pendingBoundsFromChild).withPosition (previousBounds.getPosition());
                return;
            }

            BorderedComponentBoundsConstrainer::checkBounds (bounds, previousBounds, limits,
                                                             isStretchingTop, isStretchingLeft,
                                                             isStretchingBottom, isStretchingRight);
        }

        BorderSize<int> getAdditionalBorder() const override
        {
            const auto nativeFrame = [&]() -> BorderSize<int>
            {
                if (auto* peer = window.getPeer())
                    if (const auto frameSize = peer->getFrameSizeIfPresent())
                        return *frameSize;

                return {};
            }();

            return nativeFrame.addedTo (window.getContentComponentBorder());
        }

        Rectangle<int> pendingBoundsFromChild;

    private:
        DocumentWindow& window;
    };

    DecoratorConstrainer constrainer { *this };

    static Rectangle<int> getOpeningDisplayArea()
    {
        const auto& displays = Desktop::getInstance().getDisplays();
        const auto* display = displays.getDisplayForPoint (Desktop::getInstance().getMainMouseSource().getScreenPosition());

        if (display == nullptr)
            display = displays.getPrimaryDisplay();

        return display != nullptr ? display->userBounds.toNearestInt() : Rectangle<int> { 0, 0, 800, 600 };
    }

    // Opening, and its own placing, isn't a move or resize to remember, nor is opening bringing
    // it to the front: PluginGraph::bringPluginWindowsToFront() orders a preset's windows together
    bool isPlacing = true;
    bool isDisplaced = false; // On the main display while the one it was saved on is away

    static inline int stackCounter = 0;
    static int nextStackValue()    { return ++stackCounter; }

    Curve::WindowPlacement::Saved getSaved() const
    {
        Curve::WindowPlacement::Saved saved;
        const auto& props = node->properties;

        saved.displayId = props[getDisplayProp (type)].toString().toStdString();
        saved.offsetX = (int) props[getDisplayXProp (type)];
        saved.offsetY = (int) props[getDisplayYProp (type)];
        saved.hasAbsolute = props.contains (getLastXProp (type)) && props.contains (getLastYProp (type));
        saved.x = (int) props[getLastXProp (type)];
        saved.y = (int) props[getLastYProp (type)];
        return saved;
    }

    // Where the window was saved: on the same display if it's connected, wherever that now is.
    // Positions are of the window's content, with the native title bar above
    void placeAsSaved()
    {
        using namespace Curve::WindowPlacement;

        const auto saved = getSaved();
        const auto result = place (saved, getWidth(), getHeight(), WindowArrangement::getDisplays());

        isDisplaced = result.outcome == Outcome::displaced;

        if (result.outcome != Outcome::unsaved)
        {
            setTopLeftPosition (result.x, result.y);
            return;
        }

        // New windows open on the display the mouse is on, which is normally where the graph
        // editor is: centred if an older preset's position is on no display now, else near the top left
        const auto openingArea = getOpeningDisplayArea();

        if (saved.hasAbsolute)
        {
            setTopLeftPosition (openingArea.getX() + jmax (0, (openingArea.getWidth() - getWidth()) / 2),
                                openingArea.getY() + jmax (titleBarHeight, (openingArea.getHeight() - getHeight()) / 2));
            return;
        }

        auto& random = Random::getSystemRandom();
        setTopLeftPosition (openingArea.getX() + random.nextInt (jmax (1, jmin (500, openingArea.getWidth() - getWidth()))),
                            openingArea.getY() + titleBarHeight
                                + random.nextInt (jmax (1, jmin (500, openingArea.getHeight() - getHeight() - titleBarHeight))));
    }

    // Both where it is on its display, to reopen there, and on the desktop, for older versions
    void rememberBounds()
    {
        auto& props = node->properties;
        props.set (getLastXProp (type), getX());
        props.set (getLastYProp (type), getY());

        const auto displays = WindowArrangement::getDisplays();

        if (const auto* display = Curve::WindowPlacement::findDisplayFor (displays, { getX(), getY(), getWidth(), getHeight() }))
        {
            props.set (getDisplayProp (type), String (display->id));
            props.set (getDisplayXProp (type), getX() - display->frame.x);
            props.set (getDisplayYProp (type), getY() - display->frame.y);
        }
    }

    static constexpr int titleBarHeight = Curve::WindowPlacement::titleBarHeight;

    float getDesktopScaleFactor() const override     { return 1.0f; }

    static AudioProcessorEditor* createProcessorEditor (AudioProcessor& processor,
                                                        PluginWindow::Type type)
    {
        if (type == PluginWindow::Type::normal)
        {
            if (processor.hasEditor())
                if (auto* ui = processor.createEditorAndMakeActive())
                    return ui;

            type = PluginWindow::Type::generic;
        }

        if (type == PluginWindow::Type::araHost)
        {
           #if JUCE_PLUGINHOST_ARA
            if (auto* araPluginInstanceWrapper = dynamic_cast<ARAPluginInstanceWrapper*> (&processor))
                if (auto* ui = araPluginInstanceWrapper->createARAHostEditor())
                    return ui;
           #endif
            return {};
        }

        if (type == PluginWindow::Type::generic)
        {
            auto* result = new GenericAudioProcessorEditor (processor);
            result->setResizeLimits (200, 300, 1'000, 10'000);
            return result;
        }

        if (type == PluginWindow::Type::programs)
            return new ProgramAudioProcessorEditor (processor);

        if (type == PluginWindow::Type::audioIO)
            return new IOConfigurationWindow (processor);

        if (type == PluginWindow::Type::debug)
            return new PluginDebugWindow (processor);

        jassertfalse;
        return {};
    }

    static String getTypeName (Type type)
    {
        switch (type)
        {
            case Type::normal:     return "Normal";
            case Type::generic:    return "Generic";
            case Type::programs:   return "Programs";
            case Type::audioIO:    return "IO";
            case Type::debug:      return "Debug";
            case Type::araHost:    return "ARAHost";
            case Type::numTypes:
            default:               return {};
        }
    }

    //==============================================================================
    struct ProgramAudioProcessorEditor final : public AudioProcessorEditor
    {
        explicit ProgramAudioProcessorEditor (AudioProcessor& p)
            : AudioProcessorEditor (p)
        {
            setOpaque (true);

            addAndMakeVisible (listBox);
            listBox.updateContent();

            const auto rowHeight = listBox.getRowHeight();

            setSize (400, jlimit (rowHeight, 400, p.getNumPrograms() * rowHeight));
        }

        void paint (Graphics& g) override
        {
            g.fillAll (Colours::grey);
        }

        void resized() override
        {
            listBox.setBounds (getLocalBounds());
        }

    private:
        class Model : public ListBoxModel
        {
        public:
            Model (Component& o, AudioProcessor& p)
                : owner (o), proc (p) {}

            int getNumRows() override
            {
                return proc.getNumPrograms();
            }

            void paintListBoxItem (int rowNumber,
                                   Graphics& g,
                                   int width,
                                   int height,
                                   bool rowIsSelected) override
            {
                const auto textColour = owner.findColour (ListBox::textColourId);

                if (rowIsSelected)
                {
                    const auto defaultColour = owner.findColour (ListBox::backgroundColourId);
                    const auto c = rowIsSelected ? defaultColour.interpolatedWith (textColour, 0.5f)
                                                 : defaultColour;

                    g.fillAll (c);
                }

                g.setColour (textColour);
                g.drawText (proc.getProgramName (rowNumber),
                            Rectangle<int> { width, height }.reduced (2),
                            Justification::left,
                            true);
            }

            void selectedRowsChanged (int row) override
            {
                if (0 <= row)
                    proc.setCurrentProgram (row);
            }

        private:
            Component& owner;
            AudioProcessor& proc;
        };

        Model model { *this, *getAudioProcessor() };
        ListBox listBox { "Programs", &model };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProgramAudioProcessorEditor)
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginWindow)
};
