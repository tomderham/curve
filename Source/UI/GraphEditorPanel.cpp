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

#include <JuceHeader.h>
#include "GraphEditorPanel.h"
#include "IdleReleaseController.h"
#include "CurveColours.h"
#include "FilteredAudioDeviceSelectorComponent.h"
#include "GitHubUpdater.h"
#include "GraphLayout.h"
#include "../Plugins/InternalPlugins.h"
#include "../Plugins/MeterNode.h"
#include "MainHostWindow.h"
#include "../Plugins/OutputInterfaceLoopbackNode.h"
#include "../Calibration/AUNBandEQConverter.h"
#include "../Calibration/AutoEQDataManager.h"


//==============================================================================
// Nodes and pins all sit on this grid, so that aligned nodes get straight wires
static constexpr int graphGridSize = 20;

// Pins start this far from a node's left edge, so a straight wire means aligned
// nodes. The minimum width centres a stereo pair of pins.
static constexpr int graphPinInset = 60;
static constexpr int graphMinNodeWidth = graphPinInset * 2 + graphGridSize;

static int snapToGraphGrid (double value)
{
    return roundToInt (value / graphGridSize) * graphGridSize;
}

//==============================================================================
struct GraphEditorPanel::PinComponent final : public Component,
                                              public SettableTooltipClient
{
    PinComponent (GraphEditorPanel& p, AudioProcessorGraph::NodeAndChannel pinToUse, bool isIn)
        : panel (p), graph (p.graph), pin (pinToUse), isInput (isIn)
    {
        if (auto node = graph.graph.getNodeForId (pin.nodeID))
        {
            String tip;

            if (pin.isMIDI())
            {
                tip = isInput ? "MIDI Input"
                              : "MIDI Output";
            }
            else
            {
                auto& processor = *node->getProcessor();
                auto channel = processor.getOffsetInBusBufferForAbsoluteChannelIndex (isInput, pin.channelIndex, busIdx);

                if (auto* bus = processor.getBus (isInput, busIdx))
                    tip = bus->getName() + ": " + AudioChannelSet::getAbbreviatedChannelTypeName (bus->getCurrentLayout().getTypeOfChannel (channel));
                else
                    tip = (isInput ? "Main Input: "
                                   : "Main Output: ") + String (pin.channelIndex + 1);

            }

            setTooltip (tip);
        }

        setSize (16, 16);
    }

    void paint (Graphics& g) override
    {
        const auto dot = getLocalBounds().toFloat().withSizeKeepingCentre (9.0f, 9.0f);
        const auto colour = pin.isMIDI() ? Curve::Colours::midiWire : Curve::Colours::audioWire;

        g.setColour (findColour (TextEditor::backgroundColourId));
        g.fillEllipse (dot.expanded (1.5f));

        g.setColour (colour.withRotatedHue ((float) busIdx / 5.0f));
        g.fillEllipse (dot);
    }

    void mouseDown (const MouseEvent& e) override
    {
        AudioProcessorGraph::NodeAndChannel dummy { {}, 0 };

        panel.beginConnectorDrag (isInput ? dummy : pin,
                                  isInput ? pin : dummy,
                                  e);
    }

    void mouseDrag (const MouseEvent& e) override
    {
        panel.dragConnector (e);
    }

    void mouseUp (const MouseEvent& e) override
    {
        panel.endDraggingConnector (e);
    }

    GraphEditorPanel& panel;
    PluginGraph& graph;
    AudioProcessorGraph::NodeAndChannel pin;
    const bool isInput;
    int busIdx = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PinComponent)
};

//==============================================================================
static String getTooltipForProcessor (const AudioProcessor& processor)
{
    // For hosted 3rd-party plugins (AU, VST3, ARA)
    if (auto* instance = dynamic_cast<const AudioPluginInstance*> (&processor))
    {
        auto desc = instance->getPluginDescription();

        if (desc.pluginFormatName != "Internal")
        {
            String tip = desc.name;
            if (desc.descriptiveName.isNotEmpty() && desc.descriptiveName != desc.name)
                tip << " (" << desc.descriptiveName << ")";
            if (desc.manufacturerName.isNotEmpty())
                tip << " - " << desc.manufacturerName;
            if (desc.pluginFormatName.isNotEmpty())
                tip << " [" << desc.pluginFormatName << "]";
            return tip;
        }
    }

    // Curve Internal Nodes & Audio/MIDI I/O
    const auto name = processor.getName();

    if (name == "Audio Output")
        return "Audio Output: The output interface selected in Curve's Audio Settings.";

    if (name == "Audio Input")
        return "Audio Input: The input interface selected in Curve's Audio Settings.";

    if (name == "MIDI Output")
        return "MIDI Output: The MIDI Output interface selected in Curve's Audio Settings.";

    if (name == "MIDI Input")
        return "MIDI Input: The Active MIDI inputs selected in Curve's Audio Settings.";

    if (name == "Interface Loopback (In)")
        return "Interface Loopback (In): Loopback virtual input interface; audio that would have been sent to the selected output interface.";

    if (name == "Headphone Speaker Emulation")
        return "Headphone Speaker Emulation: Mid-field studio speaker emulation processor (including crossfeed) for monitoring with headphones.";

    if (name == "Headphone Crossfeed")
        return "Headphone Crossfeed: Crossfeed processor for natural soundstage imaging when monitoring with headphones, with adjustable amount.";

    if (name == "Gain")
        return "Gain: Volume trim (-60 dB to +60 dB) and mute toggle.";

    if (name == "Invert Phase")
        return "Invert Phase: Negates polarity of audio signal on each channel.";

    if (name == "Clipper")
        return "Clipper: Hard clips each channel at a ceiling (-50 to 0 dBFS) as a safety guard; 0 latency.";

    if (name == "Meter")
        return "Meter: Shows levels, VU, loudness or a spectrum of its input. Connect it from any pin; it measures only while its window is open.";

    if (name == "Pan")
        return "Pan: Places a mono input in the stereo output with a selectable pan law; balance control for stereo input.";

    if (name == "Stereo Utility")
        return "Stereo Utility: Swap L/R, mono sum, left or right only, and stereo width (0-200%).";

    if (name == "Mid/Side Encoder")
        return "Mid/Side Encoder: Converts L/R to Mid (pin 1) and Side (pin 2). Pair with a decoder to process mid and side separately.";

    if (name == "Mid/Side Decoder")
        return "Mid/Side Decoder: Converts Mid (pin 1) and Side (pin 2) back to L/R.";

    if (name == "Delay")
        return "Delay: Whole-sample delay on each channel (up to 2 s), for time alignment.";

    if (name == "Audio Recorder")
        return "WAV Audio Capture: Multi-channel audio recorder utility, saves WAV files to Desktop.";

    return name;
}

//==============================================================================
// Power glyph that toggles a node's bypass. Stateless: it queries the node so it
// always reflects bypass changes made elsewhere.
class NodeBypassButton final : public Button
{
public:
    NodeBypassButton (std::function<bool()> isBypassedFn, std::function<bool()> isMidiLearningFn)
        : Button ("Bypass"), isBypassed (std::move (isBypassedFn)), isMidiLearning (std::move (isMidiLearningFn))
    {
        setClickingTogglesState (false);
        setWantsKeyboardFocus (false);
    }

    void resized() override
    {
        const auto glyph = getLocalBounds().toFloat().withSizeKeepingCentre (glyphSize, glyphSize).reduced (1.5f);
        const auto r = glyph.getWidth() * 0.5f;
        const auto c = glyph.getCentre();

        glyphPath.clear();
        glyphPath.addCentredArc (c.x, c.y, r, r, 0.0f,
                                 MathConstants<float>::pi * 0.2f,
                                 MathConstants<float>::twoPi - MathConstants<float>::pi * 0.2f, true);
        glyphPath.startNewSubPath (c.x, glyph.getY() - 0.5f);
        glyphPath.lineTo (c.x, c.y);
    }

    void paintButton (Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        const auto textColour = findColour (TextEditor::textColourId);
        const bool bypassed = isBypassed != nullptr && isBypassed();

        if (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown)
        {
            g.setColour (textColour.withAlpha (shouldDrawButtonAsDown ? 0.22f : 0.12f));
            g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 4.0f);
        }

