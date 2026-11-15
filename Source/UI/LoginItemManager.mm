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

        // Only display changes, not Spaces, wake or session changes: the callback recreates the
        // status item, and on macOS 26 and earlier a new item lands at the far left of the menu
        // bar, where menu bar managers such as Hidden Bar hide it. Displays connected or removed
        // during sleep still trigger the reconfiguration callback on wake.
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

std::vector<juce::uint32> getMacOSActiveDisplayIDs()
{
    uint32_t count = 0;
    if (CGGetActiveDisplayList (0, nullptr, &count) != kCGErrorSuccess || count == 0)
        return {};

    std::vector<CGDirectDisplayID> displays (count);
    if (CGGetActiveDisplayList (count, displays.data(), &count) != kCGErrorSuccess)
        return {};

    std::vector<juce::uint32> ids (displays.begin(), displays.begin() + count);
    std::sort (ids.begin(), ids.end());
    return ids;
}

// macOS keeps a status item's position under its autosave name. Without a fixed name AppKit
// assigns "Item-0", "Item-1"... and a recreated item can get a new one, with no saved position,
// so it appears at the far left of the menu bar (where menu bar managers such as Hidden Bar hide it).
static NSString* const curveStatusItemAutosaveName = @"CurveStatusItem";

void setMacOSStatusItemAutosaveName (void* nativeHandle)
{
    if (nativeHandle == nullptr)
        return;

    NSStatusItem* statusItem = (__bridge NSStatusItem*) nativeHandle;
    if (statusItem != nil && ! [[statusItem autosaveName] isEqualToString: curveStatusItemAutosaveName])
        [statusItem setAutosaveName: curveStatusItemAutosaveName];
}

void removeMacOSStatusItem (void* nativeHandle)
{
    if (nativeHandle == nullptr)
        return;

    NSStatusItem* statusItem = (__bridge NSStatusItem*) nativeHandle;
    if (statusItem == nil)
        return;

    // On macOS 26 and earlier, removing an item also deletes its saved position and visibility from
    // the app's defaults (as does setting isVisible to NO), so the replacement would lose its place.
    // Keep the values and write them back for the next item with the same autosave name.
    NSString* name = [statusItem autosaveName];
    NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    NSString* positionKey = name != nil ? [@"NSStatusItem Preferred Position " stringByAppendingString: name] : nil;
    NSString* visibleKey  = name != nil ? [@"NSStatusItem Visible " stringByAppendingString: name] : nil;
    id savedPosition = positionKey != nil ? [defaults objectForKey: positionKey] : nil;
    id savedVisible  = visibleKey  != nil ? [defaults objectForKey: visibleKey]  : nil;

    [[NSStatusBar systemStatusBar] removeStatusItem: statusItem];

    if (savedPosition != nil)
        [defaults setObject: savedPosition forKey: positionKey];
    if (savedVisible != nil)
        [defaults setObject: savedVisible forKey: visibleKey];
}
