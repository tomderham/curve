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
#include "WindowArrangement.h"

#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>

namespace WindowArrangement
{

static std::string getDisplayUUID (CGDirectDisplayID displayID)
{
    std::string result;

    if (CFUUIDRef uuid = CGDisplayCreateUUIDFromDisplayID (displayID))
    {
        if (CFStringRef string = CFUUIDCreateString (kCFAllocatorDefault, uuid))
        {
            result = juce::String::fromCFString (string).toStdString();
            CFRelease (string);
        }

        CFRelease (uuid);
    }

    // Some virtual displays have no UUID; their number is the best there is
    return result.empty() ? "display-" + std::to_string (displayID) : result;
}

std::vector<Curve::WindowPlacement::Display> getDisplays()
{
    std::vector<Curve::WindowPlacement::Display> displays;

    NSArray<NSScreen*>* screens = [NSScreen screens];

    if ([screens count] == 0)
        return displays;

    // AppKit's y goes up from the bottom of the primary display (the first screen, with the menu
    // bar), and JUCE's goes down from its top, as WindowPlacement's does
    const CGFloat primaryHeight = [[screens objectAtIndex: 0] frame].size.height;
    const CGDirectDisplayID mainID = CGMainDisplayID();

    const auto toRect = [primaryHeight] (NSRect r)
    {
        return Curve::WindowPlacement::Rect { (int) std::lround (r.origin.x),
                                              (int) std::lround (primaryHeight - (r.origin.y + r.size.height)),
                                              (int) std::lround (r.size.width),
                                              (int) std::lround (r.size.height) };
    };

    for (NSScreen* screen in screens)
    {
        NSNumber* number = [[screen deviceDescription] objectForKey: @"NSScreenNumber"];

        if (number == nil)
            continue;

        const auto displayID = (CGDirectDisplayID) [number unsignedIntValue];

        Curve::WindowPlacement::Display display;
        display.id = getDisplayUUID (displayID);
        display.frame = toRect ([screen frame]);
        display.usable = toRect ([screen visibleFrame]);
        display.isMain = displayID == mainID;
        displays.push_back (std::move (display));
    }

    return displays;
}

static NSWindow* getWindow (juce::Component& component)
{
    if (auto* peer = component.getPeer())
        return [(NSView*) peer->getNativeHandle() window];

    return nil;
}

void bringToFront (const juce::Array<juce::Component*>& backToFront)
{
    NSWindow* frontmost = nil;

    for (auto* component : backToFront)
    {
        if (component == nullptr || ! component->isVisible())
            continue;

        NSWindow* window = getWindow (*component);

        if (window == nil || [window isMiniaturized])
            continue;

        [window orderFrontRegardless];
        frontmost = window;
    }

    if (frontmost != nil && [NSApp isActive] && [frontmost canBecomeKeyWindow])
        [frontmost makeKeyWindow];
}

bool isOnActiveSpace (juce::Component& component)
{
    NSWindow* window = getWindow (component);
    return window == nil || [window isOnActiveSpace];
}

void moveToActiveSpace (juce::Component& component)
{
    NSWindow* window = getWindow (component);

    if (window == nil || [window isOnActiveSpace] || [window isMiniaturized])
        return;

    // There's no call to move a window to a Space, but one shown on every Space while it's ordered
    // in stays on the one being shown when that's undone
    const auto behaviour = [window collectionBehavior];
    [window setCollectionBehavior: (behaviour & ~(NSWindowCollectionBehaviorMoveToActiveSpace | NSWindowCollectionBehaviorStationary))
                                       | NSWindowCollectionBehaviorCanJoinAllSpaces];
    [window orderFrontRegardless];
    [window setCollectionBehavior: behaviour];
}

void setOnAllSpaces (juce::Component& component, bool shouldBeOnAll)
{
    NSWindow* window = getWindow (component);

    if (window == nil)
        return;

    auto behaviour = [window collectionBehavior] & ~(NSWindowCollectionBehaviorCanJoinAllSpaces | NSWindowCollectionBehaviorMoveToActiveSpace);

    if (shouldBeOnAll)
        behaviour |= NSWindowCollectionBehaviorCanJoinAllSpaces;

    [window setCollectionBehavior: behaviour];
}

bool isMouseButtonDown()
{
    return [NSEvent pressedMouseButtons] != 0;
}

}