        g.setColour (bypassed ? textColour.withAlpha (0.4f) : Curve::Colours::accent);
        g.strokePath (glyphPath, PathStrokeType (1.5f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    String getTooltip() override
    {
        if (isMidiLearning != nullptr && isMidiLearning())
            return "Click to MIDI learn this node's bypass";

        return (isBypassed != nullptr && isBypassed()) ? "Bypassed - click to enable"
                                                       : "Bypass (click to toggle)";
    }

private:
    static constexpr float glyphSize = 14.0f;
    std::function<bool()> isBypassed;
    std::function<bool()> isMidiLearning;
    Path glyphPath;
};

//==============================================================================
struct GraphEditorPanel::PluginComponent final : public Component,
                                                 public SettableTooltipClient,
                                                 public Timer,
                                                 private AudioProcessorParameter::Listener,
                                                 private AsyncUpdater
{
    PluginComponent (GraphEditorPanel& p, AudioProcessorGraph::NodeID id)  : panel (p), graph (p.graph), pluginID (id)
    {
        if (auto f = graph.graph.getNodeForId (pluginID))
        {
            if (auto* processor = f->getProcessor())
            {
                if (auto* bypassParam = processor->getBypassParameter())
                {
                    observedBypassParam = bypassParam;
                    observedBypassParam->addListener (this);
                }

                setTooltip (getTooltipForProcessor (*processor));

                kindColour = getKindColour (*processor);

                // No bypass for the graph's I/O nodes, or for meters, which don't change the audio
                if (dynamic_cast<AudioProcessorGraph::AudioGraphIOProcessor*> (processor) == nullptr
                    && dynamic_cast<MeterNode*> (processor) == nullptr)
                {
                    bypassButton = std::make_unique<NodeBypassButton> ([this] { return isNodeBypassed(); },
                                                                       [this] { return isMidiLearning(); });
                    bypassButton->onClick = [this] { bypassButtonClicked(); };
                    addAndMakeVisible (bypassButton.get());
                }
            }
        }

        setSize (150, 60);
    }

    PluginComponent (const PluginComponent&) = delete;
    PluginComponent& operator= (const PluginComponent&) = delete;

    ~PluginComponent() override
    {
        // Don't dereference observedBypassParam blindly: the node is removed from
        // the graph before we're notified (async), so it may already be deleted.
        if (observedBypassParam != nullptr)
            if (auto f = graph.graph.getNodeForId (pluginID))
                if (auto* processor = f->getProcessor())
                    if (processor->getBypassParameter() == observedBypassParam)
                        observedBypassParam->removeListener (this);
    }

    void mouseDown (const MouseEvent& e) override
    {
        originalPos = localPointToGlobal (Point<int>());

        toFront (true);

        if (e.source.isTouch())
        {
            originalTouchPos = e.position.toInt();
            startTimer (750);
        }
        else if (e.mods.isPopupMenu())
        {
            showPopupMenu (e.position.toInt());
        }
    }

    void mouseDrag (const MouseEvent& e) override
    {
        if (e.source.isTouch() && e.getDistanceFromDragStart() > 5)
            stopTimer();

        if (! e.mods.isPopupMenu())
        {
            panel.setDraggingNode (true);

            auto pos = originalPos + e.getOffsetFromDragStart();

            if (getParentComponent() != nullptr)
                pos = getParentComponent()->getLocalPoint (nullptr, pos);

            pos += getLocalBounds().getCentre();

            graph.setNodePosition (pluginID,
                                   { pos.x / (double) getParentWidth(),
                                     pos.y / (double) getParentHeight() });

            // Option places the node freely, off the grid
            graph.setNodePositionFree (pluginID, e.mods.isAltDown());

            // Nothing else changes until the node reaches another grid cell
            if (const auto bounds = getBoundsForPosition (getWidth(), getHeight()); bounds != getBounds())
            {
                setBounds (bounds);
                panel.updateConnectorsForNode (pluginID);
            }
        }
    }

    void mouseUp (const MouseEvent& e) override
    {
        if (e.source.isTouch())
        {
            stopTimer();
            callAfterDelay (250, []() { PopupMenu::dismissAllActiveMenus(); });
        }

        panel.setDraggingNode (false);

        if (e.mouseWasDraggedSinceMouseDown())
        {
            graph.setChangedFlag (true);
        }
        else if (e.getNumberOfClicks() == 2)
        {
            if (auto f = graph.graph.getNodeForId (pluginID))
            {
                // Other nodes without an editor have nothing to show; their I/O
                // configuration stays available from the context menu.
                if (auto* proc = f->getProcessor(); proc != nullptr
                      && (proc->hasEditor() || PluginGraph::isConfiguredByAudioSettings (*proc)))
                    if (auto* w = graph.getOrCreateWindowFor (f, PluginWindow::Type::normal))
                        w->toFront (true);
            }
        }
    }

    bool hitTest (int x, int y) override
    {
        for (auto* child : getChildren())
            if (child->getBounds().contains (x, y))
                return true;

        return x >= 3 && x < getWidth() - 6 && y >= pinSize && y < getHeight() - pinSize;
    }

    void paint (Graphics& g) override
    {
        auto boxArea = getLocalBounds().reduced (4, pinSize);
        const bool isBypassed = isNodeBypassed();

        // Stays opaque when bypassed, since wires pass behind the node
        auto boxColour = findColour (TextEditor::backgroundColourId);

        if (isBypassed)
            boxColour = boxColour.interpolatedWith (findColour (ResizableWindow::backgroundColourId), 0.6f);

        const auto box = boxArea.toFloat();

        g.setColour (boxColour);
        g.fillRoundedRectangle (box, cornerSize);

        {
            Path outline;
            outline.addRoundedRectangle (box, cornerSize);

            Graphics::ScopedSaveState clip (g);
            g.reduceClipRegion (outline);
            g.setColour (kindColour.withMultipliedAlpha (isBypassed ? 0.4f : 1.0f));
            g.fillRect (box.withWidth (3.0f));
        }

        if (isMidiLearnTarget())
        {
            g.setColour (Curve::Colours::accent);
            g.drawRoundedRectangle (box, cornerSize, 2.0f);
        }
        else
        {
            g.setColour (findColour (TextEditor::outlineColourId).withAlpha (isBypassed ? 0.3f : 0.6f));
            g.drawRoundedRectangle (box, cornerSize, 1.0f);
        }

        // Reserve the bypass slot on both sides to keep the name centred
        auto textArea = bypassButton != nullptr ? boxArea.reduced (bypassSlot + bypassMargin, 0) : boxArea;

        // The slot opposite the bypass button shows whether the node has MIDI mappings
        if (bypassButton != nullptr && graph.getMidiMappings().hasMappingsForNode (pluginID))
            drawMidiBadge (g, boxArea.removeFromRight (bypassSlot + bypassMargin)
                                     .withSizeKeepingCentre (bypassSlot, bypassSlot).toFloat());

        g.setColour (findColour (TextEditor::textColourId).withMultipliedAlpha (isBypassed ? 0.5f : 1.0f));
        g.setFont (font);
        g.drawFittedText (getName(), textArea, Justification::centred, 2);
    }

    void resized() override
    {
        if (bypassButton != nullptr)
        {
            auto boxArea = getLocalBounds().reduced (4, pinSize);
            bypassButton->setBounds (boxArea.withWidth (bypassSlot)
                                            .withSizeKeepingCentre (bypassSlot, bypassSlot)
                                            .translated (bypassMargin, 0));
        }

        if (auto f = graph.graph.getNodeForId (pluginID))
        {
            if (auto* processor = f->getProcessor())
            {
                for (auto* pin : pins)
                {
                    const bool isInput = pin->isInput;
                    auto channelIndex = pin->pin.channelIndex;

                    // Pins straddle the box's edge
                    pin->setBounds (graphPinInset + getPinSlot (*processor, isInput, channelIndex) * graphGridSize - pinSize / 2,
                                    pin->isInput ? pinSize / 2 : (getHeight() - pinSize - pinSize / 2),
                                    pinSize, pinSize);
                }
            }
        }
    }

    // Disabled buses take no space, and an empty slot sets enabled buses apart
    static int getNumBusGaps (const AudioProcessor& processor, bool isInput, int lastBus)
    {
        int numEnabled = 0;

        for (int i = 0; i <= lastBus; ++i)
            if (processor.getChannelCountOfBus (isInput, i) > 0)
                ++numEnabled;

        return jmax (0, numEnabled - 1);
    }

    static int getPinSlot (const AudioProcessor& processor, bool isInput, int channelIndex)
    {
        int busIdx = 0;

        if (channelIndex == AudioProcessorGraph::midiChannelIndex)
            return (isInput ? processor.getTotalNumInputChannels() : processor.getTotalNumOutputChannels())
                 + getNumBusGaps (processor, isInput, processor.getBusCount (isInput) - 1);

        processor.getOffsetInBusBufferForAbsoluteChannelIndex (isInput, channelIndex, busIdx);
        return channelIndex + getNumBusGaps (processor, isInput, busIdx);
    }

    static int getNumPinSlots (const AudioProcessor& processor, bool isInput)
    {
        const bool hasMidi = isInput ? processor.acceptsMidi() : processor.producesMidi();
        return getPinSlot (processor, isInput, AudioProcessorGraph::midiChannelIndex) + (hasMidi ? 1 : 0);
    }

    Rectangle<int> getBoundsForPosition (int w, int h) const
    {
        const auto p = graph.getNodePosition (pluginID);
        const auto left = p.x * getParentWidth() - w * 0.5;
        const auto top = p.y * getParentHeight() - h * 0.5;

        if (graph.isNodePositionFree (pluginID))
            return { roundToInt (left), roundToInt (top), w, h };

        return { snapToGraphGrid (left), snapToGraphGrid (top), w, h };
    }

    Point<float> getPinPos (int index, bool isInput) const
    {
        for (auto* pin : pins)
            if (pin->pin.channelIndex == index && isInput == pin->isInput)
                return getPosition().toFloat() + pin->getBounds().getCentre().toFloat();

        return {};
    }

    void update()
    {
        const AudioProcessorGraph::Node::Ptr f (graph.graph.getNodeForId (pluginID));
        jassert (f != nullptr);

        auto& processor = *f->getProcessor();

        numIns = processor.getTotalNumInputChannels();
        if (processor.acceptsMidi())
            ++numIns;

        numOuts = processor.getTotalNumOutputChannels();
        if (processor.producesMidi())
            ++numOuts;

        int h = 60;

        const auto numSlots = jmax (getNumPinSlots (processor, true), getNumPinSlots (processor, false));

        int w = jmax (graphMinNodeWidth, graphPinInset * 2 + (numSlots - 1) * graphGridSize);

        String displayName = processor.getName();
        if (auto* node = graph.graph.getNodeForId (pluginID))
        {
            auto customName = node->properties.getWithDefault ("customNodeName", juce::String()).toString();
            if (customName.isNotEmpty())
                displayName = customName;
        }

        const auto textWidth = GlyphArrangement::getStringWidthInt (font, displayName);
        w = jmax (w, 16 + (bypassButton != nullptr ? 2 * (bypassSlot + bypassMargin) : 0) + jmin (textWidth, 300));
        if (textWidth > 300)
            h = 100;

        w = graphGridSize * (int) std::ceil (w / (double) graphGridSize);

        setBounds (getBoundsForPosition (w, h));

        setName (displayName);

        auto tooltip = getTooltipForProcessor (processor);

        if (const auto mappings = graph.describeMidiMappingsForNode (pluginID); mappings.isNotEmpty())
            tooltip << "  " << String::fromUTF8 ("\xe2\x80\xa2") << "  MIDI: " << mappings;

        setTooltip (tooltip);

        // Bypass and mappings can change without any change in size
        repaint();

        if (numIns != numInputs || numOuts != numOutputs)
        {
            numInputs = numIns;
            numOutputs = numOuts;

            pins.clear();

            for (int i = 0; i < processor.getTotalNumInputChannels(); ++i)
                addAndMakeVisible (pins.add (new PinComponent (panel, { pluginID, i }, true)));

            if (processor.acceptsMidi())
                addAndMakeVisible (pins.add (new PinComponent (panel, { pluginID, AudioProcessorGraph::midiChannelIndex }, true)));

            for (int i = 0; i < processor.getTotalNumOutputChannels(); ++i)
                addAndMakeVisible (pins.add (new PinComponent (panel, { pluginID, i }, false)));

            if (processor.producesMidi())
                addAndMakeVisible (pins.add (new PinComponent (panel, { pluginID, AudioProcessorGraph::midiChannelIndex }, false)));

            resized();
        }
    }

    AudioProcessor* getProcessor() const
    {
        if (auto node = graph.graph.getNodeForId (pluginID))
            return node->getProcessor();

        return {};
    }

    bool isNodeUsingARA() const
    {
        if (auto node = graph.graph.getNodeForId (pluginID))
            return node->properties["useARA"];

        return false;
    }

    bool isOpenedWithPreset() const
    {
        if (auto node = graph.graph.getNodeForId (pluginID))
            return node->properties[PluginGraph::openWithPresetProp];

        return false;
    }

    void showPopupMenu (Point<int> localPos)
    {
        menu.reset (new PopupMenu);
        menu->addItem ("Delete This Node", [this] { graph.removeNode (pluginID); });
        menu->addItem ("Disconnect All Pins", [this] { graph.graph.disconnectNode (pluginID); });
        menu->addItem ("Toggle Bypass", [this] { toggleBypass(); });
        menu->addItem ("Rename This Node", [this]
        {
            promptRenameNode();
        });

        if (auto* engine = panel.midiEngine)
        {
            menu->addSeparator();
            menu->addItem ("MIDI Learn", true, engine->isLearning(), [engine] { engine->setLearning (! engine->isLearning()); });

            if (graph.getMidiMappings().hasMappingsForNode (pluginID))
                menu->addItem ("Clear MIDI Mappings", [this] { graph.removeMidiMappingsForNode (pluginID); });
        }

        menu->addSeparator();
        if (auto* proc = getProcessor())
        {
            if (proc->hasEditor())
                menu->addItem ("Always Open Plug-in GUI with Preset", true, isOpenedWithPreset(), [this]
                {
                    if (auto* node = graph.graph.getNodeForId (pluginID))
                    {
                        node->properties.set (PluginGraph::openWithPresetProp, ! isOpenedWithPreset());
                        graph.changed();
                    }
                });

            if (AUNBandEQConverter::isAUNBandEQ (proc))
            {
                menu->addItem ("Import EQ from AutoEQ...", [this]
                {
                    if (auto* mainWin = findParentComponentOfClass<MainHostWindow>())
                        if (auto* node = graph.graph.getNodeForId (pluginID))
                            mainWin->showOnlineCalibrationDialogForNode (node);
                });

                menu->addItem ("Import EQ from file...", [this]
                {
                    if (auto* mainWin = findParentComponentOfClass<MainHostWindow>())
                        if (auto* node = graph.graph.getNodeForId (pluginID))
                            mainWin->importCalibrationFileForNode (node);
                });
            }

           #if JUCE_PLUGINHOST_ARA
            if (auto* instance = dynamic_cast<AudioPluginInstance*> (proc))
                if (instance->getPluginDescription().hasARAExtension && isNodeUsingARA())
                    menu->addItem ("Show ARA Host Controls", [this] { showWindow (PluginWindow::Type::araHost); });
           #endif

        }

        menu->addSeparator();

        // MIDI I/O nodes have no audio buses to configure
        if (auto* proc = getProcessor(); proc != nullptr && PluginGraph::isConfiguredByAudioSettings (*proc))
            menu->addItem ("Audio Settings...", [this] { showWindow (PluginWindow::Type::audioIO); });
        else if (dynamic_cast<AudioProcessorGraph::AudioGraphIOProcessor*> (proc) == nullptr)
            menu->addItem ("Configure Audio I/O", [this] { showWindow (PluginWindow::Type::audioIO); });

        menu->addSeparator();
        menu->addItem ("Save Plug-in State", [this] { savePluginState(); });
        menu->addItem ("Load Plug-in State", [this] { loadPluginState(); });

        menu->showMenuAsync (PopupMenu::Options{}.withTargetScreenArea (Rectangle<int>{}.withPosition (localPointToGlobal (localPos))));
    }

    bool isNodeBypassed() const
    {
        if (auto* node = graph.graph.getNodeForId (pluginID))
            return node->isBypassed();

        return false;
    }

    void toggleBypass()
    {
        graph.setNodeBypassed (pluginID, ! isNodeBypassed());
        repaint();
    }

    void bypassButtonClicked()
    {
        // In MIDI learn, the button picks bypass as the target instead of toggling it
        if (isMidiLearning())
            panel.midiEngine->selectLearnTarget (MidiMappingTarget::forBypass (pluginID));
        else
            toggleBypass();
    }

    bool isMidiLearning() const
    {
        return panel.midiEngine != nullptr && panel.midiEngine->isLearning();
    }

    bool isMidiLearnTarget() const
    {
        if (! isMidiLearning())
            return false;

        const auto& target = panel.midiEngine->getLearnTarget();
        return target.has_value() && target->isNodeTarget() && target->nodeID == pluginID;
    }

    static Colour getKindColour (AudioProcessor& processor)
    {
        if (dynamic_cast<AudioProcessorGraph::AudioGraphIOProcessor*> (&processor) != nullptr
            || PluginGraph::isConfiguredByAudioSettings (processor))
            return Curve::Colours::nodeInterface;

        if (auto* instance = dynamic_cast<AudioPluginInstance*> (&processor))
            if (instance->getPluginDescription().pluginFormatName != "Internal")
                return Curve::Colours::nodeExternal;

        return Curve::Colours::nodeInternal;
    }

    // A small DIN socket: a ring with five pins along its upper half
    static void drawMidiBadge (Graphics& g, Rectangle<float> area)
    {
        const auto socket = area.withSizeKeepingCentre (12.0f, 12.0f);
        const auto centre = socket.getCentre();

        g.setColour (Curve::Colours::accent);
        g.drawEllipse (socket, 1.2f);

        for (int i = 0; i < 5; ++i)
        {
            const auto angle = -MathConstants<float>::halfPi + MathConstants<float>::pi * 0.25f * (float) i;
            g.fillEllipse (Rectangle<float> (1.6f, 1.6f).withCentre (centre.getPointOnCircumference (3.2f, angle)));
        }
    }

    void promptRenameNode()
    {
        auto* node = graph.graph.getNodeForId (pluginID);
        if (node == nullptr || node->getProcessor() == nullptr)
            return;

        juce::String currentName = node->properties.getWithDefault ("customNodeName", juce::String()).toString();
        if (currentName.isEmpty())
            currentName = node->getProcessor()->getName();

        auto* alert = new juce::AlertWindow ("Rename Node",
                                             "Enter a new name for this node:",
                                             juce::AlertWindow::QuestionIcon);
        alert->addTextEditor ("nodeName", currentName, "Name:");
        alert->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        alert->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        juce::Component::SafePointer<PluginComponent> safeThis (this);
        alert->enterModalState (true, juce::ModalCallbackFunction::create ([safeThis, alert] (int result)
        {
            if (result == 1 && safeThis != nullptr)
            {
                auto rawName = alert->getTextEditorContents ("nodeName").trim();
                if (auto* targetNode = safeThis->graph.graph.getNodeForId (safeThis->pluginID))
                {
                    if (rawName.isNotEmpty())
                        targetNode->properties.set ("customNodeName", rawName);
                    else
                        targetNode->properties.remove ("customNodeName");

                    safeThis->update();
                    safeThis->graph.changed();
                    safeThis->graph.graph.sendChangeMessage();
                }
            }
        }), true);
    }

    void showWindow (PluginWindow::Type type)
    {
        if (auto node = graph.graph.getNodeForId (pluginID))
            if (auto* w = graph.getOrCreateWindowFor (node, type))
                w->toFront (true);
    }

    void timerCallback() override
    {
        stopTimer();
        showPopupMenu (originalTouchPos);
    }

    void parameterValueChanged (int, float) override
    {
        // Parameter changes might come from the audio thread or elsewhere, but
        // we can only call repaint from the message thread.
        triggerAsyncUpdate();
    }

    void parameterGestureChanged (int, bool) override  {}

    void handleAsyncUpdate() override { repaint(); }

    void savePluginState()
    {
        fileChooser = std::make_unique<FileChooser> ("Save plugin state");

        const auto onChosen = [ref = SafePointer<PluginComponent> (this)] (const FileChooser& chooser)
        {
            if (ref == nullptr)
                return;

            const auto result = chooser.getResult();

            if (result == File())
                return;

            if (auto* node = ref->graph.graph.getNodeForId (ref->pluginID))
            {
                MemoryBlock block;
                node->getProcessor()->getStateInformation (block);
                result.replaceWithData (block.getData(), block.getSize());
            }
        };

        fileChooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting, onChosen);
    }

    void loadPluginState()
    {
        fileChooser = std::make_unique<FileChooser> ("Load plugin state");

        const auto onChosen = [ref = SafePointer<PluginComponent> (this)] (const FileChooser& chooser)
        {
            if (ref == nullptr)
                return;

            const auto result = chooser.getResult();

            if (result == File())
                return;

            if (auto* node = ref->graph.graph.getNodeForId (ref->pluginID))
            {
                if (auto stream = result.createInputStream())
                {
                    MemoryBlock block;
                    stream->readIntoMemoryBlock (block);
                    node->getProcessor()->setStateInformation (block.getData(), (int) block.getSize());
                }
            }
        };

        fileChooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, onChosen);
    }

