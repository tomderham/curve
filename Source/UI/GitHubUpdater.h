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
    GitHubUpdater() = default;

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
                settings->setValue ("lastUpdateCheck", currentTime);
                settings->saveIfNeeded();
                performGitHubRequest();
            }
        }
    }

    void performGitHubRequest()
    {
        isChecking = true;
        
        juce::URL url ("https://api.github.com/repos/" + user + "/" + repo + "/releases/latest");
        
        auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                        .withExtraHeaders ("User-Agent: " + appName + "-Updater\nAccept: application/vnd.github+json");

        juce::WeakReference<GitHubUpdater> weakSelf (this);

        juce::Thread::launch ([weakSelf, url, options]
        {
            juce::String response;
            if (auto stream = url.createInputStream (options))
                response = stream->readEntireStreamAsString();
            
            juce::MessageManager::callAsync ([weakSelf, response] 
            {
                if (auto* self = weakSelf.get())
                {
                    self->isChecking = false;
                    auto json = juce::JSON::parse (response);
                    
                    if (auto* obj = json.getDynamicObject())
                    {
                        auto latestTag = obj->getProperty ("tag_name").toString();
                        if (latestTag.isNotEmpty() && latestTag != juce::String (ProjectInfo::versionString))
                            self->handleUpdateFound (obj);
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
        juce::WeakReference<GitHubUpdater> weakSelf (this);

        juce::MessageManager::callAsync ([weakSelf, success, dmgFile]
        {
            if (auto* self = weakSelf.get())
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
            }
        });
    }

    juce::String user, repo, appName;
    bool isChecking = false;
    std::unique_ptr<juce::URL::DownloadTask> activeDownload;

    JUCE_DECLARE_WEAK_REFERENCEABLE (GitHubUpdater)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GitHubUpdater)
};