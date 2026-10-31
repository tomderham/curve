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

#include "MeterNode.h"
#include "../UI/CurveColours.h"

namespace
{
namespace Palette
{
    const juce::Colour background { 0xff141418 };
    const juce::Colour panel      { 0xff1d1d23 };
    const juce::Colour panelEdge  { 0xff2c2c35 };
    const juce::Colour text       { 0xffe8e8ec };
    const juce::Colour dimText    { 0xff8c8c98 };
    const juce::Colour grid       { 0xff2e2e38 };
    const juce::Colour blue       { Curve::Colours::accent };
    const juce::Colour teal       { 0xff2dd4bf };
    const juce::Colour green      { 0xff4ade80 };
    const juce::Colour amber      { 0xfffbbf24 };
    const juce::Colour red        { 0xffef4444 };
}

constexpr int timerHz = 30;
constexpr float noReading = -100.0f;

juce::String formatDb (float db, bool showPlus = false)
{
    if (db <= noReading)
        return "--";

    return (showPlus && db > 0.0f ? "+" : "") + juce::String (db, 1);
}

juce::FontOptions fontOf (float height, bool bold = false)
{
    return bold ? juce::FontOptions (height).withStyle ("Bold") : juce::FontOptions (height);
}

float toDb (float linear)
{
    return juce::Decibels::gainToDecibels (linear, noReading);
}

// One frame of what the views draw
struct Readings
{
    bool hasAudio = false;
    int numChannels = 2;
    std::array<float, MeterDsp::maxChannels> rmsDb { noReading, noReading };
    std::array<float, MeterDsp::maxChannels> peakDb { noReading, noReading };
    std::array<float, MeterDsp::maxChannels> vuDb { noReading, noReading };
    float correlation = 0.0f;
    float vuReferenceDb = -18.0f;
    float momentary = noReading, shortTerm = noReading, integrated = noReading;
};

//==============================================================================
// Used for the mode selector, Clip and Reset
class PillButton final : public juce::Button
{
public:
    explicit PillButton (const juce::String& text, juce::Colour onColourIn = Palette::blue)
        : Button (text), onColour (onColourIn)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paintButton (juce::Graphics& g, bool isHovered, bool isDown) override
    {
        const auto area = getLocalBounds().toFloat().reduced (0.5f);
        const auto radius = area.getHeight() / 2.0f;
        const auto on = getToggleState();

        g.setColour (on ? onColour : (isHovered ? Palette::panelEdge : Palette::panel).withMultipliedBrightness (isDown ? 0.85f : 1.0f));
        g.fillRoundedRectangle (area, radius);

        if (! on)
        {
            g.setColour (Palette::panelEdge);
            g.drawRoundedRectangle (area, radius, 1.0f);
        }

        g.setColour (on ? juce::Colours::white : (isHovered ? Palette::text : Palette::dimText));
        g.setFont (fontOf (11.5f, on));
        g.drawText (getButtonText(), getLocalBounds(), juce::Justification::centred);
    }

private:
    juce::Colour onColour;
};

//==============================================================================
// Horizontal L/R bars of RMS and true peak, with stereo correlation beneath
class LevelsView final : public juce::Component
{
public:
    LevelsView()
    {
        setInterceptsMouseClicks (false, false);
    }

    void update (const Readings& r)
    {
        numChannels = r.numChannels;
        correlation += 0.25f * ((r.hasAudio ? r.correlation : 0.0f) - correlation);

        for (size_t ch = 0; ch < MeterDsp::maxChannels; ++ch)
        {
            const auto fallPerFrame = 26.0f / (float) timerHz;

            rmsDb[ch] = r.hasAudio ? r.rmsDb[ch] : juce::jmax (noReading, rmsDb[ch] - 3.0f * fallPerFrame);
            peakDb[ch] = r.peakDb[ch] >= peakDb[ch] ? r.peakDb[ch] : juce::jmax (noReading, peakDb[ch] - fallPerFrame);

            if (r.peakDb[ch] >= holdDb[ch])
            {
                holdDb[ch] = r.peakDb[ch];
                holdFrames[ch] = 2 * timerHz;
            }
            else if (holdFrames[ch] > 0)
            {
                --holdFrames[ch];
            }
            else
            {
                holdDb[ch] = juce::jmax (noReading, holdDb[ch] - fallPerFrame);
            }
        }

        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();

        if (numChannels == 2)
            drawCorrelation (g, area.removeFromBottom (20.0f));

        auto scale = area.removeFromBottom (15.0f);

        const auto gap = 4.0f;
        const auto rowHeight = (area.getHeight() - gap * (float) (numChannels - 1)) / (float) numChannels;
        const auto columnWidth = juce::jmax (74.0f, area.getWidth() * 0.19f);
        const auto valueHeight = juce::jlimit (18.0f, 40.0f, juce::jmin (rowHeight * 0.56f, columnWidth * 0.34f));

        const auto labelWidth = 16.0f;
        const auto barsLeft = area.getX() + labelWidth;
        const auto barsRight = area.getRight() - 2.0f * columnWidth;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto row = area.removeFromTop (rowHeight);
            area.removeFromTop (gap);

            g.setColour (Palette::dimText);
            g.setFont (fontOf (12.0f, true));
            g.drawText (numChannels == 1 ? "M" : (ch == 0 ? "L" : "R"), row.withWidth (labelWidth - 4.0f), juce::Justification::centredLeft);

            auto peakColumn = row.removeFromRight (columnWidth);
            auto rmsColumn = row.removeFromRight (columnWidth);

            drawBar (g, (size_t) ch, row.withLeft (barsLeft));
            drawValue (g, rmsColumn, rmsDb[(size_t) ch], "dB RMS", valueHeight);
            drawValue (g, peakColumn, holdDb[(size_t) ch], "dBTP", valueHeight);
        }

