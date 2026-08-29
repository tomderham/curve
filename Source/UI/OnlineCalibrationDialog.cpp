/*
  ==============================================================================

    Curve - Online Calibration Dialog Implementation

  ==============================================================================
*/

#include "OnlineCalibrationDialog.h"
#include <cmath>
#include <complex>

//==============================================================================
// OnlineCalibrationCurveComponent
//==============================================================================
OnlineCalibrationCurveComponent::OnlineCalibrationCurveComponent()
{
    setOpaque (true);
}

OnlineCalibrationCurveComponent::~OnlineCalibrationCurveComponent() = default;

void OnlineCalibrationCurveComponent::setProfile (const std::optional<CalibrationProfile>& newProfile)
{
    currentProfile = newProfile;
    updateCurve();
    repaint();
}

static double evaluateBandGain (const CalibrationFilterBand& band, double f, double fs = 48000.0)
{
    if (! band.enabled || (band.type == CalibrationFilterType::peaking && std::abs (band.gainDb) < 0.001))
        return 0.0;

    const double f0 = juce::jlimit (10.0, fs * 0.49, band.frequencyHz);
    const double w0 = 2.0 * juce::MathConstants<double>::pi * f0 / fs;
    const double cosW0 = std::cos (w0);
    const double sinW0 = std::sin (w0);
    const double A = std::pow (10.0, band.gainDb / 40.0);
    const double Q = juce::jmax (0.05, band.qFactor);
    const double alpha = sinW0 / (2.0 * Q);

    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a0 = 1.0, a1 = 0.0, a2 = 0.0;

    switch (band.type)
    {
        case CalibrationFilterType::peaking:
        {
            b0 = 1.0 + alpha * A;
            b1 = -2.0 * cosW0;
            b2 = 1.0 - alpha * A;
            a0 = 1.0 + alpha / A;
            a1 = -2.0 * cosW0;
            a2 = 1.0 - alpha / A;
            break;
        }
        case CalibrationFilterType::lowShelf:
        {
            double beta = std::sqrt (A) / Q * sinW0;
            b0 = A * ((A + 1.0) - (A - 1.0) * cosW0 + beta);
            b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cosW0);
            b2 = A * ((A + 1.0) - (A - 1.0) * cosW0 - beta);
            a0 = (A + 1.0) + (A - 1.0) * cosW0 + beta;
            a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cosW0);
            a2 = (A + 1.0) + (A - 1.0) * cosW0 - beta;
            break;
        }
        case CalibrationFilterType::highShelf:
        {
            double beta = std::sqrt (A) / Q * sinW0;
            b0 = A * ((A + 1.0) + (A - 1.0) * cosW0 + beta);
            b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cosW0);
            b2 = A * ((A + 1.0) + (A - 1.0) * cosW0 - beta);
            a0 = (A + 1.0) - (A - 1.0) * cosW0 + beta;
            a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cosW0);
            a2 = (A + 1.0) - (A - 1.0) * cosW0 - beta;
            break;
        }
        case CalibrationFilterType::lowPass:
        {
            b0 = (1.0 - cosW0) / 2.0;
            b1 = 1.0 - cosW0;
            b2 = (1.0 - cosW0) / 2.0;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cosW0;
            a2 = 1.0 - alpha;
            break;
        }
        case CalibrationFilterType::highPass:
        {
            b0 = (1.0 + cosW0) / 2.0;
            b1 = -(1.0 + cosW0);
            b2 = (1.0 + cosW0) / 2.0;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cosW0;
            a2 = 1.0 - alpha;
            break;
        }
        case CalibrationFilterType::bandPass:
        {
            b0 = alpha;
            b1 = 0.0;
            b2 = -alpha;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cosW0;
            a2 = 1.0 - alpha;
            break;
        }
        case CalibrationFilterType::notch:
        {
            b0 = 1.0;
            b1 = -2.0 * cosW0;
            b2 = 1.0;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cosW0;
            a2 = 1.0 - alpha;
            break;
        }
        case CalibrationFilterType::unknown:
        default:
            return 0.0;
    }

    const double w = 2.0 * juce::MathConstants<double>::pi * f / fs;
    const std::complex<double> z_1 (std::cos (w), -std::sin (w));
    const std::complex<double> z_2 (std::cos (2.0 * w), -std::sin (2.0 * w));

    const std::complex<double> num = b0 + b1 * z_1 + b2 * z_2;
    const std::complex<double> den = a0 + a1 * z_1 + a2 * z_2;

    const double magSquared = std::norm (num) / juce::jmax (1e-15, std::norm (den));
    return 10.0 * std::log10 (juce::jmax (1e-12, magSquared));
}