    GraphEditorPanel& panel;
    PluginGraph& graph;
    const AudioProcessorGraph::NodeID pluginID;
    AudioProcessor* const processorAtCreation = getProcessor();
    OwnedArray<PinComponent> pins;
    int numInputs = 0, numOutputs = 0;
    int pinSize = 16;
    Point<int> originalPos, originalTouchPos;
    Font font = FontOptions { 13.0f };
    int numIns = 0, numOuts = 0;
    std::unique_ptr<PopupMenu> menu;
    std::unique_ptr<FileChooser> fileChooser;
    AudioProcessorParameter* observedBypassParam = nullptr;
    static constexpr int bypassSlot = 20;
    static constexpr int bypassMargin = 6;
    static constexpr float cornerSize = 6.0f;
    std::unique_ptr<NodeBypassButton> bypassButton;
    Colour kindColour = Curve::Colours::nodeInternal;
};


//==============================================================================
struct GraphEditorPanel::ConnectorComponent final : public Component,
                                                    public SettableTooltipClient
{
    explicit ConnectorComponent (GraphEditorPanel& p)
        : panel (p), graph (p.graph)
    {
    }

    void setInput (AudioProcessorGraph::NodeAndChannel newSource)
    {
        if (connection.source != newSource)
        {
            connection.source = newSource;
            update();
        }
    }

    void setOutput (AudioProcessorGraph::NodeAndChannel newDest)
    {
        if (connection.destination != newDest)
        {
            connection.destination = newDest;
            update();
        }
    }

    void dragStart (Point<float> pos)
    {
        lastInputPos = pos;
        resizeToFit();
    }

    void dragEnd (Point<float> pos)
    {
        lastOutputPos = pos;
        resizeToFit();
    }

    void update()
    {
        Point<float> p1, p2;
        getPoints (p1, p2);

        if (lastInputPos != p1 || lastOutputPos != p2)
            resizeToFit();
    }

    void resizeToFit()
    {
        Point<float> p1, p2;
        getPoints (p1, p2);

        lastInputPos = p1;
        lastOutputPos = p2;

        // The curve stays within its control points; the margin covers the arrow
        const auto route = getRoute();
        const Point<float> points[] { route.start, route.end, route.spineStart, route.spineEnd, route.control1, route.control2 };
        auto newBounds = Rectangle<float>::findAreaContainingPoints (points, (int) std::size (points))
                             .expanded (8.0f).getSmallestIntegerContainer();

        if (newBounds != getBounds())
            setBounds (newBounds);
        else
            resized();

        repaint();
    }

    void getPoints (Point<float>& p1, Point<float>& p2) const
    {
        p1 = lastInputPos;
        p2 = lastOutputPos;

        if (auto* src = panel.getComponentForPlugin (connection.source.nodeID))
            p1 = src->getPinPos (connection.source.channelIndex, false);

        if (auto* dest = panel.getComponentForPlugin (connection.destination.nodeID))
            p2 = dest->getPinPos (connection.destination.channelIndex, true);
    }

    // Wires leave and enter their pins vertically, however the nodes are placed. Going
    // down, the reach stays within the gap, so that close nodes get a flat S, not a loop.
    static std::pair<Point<float>, Point<float>> getControlPoints (Point<float> p1, Point<float> p2)
    {
        const auto drop = p2.y - p1.y;
        const auto reach = drop > 0.0f ? jmax (6.0f, drop * 0.5f)
                                       : jmax (20.0f, -drop * 0.5f);

        return { p1.translated (0.0f, reach), p2.translated (0.0f, -reach) };
    }

    // Consecutive channels between the same two nodes share one line (the spine),
    // which forks out to each pin
    struct Route
    {
        Point<float> start, spineStart, control1, control2, spineEnd, end;
        float forkLength = 0.0f;
        bool bundled = false;
    };

    Route getRoute() const
    {
        Route r;
        getPoints (r.start, r.end);

        auto* src = panel.getComponentForPlugin (connection.source.nodeID);
        auto* dest = panel.getComponentForPlugin (connection.destination.nodeID);

        r.spineStart = r.start;
        r.spineEnd = r.end;

        if (isInBundle() && src != nullptr && dest != nullptr)
        {
            const auto middleOfPins = [this] (PluginComponent& node, int channel, bool isInput)
            {
                return (node.getPinPos (channel - bundleBefore, isInput)
                        + node.getPinPos (channel + bundleAfter, isInput)) * 0.5f;
            };

            const auto from = middleOfPins (*src, connection.source.channelIndex, false);
            const auto to = middleOfPins (*dest, connection.destination.channelIndex, true);
            const auto drop = to.y - from.y;

            // Close, aligned nodes look best with their wires left parallel
            if (drop >= 48.0f || (drop >= 24.0f && std::abs (to.x - from.x) >= 1.0f))
            {
                r.bundled = true;
                r.forkLength = jmin (16.0f, drop / 3.0f);
                r.spineStart = from.translated (0.0f, r.forkLength);
                r.spineEnd = to.translated (0.0f, -r.forkLength);
            }
        }

        std::tie (r.control1, r.control2) = getControlPoints (r.spineStart, r.spineEnd);
        return r;
    }

    bool isInBundle() const { return bundleBefore + bundleAfter > 0; }

    void setBundle (int channelsBefore, int channelsAfter)
    {
        if (bundleBefore != channelsBefore || bundleAfter != channelsAfter)
        {
            bundleBefore = channelsBefore;
            bundleAfter = channelsAfter;
            resizeToFit();
        }
    }

    bool sharesBundleWith (const ConnectorComponent& other) const
    {
        const auto offset = connection.source.channelIndex - other.connection.source.channelIndex;

        return connection.source.nodeID == other.connection.source.nodeID
            && connection.destination.nodeID == other.connection.destination.nodeID
            && connection.destination.channelIndex - other.connection.destination.channelIndex == offset
            && offset >= -other.bundleBefore && offset <= other.bundleAfter;
    }

    void setStyle (Colour newColour)
    {
        if (colour != newColour)
        {
            colour = newColour;
            repaint();
        }
    }

    Colour getBaseColour() const
    {
        return connection.source.isMIDI() || connection.destination.isMIDI() ? Curve::Colours::midiWire
                                                                             : Curve::Colours::audioWire;
    }

    void paint (Graphics& g) override
    {
        g.setColour (colour);
        g.fillPath (linePath);
    }

    bool hitTest (int x, int y) override
    {
        auto pos = Point<int> (x, y).toFloat();

        if (hitPath.contains (pos))
        {
            double distanceFromStart, distanceFromEnd;
            getDistancesFromEnds (getPosition().toFloat() + pos, distanceFromStart, distanceFromEnd);

            // avoid clicking the connector when over a pin
            return distanceFromStart > 7.0 && distanceFromEnd > 7.0;
        }

        return false;
    }

    void mouseDown (const MouseEvent&) override
    {
        dragging = false;
    }

    void mouseDrag (const MouseEvent& e) override
    {
        if (dragging)
        {
            panel.dragConnector (e);
        }
        else if (e.mouseWasDraggedSinceMouseDown())
        {
            dragging = true;

            auto conn = connection;
            double distanceFromStart, distanceFromEnd;
            getDistancesFromEnds (getPosition().toFloat() + e.position, distanceFromStart, distanceFromEnd);
            const bool isNearerSource = (distanceFromStart < distanceFromEnd);

            AudioProcessorGraph::NodeAndChannel dummy { {}, 0 };

            panel.beginConnectorDrag (isNearerSource ? dummy : conn.source,
                                      isNearerSource ? conn.destination : dummy,
                                      e);

            graph.graph.removeConnection (conn);
        }
    }

    void mouseUp (const MouseEvent& e) override
    {
        if (dragging)
            panel.endDraggingConnector (e);
    }

    void resized() override
    {
        Point<float> p1, p2;
        getPoints (p1, p2);

        lastInputPos = p1;
        lastOutputPos = p2;

        p1 -= getPosition().toFloat();
        p2 -= getPosition().toFloat();

        const auto origin = getPosition().toFloat();
        const auto route = getRoute();
        const auto s1 = route.spineStart - origin, s2 = route.spineEnd - origin;
        const auto c1 = route.control1 - origin, c2 = route.control2 - origin;

        // Only the first wire of a bundle draws the spine, so that it isn't drawn twice
        Path spine;

        if (! route.bundled || bundleBefore == 0)
        {
            spine.startNewSubPath (s1);
            spine.cubicTo (c1, c2, s2);
        }

        Path forks;

        if (route.bundled)
        {
            const auto bend = route.forkLength * 0.5f;

            forks.startNewSubPath (p1);
            forks.cubicTo (p1.translated (0.0f, bend), s1.translated (0.0f, -bend), s1);
            forks.startNewSubPath (s2);
            forks.cubicTo (s2.translated (0.0f, bend), p2.translated (0.0f, -bend), p2);
        }

        Path centreLine (forks);
        centreLine.addPath (spine);

        PathStrokeType wideStroke (8.0f);
        wideStroke.createStrokedPath (hitPath, centreLine);

        PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded).createStrokedPath (linePath, forks);

        Path strokedSpine;
        // Thicker with each channel, so a bundle can't be taken for a mono wire
        const auto spineWidth = route.bundled ? jmin (6.0f, 2.0f + (float) (1 + bundleBefore + bundleAfter)) : 2.0f;

        PathStrokeType (spineWidth, PathStrokeType::curved, PathStrokeType::rounded)
            .createStrokedPath (strokedSpine, spine);

        linePath.addPath (strokedSpine);
        linePath.setUsingNonZeroWinding (true);

        // Flow is top to bottom, so only a wire clearly going against it needs an arrow
        if (p2.y < p1.y - 40.0f)
        {
            const auto middle = (p1 + (c1 + c2) * 3.0f + p2) / 8.0f;
            const auto direction = (p2 + c2) - (p1 + c1);

            Path arrow;
            arrow.addTriangle (-4.0f, 5.0f, -4.0f, -5.0f, 4.0f, 0.0f);
            arrow.applyTransform (AffineTransform::rotation (std::atan2 (direction.y, direction.x))
                                                  .translated (middle));

            linePath.addPath (arrow);
        }
    }

    void getDistancesFromEnds (Point<float> p, double& distanceFromStart, double& distanceFromEnd) const
    {
        Point<float> p1, p2;
        getPoints (p1, p2);

        distanceFromStart = p1.getDistanceFrom (p);
        distanceFromEnd   = p2.getDistanceFrom (p);
    }

    GraphEditorPanel& panel;
    PluginGraph& graph;
    AudioProcessorGraph::Connection connection { { {}, 0 }, { {}, 0 } };
    Point<float> lastInputPos, lastOutputPos;
    Path linePath, hitPath;
    Colour colour = Curve::Colours::audioWire;
    int bundleBefore = 0, bundleAfter = 0;
    bool dragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConnectorComponent)
};