        drawScale (g, scale.withLeft (barsLeft).withRight (barsRight));
    }

private:
    static constexpr float minDb = -60.0f, maxDb = 0.0f;

    static float proportionOf (float db) noexcept
    {
        return juce::jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb));
    }

    static void drawValue (juce::Graphics& g, juce::Rectangle<float> column, float db, const juce::String& unit, float valueHeight)
    {
        const auto unitHeight = 12.0f;
        const auto block = column.withSizeKeepingCentre (column.getWidth(), valueHeight + unitHeight);

        g.setColour (Palette::text);
        g.setFont (fontOf (valueHeight, true));
        g.drawText (formatDb (db), block.withHeight (valueHeight), juce::Justification::centredRight);

        g.setColour (Palette::dimText);
        g.setFont (fontOf (10.5f));
        g.drawText (unit, block.withTrimmedTop (valueHeight), juce::Justification::centredRight);
    }

    void drawBar (juce::Graphics& g, size_t ch, juce::Rectangle<float> bar)
    {
        const auto radius = 4.0f;

        juce::ColourGradient gradient (Palette::blue, bar.getTopLeft(), Palette::red, bar.getTopRight(), false);
        gradient.addColour (proportionOf (-30.0f), Palette::teal);
        gradient.addColour (proportionOf (-14.0f), Palette::green);
        gradient.addColour (proportionOf (-6.0f), Palette::amber);
        gradient.addColour (proportionOf (-1.0f), Palette::red);

        g.setColour (Palette::panel);
        g.fillRoundedRectangle (bar, radius);

        g.setGradientFill (gradient);
        g.setOpacity (0.09f);
        g.fillRoundedRectangle (bar, radius);

        const auto fillTo = [&] (float db, float opacity)
        {
            const auto width = proportionOf (db) * bar.getWidth();

            if (width < 0.5f)
                return;

            juce::Graphics::ScopedSaveState state (g);
            g.reduceClipRegion (bar.withWidth (width).getSmallestIntegerContainer());
            g.setGradientFill (gradient);
            g.setOpacity (opacity);
            g.fillRoundedRectangle (bar, radius);
        };

        fillTo (peakDb[ch], 0.32f);
        fillTo (rmsDb[ch], 1.0f);

        if (holdDb[ch] > minDb)
        {
            g.setColour (Palette::text.withAlpha (0.9f));
            g.fillRect (bar.withX (bar.getX() + proportionOf (holdDb[ch]) * bar.getWidth() - 1.0f).withWidth (2.0f));
        }
    }

    void drawCorrelation (juce::Graphics& g, juce::Rectangle<float> strip)
    {
        g.setFont (fontOf (10.5f));
        g.setColour (Palette::dimText);
        g.drawText ("Stereo correlation", strip.removeFromLeft (98.0f), juce::Justification::centredLeft);

        g.setColour (Palette::text);
        g.setFont (fontOf (12.0f, true));
        g.drawText (juce::String (correlation, 2), strip.removeFromRight (40.0f), juce::Justification::centredRight);

        auto track = strip.reduced (16.0f, 0.0f);
        track = track.withSizeKeepingCentre (track.getWidth(), 5.0f);

        const auto centreX = track.getCentreX();
        const auto dotX = centreX + correlation * track.getWidth() / 2.0f;
        const auto colour = correlation < 0.0f ? Palette::amber : Palette::teal;

        g.setColour (Palette::panel);
        g.fillRoundedRectangle (track, 2.5f);

        g.setColour (colour.withAlpha (0.55f));
        g.fillRect (track.withLeft (juce::jmin (centreX, dotX)).withRight (juce::jmax (centreX, dotX)));

        g.setColour (Palette::dimText);
        g.fillRect (track.withX (centreX - 0.5f).withWidth (1.0f));

        g.setColour (colour);
        g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ dotX, track.getCentreY() }));

        g.setFont (fontOf (10.0f));
        g.setColour (Palette::dimText);
        g.drawText ("-1", track.withX (track.getX() - 12.0f).withWidth (10.0f).withSizeKeepingCentre (14.0f, 12.0f), juce::Justification::centred);
        g.drawText ("+1", track.withX (track.getRight() + 2.0f).withWidth (10.0f).withSizeKeepingCentre (14.0f, 12.0f), juce::Justification::centred);
    }

    static void drawScale (juce::Graphics& g, juce::Rectangle<float> area)
    {
        g.setFont (fontOf (10.0f));
        g.setColour (Palette::dimText);

        for (const auto db : { -60, -48, -36, -24, -18, -12, -6, 0 })
        {
            const auto x = area.getX() + proportionOf ((float) db) * area.getWidth();
            g.drawText (juce::String (db), juce::Rectangle<float> (x - 15.0f, area.getY(), 30.0f, area.getHeight()), juce::Justification::centred);
        }
    }

    int numChannels = 2;
    std::array<float, MeterDsp::maxChannels> rmsDb { noReading, noReading }, peakDb { noReading, noReading }, holdDb { noReading, noReading };
    std::array<int, MeterDsp::maxChannels> holdFrames { 0, 0 };
    float correlation = 0.0f;
};

//==============================================================================
// A pair of traditional needle VU meters. The scale is linear in voltage, as on the
// real thing, so 0 VU sits about 70% of the way across.
class VuDialsView final : public juce::Component
{
public:
    VuDialsView()
    {
        setInterceptsMouseClicks (false, false);
    }

