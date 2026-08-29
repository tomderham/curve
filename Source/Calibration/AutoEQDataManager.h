/*
  ==============================================================================

    Curve - AutoEQ Data Manager
    Manages AutoEQ GitHub repository index, caching, and profile fetching.
    Parses AutoEQ, REW (Room EQ Wizard), and Squiglink Parametric EQ text formats.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <atomic>
#include "OnlineCalibrationModels.h"

class AutoEQDataManager : public juce::ChangeBroadcaster
{
public:
    AutoEQDataManager();
    ~AutoEQDataManager() override;

    static AutoEQDataManager& getInstance();

    static juce::File getAutoEQDirectory();
    static juce::File getProfilesDirectory();
    static juce::File getIndexFile();

    /** Checks if the index exists on disk. */
    bool hasLocalIndex() const;

    /** Checks if the index is older than 7 days. */
    bool isIndexOutdated() const;

    /** Loads the local index from disk into memory. Returns true if successful. */
    bool loadIndexFromDisk();

    /** Fetches the latest index from jsDelivr / GitHub asynchronously and saves to disk. */
    void refreshIndexAsync (bool forceRefresh = false, std::function<void (bool success, const juce::String& message)> onComplete = nullptr);

    /** Returns all loaded headphone entries from AutoEQ. */
    std::vector<CalibrationHeadphoneEntry> getEntries() const;

    /** Filters loaded entries in-place with zero-copy. */
    void filterEntries (const juce::String& searchKeyword,
                        const juce::String& sourceFilter,
                        std::vector<CalibrationHeadphoneEntry>& outResults) const;

    /** Returns list of distinct measurement sources found in the index. */
    juce::StringArray getAvailableSources() const;

    /** Fetches and parses a profile asynchronously for a given entry. */
    void fetchProfileAsync (const CalibrationHeadphoneEntry& entry,
                            std::function<void (std::optional<CalibrationProfile> profile, const juce::String& errorMessage)> onComplete);

    /** Synchronously parses Parametric EQ text content (AutoEQ, REW, or Squiglink export) into a CalibrationProfile. */
    static std::optional<CalibrationProfile> parseParametricEQText (const juce::String& text, const CalibrationHeadphoneEntry& entry);

    /** Synchronously parses a local calibration / EQ file (.txt, .req, .csv, .eq). */
    static std::optional<CalibrationProfile> parseCalibrationFile (const juce::File& file);

    bool isRefreshingIndex() const { return isFetchingIndex.load(); }

private:
    void parseMarkdownIndex (const juce::String& markdownContent);
    void saveIndexToDisk();

    std::vector<CalibrationHeadphoneEntry> entries;
    std::vector<std::function<void (bool success, const juce::String& message)>> pendingIndexCallbacks;
    std::atomic<bool> isFetchingIndex { false };
    mutable juce::CriticalSection lock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoEQDataManager)
};
