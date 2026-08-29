/*
  ==============================================================================

    Curve - AutoEQ Data Manager Implementation

  ==============================================================================
*/

#include "AutoEQDataManager.h"
#include "AUNBandEQConverter.h"

AutoEQDataManager& AutoEQDataManager::getInstance()
{
    static AutoEQDataManager instance;
    return instance;
}

AutoEQDataManager::AutoEQDataManager()
{
    loadIndexFromDisk();

    // Auto-refresh index asynchronously if it has expired or doesn't exist
    if (! hasLocalIndex() || isIndexOutdated())
    {
        refreshIndexAsync (false);
    }
}

AutoEQDataManager::~AutoEQDataManager() = default;

juce::File AutoEQDataManager::getAutoEQDirectory()
{
    auto appName = juce::JUCEApplication::getInstance() != nullptr
                       ? juce::JUCEApplication::getInstance()->getApplicationName()
                       : "Curve";

    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Application Support")
               .getChildFile (appName)
               .getChildFile ("AutoEQ");
}

juce::File AutoEQDataManager::getIndexFile()
{
    return getAutoEQDirectory().getChildFile ("index.json");
}

juce::File AutoEQDataManager::getProfilesDirectory()
{
    auto dir = getAutoEQDirectory().getChildFile ("profiles");
    dir.createDirectory();
    return dir;
}

bool AutoEQDataManager::hasLocalIndex() const
{
    return getIndexFile().existsAsFile();
}

bool AutoEQDataManager::isIndexOutdated() const
{
    auto indexFile = getIndexFile();
    if (! indexFile.existsAsFile())
        return true;

    auto lastModified = indexFile.getLastModificationTime();
    auto now = juce::Time::getCurrentTime();
    auto ageDays = (double) (now.toMilliseconds() - lastModified.toMilliseconds()) / (1000.0 * 60.0 * 60.0 * 24.0);

    return ageDays > 7.0; // Invalidate cache after 7 days
}

std::vector<CalibrationHeadphoneEntry> AutoEQDataManager::getEntries() const
{
    const juce::ScopedLock sl (lock);
    return entries;
}

bool AutoEQDataManager::loadIndexFromDisk()
{
    auto file = getIndexFile();
    if (! file.existsAsFile())
        return false;

    auto jsonVar = juce::JSON::parse (file);
    if (! jsonVar.isArray())
        return false;

    auto* arr = jsonVar.getArray();
    std::vector<CalibrationHeadphoneEntry> loaded;
    loaded.reserve ((size_t) arr->size());

    for (const auto& item : *arr)
    {
        auto* obj = item.getDynamicObject();
        if (obj == nullptr)
            continue;

        CalibrationHeadphoneEntry entry;
        entry.name       = obj->getProperty ("name").toString();
        entry.folderPath = obj->getProperty ("folderPath").toString();
        entry.formFactor = obj->getProperty ("formFactor").toString();
        entry.source     = obj->getProperty ("source").toString();
        entry.target     = obj->getProperty ("target").toString();

        if (entry.name.isNotEmpty())
            loaded.push_back (std::move (entry));
    }

    if (! loaded.empty())
    {
        const juce::ScopedLock sl (lock);
        entries = std::move (loaded);
        return true;
    }

    return false;
}

void AutoEQDataManager::saveIndexToDisk()
{
    juce::Array<juce::var> arr;
    {
        const juce::ScopedLock sl (lock);
        arr.ensureStorageAllocated ((int) entries.size());

        for (const auto& entry : entries)
        {
            auto* obj = new juce::DynamicObject();
            obj->setProperty ("name", entry.name);
            obj->setProperty ("folderPath", entry.folderPath);
            obj->setProperty ("formFactor", entry.formFactor);
            obj->setProperty ("source", entry.source);
            obj->setProperty ("target", entry.target);
            arr.add (juce::var (obj));
        }
    }

    auto dir = getAutoEQDirectory();
    dir.createDirectory();

    auto file = getIndexFile();
    file.replaceWithText (juce::JSON::toString (arr));
}