double OnlineCalibrationCurveComponent::evaluateMagnitudeDb (const CalibrationProfile& profile, double frequencyHz)
{
    double totalDb = 0.0;

    for (const auto& band : profile.bands)
        totalDb += evaluateBandGain (band, frequencyHz);

    return totalDb;
}

void OnlineCalibrationCurveComponent::updateCurve()
{
    curvePath.clear();

    if (! currentProfile.has_value() || currentProfile->bands.empty())
        return;

    const auto bounds = getLocalBounds().toFloat().reduced (8.0f);
    if (bounds.getWidth() <= 0 || bounds.getHeight() <= 0)
        return;

    const float minFreq = 20.0f;
    const float maxFreq = 20000.0f;
    const float minDb   = -18.0f;
    const float maxDb   =  18.0f;

    auto freqToX = [&] (float freq) -> float
    {
        float logMin = std::log10 (minFreq);
        float logMax = std::log10 (maxFreq);
        float logF   = std::log10 (juce::jlimit (minFreq, maxFreq, freq));
        return bounds.getX() + bounds.getWidth() * ((logF - logMin) / (logMax - logMin));
    };

    auto dbToY = [&] (float db) -> float
    {
        float norm = (db - minDb) / (maxDb - minDb);
        norm = juce::jlimit (0.0f, 1.0f, norm);
        return bounds.getBottom() - (norm * bounds.getHeight());
    };

    const int numPoints = (int) bounds.getWidth();
    bool first = true;

    for (int i = 0; i <= numPoints; ++i)
    {
        float prop = (float) i / (float) numPoints;
        float logMin = std::log10 (minFreq);
        float logMax = std::log10 (maxFreq);
        float freq = std::pow (10.0f, logMin + prop * (logMax - logMin));

        double db = evaluateMagnitudeDb (*currentProfile, (double) freq);
        float x = freqToX (freq);
        float y = dbToY ((float) db);

        if (first)
        {
            curvePath.startNewSubPath (x, y);
            first = false;
        }
        else
        {
            curvePath.lineTo (x, y);
        }
    }
}

void OnlineCalibrationCurveComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Background
    g.fillAll (juce::Colour (0xff141414));
    g.setColour (juce::Colour (0xff303030));
    g.drawRect (bounds, 1.0f);

    auto inner = bounds.reduced (8.0f);

    // Grid lines (frequencies: 100Hz, 1kHz, 10kHz)
    const float freqs[] = { 100.0f, 1000.0f, 10000.0f };
    const char* freqLabels[] = { "100 Hz", "1 kHz", "10 kHz" };

    float logMin = std::log10 (20.0f);
    float logMax = std::log10 (20000.0f);

    g.setFont (juce::FontOptions (10.0f));

    for (int i = 0; i < 3; ++i)
    {
        float f = freqs[i];
        float logF = std::log10 (f);
        float x = inner.getX() + inner.getWidth() * ((logF - logMin) / (logMax - logMin));

        g.setColour (juce::Colour (0xff252525));
        g.drawVerticalLine ((int) x, inner.getY(), inner.getBottom());

        g.setColour (juce::Colour (0xff666666));
        g.drawText (freqLabels[i], (int) x - 25, (int) inner.getBottom() - 14, 50, 12, juce::Justification::centred);
    }

    // Grid lines (dB: +12dB, +6dB, 0dB, -6dB, -12dB)
    const float dbs[] = { 12.0f, 6.0f, 0.0f, -6.0f, -12.0f };
    const float minDb = -18.0f;
    const float maxDb =  18.0f;

    for (float db : dbs)
    {
        float norm = (db - minDb) / (maxDb - minDb);
        float y = inner.getBottom() - (norm * inner.getHeight());

        g.setColour (db == 0.0f ? juce::Colour (0xff404040) : juce::Colour (0xff202020));
        g.drawHorizontalLine ((int) y, inner.getX(), inner.getRight());

        g.setColour (db == 0.0f ? juce::Colour (0xff999999) : juce::Colour (0xff555555));
        juce::String label = (db > 0.0f ? "+" : "") + juce::String ((int) db) + " dB";
        g.drawText (label, (int) inner.getX() + 2, (int) y - 6, 45, 12, juce::Justification::left);
    }

    // Draw curve
    if (! curvePath.isEmpty())
    {
        // Zero dB Y coordinate
        float zeroY = inner.getBottom() - ((0.0f - minDb) / (maxDb - minDb) * inner.getHeight());

        // Translucent gradient under the curve to zero line
        auto fillPath = curvePath;
        fillPath.lineTo (inner.getRight(), zeroY);
        fillPath.lineTo (inner.getX(), zeroY);
        fillPath.closeSubPath();

        juce::ColourGradient grad (juce::Colour (0x284a90e2), 0.0f, inner.getY(),
                                   juce::Colour (0x084a90e2), 0.0f, inner.getBottom(), false);
        g.setGradientFill (grad);
        g.fillPath (fillPath);

        // Stroke
        g.setColour (juce::Colour (0xff4a90e2));
        g.strokePath (curvePath, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (currentProfile.has_value())
        {
            juce::String infoText;
            if (std::abs (currentProfile->preampGainDb) > 0.01)
                infoText << "Preamp: " << juce::String (currentProfile->preampGainDb, 1) << " dB  |  ";
            if (currentProfile->target.isNotEmpty())
                infoText << "Target: " << currentProfile->target;

            g.setColour (juce::Colour (0xff80a0c0));
            g.setFont (juce::FontOptions (10.5f));
            g.drawText (infoText, (int) inner.getRight() - 280, (int) inner.getY() + 4, 276, 14, juce::Justification::topRight);
        }
    }
    else if (! currentProfile.has_value())
    {
        g.setColour (juce::Colour (0xff888888));
        g.setFont (juce::FontOptions (12.0f));
        g.drawText ("Select a headphone model to view response curve", bounds, juce::Justification::centred);
    }
}

void OnlineCalibrationCurveComponent::resized()
{
    updateCurve();
}

