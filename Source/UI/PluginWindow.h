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

inline String getFormatSuffix (const AudioProcessor* plugin)
{
    const auto format = [plugin]()
    {
        if (auto* instance = dynamic_cast<const AudioPluginInstance*> (plugin))
            return instance->getPluginDescription().pluginFormatName;

        return String();
    }();

    return format.isNotEmpty() ? (" (" + format + ")") : format;
}

class PluginGraph;

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
        : DocumentWindow (n->getProcessor()->getName() + getFormatSuffix (n->getProcessor()),
                          LookAndFeel::getDefaultLookAndFeel().findColour (ResizableWindow::backgroundColourId),
                          DocumentWindow::minimiseButton | DocumentWindow::closeButton),
          activeWindowList (windowList),
          node (n), type (t)
    {
        setSize (400, 300);

        if (auto* ui = createProcessorEditor (*node->getProcessor(), type))
        {
            setContentOwned (ui, true);
            setResizable (ui->isResizable(), false);
        }

        setConstrainer (&constrainer);

        // New windows open on the display the mouse is on, which is normally where
        // the graph editor is.
        const auto openingArea = getOpeningDisplayArea();
        auto& random = Random::getSystemRandom();
        const auto defaultX = openingArea.getX() + random.nextInt (jmax (1, jmin (500, openingArea.getWidth() - getWidth())));
        const auto defaultY = openingArea.getY() + random.nextInt (jmax (1, jmin (500, openingArea.getHeight() - getHeight())));

        auto savedX = (int) node->properties.getWithDefault (getLastXProp (type), defaultX);
        auto savedY = (int) node->properties.getWithDefault (getLastYProp (type), defaultY);

        // The saved position may be on a display that's gone, or in a gap between
        // displays of different sizes. If the title bar can't be reached, reopen
        // centred on the display the mouse is on instead.
        if (! isTitleBarReachable ({ savedX, savedY, getWidth(), getTitleBarHeight() }))
        {
            savedX = openingArea.getX() + jmax (0, (openingArea.getWidth() - getWidth()) / 2);
            savedY = openingArea.getY() + jmax (0, (openingArea.getHeight() - getHeight()) / 2);
        }

        setTopLeftPosition (savedX, savedY);

        node->properties.set (getOpenProp (type), true);

        setVisible (true);

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
        if (node != nullptr)
        {
            node->properties.set (getLastXProp (type), getX());
            node->properties.set (getLastYProp (type), getY());
        }
    }

    void closeButtonPressed() override
    {
        activeWindowList.removeObject (this);
    }

    static String getLastXProp (Type type)    { return "uiLastX_" + getTypeName (type); }
    static String getLastYProp (Type type)    { return "uiLastY_" + getTypeName (type); }
    static String getOpenProp  (Type type)    { return "uiopen_"  + getTypeName (type); }

    OwnedArray<PluginWindow>& activeWindowList;
    const AudioProcessorGraph::Node::Ptr node;
    const Type type;

    BorderSize<int> getBorderThickness() const override
    {
        return DocumentWindow::getBorderThickness();
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

        // Plug-in windows use a JUCE-drawn title bar, so unlike a native one, macOS
        // doesn't stop it going under the menu bar or Dock, where it can't be
        // grabbed again. While the user drags, keep it inside the usable area of
        // the display the mouse is on, which still lets windows move between
        // displays, including ones arranged above or below. AppKit also calls this
        // at other times, such as when a window is first shown or changes level,
        // while the mouse may be down on another display, so otherwise use the
        // display the window itself is on.
        void checkBounds (Rectangle<int>& bounds,
                          const Rectangle<int>& previousBounds,
                          const Rectangle<int>& limits,
                          bool isStretchingTop,
                          bool isStretchingLeft,
                          bool isStretchingBottom,
                          bool isStretchingRight) override
        {
            BorderedComponentBoundsConstrainer::checkBounds (bounds, previousBounds, limits,
                                                             isStretchingTop, isStretchingLeft,
                                                             isStretchingBottom, isStretchingRight);

            const bool isMoving = ! (isStretchingTop || isStretchingLeft || isStretchingBottom || isStretchingRight);

            if (! (isMoving || isStretchingTop))
                return;

            const auto& displays = Desktop::getInstance().getDisplays();
            const auto* display = window.isMouseButtonDown (true)
                                      ? displays.getDisplayForPoint (Desktop::getInstance().getMainMouseSource().getScreenPosition())
                                      : displays.getDisplayForRect (bounds);

            if (display == nullptr)
                return;

            const auto area = display->userBounds.toNearestInt();

            if (isStretchingTop)
            {
                if (bounds.getY() < area.getY())
                    bounds.setTop (area.getY());

                return;
            }

            const auto maxY = jmax (area.getY(), area.getBottom() - window.getTitleBarHeight());
            bounds.setY (jlimit (area.getY(), maxY, bounds.getY()));
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

    static bool isTitleBarReachable (Rectangle<int> titleBar)
    {
        const auto minVisibleWidth = jmin (100, titleBar.getWidth());

        for (const auto& display : Desktop::getInstance().getDisplays().displays)
        {
            const auto overlap = display.userBounds.toNearestInt().getIntersection (titleBar);

            if (overlap.getHeight() == titleBar.getHeight() && overlap.getWidth() >= minVisibleWidth)
                return true;
        }

        return false;
    }

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
