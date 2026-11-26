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
    NSWindow* keyWindow = [NSApp keyWindow];
    bool keyIsAmongThem = keyWindow == nil;

    for (auto* component : backToFront)
    {
        if (component == nullptr || ! component->isVisible())
            continue;

        NSWindow* window = getWindow (*component);

        if (window == nil || [window isMiniaturized])
            continue;

        [window orderFrontRegardless];
        frontmost = window;
        keyIsAmongThem = keyIsAmongThem || window == keyWindow;
    }

    // Not from another of Curve's windows, such as the graph editor or the menu bar panel, which a
    // plug-in window taking the keys would close
    if (frontmost != nil && [NSApp isActive] && keyIsAmongThem && [frontmost canBecomeKeyWindow])
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

bool isInView (juce::Component& component)
{
    NSWindow* window = getWindow (component);

    if (window == nil || ! [window isVisible] || [window isMiniaturized] || ! [window isOnActiveSpace])
        return false;

    const auto number = (CGWindowID) [window windowNumber];
    CGRect own = CGRectNull;

    if (CFArrayRef self = CGWindowListCopyWindowInfo (kCGWindowListOptionIncludingWindow, number))
    {
        if (CFArrayGetCount (self) > 0)
            CGRectMakeWithDictionaryRepresentation ((CFDictionaryRef) CFDictionaryGetValue ((CFDictionaryRef) CFArrayGetValueAtIndex (self, 0),
                                                                                         kCGWindowBounds), &own);
        CFRelease (self);
    }

    if (CGRectIsNull (own) || own.size.width <= 0 || own.size.height <= 0)
        return true;

    // The ordinary windows in front of it, of any app (not the menu bar, the Dock, or Curve's panel,
    // which are on higher layers), and how much of it they cover. Overlaps are added up, so it's an
    // overestimate where they overlap each other, which is fine for "mostly behind"
    CGFloat covered = 0;

    if (CFArrayRef above = CGWindowListCopyWindowInfo (kCGWindowListOptionOnScreenAboveWindow | kCGWindowListExcludeDesktopElements, number))
    {
        for (CFIndex i = 0; i < CFArrayGetCount (above); ++i)
        {
            auto info = (CFDictionaryRef) CFArrayGetValueAtIndex (above, i);
            int layer = 0;

            if (auto layerNumber = (CFNumberRef) CFDictionaryGetValue (info, kCGWindowLayer))
                CFNumberGetValue (layerNumber, kCFNumberIntType, &layer);

            CGRect bounds;

            if (layer != 0 || ! CGRectMakeWithDictionaryRepresentation ((CFDictionaryRef) CFDictionaryGetValue (info, kCGWindowBounds), &bounds))
                continue;

            const auto overlap = CGRectIntersection (own, bounds);

            if (! CGRectIsNull (overlap))
                covered += overlap.size.width * overlap.size.height;
        }

        CFRelease (above);
    }

    return covered < 0.5 * own.size.width * own.size.height;
}

void setMovable (juce::Component& component, bool canMove)
{
    if (NSWindow* window = getWindow (component))
        [window setMovable: canMove];
}

void setTitleBarHidden (juce::Component& component, bool shouldHide)
{
    NSWindow* window = getWindow (component);

    if (window == nil)
        return;

    const auto mask = [window styleMask];
    const bool isHidden = (mask & NSWindowStyleMaskTitled) == 0;

    if (isHidden == shouldHide)
        return;

    // The frame changes by the title bar's height, so the content stays put
    const NSRect content = [window contentRectForFrameRect: [window frame]];

    [window setStyleMask: shouldHide ? (mask & ~(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable))
                                     : (mask | NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable)];
    [window setFrame: [window frameRectForContentRect: content] display: YES];

    // Without the title bar's, a shadow sets it apart from the windows beside it
    [window setHasShadow: YES];
    [window invalidateShadow];
}

void detachFromBackdrop (juce::Component& component)
{
    NSWindow* window = getWindow (component);

    if (window != nil && [window parentWindow] != nil)
        [[window parentWindow] removeChildWindow: window];
}

//==============================================================================
struct Backdrop::Impl
{
    ~Impl()
    {
        hide();
        [window release];
    }