juce::StringArray AutoEQDataManager::getAvailableSources() const
{
    std::set<juce::String> uniqueSources;
    auto all = getEntries();
    for (const auto& entry : all)
    {
        if (entry.source.isNotEmpty())
            uniqueSources.insert (entry.source);
    }

    juce::StringArray sources;
    sources.ensureStorageAllocated ((int) uniqueSources.size());
    for (const auto& s : uniqueSources)
        sources.add (s);

    return sources;
}

void AutoEQDataManager::parseMarkdownIndex (const juce::String& markdownContent)
{
    std::vector<CalibrationHeadphoneEntry> parsed;
    auto lines = juce::StringArray::fromLines (markdownContent);

    for (const auto& rawLine : lines)
    {
        auto line = rawLine.trim();
        if (! (line.startsWith ("- [") || line.startsWith ("* [")))
            continue;

        int startBracket = line.indexOfChar ('[');
        int sepIdx = line.indexOf ("](");
        if (startBracket >= 0 && sepIdx > startBracket)
        {
            auto name = line.substring (startBracket + 1, sepIdx).trim();
            auto rest = line.substring (sepIdx + 2).trim();

            int closeParen = rest.indexOf (") by ");
            if (closeParen < 0)
                closeParen = rest.lastIndexOfChar (')');
            if (closeParen < 0)
                continue;

            auto folderPath = rest.substring (0, closeParen).trim();
            auto afterParen = rest.substring (closeParen + 1).trim();

            // Skip non-standard hobbyist miniDSP EARS fixtures
            if (folderPath.containsIgnoreCase ("EARS") || afterParen.containsIgnoreCase ("EARS"))
                continue;

            // Extract source/contributor
            juce::String source;
            if (afterParen.startsWithIgnoreCase ("by "))
            {
                auto afterBy = afterParen.substring (3).trim();
                if (afterBy.containsIgnoreCase (" on "))
                    source = afterBy.upToFirstOccurrenceOf (" on ", false, true).trim();
                else
                    source = afterBy;
            }

            if (source.isEmpty())
            {
                auto cleanPath = juce::URL::removeEscapeChars (folderPath).trimCharactersAtStart ("./\\");
                source = cleanPath.upToFirstOccurrenceOf ("/", false, true).trim();
            }

            // Extract form factor from folder path
            juce::String formFactor = "Over-Ear";
            if (folderPath.containsIgnoreCase ("in-ear"))
                formFactor = "In-Ear";
            else if (folderPath.containsIgnoreCase ("earbud"))
                formFactor = "Earbud";

            bool isInEar = (formFactor == "In-Ear" || formFactor == "Earbud");

            // Standard listener reference target
            juce::String target = isInEar ? "AutoEq in-ear" : "Harman Over-Ear 2018";

            if (name.isNotEmpty() && folderPath.isNotEmpty())
            {
                CalibrationHeadphoneEntry entry;
                entry.name       = name;
                entry.folderPath = folderPath;
                entry.formFactor = formFactor;
                entry.source     = source;
                entry.target     = target;
                parsed.push_back (std::move (entry));
            }
        }
    }

    if (! parsed.empty())
    {
        {
            const juce::ScopedLock sl (lock);
            entries = std::move (parsed);
        }
        saveIndexToDisk();
        juce::MessageManager::callAsync ([this] { sendChangeMessage(); });
    }
}

