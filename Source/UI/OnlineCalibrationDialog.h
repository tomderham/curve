/*
  ==============================================================================

    Curve - Online Calibration Dialog

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Calibration/OnlineCalibrationModels.h"
#include "../Calibration/AutoEQDataManager.h"
#include "../Calibration/AUNBandEQConverter.h"

//==============================================================================
/**
    A component that visualizes the EQ curve resulting from an online calibration profile.
*/
class OnlineCalibrationCurveComponent final : public juce::Component
{
public:
    OnlineCalibrationCurveComponent();
    ~OnlineCalibrationCurveComponent() override;

    void setProfile (const std::optional<CalibrationProfile>& newProfile);

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    std::optional<CalibrationProfile> currentProfile;
    juce::Path curvePath;

    void updateCurve();
    static double evaluateMagnitudeDb (const CalibrationProfile& profile, double frequencyHz);
};

//==============================================================================
/**
    Modal dialog allowing users to search and select AutoEQ headphone calibration profiles.
*/
class OnlineCalibrationDialog final : public juce::Component,
                                      public juce::ListBoxModel,
                                      public juce::TextEditor::Listener,
                                      public juce::ChangeListener
{
public:
    enum class Mode
    {
        createNewNode,
        configureExistingNode
    };

    OnlineCalibrationDialog (Mode dialogMode);
    ~OnlineCalibrationDialog() override;

    void setAuditionCallback (std::function<void (const CalibrationProfile&)> callback) { auditionCallback = std::move (callback); }
    void setCompletionCallback (std::function<void (bool confirmed, const std::optional<CalibrationProfile>& profile)> callback) { completionCallback = std::move (callback); }

    // Component overrides
    void paint (juce::Graphics& g) override;
    void resized() override;
    void parentHierarchyChanged() override;

    // ListBoxModel overrides
    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
    void selectedRowsChanged (int lastRowSelected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;

    // TextEditor::Listener overrides
    void textEditorTextChanged (juce::TextEditor&) override;
    void textEditorReturnKeyPressed (juce::TextEditor&) override;
    void textEditorEscapeKeyPressed (juce::TextEditor&) override;

    // ChangeListener overrides
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

private:
    void applyFilter();
    void refreshSourceDropdown();
    void loadSelectedProfile();
    void confirmSelection();
    void closeDialog();
    void notifyCompletion (bool confirmed);

    Mode mode;
    std::function<void (const CalibrationProfile&)> auditionCallback;
    std::function<void (bool confirmed, const std::optional<CalibrationProfile>& profile)> completionCallback;
    bool hasNotifiedCompletion = false;

    std::vector<CalibrationHeadphoneEntry> filteredEntries;
    std::optional<CalibrationProfile> loadedProfile;
    bool isLoadingProfile = false;

    // UI elements
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::TextButton refreshButton { "Sync Index" };
    juce::Label statusLabel;

    juce::Label searchLabel;
    juce::TextEditor searchEditor;
    juce::ComboBox sourceFilterCombo;

    juce::ListBox resultListBox;

    // Right details pane
    juce::Label selectedModelLabel;
    OnlineCalibrationCurveComponent curveVisualizer;
    juce::TextEditor filterSummaryEditor;

    juce::TextButton cancelButton { "Cancel" };
    juce::TextButton actionButton { "Use this EQ" };

    juce::Label footerCustomLabel;
    juce::HyperlinkButton autoEqLinkButton { "autoeq.app", juce::URL ("https://autoeq.app") };
    juce::Label footerCustomDotLabel;

    juce::Label footerMissingLabel;
    juce::HyperlinkButton squigLinkButton { "squig.link", juce::URL ("https://squig.link") };
    juce::Label footerMissingDotLabel;

    juce::Label footerImportLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OnlineCalibrationDialog)
};