//==============================================================================
// OnlineCalibrationDialog
//==============================================================================
OnlineCalibrationDialog::OnlineCalibrationDialog (Mode dialogMode)
    : mode (dialogMode)
{
    setOpaque (true);

    titleLabel.setText ("AutoEQ Headphone Profiles", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText (mode == Mode::createNewNode
                                ? "Select a headphone model from the AutoEQ database to create a Parametric EQ (AUNBandEQ) node."
                                : "Select a headphone model from the AutoEQ database to configure the selected Parametric EQ node.",
                           juce::dontSendNotification);
    subtitleLabel.setFont (juce::FontOptions (12.0f));
    subtitleLabel.setColour (juce::Label::textColourId, juce::Colour (0xffa0a0a0));
    addAndMakeVisible (subtitleLabel);

    refreshButton.setButtonText ("Sync Index");
    refreshButton.setTooltip ("Checks GitHub for newly added headphone models in AutoEQ");
    refreshButton.onClick = [this]
    {
        statusLabel.setText ("Syncing online databases...", juce::dontSendNotification);
        refreshButton.setEnabled (false);

        juce::Component::SafePointer<OnlineCalibrationDialog> safeThis (this);
        AutoEQDataManager::getInstance().refreshIndexAsync (true, [safeThis] (bool /*success*/, const juce::String& msg)
        {
            if (safeThis == nullptr)
                return;

            safeThis->refreshButton.setEnabled (true);
            safeThis->statusLabel.setText (msg, juce::dontSendNotification);
            safeThis->refreshSourceDropdown();
            safeThis->applyFilter();
        });
    };
    addAndMakeVisible (refreshButton);

    statusLabel.setFont (juce::FontOptions (11.0f));
    statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xff4cd964)); // Pleasant green status
    addAndMakeVisible (statusLabel);

    searchLabel.setText ("Search:", juce::dontSendNotification);
    searchLabel.setFont (juce::FontOptions (13.0f));
    addAndMakeVisible (searchLabel);

    searchEditor.setTextToShowWhenEmpty ("e.g. HD 650, WH-1000XM4, AirPods...", juce::Colour (0xff777777));
    searchEditor.addListener (this);
    addAndMakeVisible (searchEditor);

    sourceFilterCombo.addItem ("All Sources", 1);
    sourceFilterCombo.onChange = [this] { applyFilter(); };
    addAndMakeVisible (sourceFilterCombo);

    resultListBox.setModel (this);
    resultListBox.setRowHeight (28);
    resultListBox.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff1e1e1e));
    resultListBox.setColour (juce::ListBox::outlineColourId, juce::Colour (0xff383838));
    addAndMakeVisible (resultListBox);

    // Right Details
    selectedModelLabel.setText ("No headphone selected", juce::dontSendNotification);
    selectedModelLabel.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    selectedModelLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (selectedModelLabel);

    addAndMakeVisible (curveVisualizer);

    filterSummaryEditor.setMultiLine (true);
    filterSummaryEditor.setReadOnly (true);
    filterSummaryEditor.setCaretVisible (false);
    filterSummaryEditor.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff181818));
    filterSummaryEditor.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff303030));
    filterSummaryEditor.setColour (juce::TextEditor::textColourId, juce::Colour (0xffcccccc));
    filterSummaryEditor.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));
    addAndMakeVisible (filterSummaryEditor);

    cancelButton.onClick = [this] { closeDialog(); };
    addAndMakeVisible (cancelButton);

    actionButton.setButtonText ("Use this EQ");
    actionButton.setEnabled (false);
    actionButton.onClick = [this] { confirmSelection(); };
    addAndMakeVisible (actionButton);

    // Footer informational hints (3 separate lines)
    footerCustomLabel.setText ("For further customization, visit ", juce::dontSendNotification);
    footerCustomLabel.setFont (juce::FontOptions (11.0f));
    footerCustomLabel.setColour (juce::Label::textColourId, juce::Colour (0xff707070));
    footerCustomLabel.setBorderSize (juce::BorderSize<int> (0));
    footerCustomLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (footerCustomLabel);

    autoEqLinkButton.setButtonText ("autoeq.app");
    autoEqLinkButton.setURL (juce::URL ("https://autoeq.app"));
    autoEqLinkButton.setTooltip ("Open AutoEQ in browser to fine-tune target curves and sound signature, and export EQ curves");
    autoEqLinkButton.setFont (juce::FontOptions (11.0f, juce::Font::underlined), false);
    autoEqLinkButton.setColour (juce::HyperlinkButton::textColourId, juce::Colour (0xff4da6ff));
    autoEqLinkButton.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (autoEqLinkButton);

    footerCustomDotLabel.setText (".", juce::dontSendNotification);
    footerCustomDotLabel.setFont (juce::FontOptions (11.0f));
    footerCustomDotLabel.setColour (juce::Label::textColourId, juce::Colour (0xff707070));
    footerCustomDotLabel.setBorderSize (juce::BorderSize<int> (0));
    footerCustomDotLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (footerCustomDotLabel);

    footerMissingLabel.setText ("If your headphone is not in the list, try ", juce::dontSendNotification);
    footerMissingLabel.setFont (juce::FontOptions (11.0f));
    footerMissingLabel.setColour (juce::Label::textColourId, juce::Colour (0xff707070));
    footerMissingLabel.setBorderSize (juce::BorderSize<int> (0));
    footerMissingLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (footerMissingLabel);

    squigLinkButton.setButtonText ("squig.link");
    squigLinkButton.setURL (juce::URL ("https://squig.link"));
    squigLinkButton.setTooltip ("Browse crowd-sourced headphone and IEM measurements, and export EQ curves");
    squigLinkButton.setFont (juce::FontOptions (11.0f, juce::Font::underlined), false);
    squigLinkButton.setColour (juce::HyperlinkButton::textColourId, juce::Colour (0xff4da6ff));
    squigLinkButton.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (squigLinkButton);

    footerMissingDotLabel.setText (".", juce::dontSendNotification);
    footerMissingDotLabel.setFont (juce::FontOptions (11.0f));
    footerMissingDotLabel.setColour (juce::Label::textColourId, juce::Colour (0xff707070));
    footerMissingDotLabel.setBorderSize (juce::BorderSize<int> (0));
    footerMissingDotLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (footerMissingDotLabel);

    footerImportLabel.setText ("You can import EQ curves from AutoEQ, Squig.link, or REW using 'Parametric EQ (import from file...)'", juce::dontSendNotification);
    footerImportLabel.setFont (juce::FontOptions (11.0f));
    footerImportLabel.setColour (juce::Label::textColourId, juce::Colour (0xff707070));
    footerImportLabel.setBorderSize (juce::BorderSize<int> (0));
    footerImportLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (footerImportLabel);

    AutoEQDataManager::getInstance().addChangeListener (this);

    refreshSourceDropdown();
    applyFilter();

    // Check if initial index needs download
    if (! AutoEQDataManager::getInstance().hasLocalIndex() || AutoEQDataManager::getInstance().getEntries().empty())
    {
        statusLabel.setText ("Initializing online database...", juce::dontSendNotification);
        juce::Component::SafePointer<OnlineCalibrationDialog> safeThis (this);
        AutoEQDataManager::getInstance().refreshIndexAsync (true, [safeThis] (bool /*success*/, const juce::String& msg)
        {
            if (safeThis == nullptr)
                return;

            safeThis->statusLabel.setText (msg, juce::dontSendNotification);
            safeThis->refreshSourceDropdown();
            safeThis->applyFilter();
        });
    }
}