//==============================================================================
class GraphEditorPanel::BurgerButton final : public Button
{
public:
    BurgerButton() : Button ("BurgerButton") {}

    void paintButton (Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (1.0f);

        if (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown)
        {
            g.setColour (Colours::white.withAlpha (shouldDrawButtonAsDown ? 0.22f : 0.12f));
            g.fillRoundedRectangle (bounds, 4.0f);
        }

        g.setColour (Colours::white.withAlpha (shouldDrawButtonAsHighlighted ? 0.95f : 0.65f));

        const float lineW = 14.0f;
        const float lineH = 2.0f;
        const float x = bounds.getCentreX() - lineW * 0.5f;
        const float cy = bounds.getCentreY();
        const float spacing = 4.0f;

        g.fillRoundedRectangle (x, cy - spacing - lineH * 0.5f, lineW, lineH, 1.0f);
        g.fillRoundedRectangle (x, cy - lineH * 0.5f, lineW, lineH, 1.0f);
        g.fillRoundedRectangle (x, cy + spacing - lineH * 0.5f, lineW, lineH, 1.0f);
    }
};

//==============================================================================
GraphEditorPanel::GraphEditorPanel (PluginGraph& g)  : graph (g)
{
    burgerButton = std::make_unique<BurgerButton>();
    burgerButton->onClick = [this]
    {
        if (auto* mw = findParentComponentOfClass<MainHostWindow>())
            mw->showAppMenu (burgerButton.get());
    };
    addAndMakeVisible (burgerButton.get());

    graph.addChangeListener (this);
    setOpaque (true);

    addMouseListener (&hoverTracker, true);
}