void AutoEQDataManager::refreshIndexAsync (bool forceRefresh, std::function<void (bool success, const juce::String& message)> onComplete)
{
    juce::ignoreUnused (forceRefresh);

    {
        const juce::ScopedLock sl (lock);
        if (onComplete != nullptr)
            pendingIndexCallbacks.push_back (onComplete);

        if (isFetchingIndex.exchange (true))
            return;
    }

    juce::Thread::launch ([this]
    {
        const juce::String primaryUrl = "https://cdn.jsdelivr.net/gh/jaakkopasanen/AutoEq@master/results/INDEX.md";
        const juce::String fallbackUrl = "https://raw.githubusercontent.com/jaakkopasanen/AutoEq/master/results/INDEX.md";

        auto downloadString = [] (const juce::String& urlStr) -> juce::String
        {
            juce::URL url (urlStr);
            auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                              .withConnectionTimeoutMs (10000)
                              .withExtraHeaders ("User-Agent: Curve-Audio-App\nAccept: text/plain");
            if (auto stream = url.createInputStream (options))
                return stream->readEntireStreamAsString();
            return {};
        };

        auto content = downloadString (primaryUrl);
        if (content.isEmpty())
            content = downloadString (fallbackUrl);

        bool success = false;
        int totalEntries = 0;

        if (content.isNotEmpty())
        {
            parseMarkdownIndex (content); // Parses and writes index to disk on background thread
            totalEntries = (int) getEntries().size();
            success = (totalEntries > 0);
        }

        juce::MessageManager::callAsync ([this, success, totalEntries]
        {
            std::vector<std::function<void (bool success, const juce::String& message)>> callbacksToRun;
            {
                const juce::ScopedLock sl (lock);
                isFetchingIndex.store (false);
                callbacksToRun = std::move (pendingIndexCallbacks);
                pendingIndexCallbacks.clear();
            }

            if (success)
            {
                juce::String msg = "Index updated: " + juce::String (totalEntries) + " headphone models available.";
                for (auto& cb : callbacksToRun)
                    if (cb != nullptr) cb (true, msg);
            }
            else
            {
                for (auto& cb : callbacksToRun)
                    if (cb != nullptr) cb (false, "Failed to download AutoEQ index from GitHub.");
            }
        });
    });
}

void AutoEQDataManager::filterEntries (const juce::String& searchKeyword,
                                      const juce::String& sourceFilter,
                                      std::vector<CalibrationHeadphoneEntry>& outResults) const
{
    outResults.clear();
    const juce::ScopedLock sl (lock);

    outResults.reserve (entries.size());
    auto trimmedSearch = searchKeyword.trim();
    bool hasSearch = trimmedSearch.isNotEmpty();
    bool hasSource = sourceFilter.isNotEmpty() && ! sourceFilter.equalsIgnoreCase ("All Sources");

    for (const auto& entry : entries)
    {
        if (hasSource && ! entry.source.equalsIgnoreCase (sourceFilter))
            continue;

        if (hasSearch && ! entry.name.containsIgnoreCase (trimmedSearch))
            continue;

        outResults.push_back (entry);
    }
}

static juce::String sanitizeFileName (const juce::String& name)
{
    auto clean = name.trimCharactersAtStart ("./\\");
    while (clean.startsWithChar ('.'))
        clean = clean.substring (1);

    return clean.replaceCharacter ('/', '_')
                .replaceCharacter ('\\', '_')
                .replaceCharacter (':', '_')
                .replaceCharacter ('*', '_')
                .replaceCharacter ('?', '_')
                .replaceCharacter ('"', '_')
                .replaceCharacter ('<', '_')
                .replaceCharacter ('>', '_')
                .replaceCharacter ('|', '_')
                .replaceCharacter (' ', '_')
                .toLowerCase();
}

