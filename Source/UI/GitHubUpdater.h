/*
==============================================================================
   Copyright (c) Thomas Derham

   You may also use this code under the terms of the AGPLv3:
   https://www.gnu.org/licenses/agpl-3.0.en.html

   CURVE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
   WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING WARRANTY OF
   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, ARE DISCLAIMED.
==============================================================================
*/

#pragma once
#include <JuceHeader.h>

class GitHubUpdater  : public juce::URL::DownloadTask::Listener,
                       private juce::Timer
{
public:
    GitHubUpdater()
    {
        threadState = std::make_shared<std::atomic<GitHubUpdater*>> (this);
    }

    ~GitHubUpdater() override
    {
        stopTimer();
        if (threadState != nullptr)
            threadState->store (nullptr);
        activeDownload.reset();
    }

    void doCheckNow()
    {
        performGitHubRequest();
    }

    /** Starts the update monitoring. Call this in JUCEApplication::initialise(). */
    void start (const juce::String& githubUser, const juce::String& githubRepo, const juce::String& applicationName)
    {
        user = githubUser;
        repo = githubRepo;
        appName = applicationName;

        // Perform the first check immediately on startup
        if (areAutomaticChecksEnabled())
            checkIfTimeForUpdateCheck();

        // Timer callback runs every subsequent hour
        startTimer (60 * 60 * 1000); 
    }

private:
    // --- Timer Logic ---
    void timerCallback() override 
    { 
        if (areAutomaticChecksEnabled())
            checkIfTimeForUpdateCheck(); 
    }

    bool areAutomaticChecksEnabled() const
    {
        if (auto* settings = getAppProperties().getUserSettings())
            return settings->getBoolValue ("automaticUpdateChecks", true);
        return false;
    }

    void checkIfTimeForUpdateCheck()
    {
        // Don't start a check if one is already in progress
        if (isChecking || activeDownload != nullptr)
            return;

        if (auto* settings = getAppProperties().getUserSettings())
        {
            auto lastCheckStr = settings->getValue ("lastUpdateCheck", "0");
            auto lastCheck = lastCheckStr.getLargeIntValue();
            auto currentTime = juce::Time::getCurrentTime().toMilliseconds();

            if (currentTime > (lastCheck + 86400000)) // check for updates if more than 24 hours past
            {
                performGitHubRequest();
            }
        }
    }

    static bool isNewerVersion (const juce::String& latestTag, const juce::String& currentVersion)
    {
        auto cleanVersion = [] (juce::String v) -> juce::String
        {
            v = v.trim();
            if (v.startsWithIgnoreCase ("v"))
                v = v.substring (1).trim();
            return v;
        };

        auto cleanLatest = cleanVersion (latestTag);
        auto cleanCurrent = cleanVersion (currentVersion);

        if (cleanLatest.isEmpty() || cleanCurrent.isEmpty() || cleanLatest == cleanCurrent)
            return false;

        auto latestCore = cleanLatest.upToFirstOccurrenceOf ("-", false, false);
        auto latestPre = cleanLatest.fromFirstOccurrenceOf ("-", false, false);

        auto currentCore = cleanCurrent.upToFirstOccurrenceOf ("-", false, false);
        auto currentPre = cleanCurrent.fromFirstOccurrenceOf ("-", false, false);

        juce::StringArray latestCoreParts;
        latestCoreParts.addTokens (latestCore, ".", "");

        juce::StringArray currentCoreParts;
        currentCoreParts.addTokens (currentCore, ".", "");

        int maxCoreParts = juce::jmax (latestCoreParts.size(), currentCoreParts.size());
        for (int i = 0; i < maxCoreParts; ++i)
        {
            int latestNum = i < latestCoreParts.size() ? latestCoreParts[i].getIntValue() : 0;
            int currentNum = i < currentCoreParts.size() ? currentCoreParts[i].getIntValue() : 0;

            if (latestNum > currentNum)
                return true;
            if (latestNum < currentNum)
                return false;
        }

        // Core numbers are identical: compare pre-release metadata (SemVer precedence)
        // A standard release has higher precedence than a pre-release of the same core version.
        if (latestPre.isEmpty() && currentPre.isNotEmpty())
            return true;
        if (latestPre.isNotEmpty() && currentPre.isEmpty())
            return false;

        if (latestPre.isNotEmpty() && currentPre.isNotEmpty())
        {
            juce::StringArray latestPreParts;
            latestPreParts.addTokens (latestPre, ".", "");
            juce::StringArray currentPreParts;
            currentPreParts.addTokens (currentPre, ".", "");

            int maxPreParts = juce::jmax (latestPreParts.size(), currentPreParts.size());
            for (int i = 0; i < maxPreParts; ++i)
            {
                if (i >= latestPreParts.size()) return false;
                if (i >= currentPreParts.size()) return true;

                auto& lp = latestPreParts[i];
                auto& cp = currentPreParts[i];

                bool isLatestNum = lp.containsOnly ("0123456789");
                bool isCurrentNum = cp.containsOnly ("0123456789");

                if (isLatestNum && isCurrentNum)
                {
                    int lVal = lp.getIntValue();
                    int cVal = cp.getIntValue();
                    if (lVal != cVal)
                        return lVal > cVal;
                }
                else if (isLatestNum != isCurrentNum)
                {
                    return ! isLatestNum;
                }
                else
                {
                    int cmp = lp.compare (cp);
                    if (cmp != 0)
                        return cmp > 0;
                }
            }
        }

        return false;
    }

    void performGitHubRequest()
    {
        isChecking = true;
        
        juce::URL url ("https://api.github.com/repos/" + user + "/" + repo + "/releases/latest");
        
        auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                        .withConnectionTimeoutMs (10000)
                        .withExtraHeaders ("User-Agent: " + appName + "-Updater\nAccept: application/vnd.github+json");

        auto state = threadState;

        juce::Thread::launch ([state, url, options]
        {
            juce::String response;
            if (auto stream = url.createInputStream (options))
                response = stream->readEntireStreamAsString();
            
            juce::MessageManager::callAsync ([state, response] 
            {
                if (auto* self = state->load())
                {
                    self->isChecking = false;
                    auto json = juce::JSON::parse (response);
                    
                    if (auto* obj = json.getDynamicObject())
                    {
                        if (obj->hasProperty ("tag_name"))
                        {
                            // Successfully received valid release response from GitHub, save timestamp now
                            if (auto* settings = getAppProperties().getUserSettings())
                            {
                                settings->setValue ("lastUpdateCheck", juce::Time::getCurrentTime().toMilliseconds());
                                settings->saveIfNeeded();
                            }

                            auto latestTag = obj->getProperty ("tag_name").toString();
                            if (isNewerVersion (latestTag, juce::String (ProjectInfo::versionString)))
                                self->handleUpdateFound (obj);
                        }
                        else if (obj->hasProperty ("message"))
                        {
                            // GitHub API returned an error/rate-limit response (e.g. 403 rate limit exceeded).
                            // Update timestamp to avoid hammering the endpoint repeatedly on every cycle.
                            if (auto* settings = getAppProperties().getUserSettings())
                            {
                                settings->setValue ("lastUpdateCheck", juce::Time::getCurrentTime().toMilliseconds());
                                settings->saveIfNeeded();
                            }
                        }
                    }
                }
            });
        });
    }

    void handleUpdateFound (juce::DynamicObject* releaseData)
    {
        juce::WeakReference<GitHubUpdater> weakSelf (this);
        juce::NativeMessageBox::showOkCancelBox (
            juce::MessageBoxIconType::InfoIcon,
            "Update Available",
            "A new version (" + releaseData->getProperty ("tag_name").toString() + ") of Curve is available. Would you like to download it?",
            nullptr,
            juce::ModalCallbackFunction::create ([weakSelf, releaseDataVar = juce::var(releaseData)] (int result) {
                if (result == 1) // OK
                {
                    if (auto* self = weakSelf.get())
                    {
                        if (auto* validReleaseData = releaseDataVar.getDynamicObject())
                        {
                            auto* assets = validReleaseData->getProperty ("assets").getArray();
                            if (assets != nullptr)
                            {
                                for (auto& asset : *assets)
                                {
                                    juce::String name = asset.getProperty ("name", juce::var()).toString();
                                    
                                    if (name.endsWith (".dmg"))
                                    {
                                        juce::String downloadUrl = asset.getProperty ("browser_download_url", juce::var()).toString();
                                        self->downloadUpdate (downloadUrl);
                                        return;
                                    }
                                }
                            }
                        }
                    }
                }
            })
        );
    }

    void downloadUpdate (const juce::String& url)
    {
        auto tempDmg = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Curve_update.dmg");
        tempDmg.deleteFile();
        
        juce::URL::DownloadTaskOptions options;
        options = options.withListener (this);
        activeDownload = juce::URL (url).downloadToFile (tempDmg, options);
    }

    // --- Download Listener ---
    void finished (juce::URL::DownloadTask* task, bool success) override
    {
        auto dmgFile = task->getTargetLocation();
        auto state = threadState;

        juce::MessageManager::callAsync ([state, success, dmgFile]
        {
            if (auto* self = state->load())
            {
                self->activeDownload.reset();

                if (success)
                {
                    juce::NativeMessageBox::showOkCancelBox (
                        juce::MessageBoxIconType::InfoIcon,
                        "Download complete",
                        "The update for Curve has been downloaded. Would you like to quit Curve and open the installer now?",
                        nullptr,
                        juce::ModalCallbackFunction::create ([dmgFile] (int result) {
                            if (result == 1) // User clicked "OK"
                            {
                                // 1. Open the DMG visually
                                dmgFile.startAsProcess();

                                // 2. Quit the current application to free up the binary
                                juce::JUCEApplication::getInstance()->systemRequestedQuit();
                            }
                        })
                    );
                }
                else
                {
                    juce::NativeMessageBox::showMessageBoxAsync (
                        juce::MessageBoxIconType::WarningIcon,
                        "Update Download Failed",
                        "Curve was unable to download the update. Please check your internet connection or download the latest release directly from GitHub."
                    );
                }
            }
        });
    }

    std::shared_ptr<std::atomic<GitHubUpdater*>> threadState;
    juce::String user, repo, appName;
    bool isChecking = false;
    std::unique_ptr<juce::URL::DownloadTask> activeDownload;

    JUCE_DECLARE_WEAK_REFERENCEABLE (GitHubUpdater)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GitHubUpdater)
};