GraphEditorPanel::~GraphEditorPanel()
{
    removeMouseListener (&hoverTracker);
    graph.removeChangeListener (this);
    burgerButton = nullptr;
    draggingConnector = nullptr;
    nodes.clear();
    connectors.clear();
}

void GraphEditorPanel::paint (Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));

    if (draggingNode)
    {
        // A cached tile at device resolution, as this redraws throughout a drag
        const auto scale = g.getInternalContext().getPhysicalPixelScaleFactor();
        const auto tileSize = roundToInt ((float) graphGridSize * scale);

        if (gridTile.getWidth() != tileSize)
        {
            gridTile = Image (Image::ARGB, tileSize, tileSize, true);
            Graphics tile (gridTile);
            tile.setColour (Colours::white.withAlpha (0.16f));
            tile.fillEllipse (gridTile.getBounds().toFloat().withSizeKeepingCentre (2.0f * scale, 2.0f * scale));
        }

        // The dot is mid-tile, so move it onto the grid's intersections
        const auto halfGrid = (float) graphGridSize * 0.5f;

        g.setFillType (FillType (gridTile, AffineTransform::scale (1.0f / scale).translated (halfGrid, halfGrid)));
        g.fillRect (getLocalBounds());
    }
}

void GraphEditorPanel::paintOverChildren (Graphics& g)
{
    if (midiEngine != nullptr && midiEngine->isLearning())
    {
        g.setColour (Curve::Colours::accent);
        g.drawRect (getLocalBounds().toFloat(), 2.0f);
    }
}

bool GraphEditorPanel::keyPressed (const KeyPress& key)
{
    if (midiEngine == nullptr || ! midiEngine->isLearning())
        return false;

    if (key == KeyPress::escapeKey)
    {
        midiEngine->setLearning (false);
        return true;
    }

    if (key == KeyPress::deleteKey || key == KeyPress::backspaceKey)
    {
        midiEngine->removeMappingForLearnTarget();
        return true;
    }

    return false;
}

void GraphEditorPanel::midiLearnStateChanged()
{
    const bool learning = midiEngine != nullptr && midiEngine->isLearning();

    // Takes Esc and Delete while learning
    setWantsKeyboardFocus (learning);

    if (learning && isShowing())
        grabKeyboardFocus();

    updateComponents();
    repaint();
}

void GraphEditorPanel::mouseDown (const MouseEvent& e)
{
    if (e.source.isTouch())
    {
        originalTouchPos = e.position.toInt();
        startTimer (750);
    }
    else if (e.mods.isPopupMenu())
    {
        showPopupMenu (e.position.toInt());
    }
}

void GraphEditorPanel::mouseUp (const MouseEvent& e)
{
    if (e.source.isTouch())
    {
        stopTimer();
        callAfterDelay (250, []() { PopupMenu::dismissAllActiveMenus(); });
    }
}

void GraphEditorPanel::mouseDrag (const MouseEvent& e)
{
    if (e.source.isTouch() && e.getDistanceFromDragStart() > 5)
        stopTimer();
}

void GraphEditorPanel::createNewPlugin (const PluginDescriptionAndPreference& desc, Point<int> position)
{
    const double w = getWidth() > 0 ? (double) getWidth() : 800.0;
    const double h = getHeight() > 0 ? (double) getHeight() : 600.0;
    graph.addPlugin (desc, { jlimit (0.0, 1.0, (double) position.x / w),
                             jlimit (0.0, 1.0, (double) position.y / h) });
}