    void hide()
    {
        if (window == nil)
            return;

        for (NSWindow* child in [[[window childWindows] copy] autorelease])
            [window removeChildWindow: child];

        [window orderOut: nil];
    }

    NSWindow* window = nil;
};

Backdrop::Backdrop() : impl (std::make_unique<Impl>()) {}
Backdrop::~Backdrop() = default;

void Backdrop::hide()   { impl->hide(); }

void Backdrop::show (const Curve::WindowPlacement::Rect& area, const juce::Array<juce::Component*>& windows,
                     juce::Colour colour, bool onAllSpaces)
{
    NSArray<NSScreen*>* screens = [NSScreen screens];

    if (area.w <= 0 || area.h <= 0 || [screens count] == 0)
    {
        hide();
        return;
    }

    // From the desktop's y downwards to AppKit's upwards, from the bottom of the primary display
    const CGFloat primaryHeight = [[screens objectAtIndex: 0] frame].size.height;
    const NSRect frame = NSMakeRect (area.x, primaryHeight - (area.y + area.h), area.w, area.h);

    if (impl->window == nil)
    {
        impl->window = [[NSWindow alloc] initWithContentRect: frame
                                                   styleMask: NSWindowStyleMaskBorderless
                                                     backing: NSBackingStoreBuffered
                                                       defer: NO];
        [impl->window setReleasedWhenClosed: NO];
        [impl->window setOpaque: NO];
        [impl->window setBackgroundColor: [NSColor clearColor]];
        [impl->window setHasShadow: YES];

        // Rounded, as macOS's own windows are
        NSView* content = [impl->window contentView];
        [content setWantsLayer: YES];
        [[content layer] setCornerRadius: 12];
        [[content layer] setMasksToBounds: YES];
    }

    NSWindow* backdrop = impl->window;

    [[[backdrop contentView] layer] setBackgroundColor: [[NSColor colorWithSRGBRed: colour.getFloatRed()
                                                                             green: colour.getFloatGreen()
                                                                              blue: colour.getFloatBlue()
                                                                             alpha: 1.0] CGColor]];
    [backdrop setCollectionBehavior: onAllSpaces ? NSWindowCollectionBehaviorCanJoinAllSpaces : NSWindowCollectionBehaviorDefault];
    [backdrop setFrame: frame display: YES];

    // Its windows now, back to front, and no longer any that aren't
    NSMutableArray<NSWindow*>* ordered = [NSMutableArray array];

    for (auto* component : windows)
        if (component != nullptr)
            if (NSWindow* w = getWindow (*component))
                [ordered addObject: w];

    NSSet* wanted = [NSSet setWithArray: ordered];

    for (NSWindow* child in [[[backdrop childWindows] copy] autorelease])
        if (! [wanted containsObject: child])
            [backdrop removeChildWindow: child];

    const bool wasShowing = [backdrop isVisible];

    if (! wasShowing)
        [backdrop orderFrontRegardless];

    // Windows that came to the Space being shown, e.g. as a scene opened more of them there, take
    // the backdrop with them, rather than it taking them back to its own
    if (! onAllSpaces && wasShowing && ! [backdrop isOnActiveSpace])
    {
        bool anyHere = false;

        for (NSWindow* w in ordered)
            anyHere = anyHere || [w isOnActiveSpace];

        if (anyHere)
        {
            const auto behaviour = [backdrop collectionBehavior];
            [backdrop setCollectionBehavior: behaviour | NSWindowCollectionBehaviorCanJoinAllSpaces];
            [backdrop orderFrontRegardless];
            [backdrop setCollectionBehavior: behaviour];
        }
    }

    bool attached = false;

    for (NSWindow* w in ordered)
    {
        if ([w parentWindow] != backdrop)
        {
            if ([w parentWindow] != nil)
                [[w parentWindow] removeChildWindow: w];

            [backdrop addChildWindow: w ordered: NSWindowAbove];
            attached = true;
        }
    }

    // Attaching puts each in front of the others, so they're put back in the order they were stacked
    if (attached)
        for (NSWindow* w in ordered)
            [w orderFrontRegardless];
}

bool isMouseButtonDown()
{
    return [NSEvent pressedMouseButtons] != 0;
}

}