OnlineCalibrationDialog::~OnlineCalibrationDialog()
{
    notifyCompletion (false);
    AutoEQDataManager::getInstance().removeChangeListener (this);
    resultListBox.setModel (nullptr);
}

void OnlineCalibrationDialog::parentHierarchyChanged()
{
    searchEditor.grabKeyboardFocus();
}

void OnlineCalibrationDialog::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff252525));
}

void OnlineCalibrationDialog::resized()
{
    auto bounds = getLocalBounds().reduced (16);

    // Header
    auto headerArea = bounds.removeFromTop (48);
    titleLabel.setBounds (headerArea.removeFromTop (24));
    subtitleLabel.setBounds (headerArea.removeFromTop (20));

    // Bottom area (70px high for status + gap + 3 distinct sentence rows)
    auto bottomArea = bounds.removeFromBottom (70);

    // Right side action buttons: [Cancel]  [Use this EQ]
    auto buttonsArea = bottomArea.removeFromRight (210);
    buttonsArea = buttonsArea.withSizeKeepingCentre (buttonsArea.getWidth(), 30);
    cancelButton.setBounds (buttonsArea.removeFromLeft (85));
    buttonsArea.removeFromLeft (10);
    actionButton.setBounds (buttonsArea.removeFromLeft (115));

    // Left side: Status on top line, blank gap, then the 3 sentences
    auto infoArea = bottomArea;
    statusLabel.setBounds (infoArea.removeFromTop (16));

    infoArea.removeFromTop (6); // Space between status line and sentences

    auto font11 = juce::Font (juce::FontOptions (11.0f));
    int dotW = juce::GlyphArrangement::getStringWidthInt (font11, ".");

    // Sentence 1: "For further customization, visit autoeq.app."
    auto line1Area = infoArea.removeFromTop (16);
    int customW = juce::GlyphArrangement::getStringWidthInt (font11, footerCustomLabel.getText());
    int autoEqW = juce::GlyphArrangement::getStringWidthInt (font11, autoEqLinkButton.getButtonText()) + 4;
    footerCustomLabel.setBounds (line1Area.removeFromLeft (customW));
    autoEqLinkButton.setBounds (line1Area.removeFromLeft (autoEqW));
    footerCustomDotLabel.setBounds (line1Area.removeFromLeft (dotW));

    // Sentence 2: "If your headphone is not in the list, try squig.link."
    auto line2Area = infoArea.removeFromTop (16);
    int missingW = juce::GlyphArrangement::getStringWidthInt (font11, footerMissingLabel.getText());
    int squigW   = juce::GlyphArrangement::getStringWidthInt (font11, squigLinkButton.getButtonText()) + 4;
    footerMissingLabel.setBounds (line2Area.removeFromLeft (missingW));
    squigLinkButton.setBounds (line2Area.removeFromLeft (squigW));
    footerMissingDotLabel.setBounds (line2Area.removeFromLeft (dotW));

    // Sentence 3: "You can import EQ curves..."
    footerImportLabel.setBounds (infoArea.removeFromTop (16));

    bounds.removeFromBottom (10);

    // Filter toolbar
    auto filterArea = bounds.removeFromTop (32);
    refreshButton.setBounds (filterArea.removeFromRight (90));
    filterArea.removeFromRight (10);
    sourceFilterCombo.setBounds (filterArea.removeFromRight (140));
    filterArea.removeFromRight (10);
    searchLabel.setBounds (filterArea.removeFromLeft (50));
    searchEditor.setBounds (filterArea);

    bounds.removeFromTop (10);

    // Split left (list) and right (details)
    const int listWidth = 350;
    auto leftArea = bounds.removeFromLeft (listWidth);
    bounds.removeFromLeft (16);
    auto rightArea = bounds;

    resultListBox.setBounds (leftArea);

    // Right details pane layout
    selectedModelLabel.setBounds (rightArea.removeFromTop (24));

    rightArea.removeFromTop (8);
    curveVisualizer.setBounds (rightArea.removeFromTop (160));
    rightArea.removeFromTop (8);
    filterSummaryEditor.setBounds (rightArea);
}