    void update (const Readings& r)
    {
        numChannels = r.numChannels;

        for (size_t ch = 0; ch < MeterDsp::maxChannels; ++ch)
        {
            const auto hasValue = r.hasAudio && r.vuDb[ch] > noReading;

            vu[ch] = hasValue ? r.vuDb[ch] - r.vuReferenceDb : noReading;
            needle[ch] = hasValue ? positionOf (vu[ch]) : needle[ch] * 0.85f;
        }

        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        const auto gap = 8.0f;
        const auto readoutHeight = 22.0f;
        const auto dialWidth = (area.getWidth() - gap * (float) (numChannels - 1)) / (float) numChannels;

        // Real meters are wider than they are tall
        const auto faceHeight = juce::jmin (area.getHeight() - readoutHeight, dialWidth * 0.64f);

        area = area.withSizeKeepingCentre (area.getWidth(), faceHeight + readoutHeight);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto column = area.removeFromLeft (dialWidth);
            area.removeFromLeft (gap);

            drawDial (g, column.removeFromTop (faceHeight), (size_t) ch);
            drawReadout (g, column, (size_t) ch);
        }
    }

private:
    static constexpr float maxVu = 3.0f;
    static constexpr float sweepRadians = 0.84f;    // either side of centre

    static float positionOf (float vuValue) noexcept
    {
        return std::pow (10.0f, (vuValue - maxVu) / 20.0f);
    }

    static float angleOf (float position) noexcept
    {
        // Rests just below the bottom of the scale, and pins just above the top
        return (juce::jlimit (-0.02f, 1.04f, position) * 2.0f - 1.0f) * sweepRadians;
    }

    static juce::Point<float> pointOn (juce::Point<float> pivot, float radius, float angle) noexcept
    {
        return pivot + juce::Point<float> (std::sin (angle), -std::cos (angle)) * radius;
    }

    void drawDial (juce::Graphics& g, juce::Rectangle<float> face, size_t ch)
    {
        const auto ink = juce::Colour (0xff29241c);
        const auto scaleRed = juce::Colour (0xffb3261e);

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff8efd8), face.getTopLeft(),
                                                 juce::Colour (0xffe3d0a4), face.getBottomLeft(), false));
        g.fillRoundedRectangle (face, 6.0f);

        // The pivot sits at the bottom edge of the face, as on a real meter
        const auto radius = juce::jmin ((face.getWidth() - 40.0f) / (2.0f * std::sin (sweepRadians)),
                                        face.getHeight() - 27.0f);
        const juce::Point<float> pivot { face.getCentreX(), face.getBottom() + 2.0f };

        const auto zero = angleOf (positionOf (0.0f));

        juce::Path blackArc, redArc;
        blackArc.addCentredArc (pivot.x, pivot.y, radius, radius, 0.0f, angleOf (positionOf (-20.0f)), zero, true);
        redArc.addCentredArc (pivot.x, pivot.y, radius, radius, 0.0f, zero, angleOf (1.0f), true);

        g.setColour (ink);
        g.strokePath (blackArc, juce::PathStrokeType (1.5f));
        g.setColour (scaleRed);
        g.strokePath (redArc, juce::PathStrokeType (4.0f));

        g.setFont (fontOf (10.0f, true));

        for (const auto tick : { -20, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3 })
        {
            const auto angle = angleOf (positionOf ((float) tick));
            const auto isLabelled = tick == -20 || tick == -10 || tick == -7 || tick == -5 || tick == -3 || tick == 0 || tick == 3;
            const auto colour = tick > 0 ? scaleRed : ink;

            g.setColour (colour);
            g.drawLine ({ pointOn (pivot, radius, angle), pointOn (pivot, radius + (isLabelled ? 7.0f : 4.0f), angle) }, 1.25f);

            if (isLabelled)
                g.drawText ((tick > 0 ? "+" : "") + juce::String (tick),
                            juce::Rectangle<float> (26.0f, 12.0f).withCentre (pointOn (pivot, radius + 14.0f, angle)),
                            juce::Justification::centred);
        }

        g.setColour (ink.withAlpha (0.8f));
        g.setFont (fontOf (11.0f, true));
        g.drawText ("VU", juce::Rectangle<float> (40.0f, 14.0f).withCentre (pivot.translated (0.0f, -radius * 0.38f)), juce::Justification::centred);

        juce::Graphics::ScopedSaveState state (g);
        juce::Path clip;
        clip.addRoundedRectangle (face, 6.0f);
        g.reduceClipRegion (clip);

        g.setColour (ink);
        g.drawLine ({ pivot, pointOn (pivot, radius + 5.0f, angleOf (needle[ch])) }, 1.5f);
        g.fillEllipse (juce::Rectangle<float> (20.0f, 20.0f).withCentre (pivot));
    }

    void drawReadout (juce::Graphics& g, juce::Rectangle<float> row, size_t ch)
    {
        row.removeFromTop (3.0f);

        g.setColour (Palette::dimText);
        g.setFont (fontOf (12.0f, true));
        g.drawText (numChannels == 1 ? "M" : (ch == 0 ? "L" : "R"), row, juce::Justification::centredLeft);

        g.setFont (fontOf (10.5f));
        g.drawText ("VU", row.removeFromRight (16.0f), juce::Justification::centredRight);

        row.removeFromRight (4.0f);

        g.setColour (Palette::text);
        g.setFont (fontOf (16.0f, true));
        g.drawText (vu[ch] > -60.0f ? formatDb (vu[ch], true) : "--", row, juce::Justification::centredRight);
    }

    int numChannels = 2;
    std::array<float, MeterDsp::maxChannels> vu { noReading, noReading }, needle { 0.0f, 0.0f };
};

