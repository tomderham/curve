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

// Activates Curve, then brings back the windows that were showing. Activation can be declined
// since macOS 14 (e.g. while the Dock still has focus), which would leave them behind other apps,
// so they're ordered to the front explicitly, which works whether or not Curve is active.
static void reactivate (NSArray<NSWindow*>* windows, NSWindow* keyWindow)
{
    if (@available (macOS 14.0, *))
        [NSApp activate];
    else
        [NSApp activateIgnoringOtherApps: YES];

    for (NSWindow* window in windows)
        [window orderFrontRegardless];

    if (keyWindow != nil && [keyWindow isVisible])
        [keyWindow makeKeyAndOrderFront: nil];
}

static NSArray<NSWindow*>* getVisibleWindows()
{
    NSMutableArray<NSWindow*>* visible = [NSMutableArray array];

    // Back to front, so ordering each to the front keeps their stacking
    for (NSWindow* window in [[NSApp orderedWindows] reverseObjectEnumerator])
        if ([window isVisible] && ! [window isKindOfClass: [NSPanel class]])
            [visible addObject: window];

    return visible;
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
    void setShowInDock (bool showInDock, std::function<void()> onceDone)
    {
        // Retained: the block below runs after this returns, and Curve is built without ARC
        NSArray<NSWindow*>* windows = [getVisibleWindows() retain];
        NSWindow* keyWindow = [[NSApp keyWindow] retain];

        if (! setActivationPolicy (showInDock))
        {
            [windows release];
            [keyWindow release];

            if (onceDone)
                onceDone();

            return;
        }

        if (showInDock)
        {
            // An app that becomes Regular while it's already active can end up with a
            // menu bar that doesn't respond until it's re-activated, so hand focus to
            // the Dock and take it back.
            if (auto* dock = [[NSRunningApplication runningApplicationsWithBundleIdentifier: @"com.apple.dock"] firstObject])
                [dock activateWithOptions: 0];

            // Copied, as a block keeps a copy of what it uses
            auto done = std::make_shared<std::function<void()>> (std::move (onceDone));

            dispatch_after (dispatch_time (DISPATCH_TIME_NOW, (int64_t) (0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
                reactivate (windows, keyWindow);
                [windows release];
                [keyWindow release];

                if (*done)
                    (*done)();
            });
        }
        else
        {
            // Changing policy can deactivate the app, which leaves its windows behind
            // other apps; bring it back to the front so open windows stay visible.
            reactivate (windows, keyWindow);
            [windows release];
            [keyWindow release];

            if (onceDone)
                onceDone();
        }
    }

    static bool editorOpen = false;

    Mode getMode()
    {
        if (auto* settings = getUserSettings())
        {
            if (settings->containsKey ("dockIcon"))
                return (Mode) juce::jlimit (0, 2, settings->getIntValue ("dockIcon"));

            if (settings->getBoolValue ("showInDock", false))
                return Mode::always;
        }

        return Mode::whileEditorOpen;
    }

    static bool isWanted()
    {
        const auto mode = getMode();
        return mode == Mode::always || (mode == Mode::whileEditorOpen && editorOpen);
    }

    void setMode (Mode mode)
    {
        if (auto* settings = getUserSettings())
        {
            settings->setValue ("dockIcon", (int) mode);
            settings->saveIfNeeded();
        }

        setShowInDock (isWanted());
    }

    void setEditorOpen (bool isOpen, std::function<void()> onceDone)
    {
        if (editorOpen == isOpen || getMode() != Mode::whileEditorOpen)
        {
            editorOpen = isOpen;

            if (onceDone)
                onceDone();

            return;
        }

        editorOpen = isOpen;
        setShowInDock (isWanted(), std::move (onceDone));
    }

    void applyCurrentSetting()
    {
        // Called at launch, before AppKit finishes launching: setting the policy now
        // is enough, and Curve shouldn't take focus when it opens at login.
        setActivationPolicy (isWanted());
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
