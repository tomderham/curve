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
#include "DockIconManager.h"

#import <AppKit/AppKit.h>

juce::PropertiesFile* getUserSettings();

static std::function<void()> dockReopenCallback;

@interface CurveDockReopenHandler : NSObject
- (void) handleReopen: (NSAppleEventDescriptor*) event withReplyEvent: (NSAppleEventDescriptor*) reply;
@end

@implementation CurveDockReopenHandler
- (void) handleReopen: (NSAppleEventDescriptor*) event withReplyEvent: (NSAppleEventDescriptor*) reply
{
    juce::ignoreUnused (event, reply);

    if (dockReopenCallback)
        dockReopenCallback();
}
@end

static bool setActivationPolicy (bool showInDock)
{
    const auto policy = showInDock ? NSApplicationActivationPolicyRegular
                                   : NSApplicationActivationPolicyAccessory;

    if ([NSApp activationPolicy] == policy)
        return false;

    [NSApp setActivationPolicy: policy];
    return true;
}

static void installReopenHandler()
{
    static CurveDockReopenHandler* handler = [[CurveDockReopenHandler alloc] init];

    [[NSAppleEventManager sharedAppleEventManager] setEventHandler: handler
                                                       andSelector: @selector (handleReopen:withReplyEvent:)
                                                     forEventClass: kCoreEventClass
                                                        andEventID: kAEReopenApplication];
}

namespace DockIconManager
{
    void setShowInDock (bool showInDock)
    {
        if (! setActivationPolicy (showInDock))
            return;

        if (showInDock)
        {
            // An app that becomes Regular while it's already active can end up with a
            // menu bar that doesn't respond until it's re-activated, so hand focus to
            // the Dock and take it back.
            if (auto* dock = [[NSRunningApplication runningApplicationsWithBundleIdentifier: @"com.apple.dock"] firstObject])
                [dock activateWithOptions: 0];

            dispatch_after (dispatch_time (DISPATCH_TIME_NOW, (int64_t) (0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
                [NSApp activateIgnoringOtherApps: YES];
            });
        }
        else
        {
            // Changing policy can deactivate the app, which leaves its windows behind
            // other apps; bring it back to the front so open windows stay visible.
            [NSApp activateIgnoringOtherApps: YES];
        }
    }

    bool isShowInDockEnabled()
    {
        if (auto* settings = getUserSettings())
            return settings->getBoolValue ("showInDock", false);

        return false;
    }

    void applyCurrentSetting()
    {
        // Called at launch, before AppKit finishes launching: setting the policy now
        // is enough, and Curve shouldn't take focus when it opens at login.
        setActivationPolicy (isShowInDockEnabled());
    }

    void setReopenCallback (std::function<void()> onReopen)
    {
        dockReopenCallback = std::move (onReopen);

        // AppKit installs its own reopen handler while it finishes launching, which
        // would replace ours, so wait until launching has finished.
        if ([[NSRunningApplication currentApplication] isFinishedLaunching])
        {
            installReopenHandler();
            return;
        }

        static id observer = nil;

        if (observer != nil)
            return;

        observer = [[NSNotificationCenter defaultCenter] addObserverForName: NSApplicationDidFinishLaunchingNotification
                                                                     object: nil
                                                                      queue: [NSOperationQueue mainQueue]
                                                                 usingBlock: ^(NSNotification*) {
            installReopenHandler();
            [[NSNotificationCenter defaultCenter] removeObserver: observer];
            observer = nil;
        }];
    }
}