//==============================================================================
// Three loudness readouts, above a rolling minute of history
class LoudnessView final : public juce::Component
{
public:
    LoudnessView()
    {
        setInterceptsMouseClicks (false, false);
        momentaryHistory.fill (noReading);
        shortTermHistory.fill (noReading);
    }

    void update (const Readings& r)
    {
        readings = r;

        if (r.hasAudio && ++framesSinceHistory >= timerHz / historyPerSecond)
        {
            framesSinceHistory = 0;
            momentaryHistory[(size_t) historyIndex] = r.momentary;
            shortTermHistory[(size_t) historyIndex] = r.shortTerm;
            historyIndex = (historyIndex + 1) % historySize;
        }

        repaint();
    }

    void clearHistory()
    {
        momentaryHistory.fill (noReading);
        shortTermHistory.fill (noReading);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        auto tiles = area.removeFromTop (juce::jlimit (58.0f, 84.0f, area.getHeight() * 0.42f));
        area.removeFromTop (6.0f);

        const auto gap = 6.0f;
        const auto tileWidth = (tiles.getWidth() - 2.0f * gap) / 3.0f;

        drawTile (g, tiles.removeFromLeft (tileWidth), "Momentary", readings.hasAudio ? readings.momentary : noReading, Palette::text);
        tiles.removeFromLeft (gap);
        drawTile (g, tiles.removeFromLeft (tileWidth), "Short-term", readings.hasAudio ? readings.shortTerm : noReading, Palette::text);
        tiles.removeFromLeft (gap);
        drawTile (g, tiles, "Integrated", readings.integrated, Palette::teal);

        drawHistory (g, area);
    }

private:
    static constexpr int historyPerSecond = 10;
    static constexpr int historySize = 60 * historyPerSecond;
    static constexpr float minLufs = -60.0f, maxLufs = 0.0f;

    void drawTile (juce::Graphics& g, juce::Rectangle<float> tile, const juce::String& title, float lufs, juce::Colour colour)
    {
        g.setColour (Palette::panel);
        g.fillRoundedRectangle (tile, 8.0f);

        auto inner = tile.reduced (10.0f, 7.0f);

        g.setColour (Palette::dimText);
        g.setFont (fontOf (11.0f));
        g.drawText (title, inner.removeFromTop (14.0f), juce::Justification::centredLeft);

        const auto hasValue = lufs > noReading;

        g.setColour (hasValue ? colour : Palette::dimText);
        g.setFont (fontOf (juce::jlimit (20.0f, 36.0f, juce::jmin (inner.getHeight(), inner.getWidth() * 0.3f)), true));
        g.drawText (hasValue ? formatDb (lufs) : "--", inner, juce::Justification::centredLeft);

        g.setColour (Palette::dimText);
        g.setFont (fontOf (10.5f));
        g.drawText ("LUFS", inner, juce::Justification::bottomRight);
    }

    float yFor (float lufs, juce::Rectangle<float> plot) const noexcept
    {
        return plot.getY() + (maxLufs - juce::jlimit (minLufs, maxLufs, lufs)) / (maxLufs - minLufs) * plot.getHeight();
    }

    void drawHistory (juce::Graphics& g, juce::Rectangle<float> area)
    {
        g.setColour (Palette::panel);
        g.fillRoundedRectangle (area, 8.0f);

        const auto plot = area.reduced (10.0f, 6.0f);

        g.setFont (fontOf (10.5f));

        for (const auto lufs : { -14.0f, -23.0f, -40.0f })
        {
            const auto y = yFor (lufs, plot);

            g.setColour (Palette::grid);
            g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
            g.setColour (Palette::dimText);
            g.drawText (juce::String ((int) lufs), plot.withY (y - 13.0f).withHeight (12.0f), juce::Justification::topLeft);
        }

        const auto buildPath = [&] (const std::array<float, historySize>& history, bool closed)
        {
            juce::Path path;
            auto started = false;
            auto lastX = plot.getX();

            for (int i = 0; i < historySize; ++i)
            {
                const auto lufs = history[(size_t) ((historyIndex + i) % historySize)];

                if (lufs <= minLufs)
                    continue;

                const auto x = plot.getX() + (float) i / (float) (historySize - 1) * plot.getWidth();

                if (! started)
                {
                    started = true;
                    path.startNewSubPath (x, closed ? plot.getBottom() : yFor (lufs, plot));
                }

                path.lineTo (x, yFor (lufs, plot));
                lastX = x;
            }

            if (started && closed)
            {
                path.lineTo (lastX, plot.getBottom());
                path.closeSubPath();
            }

            return path;
        };

        g.setGradientFill (juce::ColourGradient (Palette::teal.withAlpha (0.32f), plot.getTopLeft(),
                                                 Palette::teal.withAlpha (0.02f), plot.getBottomLeft(), false));
        g.fillPath (buildPath (shortTermHistory, true));

        g.setColour (Palette::blue.withAlpha (0.55f));
        g.strokePath (buildPath (momentaryHistory, false), juce::PathStrokeType (1.0f));

        g.setColour (Palette::teal);
        g.strokePath (buildPath (shortTermHistory, false), juce::PathStrokeType (1.75f, juce::PathStrokeType::curved));

        g.setColour (Palette::dimText);
        g.drawText ("Last minute", plot.withHeight (14.0f), juce::Justification::topRight);
    }

    Readings readings;
    std::array<float, historySize> momentaryHistory {}, shortTermHistory {};
    int historyIndex = 0, framesSinceHistory = 0;
};