std::optional<CalibrationProfile> AutoEQDataManager::parseParametricEQText (const juce::String& text, const CalibrationHeadphoneEntry& entry)
{
    if (text.isEmpty())
        return std::nullopt;

    CalibrationProfile profile;
    profile.name = entry.name.isNotEmpty() ? entry.name : "Parametric EQ Profile";
    profile.source = entry.source.isNotEmpty() ? entry.source : "Imported Filter Set";
    profile.formFactor = entry.formFactor;
    profile.target = entry.target.isNotEmpty() ? entry.target : "Custom / Flat";

    auto lines = juce::StringArray::fromLines (text);

    for (const auto& rawLine : lines)
    {
        auto line = rawLine.trim();
        if (line.isEmpty())
            continue;

        if (line.startsWithIgnoreCase ("# Target:") || line.startsWithIgnoreCase ("Target:"))
        {
            auto valStr = line.fromFirstOccurrenceOf (":", false, true).trim();
            if (valStr.isNotEmpty())
                profile.target = valStr;
            continue;
        }

        if (line.startsWithChar ('#'))
            continue;

        if (line.startsWithIgnoreCase ("Preamp:") || line.startsWithIgnoreCase ("Global Gain:"))
        {
            auto valStr = line.fromFirstOccurrenceOf (":", false, true).trim();
            valStr = valStr.upToFirstOccurrenceOf ("dB", false, true).trim();
            profile.preampGainDb = valStr.getDoubleValue();
        }
        else if (line.startsWithIgnoreCase ("Filter Type"))
        {
            // Header row in autoeq.app exports, skip
            continue;
        }
        else if (line.startsWithIgnoreCase ("Low Shelf") || line.startsWithIgnoreCase ("High Shelf")
                 || line.startsWithIgnoreCase ("Parametric") || line.startsWithIgnoreCase ("Low Pass")
                 || line.startsWithIgnoreCase ("High Pass"))
        {
            // Format: autoeq.app table export (e.g. "Low Shelf 105 1.92 -6.0" or "Parametric 63 1.37 3.6")
            CalibrationFilterBand band;
            band.enabled = true;

            int numStart = 0;
            if (line.startsWithIgnoreCase ("Low Shelf"))
            {
                band.type = CalibrationFilterType::lowShelf;
                numStart = 9;
            }
            else if (line.startsWithIgnoreCase ("High Shelf"))
            {
                band.type = CalibrationFilterType::highShelf;
                numStart = 10;
            }
            else if (line.startsWithIgnoreCase ("Parametric"))
            {
                band.type = CalibrationFilterType::peaking;
                numStart = 10;
            }
            else if (line.startsWithIgnoreCase ("Low Pass"))
            {
                band.type = CalibrationFilterType::lowPass;
                numStart = 8;
            }
            else if (line.startsWithIgnoreCase ("High Pass"))
            {
                band.type = CalibrationFilterType::highPass;
                numStart = 9;
            }

            auto rest = line.substring (numStart).trim();
            auto tokens = juce::StringArray::fromTokens (rest, true);
            if (tokens.size() >= 3)
            {
                band.frequencyHz = tokens[0].getDoubleValue();
                band.qFactor = tokens[1].getDoubleValue();
                band.bandwidthOctaves = AUNBandEQConverter::qFactorToOctaveBandwidth (band.qFactor);
                band.gainDb = tokens[2].getDoubleValue();
                profile.bands.push_back (band);
            }
        }
        else if (line.startsWithIgnoreCase ("Filter"))
        {
            // Handles:
            // Equalizer APO / Squiglink / AutoEQ: Filter 1: ON PK Fc 32 Hz Gain 2.1 dB Q 1.41
            // REW export: Filter 1: ON PK Fc 42.0 Hz Gain -6.5 dB Q 3.500
            // REW shelf:  Filter 2: ON LS Fc 105.0 Hz Gain +5.0 dB Q 0.707
            // Tabular:    Filter 1: ON PK 1000 Hz -3.0 dB 1.41
            auto afterColon = line.fromFirstOccurrenceOf (":", false, true).trim();
            auto tokens = juce::StringArray::fromTokens (afterColon, true);

            if (tokens.size() >= 2)
            {
                CalibrationFilterBand band;
                band.enabled = tokens[0].equalsIgnoreCase ("ON");

                auto typeStr = tokens[1].toUpperCase();
                if (typeStr == "PK" || typeStr == "PEAK" || typeStr == "NONE")
                    band.type = CalibrationFilterType::peaking;
                else if (typeStr == "LSC" || typeStr == "LS" || typeStr == "LOW_SHELF" || typeStr == "LS6" || typeStr == "LS12")
                    band.type = CalibrationFilterType::lowShelf;
                else if (typeStr == "HSC" || typeStr == "HS" || typeStr == "HIGH_SHELF" || typeStr == "HS6" || typeStr == "HS12")
                    band.type = CalibrationFilterType::highShelf;
                else if (typeStr == "LP" || typeStr == "LPF" || typeStr == "LOW_PASS" || typeStr == "LP6" || typeStr == "LP12")
                    band.type = CalibrationFilterType::lowPass;
                else if (typeStr == "HP" || typeStr == "HPF" || typeStr == "HIGH_PASS" || typeStr == "HP6" || typeStr == "HP12")
                    band.type = CalibrationFilterType::highPass;
                else if (typeStr == "BP" || typeStr == "BAND_PASS")
                    band.type = CalibrationFilterType::bandPass;
                else if (typeStr == "NOTCH" || typeStr == "NO")
                    band.type = CalibrationFilterType::notch;
                else
                    band.type = CalibrationFilterType::peaking;

                bool foundFc = false, foundGain = false, foundQ = false;
                for (int i = 2; i < tokens.size(); ++i)
                {
                    if (tokens[i].equalsIgnoreCase ("Fc") && i + 1 < tokens.size())
                    {
                        band.frequencyHz = tokens[i + 1].getDoubleValue();
                        foundFc = true;
                    }
                    else if (tokens[i].equalsIgnoreCase ("Gain") && i + 1 < tokens.size())
                    {
                        band.gainDb = tokens[i + 1].getDoubleValue();
                        foundGain = true;
                    }
                    else if (tokens[i].equalsIgnoreCase ("Q") && i + 1 < tokens.size())
                    {
                        band.qFactor = tokens[i + 1].getDoubleValue();
                        foundQ = true;
                    }
                }

                // If explicit Fc/Gain/Q tags were absent entirely, parse positional values
                // (e.g. Filter 1: ON PK 1000 -3.0 1.41 or 1000 Hz -3.0 dB 1.41). Guarded on
                // "none of the tags were found" rather than "values equal the struct defaults",
                // so a legitimately-tagged line whose values happen to equal the defaults
                // (e.g. Fc 1000 Hz Gain 0.0 dB) is not overwritten by the positional fallback.
                if (! foundFc && ! foundGain && ! foundQ && tokens.size() >= 3)
                {
                    std::vector<double> numericVals;
                    for (int i = 2; i < tokens.size(); ++i)
                    {
                        auto t = tokens[i].trim();
                        // Ignore unit/tag tokens
                        if (t.equalsIgnoreCase ("Hz") || t.equalsIgnoreCase ("dB") || t.equalsIgnoreCase ("Q") || t.equalsIgnoreCase ("Fc") || t.equalsIgnoreCase ("Gain"))
                            continue;

                        // Check if numeric
                        if (t.containsOnly ("0123456789.+-"))
                            numericVals.push_back (t.getDoubleValue());
                    }

                    if (numericVals.size() >= 1)
                        band.frequencyHz = numericVals[0];
                    if (numericVals.size() >= 2)
                        band.gainDb = numericVals[1];
                    if (numericVals.size() >= 3)
                        band.qFactor = numericVals[2];
                }

                band.bandwidthOctaves = AUNBandEQConverter::qFactorToOctaveBandwidth (band.qFactor);
                profile.bands.push_back (band);
            }
        }
        else if (line.containsChar ('|') && (line.containsIgnoreCase ("PK") || line.containsIgnoreCase ("LSC") || line.containsIgnoreCase ("HSC")))
        {
            // Tabular format: [ON] PK | 1000 Hz | -3.0 dB | Q: 1.41
            auto parts = juce::StringArray::fromTokens (line, "|", "");
            if (parts.size() >= 3)
            {
                CalibrationFilterBand band;
                band.enabled = ! parts[0].containsIgnoreCase ("OFF");

                if (parts[0].containsIgnoreCase ("LSC") || parts[0].containsIgnoreCase ("LS"))
                    band.type = CalibrationFilterType::lowShelf;
                else if (parts[0].containsIgnoreCase ("HSC") || parts[0].containsIgnoreCase ("HS"))
                    band.type = CalibrationFilterType::highShelf;
                else
                    band.type = CalibrationFilterType::peaking;

                band.frequencyHz = parts[1].getDoubleValue();
                band.gainDb = parts[2].getDoubleValue();
                if (parts.size() >= 4 && parts[3].containsIgnoreCase ("Q:"))
                    band.qFactor = parts[3].fromFirstOccurrenceOf ("Q:", false, true).getDoubleValue();

                band.bandwidthOctaves = AUNBandEQConverter::qFactorToOctaveBandwidth (band.qFactor);
                profile.bands.push_back (band);
            }
        }
    }

    if (profile.bands.empty())
        return std::nullopt;

    // Sort bands in ascending frequency order so AUNBandEQ controls are intuitive
    std::sort (profile.bands.begin(), profile.bands.end(), [] (const CalibrationFilterBand& a, const CalibrationFilterBand& b)
    {
        return a.frequencyHz < b.frequencyHz;
    });

    for (size_t i = 0; i < profile.bands.size(); ++i)
        profile.bands[i].index = (int) i;

    return profile;
}