void OnlineCalibrationDialog::refreshSourceDropdown()
{
    auto currentSelection = sourceFilterCombo.getText();
    sourceFilterCombo.clear (juce::dontSendNotification);
    sourceFilterCombo.addItem ("All Sources", 1);

    auto sources = AutoEQDataManager::getInstance().getAvailableSources();
    for (int i = 0; i < sources.size(); ++i)
        sourceFilterCombo.addItem (sources[i], i + 2);

    int idToSelect = 1;
    if (currentSelection.isNotEmpty() && currentSelection != "All Sources")
    {
        for (int i = 0; i < sources.size(); ++i)
            if (sources[i] == currentSelection)
                idToSelect = i + 2;
    }

    sourceFilterCombo.setSelectedId (idToSelect, juce::dontSendNotification);
}

void OnlineCalibrationDialog::applyFilter()
{
    const auto query = searchEditor.getText().trim();
    const auto sourceFilter = (sourceFilterCombo.getSelectedId() > 1) ? sourceFilterCombo.getText() : juce::String();

    AutoEQDataManager::getInstance().filterEntries (query, sourceFilter, filteredEntries);

    resultListBox.updateContent();
    resultListBox.deselectAllRows();
    loadSelectedProfile();
    resultListBox.repaint();
}

int OnlineCalibrationDialog::getNumRows()
{
    return (int) filteredEntries.size();
}