//==============================================================================
// Log-frequency spectrum, of both channels' combined power or of each one overlaid.
// Works on the message thread, from the node's FIFO.
class SpectrumView final : public juce::Component
{
public:
    explicit SpectrumView (MeterNode& nodeIn) : node (nodeIn)
    {
        setInterceptsMouseClicks (false, false);
        for (auto& curve : smoothedDb)
            curve.fill (floorDb);

        heldDb.fill (floorDb);
    }

    void setSplit (bool shouldSplit)
    {
        split = shouldSplit;
        repaint();
    }

    void update (bool hasAudio)
    {
        const auto hadNewSamples = readFifo();

        if (hadNewSamples && hasAudio)
            analyse();
        else
            for (auto& curve : targetDb)
                curve.fill (floorDb);

        for (size_t curve = 0; curve < targetDb.size(); ++curve)
        {
            for (size_t i = 0; i < numBins; ++i)
            {
                const auto rate = targetDb[curve][i] > smoothedDb[curve][i] ? 0.6f : 0.22f;
                smoothedDb[curve][i] += rate * (targetDb[curve][i] - smoothedDb[curve][i]);
            }
        }

        for (size_t i = 0; i < numBins; ++i)
            heldDb[i] = juce::jmax (smoothedDb[combined][i], heldDb[i] - 8.0f / (float) timerHz);

        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat();

        g.setColour (Palette::panel);
        g.fillRoundedRectangle (area, 8.0f);

        const auto plot = area.withTrimmedLeft (30.0f).withTrimmedRight (10.0f).withTrimmedTop (8.0f).withTrimmedBottom (20.0f);

        drawGrid (g, plot);

        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (plot.getSmallestIntegerContainer());

        if (split)
        {
            g.setColour (leftColour);
            g.strokePath (buildCurve (smoothedDb[0], plot), juce::PathStrokeType (1.5f, juce::PathStrokeType::curved));

            g.setColour (rightColour);
            g.strokePath (buildCurve (smoothedDb[1], plot), juce::PathStrokeType (1.5f, juce::PathStrokeType::curved));

            auto legend = plot.reduced (6.0f, 3.0f).removeFromTop (14.0f);
            g.setFont (fontOf (12.0f, true));
            g.setColour (rightColour);
            g.drawText ("R", legend.removeFromRight (12.0f), juce::Justification::centred);
            g.setColour (leftColour);
            g.drawText ("L", legend.removeFromRight (14.0f), juce::Justification::centred);
            return;
        }

        auto curve = buildCurve (smoothedDb[combined], plot);
        const auto line = curve;

        curve.lineTo (plot.getRight(), plot.getBottom());
        curve.lineTo (plot.getX(), plot.getBottom());
        curve.closeSubPath();

        juce::ColourGradient fill (Palette::teal.withAlpha (0.55f), plot.getTopLeft(),
                                   Palette::blue.withAlpha (0.04f), plot.getBottomLeft(), false);
        fill.addColour (0.55, Palette::blue.withAlpha (0.34f));

        g.setGradientFill (fill);
        g.fillPath (curve);

        g.setColour (Palette::text.withAlpha (0.28f));
        g.strokePath (buildCurve (heldDb, plot), juce::PathStrokeType (1.0f));

        g.setColour (Palette::teal);
        g.strokePath (line, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved));
    }