std::optional<CalibrationProfile> AutoEQDataManager::parseCalibrationFile (const juce::File& file)
{
    if (! file.existsAsFile())
        return std::nullopt;

    auto text = file.loadFileAsString();
    if (text.trim().isEmpty())
        return std::nullopt;

    CalibrationHeadphoneEntry dummyEntry;
    dummyEntry.name = file.getFileNameWithoutExtension();
    dummyEntry.source = "Imported File";
    dummyEntry.formFactor = "Over-Ear";

    return parseParametricEQText (text, dummyEntry);
}

static juce::String serializeProfileToParametricText (const CalibrationProfile& profile)
{
    juce::String text;
    if (profile.target.isNotEmpty())
        text << "# Target: " << profile.target << "\n";
    text << "Preamp: " << juce::String (profile.preampGainDb, 1) << " dB\n";

    for (int i = 0; i < (int) profile.bands.size(); ++i)
    {
        const auto& b = profile.bands[(size_t) i];
        text << "Filter " << (i + 1) << ": ";
        text << (b.enabled ? "ON " : "OFF ");

        if (b.type == CalibrationFilterType::lowShelf)
            text << "LSC ";
        else if (b.type == CalibrationFilterType::highShelf)
            text << "HSC ";
        else if (b.type == CalibrationFilterType::lowPass)
            text << "LP ";
        else if (b.type == CalibrationFilterType::highPass)
            text << "HP ";
        else if (b.type == CalibrationFilterType::bandPass)
            text << "BP ";
        else if (b.type == CalibrationFilterType::notch)
            text << "NOTCH ";
        else
            text << "PK ";

        text << "Fc " << juce::String (std::round (b.frequencyHz), 0) << " Hz ";
        text << "Gain " << juce::String (b.gainDb, 1) << " dB ";
        text << "Q " << juce::String (b.qFactor, 2) << "\n";
    }

    return text;
}