void OnlineCalibrationDialog::paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    if (! juce::isPositiveAndBelow (rowNumber, (int) filteredEntries.size()))
        return;

    const auto& entry = filteredEntries[(size_t) rowNumber];

    if (rowIsSelected)
    {
        g.fillAll (juce::Colour (0xff005fb8));
    }
    else if (rowNumber % 2 == 1)
    {
        g.fillAll (juce::Colour (0xff222222));
    }

    g.setFont (juce::FontOptions (12.0f));
    g.setColour (rowIsSelected ? juce::Colours::white : juce::Colour (0xffe0e0e0));

    auto textRect = juce::Rectangle<int> (8, 0, width - 125, height);
    g.drawText (entry.name, textRect, juce::Justification::centredLeft, true);

    // Source / Target badge
    g.setFont (juce::FontOptions (10.0f));
    g.setColour (rowIsSelected ? juce::Colour (0xffd0e0ff) : juce::Colour (0xff888888));
    auto sourceRect = juce::Rectangle<int> (width - 120, 0, 115, height);

    juce::String badgeText = entry.source;
    if (entry.target == "Optimum Hi-Fi")
        badgeText = "Optimum Hi-Fi";
    else if (entry.target == "Diffuse Field")
        badgeText = "Diffuse Field";
    else if (entry.target == "Free Field")
        badgeText = "Free Field";

    g.drawText (badgeText, sourceRect, juce::Justification::centredRight, true);
}

void OnlineCalibrationDialog::selectedRowsChanged (int /*lastRowSelected*/)
{
    loadSelectedProfile();
}

void OnlineCalibrationDialog::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    if (juce::isPositiveAndBelow (row, (int) filteredEntries.size()))
    {
        resultListBox.selectRow (row, false, false);
        loadSelectedProfile();
    }
}

void OnlineCalibrationDialog::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    if (juce::isPositiveAndBelow (row, (int) filteredEntries.size()) && loadedProfile.has_value())
    {
        confirmSelection();
    }
}