private:
    static constexpr int fftOrder = 12;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr size_t numBins = (size_t) fftSize / 2 + 1;
    static constexpr float floorDb = -100.0f, plotMinDb = -90.0f, plotMaxDb = 0.0f;
    static constexpr float minFrequency = 20.0f;
    static constexpr size_t combined = 2;           // after the left and right curves

    const juce::Colour leftColour = Palette::teal, rightColour = Palette::amber;

    double getSampleRate() const
    {
        const auto rate = node.getCurrentSampleRate();
        return rate > 0.0 ? rate : 48000.0;
    }

    bool readFifo()
    {
        auto gotAny = false;

        for (;;)
        {
            const auto wanted = fftSize - writeIndex;
            const auto got = node.readSpectrumSamples (history[0].data() + writeIndex, history[1].data() + writeIndex, wanted);

            gotAny = gotAny || got > 0;
            writeIndex = (writeIndex + got) % fftSize;

            if (got < wanted)
                return gotAny;
        }
    }

    void analyse()
    {
        std::array<float, numBins> totalPower {};

        // A Hann window halves the amplitude. Calibrated so a sine at 0 dBFS reads 0 dB.
        const auto scale = 4.0f / (float) fftSize;
        const auto toDisplayDb = [] (float power) { return juce::jmax (floorDb, 10.0f * std::log10 (power + 1.0e-12f)); };

        for (size_t ch = 0; ch < history.size(); ++ch)
        {
            for (int i = 0; i < fftSize; ++i)
                fftData[(size_t) i] = history[ch][(size_t) ((writeIndex + i) % fftSize)];

            window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
            juce::FloatVectorOperations::clear (fftData.data() + fftSize, fftSize);
            fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

            for (size_t i = 0; i < numBins; ++i)
            {
                const auto power = fftData[i] * fftData[i] * scale * scale;

                targetDb[ch][i] = toDisplayDb (power);
                totalPower[i] += power;
            }
        }

        // Powers add, so that anti-phase content doesn't vanish as a sum of samples would
        for (size_t i = 0; i < numBins; ++i)
            targetDb[combined][i] = toDisplayDb (totalPower[i] / 2.0f);
    }

    float maxFrequency() const
    {
        return juce::jmin (20000.0f, (float) getSampleRate() / 2.0f);
    }

    float xFor (float frequency, juce::Rectangle<float> plot) const
    {
        return plot.getX() + plot.getWidth() * std::log (frequency / minFrequency) / std::log (maxFrequency() / minFrequency);
    }

    float yFor (float db, juce::Rectangle<float> plot) const
    {
        return plot.getY() + (plotMaxDb - db) / (plotMaxDb - plotMinDb) * plot.getHeight();
    }

    // Catmull-Rom between bins, where a pixel is narrower than one
    float interpolated (const std::array<float, numBins>& bins, float position) const
    {
        const auto i = juce::jlimit (1, (int) numBins - 3, (int) position);
        const auto t = juce::jlimit (0.0f, 1.0f, position - (float) i);
        const auto p0 = bins[(size_t) i - 1], p1 = bins[(size_t) i], p2 = bins[(size_t) i + 1], p3 = bins[(size_t) i + 2];

        return p1 + 0.5f * t * (p2 - p0 + t * (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3 + t * (3.0f * (p1 - p2) + p3 - p0)));
    }

    juce::Path buildCurve (const std::array<float, numBins>& bins, juce::Rectangle<float> plot) const
    {
        const auto binHz = (float) getSampleRate() / (float) fftSize;
        const auto ratio = maxFrequency() / minFrequency;
        const auto step = 2.0f;

        juce::Path path;

        for (float x = 0.0f; x <= plot.getWidth(); x += step)
        {
            const auto binAt = [&] (float px) { return minFrequency * std::pow (ratio, px / plot.getWidth()) / binHz; };
            const auto first = binAt (x), last = binAt (x + step);

            auto db = interpolated (bins, first);

            // Where a pixel spans several bins, keep the highest so narrow peaks still show
            for (auto i = (int) std::ceil (first); i <= (int) last && i < (int) numBins; ++i)
                db = juce::jmax (db, bins[(size_t) i]);

            const auto y = yFor (juce::jlimit (plotMinDb - 4.0f, plotMaxDb, db), plot);

            if (x == 0.0f)
                path.startNewSubPath (plot.getX(), y);
            else
                path.lineTo (plot.getX() + x, y);
        }

        return path;
    }

    void drawGrid (juce::Graphics& g, juce::Rectangle<float> plot)
    {
        g.setFont (fontOf (10.5f));

        for (auto db = plotMinDb; db <= plotMaxDb; db += 15.0f)
        {
            const auto y = yFor (db, plot);

            g.setColour (Palette::grid);
            g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());

            if (db > plotMinDb)
            {
                g.setColour (Palette::dimText);
                g.drawText (juce::String ((int) db), juce::Rectangle<float> (0.0f, y - 7.0f, plot.getX() - 6.0f, 14.0f), juce::Justification::centredRight);
            }
        }

        for (const auto frequency : { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f })
        {
            if (frequency > maxFrequency())
                continue;

            const auto x = xFor (frequency, plot);

            g.setColour (Palette::grid);
            g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());

            g.setColour (Palette::dimText);
            g.drawText (frequency >= 1000.0f ? juce::String ((int) (frequency / 1000.0f)) + "k" : juce::String ((int) frequency),
                        juce::Rectangle<float> (x - 20.0f, plot.getBottom() + 4.0f, 40.0f, 14.0f), juce::Justification::centred);
        }
    }

    MeterNode& node;
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann, false };

    std::array<std::array<float, (size_t) fftSize>, 2> history {};
    std::array<float, 2 * (size_t) fftSize> fftData {};
    std::array<std::array<float, numBins>, 3> targetDb {}, smoothedDb {};
    std::array<float, numBins> heldDb {};
    int writeIndex = 0;
    bool split = false;
};

