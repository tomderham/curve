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

#define DONT_SET_USING_JUCE_NAMESPACE 1
#include "LoginItemManager.h"

#if JUCE_MAC

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import <ServiceManagement/ServiceManagement.h>

#include <vector>

namespace LoginItemManager
{
    Status getStatus()
    {
        switch ([SMAppService mainAppService].status)
        {
            case SMAppServiceStatusEnabled:          return Status::enabled;
            case SMAppServiceStatusRequiresApproval:  return Status::requiresApproval;
            case SMAppServiceStatusNotFound:          return Status::notFound;
            case SMAppServiceStatusNotRegistered:
            default:                                  return Status::notRegistered;
        }
    }

    bool setEnabled (bool shouldEnable, juce::String& errorMessage)
    {
        NSError* error = nil;
        BOOL success = NO;

        SMAppService* service = [SMAppService mainAppService];

        if (shouldEnable)
            success = [service registerAndReturnError: &error];
        else
            success = [service unregisterAndReturnError: &error];

        if (! success && error != nil)
            errorMessage = juce::String ([error.localizedDescription UTF8String]);

        return success == YES;
    }

    void openSystemSettingsLoginItemsPane()
    {
        NSURL* url = [NSURL URLWithString: @"x-apple.systempreferences:com.apple.LoginItems-Settings.extension"];

        if (url != nil)
            [[NSWorkspace sharedWorkspace] openURL: url];
    }
}

#import <CoreGraphics/CoreGraphics.h>

class MacOSDisplayChangeNotifier final : public MacOSDisplayChangeNotifierBase
{
public:
    explicit MacOSDisplayChangeNotifier (std::function<void()> callbackIn)
        : callback (std::move (callbackIn))
    {
        CGDisplayRegisterReconfigurationCallback (displayReconfigurationCallback, this);

        auto addObserver = [this] (NSNotificationCenter* center, NSNotificationName name)
        {
            if (center == nil || name == nil)
                return;

            id obs = [center addObserverForName: name
                                         object: nil
                                          queue: [NSOperationQueue mainQueue]
                                     usingBlock: ^(NSNotification*) {
                triggerCallback();
            }];

            if (obs != nil)
                observers.push_back ({ center, obs });
        };

        // Screen configuration / resolution changes
        NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
        addObserver (defaultCenter, NSApplicationDidChangeScreenParametersNotification);

        // Sleep / wake / spaces / session change notifications
        NSNotificationCenter* wsCenter = [[NSWorkspace sharedWorkspace] notificationCenter];
        addObserver (wsCenter, NSWorkspaceScreensDidSleepNotification);
        addObserver (wsCenter, NSWorkspaceScreensDidWakeNotification);
        addObserver (wsCenter, NSWorkspaceDidWakeNotification);
        addObserver (wsCenter, NSWorkspaceSessionDidBecomeActiveNotification);
        addObserver (wsCenter, NSWorkspaceActiveSpaceDidChangeNotification);
    }

    ~MacOSDisplayChangeNotifier() override
    {
        CGDisplayRemoveReconfigurationCallback (displayReconfigurationCallback, this);

        for (auto& entry : observers)
        {
            if (entry.center != nil && entry.observer != nil)
                [entry.center removeObserver: entry.observer];
        }
        observers.clear();
    }

    void triggerCallback()
    {
        if (callback)
            callback();
    }

private:
    static void displayReconfigurationCallback (CGDirectDisplayID, CGDisplayChangeSummaryFlags flags, void* userInfo)
    {
        if (flags & kCGDisplayBeginConfigurationFlag)
            return;

        auto* notifier = static_cast<MacOSDisplayChangeNotifier*> (userInfo);
        if (notifier != nullptr)
        {
            dispatch_async (dispatch_get_main_queue(), ^{
                notifier->triggerCallback();
            });
        }
    }

    struct ObserverEntry
    {
        NSNotificationCenter* center = nil;
        id observer = nil;
    };

    std::function<void()> callback;
    std::vector<ObserverEntry> observers;
};

std::unique_ptr<MacOSDisplayChangeNotifierBase> createMacOSDisplayChangeNotifier (std::function<void()> callback)
{
    return std::make_unique<MacOSDisplayChangeNotifier> (std::move (callback));
}

void forceRefreshMacOSStatusItem (void* nativeHandle)
{
    if (nativeHandle == nullptr)
        return;

    NSStatusItem* statusItem = (__bridge NSStatusItem*) nativeHandle;
    if (statusItem != nil)
    {
        if (NSStatusBarButton* button = [statusItem button])
        {
            [button setNeedsDisplay: YES];
            [[button window] displayIfNeeded];
        }

        // Toggling visibility forces macOS WindowServer / AppKit to re-anchor and clone
        // the status item across all active display menu bars if the space configuration changed.
        [statusItem setVisible: NO];
        [statusItem setVisible: YES];
    }
}

void removeMacOSStatusItem (void* nativeHandle)
{
    if (nativeHandle == nullptr)
        return;

    NSStatusItem* statusItem = (__bridge NSStatusItem*) nativeHandle;
    if (statusItem != nil)
    {
        [[NSStatusBar systemStatusBar] removeStatusItem: statusItem];
    }
}


#else

namespace LoginItemManager
{
    Status getStatus()                                              { return Status::unavailable; }
    bool setEnabled (bool, juce::String& errorMessage)               { errorMessage = "Not supported on this platform."; return false; }
    void openSystemSettingsLoginItemsPane()                          {}
}

#endif