GraphEditorPanel::PluginComponent* GraphEditorPanel::getComponentForPlugin (AudioProcessorGraph::NodeID nodeID) const
{
    for (auto* fc : nodes)
       if (fc->pluginID == nodeID)
            return fc;

    return nullptr;
}

AudioProcessorGraph::Node::Ptr GraphEditorPanel::getNodeAt (Point<int> pos) const
{
    for (auto* fc : nodes)
    {
        if (fc != nullptr && fc->getBounds().contains (pos))
            return graph.graph.getNodeForId (fc->pluginID);
    }

    return nullptr;
}

GraphEditorPanel::ConnectorComponent* GraphEditorPanel::getComponentForConnection (const AudioProcessorGraph::Connection& conn) const
{
    for (auto* cc : connectors)
        if (cc->connection == conn)
            return cc;

    return nullptr;
}

GraphEditorPanel::PinComponent* GraphEditorPanel::findPinAt (Point<float> pos) const
{
    for (auto* fc : nodes)
    {
        auto* comp = fc->getComponentAt (pos.toInt() - fc->getPosition());

        if (auto* pin = dynamic_cast<PinComponent*> (comp))
            return pin;
    }

    return nullptr;
}

void GraphEditorPanel::resized()
{
    if (burgerButton != nullptr)
        burgerButton->setBounds (10, 8, 24, 24);

    updateComponents();
}

void GraphEditorPanel::changeListenerCallback (ChangeBroadcaster*)
{
    updateComponents();
}

void GraphEditorPanel::updateComponents()
{
    // A preset load can reuse a node ID for a different processor, so rebuild those too
    for (int i = nodes.size(); --i >= 0;)
    {
        auto* comp = nodes.getUnchecked (i);
        auto* node = graph.graph.getNodeForId (comp->pluginID);

        if (node == nullptr || node->getProcessor() != comp->processorAtCreation)
        {
            // The node may be mid-drag, and its ID may come back as a different node
            setDraggingNode (false);
            layoutBeforeTidy.clear();

            if (comp->pluginID == hoveredNode)
                hoveredNode = {};

            nodes.remove (i);
        }
    }

    for (int i = connectors.size(); --i >= 0;)
    {
        if (! graph.graph.isConnected (connectors.getUnchecked (i)->connection))
        {
            if (connectors.getUnchecked (i) == hoveredConnector)
                hoveredConnector = nullptr;

            connectors.remove (i);
        }
    }

    for (auto* fc : nodes)
        fc->update();

    for (auto* f : graph.graph.getNodes())
    {
        if (getComponentForPlugin (f->nodeID) == nullptr)
        {
            auto* comp = nodes.add (new PluginComponent (*this, f->nodeID));
            addAndMakeVisible (comp);
            comp->update();
        }
    }

    // After creating nodes, so connectors see rebuilt nodes' pins
    for (auto* cc : connectors)
        cc->update();

    for (auto& c : graph.graph.getConnections())
    {
        if (getComponentForConnection (c) == nullptr)
        {
            auto* comp = connectors.add (new ConnectorComponent (*this));
            addAndMakeVisible (comp);

            // Behind the nodes, so that wires never cross a node's name
            comp->toBack();

            comp->setInput (c.source);
            comp->setOutput (c.destination);
        }
    }

    updateConnectorBundles();
    updateConnectorStyles();
}

bool GraphEditorPanel::isSameBus (AudioProcessorGraph::NodeID nodeID, bool isInput, int channelA, int channelB) const
{
    auto* node = graph.graph.getNodeForId (nodeID);

    if (node == nullptr || node->getProcessor() == nullptr)
        return false;

    int busA = 0, busB = 0;
    node->getProcessor()->getOffsetInBusBufferForAbsoluteChannelIndex (isInput, channelA, busA);
    node->getProcessor()->getOffsetInBusBufferForAbsoluteChannelIndex (isInput, channelB, busB);

    return busA == busB;
}

void GraphEditorPanel::updateConnectorBundles()
{
    for (auto* cc : connectors)
    {
        const auto countConnected = [this, cc] (int step)
        {
            int count = 0;

            if (cc->connection.source.isMIDI() || cc->connection.destination.isMIDI())
                return count;

            for (auto c = cc->connection;; ++count)
            {
                c.source.channelIndex += step;
                c.destination.channelIndex += step;

                // Separate buses stay separate wires, e.g. a main input and a sidechain
                if (! graph.graph.isConnected (c)
                    || ! isSameBus (c.source.nodeID, false, cc->connection.source.channelIndex, c.source.channelIndex)
                    || ! isSameBus (c.destination.nodeID, true, cc->connection.destination.channelIndex, c.destination.channelIndex))
                    return count;
            }
        };

        cc->setBundle (countConnected (-1), countConnected (1));
    }
}

void GraphEditorPanel::updateConnectorsForNode (AudioProcessorGraph::NodeID nodeID)
{
    for (auto* cc : connectors)
        if (cc->connection.source.nodeID == nodeID || cc->connection.destination.nodeID == nodeID)
            cc->update();
}

void GraphEditorPanel::updateConnectorStyles()
{
    const auto isMeter = [this] (AudioProcessorGraph::NodeID nodeID)
    {
        auto* node = graph.graph.getNodeForId (nodeID);
        return node != nullptr && dynamic_cast<MeterNode*> (node->getProcessor()) != nullptr;
    };

    for (auto* cc : connectors)
    {
        const auto& c = cc->connection;
        const bool touchesHoveredNode = hoveredNode != AudioProcessorGraph::NodeID()
                                     && (c.source.nodeID == hoveredNode || c.destination.nodeID == hoveredNode);

        auto colour = cc->getBaseColour();

        // A meter only taps the signal, so its wires recede from the signal path
        if (isMeter (c.destination.nodeID))
            colour = colour.withMultipliedAlpha (0.4f);

        if (touchesHoveredNode || (hoveredConnector != nullptr && cc->sharesBundleWith (*hoveredConnector)))
            colour = Curve::Colours::accent;
        else if (hoveredNode != AudioProcessorGraph::NodeID())
            colour = colour.withMultipliedAlpha (0.45f);

        cc->setStyle (colour);
    }
}

void GraphEditorPanel::setHoverTarget (Component* target)
{
    AudioProcessorGraph::NodeID newNode;
    ConnectorComponent* newConnector = nullptr;

    if (target != nullptr)
    {
        if (auto* connector = dynamic_cast<ConnectorComponent*> (target))
            newConnector = connector != draggingConnector.get() ? connector : nullptr;
        else if (auto* node = dynamic_cast<PluginComponent*> (target))
            newNode = node->pluginID;
        else if (auto* parent = target->findParentComponentOfClass<PluginComponent>())
            newNode = parent->pluginID;
    }

    if (newNode != hoveredNode || newConnector != hoveredConnector)
    {
        hoveredNode = newNode;
        hoveredConnector = newConnector;
        updateConnectorStyles();
    }
}

void GraphEditorPanel::setDraggingNode (bool isDragging)
{
    if (draggingNode != isDragging)
    {
        draggingNode = isDragging;

        // The layout has moved on from the one that undo would restore
        if (isDragging)
            layoutBeforeTidy.clear();

        repaint();
    }
}

void GraphEditorPanel::tidyLayout()
{
    // Every node in the graph needs a component, for its size
    updateComponents();

    if (getWidth() <= 0 || getHeight() <= 0 || nodes.isEmpty())
        return;

    std::vector<Curve::GraphLayout::NodeInfo> infos;
    std::vector<AudioProcessorGraph::NodeID> ids;

    for (auto* comp : nodes)
    {
        auto* io = dynamic_cast<AudioProcessorGraph::AudioGraphIOProcessor*> (comp->getProcessor());

        infos.push_back ({ comp->getWidth(), comp->getHeight(), io != nullptr && ! io->isInput(), graphPinInset });
        ids.push_back (comp->pluginID);
    }

    const auto indexOf = [&ids] (AudioProcessorGraph::NodeID nodeID)
    {
        return (int) std::distance (ids.begin(), std::find (ids.begin(), ids.end(), nodeID));
    };

    std::vector<Curve::GraphLayout::Edge> edges;

    for (auto& c : graph.graph.getConnections())
        edges.emplace_back (indexOf (c.source.nodeID), indexOf (c.destination.nodeID));

    Curve::GraphLayout::Options options;
    options.nodeGap = graphGridSize;
    options.areaWidth = getWidth();
    options.areaHeight = getHeight();

    // Nothing can zoom the graph, so a window too small for it has to grow
    Curve::GraphLayout::Extent required;
    Curve::GraphLayout::arrange (infos, edges, options, &required);

    windowBeforeTidy = {};

    if (auto* window = getTopLevelComponent(); window != nullptr
        && (required.width > getWidth() || required.height > getHeight()))
    {
        windowBeforeTidy = window->getBounds();

        const auto newBounds = windowBeforeTidy.withWidth (windowBeforeTidy.getWidth() + jmax (0, required.width - getWidth()))
                                               .withHeight (windowBeforeTidy.getHeight() + jmax (0, required.height - getHeight()));

        if (auto* display = Desktop::getInstance().getDisplays().getDisplayForRect (windowBeforeTidy))
            window->setBounds (newBounds.constrainedWithin (display->userBounds.toNearestInt()));
        else
            window->setBounds (newBounds);

        options.areaWidth = getWidth();
        options.areaHeight = getHeight();
    }

    const auto centres = Curve::GraphLayout::arrange (infos, edges, options);

    layoutBeforeTidy.clear();

    for (size_t i = 0; i < ids.size(); ++i)
    {
        layoutBeforeTidy.push_back ({ ids[i], graph.getNodePosition (ids[i]), graph.isNodePositionFree (ids[i]) });
        graph.setNodePosition (ids[i], { centres[i].x / getWidth(), centres[i].y / getHeight() });
        graph.setNodePositionFree (ids[i], false);
    }

    graph.setChangedFlag (true);
    updateComponents();
}