//==============================================================================
class MeterEditor final : public juce::AudioProcessorEditor,
                          private juce::Timer
{
public:
    explicit MeterEditor (MeterNode& owner)
        : AudioProcessorEditor (owner),
          meterNode (owner),
          dsp (owner.getDsp())
    {
        const auto addMode = [this] (PillButton& button, MeterNode::Mode mode)
        {
            button.setClickingTogglesState (true);
            button.setRadioGroupId (1);
            button.onClick = [this, &button, mode]
            {
                if (button.getToggleState())
                    showMode (mode);
            };
            addAndMakeVisible (button);
        };

        addMode (levelsButton, MeterNode::Mode::levels);
        addMode (vuButton, MeterNode::Mode::vu);
        addMode (loudnessButton, MeterNode::Mode::loudness);
        addMode (spectrumButton, MeterNode::Mode::spectrum);

        clipButton.onClick = [this]
        {
            dsp.clearOvers();
            clipButton.setToggleState (false, juce::dontSendNotification);
        };
        addAndMakeVisible (clipButton);

        vuReferenceButton.setTooltip ("The level of sine that reads 0 VU");
        vuReferenceButton.onClick = [this] { showVuReferenceMenu(); };
        addChildComponent (vuReferenceButton);

        splitButton.setTooltip ("Show the left and right channels separately");
        splitButton.setClickingTogglesState (true);
        splitButton.setToggleState (meterNode.getSpectrumSplit(), juce::dontSendNotification);
        splitButton.onClick = [this]
        {
            meterNode.setSpectrumSplit (splitButton.getToggleState());
            spectrumView.setSplit (splitButton.getToggleState());
        };
        addChildComponent (splitButton);
        spectrumView.setSplit (meterNode.getSpectrumSplit());

        resetButton.setTooltip ("Restart loudness measurement");
        resetButton.onClick = [this]
        {
            dsp.requestLoudnessReset();
            loudnessView.clearHistory();
        };
        addChildComponent (resetButton);

        updateVuReferenceText();

        addChildComponent (levelsView);
        addChildComponent (vuView);
        addChildComponent (loudnessView);
        addChildComponent (spectrumView);

        dsp.setMeasuring (true);
        lastBlocksProcessed = dsp.getBlocksProcessed();

        // Setting the limits resizes the editor, which updates the saved size
        const auto size = meterNode.getEditorSize();

        setResizable (true, false);
        setResizeLimits (390, 200, 1800, 1100);
        setSize (size.x, size.y);

        buttonFor (meterNode.getMode()).setToggleState (true, juce::dontSendNotification);
        showMode (meterNode.getMode());

        startTimerHz (timerHz);
    }

    ~MeterEditor() override
    {
        stopTimer();
        meterNode.setSpectrumWanted (false);
        dsp.setMeasuring (false);
        meterNode.editorBeingDeleted (this);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Palette::background);
    }

    // Over the views, so that it costs no space when audio is flowing
    void paintOverChildren (juce::Graphics& g) override
    {
        if (hasAudio)
            return;

        const auto badge = getContentBounds().toFloat().withSizeKeepingCentre (84.0f, 24.0f);

        g.setColour (Palette::background.withAlpha (0.88f));
        g.fillRoundedRectangle (badge, 12.0f);
        g.setColour (Palette::amber);
        g.setFont (fontOf (12.0f, true));
        g.drawText ("No audio", badge, juce::Justification::centred);
    }

    void resized() override
    {
        auto header = getLocalBounds().reduced (10, 0).withTrimmedTop (10).removeFromTop (24);

        const auto place = [&header] (PillButton& button, int width)
        {
            button.setBounds (header.removeFromLeft (width));
            header.removeFromLeft (5);
        };

        place (levelsButton, 54);
        place (vuButton, 34);
        place (loudnessButton, 68);
        place (spectrumButton, 68);

        clipButton.setBounds (header.removeFromRight (42));
        header.removeFromRight (5);

        const auto optionWidth = header.removeFromRight (70);

        for (auto* button : { &resetButton, &vuReferenceButton, &splitButton })
            button->setBounds (button == &vuReferenceButton ? optionWidth : optionWidth.withLeft (optionWidth.getRight() - 46));

        const auto content = getContentBounds();

        for (juce::Component* view : { static_cast<juce::Component*> (&levelsView), static_cast<juce::Component*> (&vuView),
                                       static_cast<juce::Component*> (&loudnessView), static_cast<juce::Component*> (&spectrumView) })
            view->setBounds (content);

        meterNode.setEditorSize ({ getWidth(), getHeight() });
    }

private:
    juce::Rectangle<int> getContentBounds() const
    {
        return getLocalBounds().reduced (10, 0).withTrimmedTop (42).withTrimmedBottom (10);
    }

    PillButton& buttonFor (MeterNode::Mode mode)
    {
        switch (mode)
        {
            case MeterNode::Mode::vu:       return vuButton;
            case MeterNode::Mode::loudness: return loudnessButton;
            case MeterNode::Mode::spectrum: return spectrumButton;
            case MeterNode::Mode::levels:   break;
        }

        return levelsButton;
    }

    void updateVuReferenceText()
    {
        vuReferenceButton.setButtonText ("0 VU = " + juce::String (meterNode.getVuReferenceDb()));
    }

    void showVuReferenceMenu()
    {
        juce::PopupMenu menu;

        for (const auto reference : MeterNode::vuReferences)
            menu.addItem (juce::String (reference) + " dBFS", true, reference == meterNode.getVuReferenceDb(),
                          [this, reference]
                          {
                              meterNode.setVuReferenceDb (reference);
                              updateVuReferenceText();
                          });

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (vuReferenceButton));
    }

    void showMode (MeterNode::Mode mode)
    {
        meterNode.setMode (mode);

        levelsView.setVisible (mode == MeterNode::Mode::levels);
        vuView.setVisible (mode == MeterNode::Mode::vu);
        loudnessView.setVisible (mode == MeterNode::Mode::loudness);
        spectrumView.setVisible (mode == MeterNode::Mode::spectrum);
        resetButton.setVisible (mode == MeterNode::Mode::loudness);
        vuReferenceButton.setVisible (mode == MeterNode::Mode::vu);
        splitButton.setVisible (mode == MeterNode::Mode::spectrum && meterNode.getMainBusNumInputChannels() > 1);

        meterNode.setSpectrumWanted (mode == MeterNode::Mode::spectrum);
    }

    Readings takeReadings()
    {
        Readings r;
        r.hasAudio = hasAudio;
        r.numChannels = juce::jlimit (1, MeterDsp::maxChannels, meterNode.getMainBusNumInputChannels());
        r.vuReferenceDb = (float) meterNode.getVuReferenceDb();

        for (int ch = 0; ch < r.numChannels; ++ch)
        {
            const auto i = (size_t) ch;
            r.peakDb[i] = toDb (dsp.consumeTruePeak (ch));

            if (hasAudio)
            {
                r.rmsDb[i] = toDb (dsp.getRms (ch));
                r.vuDb[i] = toDb (dsp.getVu (ch));
            }
        }

        r.correlation = dsp.getCorrelation();

        const auto toReading = [] (float lufs) { return lufs <= MeterDsp::noLoudness ? noReading : lufs; };

        r.momentary = toReading (dsp.getMomentaryLufs());
        r.shortTerm = toReading (dsp.getShortTermLufs());
        r.integrated = toReading (dsp.getIntegratedLufs());
        return r;
    }

    void updateHasAudio()
    {
        const auto blocks = dsp.getBlocksProcessed();

        framesWithoutAudio = blocks != lastBlocksProcessed ? 0 : framesWithoutAudio + 1;
        lastBlocksProcessed = blocks;

        const auto nowHasAudio = framesWithoutAudio < timerHz / 2;

        if (hasAudio != nowHasAudio)
        {
            hasAudio = nowHasAudio;
            repaint();
        }
    }

    void timerCallback() override
    {
        updateHasAudio();

        const auto readings = takeReadings();

        switch (meterNode.getMode())
        {
            case MeterNode::Mode::levels:   levelsView.update (readings); break;
            case MeterNode::Mode::vu:       vuView.update (readings); break;
            case MeterNode::Mode::loudness: loudnessView.update (readings); break;
            case MeterNode::Mode::spectrum: spectrumView.update (hasAudio); break;
        }

        const auto over = dsp.hasOver (0) || dsp.hasOver (1);

        if (over != clipButton.getToggleState())
            clipButton.setToggleState (over, juce::dontSendNotification);
    }

    MeterNode& meterNode;
    MeterDsp& dsp;

    PillButton levelsButton { "Levels" }, vuButton { "VU" }, loudnessButton { "Loudness" }, spectrumButton { "Spectrum" };
    PillButton resetButton { "Reset" }, vuReferenceButton { "0 VU" }, splitButton { "L/R" }, clipButton { "Clip", Palette::red };

    LevelsView levelsView;
    VuDialsView vuView;
    LoudnessView loudnessView;
    SpectrumView spectrumView { meterNode };

    juce::TooltipWindow tooltipWindow { this };

    bool hasAudio = false;
    int framesWithoutAudio = timerHz;
    std::uint32_t lastBlocksProcessed = 0;
};
}