void OnlineCalibrationDialog::loadSelectedProfile()
{
    int row = resultListBox.getSelectedRow();
    if (! juce::isPositiveAndBelow (row, (int) filteredEntries.size()))
    {
        selectedModelLabel.setText ("No headphone selected", juce::dontSendNotification);
        statusLabel.setText ({}, juce::dontSendNotification);
        curveVisualizer.setProfile (std::nullopt);
        filterSummaryEditor.setText ({});
        actionButton.setEnabled (false);
        loadedProfile = std::nullopt;
        isLoadingProfile = false;
        return;
    }

    const auto& entry = filteredEntries[(size_t) row];
    const auto requestedPath = entry.folderPath;

    selectedModelLabel.setText (entry.name + " (" + entry.source + ")", juce::dontSendNotification);
    statusLabel.setText ("Loading profile for " + entry.name + "...", juce::dontSendNotification);

    // Immediately clear previous profile, curve, and summary text so load state is obvious
    loadedProfile = std::nullopt;
    curveVisualizer.setProfile (std::nullopt);
    filterSummaryEditor.setText ({});
    actionButton.setEnabled (false);
    isLoadingProfile = true;

    juce::Component::SafePointer<OnlineCalibrationDialog> safeThis (this);
    AutoEQDataManager::getInstance().fetchProfileAsync (entry, [safeThis, entry, requestedPath] (std::optional<CalibrationProfile> profile, const juce::String& err)
    {
        if (safeThis == nullptr)
            return;

        // Discard result if user changed selection while request was in-flight
        int currentRow = safeThis->resultListBox.getSelectedRow();
        if (! juce::isPositiveAndBelow (currentRow, (int) safeThis->filteredEntries.size())
            || safeThis->filteredEntries[(size_t) currentRow].folderPath != requestedPath)
        {
            return;
        }

        safeThis->isLoadingProfile = false;

        if (profile.has_value())
        {
            safeThis->loadedProfile = profile;
            safeThis->statusLabel.setText ("Profile loaded.", juce::dontSendNotification);
            safeThis->curveVisualizer.setProfile (profile);

            juce::String summary;
            summary << "Headphone: " << profile->name << "\n";
            summary << "Source:    " << profile->source << "\n";
            summary << "Target:    " << profile->target << "\n";
            summary << "Preamp:    " << juce::String (profile->preampGainDb, 1) << " dB\n";
            summary << "Filters:   " << (int) profile->bands.size() << " bands\n\n";

            for (const auto& band : profile->bands)
            {
                juce::String typeStr = "PK ";
                if (band.type == CalibrationFilterType::lowShelf)  typeStr = "LSC";
                if (band.type == CalibrationFilterType::highShelf) typeStr = "HSC";
                if (band.type == CalibrationFilterType::lowPass)   typeStr = "LP ";
                if (band.type == CalibrationFilterType::highPass)  typeStr = "HP ";

                summary << (band.enabled ? "[ON] " : "[OFF]")
                        << typeStr
                        << " | " << juce::String ((int) band.frequencyHz).paddedLeft (' ', 5) << " Hz"
                        << " | " << (band.gainDb >= 0 ? "+" : "") << juce::String (band.gainDb, 1).paddedLeft (' ', 5) << " dB"
                        << " | Q: " << juce::String (band.qFactor, 2)
                        << " (Width: " << juce::String (band.bandwidthOctaves, 2) << " oct)\n";
            }

            safeThis->filterSummaryEditor.setText (summary);
            safeThis->actionButton.setEnabled (true);

            // Instant Audition: update the active EQ node in real-time
            if (safeThis->auditionCallback != nullptr)
                safeThis->auditionCallback (*profile);
        }
        else
        {
            safeThis->loadedProfile = std::nullopt;
            safeThis->statusLabel.setText ("Could not load EQ profile.", juce::dontSendNotification);
            safeThis->curveVisualizer.setProfile (std::nullopt);
            safeThis->filterSummaryEditor.setText (err.isNotEmpty() ? err : "Failed to load parametric EQ parameters.");
            safeThis->actionButton.setEnabled (false);
        }
    });
}

void OnlineCalibrationDialog::textEditorTextChanged (juce::TextEditor&)
{
    applyFilter();
}

void OnlineCalibrationDialog::textEditorReturnKeyPressed (juce::TextEditor&)
{
    if (resultListBox.getSelectedRow() < 0 && ! filteredEntries.empty())
    {
        resultListBox.selectRow (0, false, false);
        loadSelectedProfile();
    }
    else if (loadedProfile.has_value())
    {
        confirmSelection();
    }
}

void OnlineCalibrationDialog::textEditorEscapeKeyPressed (juce::TextEditor&)
{
    closeDialog();
}

void OnlineCalibrationDialog::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshSourceDropdown();
    applyFilter();
}

void OnlineCalibrationDialog::confirmSelection()
{
    if (loadedProfile.has_value())
    {
        notifyCompletion (true);
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState (1);
    }
}

void OnlineCalibrationDialog::closeDialog()
{
    notifyCompletion (false);
    if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
        dw->exitModalState (0);
}

void OnlineCalibrationDialog::notifyCompletion (bool confirmed)
{
    if (! hasNotifiedCompletion)
    {
        hasNotifiedCompletion = true;
        if (completionCallback != nullptr)
            completionCallback (confirmed, loadedProfile);
    }
}