void GraphEditorPanel::undoTidyLayout()
{
    if (auto* window = getTopLevelComponent(); window != nullptr && ! windowBeforeTidy.isEmpty())
        window->setBounds (windowBeforeTidy);

    for (const auto& saved : layoutBeforeTidy)
    {
        graph.setNodePosition (saved.nodeID, saved.position);
        graph.setNodePositionFree (saved.nodeID, saved.isFree);
    }

    layoutBeforeTidy.clear();
    graph.setChangedFlag (true);
    updateComponents();
}

void GraphEditorPanel::showPopupMenu (Point<int> localMousePos)
{
    menu.reset (new PopupMenu);

    if (auto* mainWindow = findParentComponentOfClass<MainHostWindow>())
    {
        mainWindow->addPluginsToMenu (*menu);

        menu->addSeparator();
        menu->addItem ("Tidy Up Layout", ! nodes.isEmpty(), false, [this] { tidyLayout(); });

        if (! layoutBeforeTidy.empty())
            menu->addItem ("Undo Tidy Up", [this] { undoTidyLayout(); });

        menu->showMenuAsync (PopupMenu::Options{}.withTargetScreenArea (Rectangle<int>{}.withPosition (localPointToGlobal (localMousePos))),
                             ModalCallbackFunction::create ([this, localMousePos] (int r)
                                                            {
                                                                if (auto* mainWin = findParentComponentOfClass<MainHostWindow>())
                                                                {
                                                                    if (r == MainHostWindow::onlineCalibrationCreateMenuID)
                                                                    {
                                                                        mainWin->showOnlineCalibrationDialogForCreation (localMousePos);
                                                                    }
                                                                    else if (r == MainHostWindow::importCalibrationCreateMenuID)
                                                                    {
                                                                        mainWin->importCalibrationFileForCreation (localMousePos);
                                                                    }
                                                                    else if (const auto chosen = mainWin->getChosenType (r))
                                                                    {
                                                                        createNewPlugin (*chosen, localMousePos);
                                                                    }
                                                                }
                                                            }));
    }
}

void GraphEditorPanel::beginConnectorDrag (AudioProcessorGraph::NodeAndChannel source,
                                           AudioProcessorGraph::NodeAndChannel dest,
                                           const MouseEvent& e)
{
    auto* c = dynamic_cast<ConnectorComponent*> (e.originalComponent);
    connectors.removeObject (c, false);
    draggingConnector.reset (c);

    if (draggingConnector == nullptr)
        draggingConnector.reset (new ConnectorComponent (*this));

    draggingConnector->setInput (source);
    draggingConnector->setOutput (dest);

    addAndMakeVisible (draggingConnector.get());
    draggingConnector->setAlwaysOnTop (true);
    draggingConnector->setStyle (Curve::Colours::accent);
    draggingConnector->toFront (false);

    // A wire taken from the graph mustn't stay the hovered one
    setHoverTarget (nullptr);

    dragConnector (e);
}

void GraphEditorPanel::dragConnector (const MouseEvent& e)
{
    auto e2 = e.getEventRelativeTo (this);

    if (draggingConnector != nullptr)
    {
        draggingConnector->setTooltip ({});

        auto pos = e2.position;

        if (auto* pin = findPinAt (pos))
        {
            auto connection = draggingConnector->connection;

            if (connection.source.nodeID == AudioProcessorGraph::NodeID() && ! pin->isInput)
            {
                connection.source = pin->pin;
            }
            else if (connection.destination.nodeID == AudioProcessorGraph::NodeID() && pin->isInput)
            {
                connection.destination = pin->pin;
            }

            if (graph.graph.canConnect (connection))
            {
                pos = (pin->getParentComponent()->getPosition() + pin->getBounds().getCentre()).toFloat();
                draggingConnector->setTooltip (pin->getTooltip());
            }
        }

        if (draggingConnector->connection.source.nodeID == AudioProcessorGraph::NodeID())
            draggingConnector->dragStart (pos);
        else
            draggingConnector->dragEnd (pos);
    }
}

void GraphEditorPanel::endDraggingConnector (const MouseEvent& e)
{
    if (draggingConnector == nullptr)
        return;

    draggingConnector->setTooltip ({});

    auto e2 = e.getEventRelativeTo (this);
    auto connection = draggingConnector->connection;

    draggingConnector = nullptr;

    if (auto* pin = findPinAt (e2.position))
    {
        if (connection.source.nodeID == AudioProcessorGraph::NodeID())
        {
            if (pin->isInput)
                return;

            connection.source = pin->pin;
        }
        else
        {
            if (! pin->isInput)
                return;

            connection.destination = pin->pin;
        }

        graph.graph.addConnection (connection);
    }
}

void GraphEditorPanel::timerCallback()
{
    stopTimer();
    showPopupMenu (originalTouchPos);
}

//==============================================================================
struct GraphDocumentComponent::TooltipBar final : public Component,
                                                  private Timer
{
    TooltipBar()
    {
        startTimer (40);
    }

    void visibilityChanged() override
    {
        if (isShowing())
            startTimer (40);
        else
            stopTimer();
    }

    void parentHierarchyChanged() override
    {
        visibilityChanged();
    }

    void paint (Graphics& g) override
    {
        g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));

        auto bounds = getLocalBounds().reduced (12, 0);

        if (message.isNotEmpty())
        {
            g.setColour (Curve::Colours::accent);
            g.setFont (FontOptions { 13.0f });
            g.drawFittedText (message, bounds, Justification::centred, 1);
        }
        else if (tip.isNotEmpty())
        {
            g.setColour (Colour (0xff93c5fd));
            g.setFont (FontOptions { 13.0f });
            g.drawFittedText (tip, bounds, Justification::centred, 1);
        }
        else
        {
            g.setColour (Colours::white.withAlpha (0.28f));
            g.setFont (FontOptions { 13.0f });
            auto hintText = TRANS ("Right-click anywhere to add plug-ins")
                            + "  " + String::charToString (0x2022) + "  "
                            + TRANS ("Drag pins to connect audio and MIDI");
            g.drawFittedText (hintText, bounds, Justification::centred, 1);
        }
    }

    void timerCallback() override
    {
        String newTip;

        if (auto* underMouse = Desktop::getInstance().getMainMouseSource().getComponentUnderMouse())
            if (auto* ttc = dynamic_cast<TooltipClient*> (underMouse))
                if (! (underMouse->isMouseButtonDown() || underMouse->isCurrentlyBlockedByAnotherModalComponent()))
                    newTip = ttc->getTooltip();

        if (newTip != tip)
        {
            tip = newTip;
            repaint();
        }
    }

    void setMessage (const String& newMessage)
    {
        if (newMessage != message)
        {
            message = newMessage;
            repaint();
        }
    }

    String tip, message;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TooltipBar)
};

GraphDocumentComponent::GraphDocumentComponent (AudioPluginFormatManager& fm,
                                                AudioDeviceManager& dm,
                                                KnownPluginList& kpl)
    : graph (new PluginGraph (fm, kpl)),
      deviceManager (dm)
{
    midiEngine = std::make_unique<MidiControlEngine> (graphPlayer.getMidiMessageCollector());

    init();

    deviceManager.addChangeListener (graphPanel.get());
    deviceManager.addAudioCallback (&graphPlayer);

    // The engine forwards whatever isn't mapped to the graph's MIDI input
    deviceManager.addMidiInputDeviceCallback ({}, midiEngine.get());
    deviceManager.addChangeListener (this);
}

void GraphDocumentComponent::init()
{
    updateMidiOutput();

    graphPanel.reset (new GraphEditorPanel (*graph));
    addAndMakeVisible (graphPanel.get());

    statusBar.reset (new TooltipBar());
    addAndMakeVisible (statusBar.get());

    graphPlayer.setProcessor (&graph->graph);

    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        lastDeviceName = device->getName();
        lastSampleRate = device->getCurrentSampleRate();
        lastBufferSize = device->getCurrentBufferSizeSamples();
        lastInputChannels = device->getActiveInputChannels();
        lastOutputChannels = device->getActiveOutputChannels();
    }

    keyState.addListener (&graphPlayer.getMidiMessageCollector());

    graphPanel->midiEngine = midiEngine.get();
    midiEngine->setGraph (graph.get());
    midiEngine->onStatusMessage = [this] (const String& message) { showStatusMessage (message); };
    midiEngine->onLearnStateChanged = [this]
    {
        if (graph != nullptr)
            graph->setPluginWindowsFloating (midiEngine->isLearning());

        if (graphPanel != nullptr)
            graphPanel->midiLearnStateChanged();
    };

    graph->graph.addChangeListener (this);
    graph->addChangeListener (this);

    graphPanel->updateComponents();
}