void AutoEQDataManager::fetchProfileAsync (const CalibrationHeadphoneEntry& entry,
                                          std::function<void (std::optional<CalibrationProfile>, const juce::String&)> onComplete)
{
    auto sanitized = sanitizeFileName (entry.folderPath);
    auto cacheFile = getProfilesDirectory().getChildFile (sanitized + ".txt");

    juce::Thread::launch ([entry, cacheFile, onComplete]
    {
        if (cacheFile.existsAsFile())
        {
            auto text = cacheFile.loadFileAsString();
            auto profile = parseParametricEQText (text, entry);
            if (profile.has_value())
            {
                juce::MessageManager::callAsync ([onComplete, profile]
                {
                    if (onComplete != nullptr)
                        onComplete (profile, {});
                });
                return;
            }
        }
        auto unescapedPath = juce::URL::removeEscapeChars (entry.folderPath).trimCharactersAtStart ("./\\");
        auto modelName = unescapedPath.fromLastOccurrenceOf ("/", false, true);
        if (modelName.isEmpty())
            modelName = entry.name;

        juce::StringArray pathParts;
        pathParts.addTokens (unescapedPath, "/", "");
        juce::String encodedFolderPath;
        for (int i = 0; i < pathParts.size(); ++i)
        {
            if (i > 0) encodedFolderPath << "/";
            encodedFolderPath << juce::URL::addEscapeChars (pathParts[i], true);
        }

        auto downloadString = [] (const juce::String& urlStr) -> juce::String
        {
            juce::URL url (urlStr);
            auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                              .withConnectionTimeoutMs (8000)
                              .withExtraHeaders ("User-Agent: Curve-Audio-App\nAccept: text/plain");
            if (auto stream = url.createInputStream (options))
                return stream->readEntireStreamAsString();
            return {};
        };

        juce::StringArray candidateUrls;
        auto encodedModelParametric = juce::URL::addEscapeChars (modelName + " ParametricEQ.txt", true);
        auto encodedNameParametric  = juce::URL::addEscapeChars (entry.name + " ParametricEQ.txt", true);

        candidateUrls.add ("https://cdn.jsdelivr.net/gh/jaakkopasanen/AutoEq@master/results/" + encodedFolderPath + "/" + encodedModelParametric);
        candidateUrls.add ("https://raw.githubusercontent.com/jaakkopasanen/AutoEq/master/results/" + encodedFolderPath + "/" + encodedModelParametric);
        if (entry.name != modelName)
        {
            candidateUrls.add ("https://cdn.jsdelivr.net/gh/jaakkopasanen/AutoEq@master/results/" + encodedFolderPath + "/" + encodedNameParametric);
            candidateUrls.add ("https://raw.githubusercontent.com/jaakkopasanen/AutoEq/master/results/" + encodedFolderPath + "/" + encodedNameParametric);
        }
        candidateUrls.add ("https://cdn.jsdelivr.net/gh/jaakkopasanen/AutoEq@master/results/" + encodedFolderPath + "/ParametricEQ.txt");
        candidateUrls.add ("https://raw.githubusercontent.com/jaakkopasanen/AutoEq/master/results/" + encodedFolderPath + "/ParametricEQ.txt");

        juce::String content;
        for (const auto& urlToTry : candidateUrls)
        {
            content = downloadString (urlToTry);
            if (content.isNotEmpty() && content.containsIgnoreCase ("Filter"))
                break;
        }

        // Parse and write the cache file here on the background thread, before hopping
        // to the message thread, so the completion callback doesn't do blocking disk I/O
        // on the UI thread.
        std::optional<CalibrationProfile> profile;
        if (content.isNotEmpty())
        {
            profile = parseParametricEQText (content, entry);
            if (profile.has_value())
            {
                auto serialized = serializeProfileToParametricText (*profile);
                cacheFile.replaceWithText (serialized);
            }
        }

        juce::MessageManager::callAsync ([profile, entry, onComplete] ()
        {
            if (onComplete == nullptr)
                return;

            if (profile.has_value())
                onComplete (profile, {});
            else
                onComplete (std::nullopt, "Could not load EQ profile for " + entry.name);
        });
    });
}