//==============================================================================
MeterNode::MeterNode()
    : AudioPluginInstance (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                           .withOutput ("Output", juce::AudioChannelSet::stereo(), false))
{
}

void MeterNode::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    dsp.prepare (sampleRate);
    currentSampleRate.store (sampleRate, std::memory_order_relaxed);
}

void MeterNode::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (isSuspended())
        return;

    const int numChannels = juce::jmin (getTotalNumInputChannels(), buffer.getNumChannels(), MeterDsp::maxChannels);
    const int numSamples = buffer.getNumSamples();

    if (numSamples == 0 || numChannels == 0)
        return;

    const auto* const* data = buffer.getArrayOfReadPointers();

    dsp.process (data, numChannels, numSamples);

    if (spectrumWanted.load (std::memory_order_relaxed))
        pushSpectrumSamples (data[0], data[numChannels - 1], numSamples);
}

// If the window has stalled and the FIFO fills, drop samples: it's only a display
void MeterNode::pushSpectrumSamples (const float* left, const float* right, int numSamples) noexcept
{
    const auto scope = spectrumFifo.write (juce::jmin (numSamples, spectrumFifo.getFreeSpace()));

    const auto copy = [&] (int start, int count, int offset)
    {
        if (count > 0)
        {
            spectrumSamples.copyFrom (0, start, left + offset, count);
            spectrumSamples.copyFrom (1, start, right + offset, count);
        }
    };

    copy (scope.startIndex1, scope.blockSize1, 0);
    copy (scope.startIndex2, scope.blockSize2, scope.blockSize1);
}

int MeterNode::readSpectrumSamples (float* left, float* right, int maxSamples) noexcept
{
    const auto scope = spectrumFifo.read (juce::jmin (maxSamples, spectrumFifo.getNumReady()));

    const auto copy = [&] (int start, int count, int offset)
    {
        if (count > 0)
        {
            juce::FloatVectorOperations::copy (left + offset, spectrumSamples.getReadPointer (0, start), count);
            juce::FloatVectorOperations::copy (right + offset, spectrumSamples.getReadPointer (1, start), count);
        }
    };

    copy (scope.startIndex1, scope.blockSize1, 0);
    copy (scope.startIndex2, scope.blockSize2, scope.blockSize1);

    return scope.blockSize1 + scope.blockSize2;
}

juce::AudioProcessorEditor* MeterNode::createEditor()
{
    return new MeterEditor (*this);
}

bool MeterNode::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto inSet  = layouts.getMainInputChannelSet();
    const auto outSet = layouts.getMainOutputChannelSet();

    return (inSet.size() == 1 || inSet.size() == 2) && (outSet.isDisabled() || outSet == inSet);
}

void MeterNode::fillInPluginDescription (juce::PluginDescription& description) const
{
    description.name = getName();
    description.descriptiveName = getName();
    description.pluginFormatName = "Internal";
    description.category = "Plugins";
    description.fileOrIdentifier = "Meter";
    description.uniqueId = 0x4D455452; // "METR"
    description.isInstrument = false;
    description.numInputChannels = getMainBusNumInputChannels();
    description.numOutputChannels = getMainBusNumOutputChannels();
}

void MeterNode::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement xml ("METER_SETTINGS");
    xml.setAttribute ("mode", (int) mode);
    xml.setAttribute ("vuReferenceDb", vuReferenceDb);
    xml.setAttribute ("spectrumSplit", spectrumSplit);
    xml.setAttribute ("width", editorSize.x);
    xml.setAttribute ("height", editorSize.y);
    copyXmlToBinary (xml, destData);
}

void MeterNode::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName ("METER_SETTINGS"))
        {
            mode = (Mode) juce::jlimit (0, (int) Mode::spectrum, xml->getIntAttribute ("mode"));
            vuReferenceDb = xml->getIntAttribute ("vuReferenceDb", vuReferenceDb);
            spectrumSplit = xml->getBoolAttribute ("spectrumSplit");
            editorSize = { xml->getIntAttribute ("width", editorSize.x), xml->getIntAttribute ("height", editorSize.y) };
        }
    }
}