GraphDocumentComponent::~GraphDocumentComponent()
{
    deviceManager.removeChangeListener (this);

    if (midiOutput != nullptr)
        midiOutput->stopBackgroundThread();

    releaseGraph();

    keyState.removeListener (&graphPlayer.getMidiMessageCollector());
}

//==============================================================================
class GraphDocumentComponent::UpdateBannerComponent final : public Component
{
public:
    UpdateBannerComponent (const String& version, const String& url, std::function<void()> onUpdate, std::function<void()> onDismiss)
        : downloadUrl (url), updateCallback (std::move (onUpdate)), dismissCallback (std::move (onDismiss))
    {
        label.setText (TRANS ("A new version of Curve (") + version + TRANS (") is available!"), dontSendNotification);
        label.setFont (FontOptions { 13.0f }.withStyle ("Bold"));
        label.setColour (Label::textColourId, Colours::white);
        addAndMakeVisible (label);

        updateButton.setButtonText (TRANS ("Update Now"));
        updateButton.setColour (TextButton::buttonColourId, Colour (0xff2563eb));
        updateButton.setColour (TextButton::textColourOnId, Colours::white);
        updateButton.setColour (TextButton::textColourOffId, Colours::white);
        updateButton.onClick = [this] { if (updateCallback) updateCallback(); };
        addAndMakeVisible (updateButton);

        whatsNewButton.setButtonText (TRANS ("What's New"));
        whatsNewButton.onClick = [] { URL (GitHubUpdater::releasesUrl).launchInDefaultBrowser(); };
        addAndMakeVisible (whatsNewButton);

        dismissButton.setButtonText (String::charToString (0x00D7)); // ×
        dismissButton.onClick = [this] { if (dismissCallback) dismissCallback(); };
        addAndMakeVisible (dismissButton);
    }

    void paint (Graphics& g) override
    {
        g.fillAll (Colour (0xff1e293b));
        g.setColour (Colour (0xff3b82f6));
        g.fillRect (0, getHeight() - 1, getWidth(), 1);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8, 4);
        dismissButton.setBounds (r.removeFromRight (24));
        r.removeFromRight (8);
        updateButton.setBounds (r.removeFromRight (95));
        r.removeFromRight (8);
        whatsNewButton.setBounds (r.removeFromRight (90));
        r.removeFromRight (8);
        label.setBounds (r);
    }

private:
    String downloadUrl;
    std::function<void()> updateCallback;
    std::function<void()> dismissCallback;
    Label label;
    TextButton updateButton;
    TextButton whatsNewButton;
    TextButton dismissButton;
};

void GraphDocumentComponent::showUpdateBanner (const String& version, const String& downloadUrl)
{
    if (updateBanner != nullptr)
        return;

    Component::SafePointer<GraphDocumentComponent> safeSelf (this);
    updateBanner = std::make_unique<UpdateBannerComponent> (
        version,
        downloadUrl,
        [downloadUrl]
        {
            if (downloadUrl.isNotEmpty())
            {
                if (downloadUrl.startsWithIgnoreCase ("http"))
                    URL (downloadUrl).launchInDefaultBrowser();
            }
        },
        [safeSelf]
        {
            if (auto* self = safeSelf.getComponent())
            {
                self->updateBanner = nullptr;
                self->resized();
            }
        }
    );

    addAndMakeVisible (updateBanner.get());
    resized();
}

void GraphDocumentComponent::resized()
{
    auto r = getLocalBounds();

    if (updateBanner != nullptr)
        updateBanner->setBounds (r.removeFromTop (32));

    const int statusBarHeight = 24;
    if (statusBar != nullptr)
        statusBar->setBounds (r.removeFromBottom (statusBarHeight));

    if (graphPanel != nullptr)
        graphPanel->setBounds (r);
}

void GraphDocumentComponent::showStatusMessage (const String& message)
{
    if (statusBar != nullptr)
        statusBar->setMessage (message);
}

void GraphDocumentComponent::createNewPlugin (const PluginDescriptionAndPreference& desc, Point<int> pos)
{
    graphPanel->createNewPlugin (desc, pos);
}

void GraphDocumentComponent::releaseGraph()
{
    deviceManager.removeAudioCallback (&graphPlayer);
    deviceManager.removeMidiInputDeviceCallback ({}, midiEngine.get());

    midiEngine->setLearning (false);
    midiEngine->setGraph (nullptr);
    midiEngine->onStatusMessage = nullptr;
    midiEngine->onLearnStateChanged = nullptr;

    if (graphPanel != nullptr)
    {
        deviceManager.removeChangeListener (graphPanel.get());
        graphPanel = nullptr;
    }

    statusBar = nullptr;

    if (graph != nullptr)
    {
        graph->graph.removeChangeListener (this);
        graph->removeChangeListener (this);
    }

    cancelPendingUpdate();

    graphPlayer.setProcessor (nullptr);
    graph = nullptr;
}

void GraphDocumentComponent::propagateDeviceSettingsToNodes()
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        juce::String currentDeviceName = device->getName();
        double currentSampleRate = device->getCurrentSampleRate();
        int currentBufferSize = device->getCurrentBufferSizeSamples();
        auto workgroup = deviceManager.getDeviceAudioWorkgroup();

        lastDeviceName = currentDeviceName;
        lastSampleRate = currentSampleRate;
        lastBufferSize = currentBufferSize;
        lastInputChannels = device->getActiveInputChannels();
        lastOutputChannels = device->getActiveOutputChannels();

        bool anyActiveLoopback = false;

        if (graph != nullptr)
        {
            for (auto* node : graph->graph.getNodes())
            {
                if (auto* captureNode = dynamic_cast<OutputInterfaceLoopbackNode*> (node->getProcessor()))
                {
                    bool isBypassed = node->isBypassed();
                    bool hasOutputConnection = false;
                    for (const auto& conn : graph->graph.getConnections())
                    {
                        if (conn.source.nodeID == node->nodeID)
                        {
                            hasOutputConnection = true;
                            break;
                        }
                    }

                    bool shouldBeActive = (! isBypassed) && hasOutputConnection;
                    if (shouldBeActive)
                        anyActiveLoopback = true;

                    captureNode->setTargetOutputDeviceName (currentDeviceName);
                    captureNode->setActiveState (shouldBeActive);
                    captureNode->setAudioWorkgroup (workgroup);
                }
            }
        }

        if (anyActiveLoopback)
        {
            OutputInterfaceLoopbackNode::warmUpTap (currentDeviceName, currentSampleRate, currentBufferSize);
        }
        else
        {
            OutputInterfaceLoopbackNode::teardownTap();
        }

        bool autoSync = true;
        if (auto* settings = getAppProperties().getUserSettings())
            autoSync = settings->getBoolValue ("autoSyncSystemOutput", true);

        if (autoSync)
            OutputInterfaceLoopbackNode::syncSystemOutputDevice (currentDeviceName);
    }
    else
    {
        lastDeviceName.clear();
        lastSampleRate = 0.0;
        lastBufferSize = 0;
        lastInputChannels.clear();
        lastOutputChannels.clear();
        OutputInterfaceLoopbackNode::teardownTap();
    }
}

bool GraphDocumentComponent::closeAnyOpenPluginWindows()
{
    return graphPanel != nullptr && graphPanel->graph.closeAnyOpenPluginWindows();
}

void GraphDocumentComponent::changeListenerCallback (ChangeBroadcaster* source)
{
    updateMidiOutput();

    if (source == &graph->graph)
        if (auto* idle = getIdleReleaseController())
            idle->graphChanged();

    if (source == &deviceManager)
    {
        if (auto* device = deviceManager.getCurrentAudioDevice())
        {
            juce::String currentDeviceName = device->getName();
            double currentSampleRate = device->getCurrentSampleRate();
            int currentBufferSize = device->getCurrentBufferSizeSamples();
            auto currentIn = device->getActiveInputChannels();
            auto currentOut = device->getActiveOutputChannels();

            if (currentDeviceName == lastDeviceName
                && std::abs (currentSampleRate - lastSampleRate) < 0.001
                && currentBufferSize == lastBufferSize
                && currentIn == lastInputChannels
                && currentOut == lastOutputChannels)
            {
                // Audio device parameters haven't changed (e.g. CoreAudio aggregate device tap notification).
                // Do not trigger redundant node propagation cycles.
                return;
            }
        }
        else if (lastDeviceName.isEmpty())
        {
            return;
        }
    }

    triggerAsyncUpdate();
}

void GraphDocumentComponent::handleAsyncUpdate()
{
    propagateDeviceSettingsToNodes();
}

void GraphDocumentComponent::updateMidiOutput()
{
    auto* defaultMidiOutput = deviceManager.getDefaultMidiOutput();

    if (midiOutput != defaultMidiOutput)
    {
        if (midiOutput != nullptr)
            midiOutput->stopBackgroundThread();

        midiOutput = defaultMidiOutput;

        if (midiOutput != nullptr)
            midiOutput->startBackgroundThread();

        graphPlayer.setMidiOutput (midiOutput);
    }
}
