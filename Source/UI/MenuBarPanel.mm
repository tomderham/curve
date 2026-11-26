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
#include "MenuBarPanel.h"
#include "PresetSlots.h"

#import <AppKit/AppKit.h>
#import <Carbon/Carbon.h> // kVK_ key codes

#include <map>
#include <set>

juce::PropertiesFile* getUserSettings();

static NSString* toNS (const juce::String& s)
{
    return [NSString stringWithUTF8String: s.toRawUTF8()];
}

static NSColor* toNS (juce::Colour c)
{
    return [NSColor colorWithSRGBRed: c.getFloatRed() green: c.getFloatGreen() blue: c.getFloatBlue() alpha: c.getFloatAlpha()];
}

//==============================================================================
/** Calls a function from a control's action or a menu item. Controls don't retain their targets,
    so the panel keeps these until it rebuilds its views. */
@interface CurvePanelAction : NSObject
{
@public
    std::function<void (id)> function;
}
- (void) perform: (id) sender;
@end

@implementation CurvePanelAction
- (void) perform: (id) sender
{
    if (function)
        function (sender);
}
@end

//==============================================================================
/** The panel's root view: takes the preset keys while the panel is open. */
@interface CurvePanelView : NSView
{
@public
    std::function<bool (NSEvent*)> onKey;
    std::function<void()> onEscape;
}
@end

@implementation CurvePanelView
- (BOOL) acceptsFirstResponder { return YES; }
- (BOOL) isFlipped             { return YES; }

- (void) keyDown: (NSEvent*) event
{
    if (! (onKey && onKey (event)))
        [super keyDown: event];
}

// Escape
- (void) cancelOperation: (id) sender
{
    if (onEscape)
        onEscape();
}
@end

//==============================================================================
// The panel's controls act on the first click, as the preset rows do, even when the panel isn't the
// key window yet, e.g. while Curve is still becoming active: otherwise that click only focuses it
// A light wash for what sits on the panel, e.g. its cards: white, so it lightens the panel in light
// mode too, where a wash of the text colour, black, would darken it under black text
static NSColor* panelWash (NSView* view, CGFloat lightAlpha, CGFloat darkAlpha)
{
    const bool dark = [[view effectiveAppearance] bestMatchFromAppearancesWithNames: @[ NSAppearanceNameAqua, NSAppearanceNameDarkAqua ]]
                        == NSAppearanceNameDarkAqua;
    return [[NSColor whiteColor] colorWithAlphaComponent: dark ? darkAlpha : lightAlpha];
}

@interface CurvePanelButton : NSButton
@end
@implementation CurvePanelButton
- (BOOL) acceptsFirstMouse: (NSEvent*) event { return YES; }
@end

@interface CurvePanelSwitch : NSSwitch
@end
@implementation CurvePanelSwitch
- (BOOL) acceptsFirstMouse: (NSEvent*) event { return YES; }
@end

@interface CurvePanelSlider : NSSlider
@end
@implementation CurvePanelSlider
- (BOOL) acceptsFirstMouse: (NSEvent*) event { return YES; }
@end

//==============================================================================
/** A preset, as a row: its name, its key, and whether it's the current one. */
@interface CurvePresetTile : NSView
{
@public
    NSString* name;
    NSString* shortcut;
    BOOL isCurrent, isHovered, isPressed;
    std::function<void()> onClick;
    std::function<NSMenu*()> makeMenu; // Its right-click menu
    id scrollObserver;
}
- (void) refreshHover;
@end

@implementation CurvePresetTile

- (void) dealloc
{
    [self stopObservingScrolls];
    [name release];
    [shortcut release];
    [super dealloc];
}

// Whether the pointer's over it, from where the pointer is now: entering and leaving aren't told when
// the list scrolls under a pointer that's still, so it's also asked then
- (void) refreshHover
{
    const NSPoint p = [self convertPoint: [[self window] mouseLocationOutsideOfEventStream] fromView: nil];
    // Within its bounds, as visibleRect can be more than them: the part of the list that's shown
    const BOOL inside = [self window] != nil && NSPointInRect (p, NSIntersectionRect ([self bounds], [self visibleRect]));

    if (inside != isHovered)
    {
        isHovered = inside;

        if (! inside)
            isPressed = NO;

        [self setNeedsDisplay: YES];
    }
}

- (void) stopObservingScrolls
{
    if (scrollObserver != nil)
    {
        [[NSNotificationCenter defaultCenter] removeObserver: scrollObserver];
        [scrollObserver release];
        scrollObserver = nil;
    }
}

- (void) viewDidMoveToWindow
{
    [super viewDidMoveToWindow];
    [self stopObservingScrolls];

    if (NSClipView* clip = [[self enclosingScrollView] contentView]; clip != nil && [self window] != nil)
    {
        [clip setPostsBoundsChangedNotifications: YES];

        // Not retained by the block, so it can go; it stops observing as it leaves the window
        __block CurvePresetTile* tile = self;
        scrollObserver = [[[NSNotificationCenter defaultCenter] addObserverForName: NSViewBoundsDidChangeNotification
                                                                            object: clip
                                                                             queue: nil
                                                                        usingBlock: ^(NSNotification*) { [tile refreshHover]; }] retain];
    }
}

- (BOOL) isFlipped { return YES; }
- (NSSize) intrinsicContentSize { return NSMakeSize (NSViewNoIntrinsicMetric, 30); }
- (NSMenu*) menuForEvent: (NSEvent*) event { return makeMenu ? makeMenu() : nil; }
- (BOOL) acceptsFirstMouse: (NSEvent*) event { return YES; }

- (void) updateTrackingAreas
{
    for (NSTrackingArea* area in [[[self trackingAreas] copy] autorelease])
        [self removeTrackingArea: area];

    [self addTrackingArea: [[[NSTrackingArea alloc] initWithRect: [self bounds]
                                                        options: NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved | NSTrackingActiveAlways | NSTrackingInVisibleRect
                                                          owner: self
                                                       userInfo: nil] autorelease]];
    [super updateTrackingAreas];
}

- (void) mouseEntered: (NSEvent*) e { [self refreshHover]; }
- (void) mouseExited: (NSEvent*) e  { [self refreshHover]; }
- (void) mouseMoved: (NSEvent*) e   { [self refreshHover]; }
- (void) mouseDown: (NSEvent*) e    { isPressed = YES; [self setNeedsDisplay: YES]; }

- (void) mouseUp: (NSEvent*) e
{
    const bool inside = NSPointInRect ([self convertPoint: [e locationInWindow] fromView: nil], [self bounds]);
    isPressed = NO;
    [self setNeedsDisplay: YES];

    if (inside && onClick)
        onClick();
}

- (void) drawRect: (NSRect) dirty
{
    const NSRect bounds = [self bounds];

    // The current one tinted with the accent, with its checkmark, so it stands out without a solid
    // block of colour or a second edge inside its card's; the one under the pointer a quieter grey
    NSColor* accent = [NSColor controlAccentColor];
    NSBezierPath* shape = [NSBezierPath bezierPathWithRoundedRect: NSInsetRect (bounds, 4.5, 2) xRadius: 6 yRadius: 6];

    if (isCurrent)
    {
        // Across the whole row, to its card's edges, which round its corners, with a gentle gradient
        const CGFloat strength = isPressed ? 1.3 : isHovered ? 1.15 : 1.0;
        NSGradient* tint = [[[NSGradient alloc] initWithStartingColor: [accent colorWithAlphaComponent: 0.36 * strength]
                                                          endingColor: [accent colorWithAlphaComponent: 0.26 * strength]] autorelease];
        [tint drawInRect: bounds angle: 90];
    }
    else if (isPressed || isHovered)
    {
        [[[NSColor labelColor] colorWithAlphaComponent: isPressed ? 0.14 : 0.09] setFill];
        [shape fill];
    }

    // A checkmark for the current one, in a column of its own, so the names line up
    if (isCurrent)
    {
        NSImageSymbolConfiguration* config = [[NSImageSymbolConfiguration configurationWithPointSize: 10 weight: NSFontWeightBold]
                                                 configurationByApplyingConfiguration: [NSImageSymbolConfiguration configurationWithPaletteColors: @[ accent ]]];
        NSImage* check = [[NSImage imageWithSystemSymbolName: @"checkmark" accessibilityDescription: nil] imageWithSymbolConfiguration: config];
        const NSSize size = [check size];
        [check drawInRect: NSMakeRect (12, (bounds.size.height - size.height) / 2, size.width, size.height)];
    }

    NSMutableParagraphStyle* truncating = [[[NSMutableParagraphStyle alloc] init] autorelease];
    [truncating setLineBreakMode: NSLineBreakByTruncatingTail];

    // The key, on a keycap of its own
    NSDictionary* keyAttributes = @{ NSFontAttributeName: [NSFont monospacedDigitSystemFontOfSize: 10 weight: NSFontWeightMedium],
                                     NSForegroundColorAttributeName: isCurrent ? [NSColor labelColor] : [NSColor secondaryLabelColor] };
    const NSSize keySize = shortcut != nil ? [shortcut sizeWithAttributes: keyAttributes] : NSZeroSize;
    const NSRect keycap = NSMakeRect (bounds.size.width - 12 - (keySize.width + 10), (bounds.size.height - 17) / 2, keySize.width + 10, 17);

    NSDictionary* nameAttributes = @{ NSFontAttributeName: [NSFont systemFontOfSize: 12.5 weight: isCurrent ? NSFontWeightSemibold : NSFontWeightRegular],
                                      NSForegroundColorAttributeName: [NSColor labelColor],
                                      NSParagraphStyleAttributeName: truncating };

    const CGFloat left = 30, right = shortcut != nil ? bounds.size.width - NSMinX (keycap) + 6 : 14;
    const NSSize nameSize = [name sizeWithAttributes: nameAttributes];

    [name drawInRect: NSMakeRect (left, (bounds.size.height - nameSize.height) / 2, bounds.size.width - left - right, nameSize.height)
      withAttributes: nameAttributes];

    if (shortcut != nil)
    {
        [(isCurrent ? [accent colorWithAlphaComponent: 0.35] : panelWash (self, 0.75, 0.12)) setFill];
        [[NSBezierPath bezierPathWithRoundedRect: keycap xRadius: 4 yRadius: 4] fill];
        [shortcut drawAtPoint: NSMakePoint (NSMidX (keycap) - keySize.width / 2, NSMidY (keycap) - keySize.height / 2)
               withAttributes: keyAttributes];
    }
}

- (BOOL) isAccessibilityElement             { return YES; }
- (NSAccessibilityRole) accessibilityRole   { return NSAccessibilityButtonRole; }
- (NSString*) accessibilityLabel            { return name; }
- (BOOL) accessibilityPerformPress          { if (onClick) onClick(); return YES; }

@end

//==============================================================================
// A rounded card that rows sit on, as System Settings' and Control Center's grouped lists do. A
// scene group's has its colour down its left edge
@interface CurvePanelCard : NSView
{
@public
    NSColor* edgeColour;
}
@end

@implementation CurvePanelCard
- (void) dealloc { [edgeColour release]; [super dealloc]; }
- (BOOL) isFlipped { return YES; }

- (void) drawRect: (NSRect) dirty
{
    NSBezierPath* shape = [NSBezierPath bezierPathWithRoundedRect: [self bounds] xRadius: 9 yRadius: 9];
    [panelWash (self, 0.36, 0.07) setFill];
    [shape fill];

    if (edgeColour != nil)
    {
        [NSGraphicsContext saveGraphicsState];
        [shape addClip];
        [edgeColour setFill];
        NSRectFill (NSMakeRect (0, 0, 4, [self bounds].size.height));
        [NSGraphicsContext restoreGraphicsState];
    }
}
@end

// A hairline between rows of a card, from where their text starts
@interface CurvePanelRowDivider : NSView
{
@public
    CGFloat inset;
}
@end

@implementation CurvePanelRowDivider
- (NSSize) intrinsicContentSize { return NSMakeSize (NSViewNoIntrinsicMetric, 1); }

- (void) drawRect: (NSRect) dirty
{
    [[NSColor separatorColor] setFill];
    NSRectFillUsingOperation (NSMakeRect (inset, 0, [self bounds].size.width - inset - 10, 1.0 / std::max ((CGFloat) 1, [[self window] backingScaleFactor])),
                              NSCompositingOperationSourceOver);
}
@end

//==============================================================================
//==============================================================================
/** The output's level: a bar for each side, from -60 dB to 0 dB, which falls back smoothly. */
@interface CurvePanelMeter : NSView
{
@public
    float levelsDb[2];
    BOOL vertical; // Side by side, filling upwards, rather than one above the other
}
- (void) showPeaks: (float) left right: (float) right;
@end

@implementation CurvePanelMeter

static constexpr float meterFloorDb = -60.0f, meterFallPerUpdateDb = 1.5f;

- (instancetype) initWithFrame: (NSRect) frame
{
    if ((self = [super initWithFrame: frame]))
        levelsDb[0] = levelsDb[1] = meterFloorDb;

    return self;
}

- (NSSize) intrinsicContentSize { return vertical ? NSMakeSize (9, NSViewNoIntrinsicMetric) : NSMakeSize (NSViewNoIntrinsicMetric, 9); }
- (BOOL) isFlipped              { return YES; }

- (void) showPeaks: (float) left right: (float) right
{
    const float peaks[] = { left, right };
    bool changed = false;

    for (int i = 0; i < 2; ++i)
    {
        // To 0 dB at most, and nothing for a sample that isn't a number
        const auto db = peaks[i] > 0.0f && std::isfinite (peaks[i]) ? juce::jlimit (meterFloorDb, 0.0f, 20.0f * std::log10 (peaks[i]))
                                                                     : meterFloorDb;
        const auto shown = std::max (db, levelsDb[i] - meterFallPerUpdateDb);
        changed = changed || shown != levelsDb[i];
        levelsDb[i] = shown;
    }

    if (changed)
        [self setNeedsDisplay: YES];
}

- (void) drawRect: (NSRect) dirty
{
    const NSRect bounds = [self bounds];
    const CGFloat across = ((vertical ? bounds.size.width : bounds.size.height) - 1) / 2;

    for (int i = 0; i < 2; ++i)
    {
        const NSRect bar = vertical ? NSMakeRect (i * (across + 1), 0, across, bounds.size.height)
                                    : NSMakeRect (0, i * (across + 1), bounds.size.width, across);
        [[[NSColor labelColor] colorWithAlphaComponent: 0.08] setFill];
        [[NSBezierPath bezierPathWithRoundedRect: bar xRadius: 2 yRadius: 2] fill];

        const auto proportion = (levelsDb[i] - meterFloorDb) / -meterFloorDb;

        if (proportion <= 0.0f)
            continue;

        // Green, then amber above -6 dB, red above -1 dB
        NSColor* colour = levelsDb[i] > -1.0f ? [NSColor systemRedColor]
                        : levelsDb[i] > -6.0f ? [NSColor systemOrangeColor]
                                              : [NSColor systemGreenColor];
        [colour setFill];

        // Flipped, so a vertical bar fills up from the bottom
        const NSRect level = vertical ? NSMakeRect (bar.origin.x, bar.size.height * (1.0f - proportion), across, bar.size.height * proportion)
                                      : NSMakeRect (0, bar.origin.y, bar.size.width * proportion, across);
        [[NSBezierPath bezierPathWithRoundedRect: level xRadius: 2 yRadius: 2] fill];
    }
}

- (BOOL) isAccessibilityElement { return NO; }

@end

//==============================================================================
/** A Meter plug-in's measurements, under its row in the panel when it's opened up, as its window
    shows them: levels (peak, with RMS within it, and the peak held), VU, or loudness. */
@interface CurvePanelMeterDetail : NSView
{
@public
    MenuBarPanelState::MeterReading reading;
    float peakDb[2], heldDb[2], heldTruePeakDb;
    int heldFor[2], truePeakHeldFor;
}
- (void) show: (const MenuBarPanelState::MeterReading&) newReading;
+ (CGFloat) heightFor: (MenuBarPanelState::MeterReading::Mode) mode;
@end

@implementation CurvePanelMeterDetail

static constexpr float detailFloorDb = -60.0f;

- (instancetype) initWithFrame: (NSRect) frame
{
    if ((self = [super initWithFrame: frame]))
    {
        for (int i = 0; i < 2; ++i)
        {
            peakDb[i] = heldDb[i] = detailFloorDb;
            heldFor[i] = 0;
        }

        heldTruePeakDb = detailFloorDb;
        truePeakHeldFor = 0;
    }

    return self;
}

- (BOOL) isFlipped { return YES; }

+ (CGFloat) heightFor: (MenuBarPanelState::MeterReading::Mode) mode
{
    using Mode = MenuBarPanelState::MeterReading::Mode;
    return mode == Mode::loudness ? 50 : mode == Mode::vu ? 42 : 60;
}

static float toDb (float linear)
{
    return linear > 0.0f && std::isfinite (linear) ? 20.0f * std::log10 (linear) : -200.0f;
}

- (void) show: (const MenuBarPanelState::MeterReading&) newReading
{
    reading = newReading;

    for (int i = 0; i < 2; ++i)
    {
        // Falling back gently, with the highest held for a second and a half
        const auto db = std::max (detailFloorDb, std::min (0.0f, toDb (reading.peak[i])));
        peakDb[i] = std::max (db, peakDb[i] - 1.5f);

        if (db >= heldDb[i])
        {
            heldDb[i] = db;
            heldFor[i] = 45;
        }
        else if (--heldFor[i] <= 0)
        {
            heldDb[i] = std::max (detailFloorDb, heldDb[i] - 1.5f);
        }
    }

    // The higher side's true peak, held as the peaks are
    const auto truePeak = std::min (6.0f, toDb (std::max (reading.truePeak[0], reading.truePeak[1])));

    if (truePeak >= heldTruePeakDb)
    {
        heldTruePeakDb = truePeak;
        truePeakHeldFor = 45;
    }
    else if (--truePeakHeldFor <= 0)
    {
        heldTruePeakDb = std::max (detailFloorDb, heldTruePeakDb - 1.5f);
    }

    [self setNeedsDisplay: YES];
}

static NSColor* levelColour (float db)
{
    return db > -1.0f ? [NSColor systemRedColor] : db > -6.0f ? [NSColor systemOrangeColor] : [NSColor systemGreenColor];
}

- (void) drawRect: (NSRect) dirty
{
    using Mode = MenuBarPanelState::MeterReading::Mode;
    const NSRect bounds = [self bounds];

    NSDictionary* small = @{ NSFontAttributeName: [NSFont systemFontOfSize: 9.5 weight: NSFontWeightMedium],
                             NSForegroundColorAttributeName: [NSColor secondaryLabelColor] };

    if (reading.mode == Mode::loudness)
    {
        // Momentary, short-term and integrated, side by side, as numbers
        const float values[] = { reading.momentary, reading.shortTerm, reading.integrated };
        NSString* names[] = { @"MOMENTARY", @"SHORT-TERM", @"INTEGRATED" };
        const CGFloat left = 30, width = (bounds.size.width - left - 12) / 3;

        for (int i = 0; i < 3; ++i)
        {
            const CGFloat x = left + i * width;
            NSString* value = values[i] > -100.0f ? [NSString stringWithFormat: @"%.1f", values[i]] : @"–";
            NSDictionary* big = @{ NSFontAttributeName: [NSFont monospacedDigitSystemFontOfSize: 15 weight: NSFontWeightMedium],
                                   NSForegroundColorAttributeName: [NSColor labelColor] };
            [value drawAtPoint: NSMakePoint (x, 6) withAttributes: big];
            [names[i] drawAtPoint: NSMakePoint (x, 28) withAttributes: small];
        }

        [@"LUFS" drawAtPoint: NSMakePoint (bounds.size.width - 12 - [@"LUFS" sizeWithAttributes: small].width, 9) withAttributes: small];
        return;
    }

    // Levels, or VU, as a bar for each side, with its number at the right
    const bool isVu = reading.mode == Mode::vu;
    const CGFloat left = 30, numberWidth = 54, barRight = bounds.size.width - 12 - numberWidth - 6;

    for (int i = 0; i < 2; ++i)
    {
        const CGFloat y = 7 + i * 16;
        [(i == 0 ? @"L" : @"R") drawAtPoint: NSMakePoint (left - 12, y - 1) withAttributes: small];

        const NSRect track = NSMakeRect (left, y + 1, barRight - left, 8);
        [[[NSColor labelColor] colorWithAlphaComponent: 0.08] setFill];
        [[NSBezierPath bezierPathWithRoundedRect: track xRadius: 2 yRadius: 2] fill];

        NSString* number = nil;
        NSColor* numberColour = [NSColor labelColor];

        if (isVu)
        {
            // -20 to +3 VU, from the reference that reads 0 VU
            const auto vu = toDb (reading.vu[i]) - (float) reading.vuReferenceDb;
            const auto proportion = std::clamp ((vu + 20.0f) / 23.0f, 0.0f, 1.0f);
            const auto zero = track.origin.x + track.size.width * (20.0f / 23.0f);

            [(vu > 0.0f ? [NSColor systemRedColor] : [NSColor systemGreenColor]) setFill];
            [[NSBezierPath bezierPathWithRoundedRect: NSMakeRect (track.origin.x, track.origin.y, track.size.width * proportion, track.size.height)
                                             xRadius: 2 yRadius: 2] fill];
            [[[NSColor labelColor] colorWithAlphaComponent: 0.35] setFill];
            NSRectFill (NSMakeRect (zero, track.origin.y - 1, 1, track.size.height + 2));

            number = vu > -40.0f ? [NSString stringWithFormat: @"%+.1f VU", vu] : @"– VU";
        }
        else
        {
            const auto proportion = [&] (float db) { return std::clamp ((db - detailFloorDb) / -detailFloorDb, 0.0f, 1.0f); };
            const auto rmsDb = std::max (detailFloorDb, toDb (reading.rms[i]));

            // The peak, with the RMS within it darker, and the highest held as a line
            [levelColour (peakDb[i]) setFill];
            [[NSBezierPath bezierPathWithRoundedRect: NSMakeRect (track.origin.x, track.origin.y, track.size.width * proportion (peakDb[i]), track.size.height)
                                             xRadius: 2 yRadius: 2] fill];
            [[[NSColor blackColor] colorWithAlphaComponent: 0.25] setFill];
            [[NSBezierPath bezierPathWithRoundedRect: NSMakeRect (track.origin.x, track.origin.y, track.size.width * proportion (rmsDb), track.size.height)
                                             xRadius: 2 yRadius: 2] fill];

            if (heldDb[i] > detailFloorDb)
            {
                [levelColour (heldDb[i]) setFill];
                NSRectFill (NSMakeRect (track.origin.x + track.size.width * proportion (heldDb[i]) - 1, track.origin.y, 2, track.size.height));
            }

            number = heldDb[i] > detailFloorDb ? [NSString stringWithFormat: @"%.1f dB", heldDb[i]] : @"– dB";

            if (reading.over[i])
                numberColour = [NSColor systemRedColor];
        }

        NSDictionary* numberAttributes = @{ NSFontAttributeName: [NSFont monospacedDigitSystemFontOfSize: 10.5 weight: NSFontWeightRegular],
                                            NSForegroundColorAttributeName: numberColour };
        const NSSize size = [number sizeWithAttributes: numberAttributes];
        [number drawAtPoint: NSMakePoint (bounds.size.width - 12 - size.width, y - 1) withAttributes: numberAttributes];
    }

    if (isVu)
        return;

    // What's worked out from them, as the meter's window has it: RMS, true peak and correlation
    NSDictionary* value = @{ NSFontAttributeName: [NSFont monospacedDigitSystemFontOfSize: 10.5 weight: NSFontWeightMedium],
                             NSForegroundColorAttributeName: [NSColor labelColor] };
    NSDictionary* overValue = @{ NSFontAttributeName: [NSFont monospacedDigitSystemFontOfSize: 10.5 weight: NSFontWeightMedium],
                                 NSForegroundColorAttributeName: [NSColor systemRedColor] };

    const auto describeDb = [] (float linear)
    {
        const auto db = toDb (linear);
        return db > detailFloorDb ? [NSString stringWithFormat: @"%.1f", db] : @"–";
    };

    NSString* rms = reading.numChannels > 1 ? [NSString stringWithFormat: @"%@ / %@", describeDb (reading.rms[0]), describeDb (reading.rms[1])]
                                            : describeDb (reading.rms[0]);
    NSString* truePeak = heldTruePeakDb > detailFloorDb ? [NSString stringWithFormat: @"%.1f", heldTruePeakDb] : @"–";
    NSString* correlation = reading.numChannels > 1 ? [NSString stringWithFormat: @"%+.2f", reading.correlation] : @"–";

    struct Item { NSString* name; NSString* text; NSDictionary* attributes; };
    const Item items[] = { { @"RMS", rms, value },
                           { @"TRUE PEAK", truePeak, heldTruePeakDb > -1.0f ? overValue : value },
                           { @"CORR", correlation, reading.correlation < 0.0f && reading.numChannels > 1 ? overValue : value } };

    // RMS has both sides, so the most room
    const CGFloat y = 41, width = bounds.size.width - 18 - 12;
    const CGFloat starts[] = { 18, 18 + width * 0.42f, 18 + width * 0.73f };

    for (int i = 0; i < 3; ++i)
    {
        const CGFloat x = starts[i];
        [items[i].name drawAtPoint: NSMakePoint (x, y + 1) withAttributes: small];
        const CGFloat nameWidth = [items[i].name sizeWithAttributes: small].width;
        [items[i].text drawAtPoint: NSMakePoint (x + nameWidth + 5, y) withAttributes: items[i].attributes];
    }
}

- (BOOL) isAccessibilityElement { return NO; }

@end

//==============================================================================
/** An output switch: an icon and a name in a pill, filled with its own colour while it's on. */
@interface CurvePanelToggle : NSView
{
@public
    NSString* title;
    NSString* symbol;
    NSColor* onColour;
    BOOL isOn, isHovered, isPressed;
    BOOL compact; // Smaller, e.g. in a section's heading
    std::function<void (bool)> onChange;
}
@end

@implementation CurvePanelToggle

- (void) dealloc
{
    [title release];
    [symbol release];
    [onColour release];
    [super dealloc];
}

- (BOOL) isFlipped { return YES; }
- (NSDictionary*) titleAttributes
{
    return @{ NSFontAttributeName: [NSFont systemFontOfSize: compact ? 11 : 12 weight: NSFontWeightMedium],
              NSForegroundColorAttributeName: isOn ? [NSColor whiteColor] : [NSColor labelColor] };
}

// Compact ones fit their title; the others fill what they're given
- (NSSize) intrinsicContentSize
{
    if (compact)
        return NSMakeSize (std::ceil ([title sizeWithAttributes: [self titleAttributes]].width) + 20, 20);

    return NSMakeSize (NSViewNoIntrinsicMetric, 26);
}

- (BOOL) acceptsFirstMouse: (NSEvent*) event { return YES; }

- (void) updateTrackingAreas
{
    for (NSTrackingArea* area in [[[self trackingAreas] copy] autorelease])
        [self removeTrackingArea: area];

    [self addTrackingArea: [[[NSTrackingArea alloc] initWithRect: [self bounds]
                                                        options: NSTrackingMouseEnteredAndExited | NSTrackingActiveAlways | NSTrackingInVisibleRect
                                                          owner: self
                                                       userInfo: nil] autorelease]];
    [super updateTrackingAreas];
}

- (void) mouseEntered: (NSEvent*) e { isHovered = YES; [self setNeedsDisplay: YES]; }
- (void) mouseExited: (NSEvent*) e  { isHovered = NO; isPressed = NO; [self setNeedsDisplay: YES]; }
- (void) mouseDown: (NSEvent*) e    { isPressed = YES; [self setNeedsDisplay: YES]; }

- (void) mouseUp: (NSEvent*) e
{
    const bool inside = NSPointInRect ([self convertPoint: [e locationInWindow] fromView: nil], [self bounds]);
    isPressed = NO;

    if (inside)
    {
        isOn = ! isOn;

        if (onChange)
            onChange (isOn);
    }

    [self setNeedsDisplay: YES];
}

- (void) drawRect: (NSRect) dirty
{
    const NSRect bounds = [self bounds];
    const CGFloat radius = compact ? 6 : 7;
    NSBezierPath* shape = [NSBezierPath bezierPathWithRoundedRect: NSInsetRect (bounds, 0.5, 0.5) xRadius: radius yRadius: radius];

    NSColor* fill = isOn ? [onColour colorWithAlphaComponent: isPressed ? 0.7 : isHovered ? 0.95 : 0.85]
                         : isPressed || isHovered ? [[NSColor labelColor] colorWithAlphaComponent: isPressed ? 0.16 : 0.11]
                                                  : panelWash (self, 0.45, 0.08);
    [fill setFill];
    [shape fill];

    NSColor* ink = isOn ? [NSColor whiteColor] : [NSColor secondaryLabelColor];

    NSImageSymbolConfiguration* config = [[NSImageSymbolConfiguration configurationWithPointSize: 11 weight: NSFontWeightSemibold]
                                             configurationByApplyingConfiguration: [NSImageSymbolConfiguration configurationWithHierarchicalColor: ink]];
    NSImage* icon = symbol != nil ? [[NSImage imageWithSystemSymbolName: symbol accessibilityDescription: nil] imageWithSymbolConfiguration: config]
                                  : nil;

    NSDictionary* attributes = [self titleAttributes];
    const NSSize textSize = [title sizeWithAttributes: attributes];
    const NSSize iconSize = icon != nil ? [icon size] : NSZeroSize;
    const CGFloat gap = icon != nil ? 5 : 0;
    CGFloat x = (bounds.size.width - (iconSize.width + gap + textSize.width)) / 2;

    if (icon != nil)
    {
        [icon drawInRect: NSMakeRect (x, (bounds.size.height - iconSize.height) / 2, iconSize.width, iconSize.height)
                fromRect: NSZeroRect operation: NSCompositingOperationSourceOver fraction: 1.0 respectFlipped: YES hints: nil];
        x += iconSize.width + gap;
    }

    [title drawAtPoint: NSMakePoint (x, (bounds.size.height - textSize.height) / 2) withAttributes: attributes];
}

- (BOOL) isAccessibilityElement             { return YES; }
- (NSAccessibilityRole) accessibilityRole   { return NSAccessibilityCheckBoxRole; }
- (NSString*) accessibilityLabel            { return title; }
- (id) accessibilityValue                   { return @(isOn ? 1 : 0); }
- (BOOL) accessibilityPerformPress          { isOn = ! isOn; if (onChange) onChange (isOn); [self setNeedsDisplay: YES]; return YES; }

@end

//==============================================================================
/** A plug-in's row, which right-clicks for its menu. */
@interface CurvePluginRow : NSStackView
{
@public
    std::function<NSMenu*()> makeMenu;
}
@end

@implementation CurvePluginRow
- (NSMenu*) menuForEvent: (NSEvent*) event
{
    return makeMenu ? makeMenu() : [super menuForEvent: event];
}
@end

//==============================================================================
@interface CurvePanelPopoverDelegate : NSObject <NSPopoverDelegate>
{
@public
    std::function<void()> onClose;
}
@end

@implementation CurvePanelPopoverDelegate
- (void) popoverDidClose: (NSNotification*) notification
{
    if (onClose)
        onClose();
}
@end

//==============================================================================
struct MenuBarPanel::Impl : private juce::Timer
{
    Impl (MenuBarPanelState::Source& s, Callbacks c) : source (s), callbacks (std::move (c))
    {
        actions = [[NSMutableArray alloc] init];
        delegate = [[CurvePanelPopoverDelegate alloc] init];
        delegate->onClose = [this] { closed(); };

        source.onChange = [this] { if (isOpen()) rebuild(); };
    }

    ~Impl() override
    {
        *alive = false;
        source.setMetering (false);
        stopWatching();
        stopTimer();
        source.onChange = nullptr;
        source.setListening (false);

        if (popover != nil)
        {
            [popover setDelegate: nil];
            [popover close];
            [popover release];
        }

        [controller release];
        [delegate release];
        [actions release];
    }

    bool isOpen() const { return popover != nil && [popover isShown]; }

    void toggle (void* statusItemHandle)
    {
        if (isOpen())
        {
            [popover performClose: nil];
            return;
        }

        // Clicking the icon closes the open panel before the click reaches Curve, so it would open again
        if (juce::Time::getMillisecondCounter() - lastCloseTime < 300)
            return;

        auto* statusItem = (NSStatusItem*) statusItemHandle;
        NSStatusBarButton* button = statusItem != nil ? [statusItem button] : nil;

        if (button == nil)
            return;

        if (popover == nil)
        {
            controller = [[NSViewController alloc] init];
            root = [[[CurvePanelView alloc] initWithFrame: NSMakeRect (0, 0, panelWidth, 100)] autorelease];
            root->onKey = [this] (NSEvent* e) { return handleKey (e); };
            root->onEscape = [this] { close(); };
            [controller setView: root];

            popover = [[NSPopover alloc] init];
            // Closed by Curve rather than by macOS, which closes a transient popover whenever
            // another window takes the keys, as many plug-ins' windows do as they open
            [popover setBehavior: NSPopoverBehaviorApplicationDefined];
            [popover setAnimates: NO];
            [popover setContentViewController: controller];
            [popover setDelegate: delegate];
        }

        // The screen the menu bar icon is on, which may not be the main one
        if (NSScreen* screen = [[button window] screen] != nil ? [[button window] screen] : [NSScreen mainScreen])
            availableHeight = NSHeight ([screen visibleFrame]) - 24;

        // Light or dark, or as macOS is, as set in Preferences
        switch (MenuBarPanel::getAppearance())
        {
            case MenuBarPanel::Appearance::light:  [popover setAppearance: [NSAppearance appearanceNamed: NSAppearanceNameAqua]]; break;
            case MenuBarPanel::Appearance::dark:   [popover setAppearance: [NSAppearance appearanceNamed: NSAppearanceNameDarkAqua]]; break;
            case MenuBarPanel::Appearance::system: [popover setAppearance: nil]; break;
        }

        source.setListening (true);
        source.setMetering (getSetting (showMeterSetting, true));
        rebuild();

        // Curve has no Dock icon by default, so it's made active for the panel to take keys
        [NSApp activateIgnoringOtherApps: YES];
        [popover showRelativeToRect: [button bounds] ofView: button preferredEdge: NSRectEdgeMinY];
        [[root window] makeFirstResponder: root];
        watchForClicksOutside();

        startTimer (33);
    }

    void close()
    {
        if (isOpen())
            [popover performClose: nil];
    }

private:
    static constexpr CGFloat panelWidth = 320, margin = 12;
    static constexpr CGFloat tileHeight = 30, tileGap = 1, pluginRowHeight = 30, pluginRowGap = 1, cardPadding = 3;

    // How tall the panel can be on the screen it opens on: below the menu bar, with room for its arrow
    CGFloat availableHeight = 800;

    // The lists' heights, which shrink, presets first, if the panel would be taller than that
    NSLayoutConstraint* presetsHeight = nil;
    NSLayoutConstraint* pluginsHeight = nil;

    // How tall a list may be before it scrolls: a share of the screen's height, and no more than a
    // few rows, so the panel stays short however many presets and plug-ins there are
    CGFloat getListHeightLimit (double share, int maxRows, CGFloat rowPitch) const
    {
        return std::max (3 * rowPitch, std::min ((CGFloat) share * availableHeight, maxRows * rowPitch));
    }

    // A list in a scroll view as tall as it is, up to maxHeight, which can be made shorter later
    NSScrollView* makeScrollingList (NSView* list, CGFloat contentHeight, CGFloat maxHeight, CGFloat trailingRoom,
                                     NSLayoutConstraint*& heightConstraint)
    {
        NSScrollView* scroll = [[[NSScrollView alloc] init] autorelease];
        [scroll setHasVerticalScroller: YES];
        [scroll setDrawsBackground: NO];
        [scroll setAutohidesScrollers: YES];

        CurvePanelView* document = [[[CurvePanelView alloc] init] autorelease]; // flipped, so the list starts at the top
        [document setTranslatesAutoresizingMaskIntoConstraints: NO];
        [list setTranslatesAutoresizingMaskIntoConstraints: NO];
        [document addSubview: list];
        [scroll setDocumentView: document];

        NSClipView* clip = [scroll contentView];
        heightConstraint = [[scroll heightAnchor] constraintEqualToConstant: std::min (contentHeight, maxHeight)];

        [NSLayoutConstraint activateConstraints: @[
            [[list leadingAnchor] constraintEqualToAnchor: [document leadingAnchor]],
            [[list trailingAnchor] constraintEqualToAnchor: [document trailingAnchor] constant: -trailingRoom],
            [[list topAnchor] constraintEqualToAnchor: [document topAnchor]],
            [[list bottomAnchor] constraintEqualToAnchor: [document bottomAnchor]],
            [[document leadingAnchor] constraintEqualToAnchor: [clip leadingAnchor]],
            [[document trailingAnchor] constraintEqualToAnchor: [clip trailingAnchor]],
            [[document topAnchor] constraintEqualToAnchor: [clip topAnchor]],
            heightConstraint
        ]];

        return scroll;
    }

    // Rows on a rounded card, with hairlines between them from where their text starts
    static NSView* makeCard (NSArray<NSView*>* rows, CGFloat dividerInset, NSColor* edgeColour = nil, CGFloat padding = cardPadding)
    {
        CurvePanelCard* card = [[[CurvePanelCard alloc] init] autorelease];

        // Its rows' highlights, e.g. the current preset's, go to its edges, rounded by its corners
        [card setWantsLayer: YES];
        [[card layer] setCornerRadius: 9];
        [[card layer] setMasksToBounds: YES];
        card->edgeColour = [edgeColour retain];
        [card setTranslatesAutoresizingMaskIntoConstraints: NO];

        NSStackView* stack = [[[NSStackView alloc] init] autorelease];
        [stack setOrientation: NSUserInterfaceLayoutOrientationVertical];
        [stack setAlignment: NSLayoutAttributeLeading];
        [stack setSpacing: 0];
        [stack setEdgeInsets: NSEdgeInsetsMake (padding, 0, padding, 0)];
        [stack setTranslatesAutoresizingMaskIntoConstraints: NO];
        [card addSubview: stack];

        [NSLayoutConstraint activateConstraints: @[
            [[stack leadingAnchor] constraintEqualToAnchor: [card leadingAnchor]],
            [[stack trailingAnchor] constraintEqualToAnchor: [card trailingAnchor]],
            [[stack topAnchor] constraintEqualToAnchor: [card topAnchor]],
            [[stack bottomAnchor] constraintEqualToAnchor: [card bottomAnchor]]
        ]];

        for (NSUInteger i = 0; i < [rows count]; ++i)
        {
            // Not between a Meter plug-in and its measurements, which belong to its row
            if (i > 0 && ! [[rows objectAtIndex: i] isKindOfClass: [CurvePanelMeterDetail class]])
            {
                CurvePanelRowDivider* divider = [[[CurvePanelRowDivider alloc] init] autorelease];
                divider->inset = dividerInset;
                [stack addArrangedSubview: divider];
                [[[divider widthAnchor] constraintEqualToAnchor: [stack widthAnchor]] setActive: YES];
                [[[divider heightAnchor] constraintEqualToConstant: 1] setActive: YES];
            }

            NSView* row = [rows objectAtIndex: i];
            [stack addArrangedSubview: row];
            [[[row widthAnchor] constraintEqualToAnchor: [stack widthAnchor]] setActive: YES];
        }

        return card;
    }

    MenuBarPanelState::Source& source;
    Callbacks callbacks;

    NSPopover* popover = nil;
    NSViewController* controller = nil;
    CurvePanelView* root = nil; // Owned by the controller
    CurvePanelPopoverDelegate* delegate = nil;
    NSMutableArray* actions = nil;

    CurvePanelMeter* meter = nil;  // Owned by the view hierarchy, while the panel shows it
    int ticksSinceVolume = 0;
    std::function<void()> scrollAfterLayout;
    int volumeKind = 0; // What the slider shown sets
    NSSlider* volumeSlider = nil;
    NSTextField* volumeLabel = nil;
    NSButton* muteButton = nil;
    NSTextField* tempoField = nil;
    NSButton* playButton = nil;
    // Meter plug-ins' levels: a bar in the row, and the rest under it if it's opened up
    struct PluginMeter { juce::uint32 id; CurvePanelMeter* bar; CurvePanelMeterDetail* detail; };
    std::vector<PluginMeter> pluginMeters;
    std::set<juce::uint32> expandedMeters; // Opened up, until Curve quits

    juce::Array<juce::File> presetFiles; // In the order of their keys

    // For callbacks that may run after this has gone
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);

    void callAsyncWhileAlive (std::function<void()> f)
    {
        juce::MessageManager::callAsync ([weak = std::weak_ptr<bool> (alive), f = std::move (f)]
        {
            if (auto isAlive = weak.lock(); isAlive != nullptr && *isAlive)
                f();
        });
    }
    juce::uint32 lastCloseTime = 0;

    // Closes the panel on a click anywhere outside it: in another app, or in another of Curve's
    // windows (including a plug-in's, once it's been opened), and when Curve stops being active
    void watchForClicksOutside()
    {
        stopWatching();

        const auto mask = NSEventMaskLeftMouseDown | NSEventMaskRightMouseDown | NSEventMaskOtherMouseDown;

        globalMonitor = [[NSEvent addGlobalMonitorForEventsMatchingMask: mask handler: ^(NSEvent* e)
        {
            // A click on the panel itself, said to be another app's, e.g. just after Curve's Dock icon
            // has gone, as macOS catches up with that: not outside it, so it stays open
            if (NSPointInRect ([NSEvent mouseLocation], [[root window] frame]))
                return;

            close();
        }] retain];

        localMonitor = [[NSEvent addLocalMonitorForEventsMatchingMask: mask handler: ^NSEvent* (NSEvent* e)
        {
            // Within the panel, and its own menus, which are windows of their own
            if ([e window] != [root window] && ! [[e window] isKindOfClass: NSClassFromString (@"NSMenuWindowManagerWindow")]
                && [[e window] level] < NSPopUpMenuWindowLevel)
                close();

            return e;
        }] retain];

        resignObserver = [[[NSNotificationCenter defaultCenter] addObserverForName: NSApplicationDidResignActiveNotification
                                                                            object: nil
                                                                             queue: nil // posted on the main thread, so run there and then
                                                                        usingBlock: ^(NSNotification*) { close(); }] retain];
    }

    void stopWatching()
    {
        if (globalMonitor != nil)  { [NSEvent removeMonitor: globalMonitor]; [globalMonitor release]; globalMonitor = nil; }
        if (localMonitor != nil)   { [NSEvent removeMonitor: localMonitor];  [localMonitor release];  localMonitor = nil; }

        if (resignObserver != nil)
        {
            [[NSNotificationCenter defaultCenter] removeObserver: resignObserver];
            [resignObserver release];
            resignObserver = nil;
        }
    }

    id globalMonitor = nil, localMonitor = nil, resignObserver = nil;

    void closed()
    {
        stopWatching();
        stopTimer();
        source.setListening (false);
        source.setMetering (false);
        meter = nil;
        lastCloseTime = juce::Time::getMillisecondCounter();
    }

    void timerCallback() override
    {
        if (meter != nil)
        {
            float left = 0.0f, right = 0.0f;
            source.readOutputPeaks (left, right);
            [meter showPeaks: left right: right];
        }

        for (auto& m : pluginMeters)
        {
            const auto reading = source.readMeter (m.id);
            if (m.bar != nil)
                [m.bar showPeaks: reading.peak[0] right: reading.peak[1]];

            if (m.detail != nil)
                [m.detail show: reading];
        }

        // The volume can change from the volume keys while the panel is open, and the tempo and
        // transport from MIDI or the editor
        if ((volumeSlider != nil || tempoField != nil) && ++ticksSinceVolume >= 8)
        {
            ticksSinceVolume = 0;
            const auto state = source.getState();

            if (volumeSlider != nil && [[NSApp currentEvent] type] != NSEventTypeLeftMouseDragged)
                updateVolume (state);

            updateTempo (state);
        }
    }

    //==============================================================================
    CurvePanelAction* makeAction (std::function<void (id)> f)
    {
        auto* action = [[[CurvePanelAction alloc] init] autorelease];
        action->function = std::move (f);
        [actions addObject: action];
        return action;
    }

    static NSTextField* makeLabel (NSString* text, CGFloat size, NSFontWeight weight, NSColor* colour)
    {
        NSTextField* label = [NSTextField labelWithString: text];
        [label setFont: [NSFont systemFontOfSize: size weight: weight]];
        [label setTextColor: colour];
        [label setLineBreakMode: NSLineBreakByTruncatingTail];
        [label setContentCompressionResistancePriority: NSLayoutPriorityDefaultLow forOrientation: NSLayoutConstraintOrientationHorizontal];
        return label;
    }

    NSButton* makeSymbolButton (NSString* symbol, NSString* description, std::function<void (id)> f)
    {
        NSImage* image = [NSImage imageWithSystemSymbolName: symbol accessibilityDescription: description];
        auto* action = makeAction (std::move (f));
        NSButton* button = [CurvePanelButton buttonWithImage: image target: action action: @selector (perform:)];
        [button setBordered: NO];
        [button setToolTip: description];
        [button setContentTintColor: [NSColor secondaryLabelColor]];
        return button;
    }

    static NSView* makeSeparator()
    {
        NSBox* line = [[[NSBox alloc] init] autorelease];
        [line setBoxType: NSBoxSeparator];
        return line;
    }

    static NSTextField* makeSectionHeader (NSString* text)
    {
        return makeLabel ([text uppercaseString], 10, NSFontWeightSemibold, [NSColor tertiaryLabelColor]);
    }

    static NSStackView* makeRow (NSArray<NSView*>* views)
    {
        NSStackView* row = [NSStackView stackViewWithViews: views];
        [row setOrientation: NSUserInterfaceLayoutOrientationHorizontal];
        [row setAlignment: NSLayoutAttributeCenterY];
        [row setSpacing: 8];
        return row;
    }

    static NSView* makeSpacer()
    {
        NSView* spacer = [[[NSView alloc] init] autorelease];
        [spacer setContentHuggingPriority: 1 forOrientation: NSLayoutConstraintOrientationHorizontal];
        return spacer;
    }

    //==============================================================================
    void rebuild()
    {
        if (root == nil)
            return;

        const auto state = source.getState();

        for (NSView* v in [[[root subviews] copy] autorelease])
            [v removeFromSuperview];

        [actions removeAllObjects];
        presetsHeight = nil;
        pluginsHeight = nil;
        meter = nil;
        volumeSlider = nil;
        tempoField = nil;
        playButton = nil;
        pluginMeters.clear();
        volumeLabel = nil;
        muteButton = nil;

        NSStackView* stack = [[[NSStackView alloc] init] autorelease];
        [stack setOrientation: NSUserInterfaceLayoutOrientationVertical];
        [stack setAlignment: NSLayoutAttributeLeading];
        [stack setSpacing: 10];
        [stack setEdgeInsets: NSEdgeInsetsMake (margin, margin, margin, margin)];
        [stack setTranslatesAutoresizingMaskIntoConstraints: NO];
        [root addSubview: stack];

        [NSLayoutConstraint activateConstraints: @[
            [[stack leadingAnchor] constraintEqualToAnchor: [root leadingAnchor]],
            [[stack trailingAnchor] constraintEqualToAnchor: [root trailingAnchor]],
            [[stack topAnchor] constraintEqualToAnchor: [root topAnchor]],
            [[stack widthAnchor] constraintEqualToConstant: panelWidth]
        ]];

        const auto addFullWidth = [stack] (NSView* view)
        {
            [stack addArrangedSubview: view];
            [[[view widthAnchor] constraintEqualToAnchor: [stack widthAnchor] constant: -2 * margin] setActive: YES];
        };

        addFullWidth (makeHeader (state));
        addFullWidth (makeSeparator());
        addPresets (state, addFullWidth);

        if (getSetting (showPluginsSetting, true) && ! state.plugins.empty())
        {
            addFullWidth (makeSeparator());
            addPlugins (state, addFullWidth);
        }

        if (getSetting (showTempoSetting, false) && state.deviceName.isNotEmpty())
        {
            addFullWidth (makeSeparator());
            addFullWidth (makeTempo (state));
        }

        addOutput (state, addFullWidth);

        addFullWidth (makeSeparator());
        addFullWidth (makeFooter (state));

        // Frosted, as macOS's menus and the menu bar's own panels are; the popover's own clearer glass;
        // or solid, as set in Preferences
        const auto background = MenuBarPanel::getBackground();
        [root setWantsLayer: YES];
        [[root layer] setBackgroundColor: background == MenuBarPanel::Background::solid ? [[NSColor windowBackgroundColor] CGColor] : nil];

        if (background == MenuBarPanel::Background::frosted)
        {
            NSVisualEffectView* frost = [[[NSVisualEffectView alloc] initWithFrame: [root bounds]] autorelease];
            [frost setMaterial: NSVisualEffectMaterialMenu];
            [frost setBlendingMode: NSVisualEffectBlendingModeBehindWindow];
            [frost setState: NSVisualEffectStateActive];
            [frost setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable];
            [root addSubview: frost positioned: NSWindowBelow relativeTo: nil];
        }

        // Meter plug-ins measure for the panel only while metering's on
        if (! pluginMeters.empty())
            source.setMetering (true);

        [root layoutSubtreeIfNeeded];

        // Taller than the screen allows: the lists made shorter, presets first, down to a few rows each
        if (auto excess = [stack fittingSize].height - availableHeight; excess > 0)
        {
            for (auto [constraint, minHeight] : { std::pair { presetsHeight, 3 * (tileHeight + tileGap) - tileGap },
                                                  { pluginsHeight, 3 * (pluginRowHeight + pluginRowGap) - pluginRowGap } })
            {
                if (constraint == nil || excess <= 0)
                    continue;

                const CGFloat by = std::min (excess, [constraint constant] - minHeight);

                if (by > 0)
                {
                    [constraint setConstant: [constraint constant] - by];
                    excess -= by;
                }
            }

            [root layoutSubtreeIfNeeded];
        }

        if (auto scroll = std::exchange (scrollAfterLayout, nullptr))
            scroll();

        const NSSize size = NSMakeSize (panelWidth, [stack fittingSize].height);
        [root setFrameSize: size];
        [popover setContentSize: size];
    }

    // Which interface the audio goes to, which matters more day to day than its buffer size
    NSView* makeHeader (const MenuBarPanelState::State& state)
    {
        NSTextField* title = makeLabel (@"Curve", 13, NSFontWeightBold, [NSColor labelColor]);

        juce::String device, tooltip;

        if (state.deviceName.isEmpty())
        {
            device = "No audio interface";
            tooltip = "Choose one in Audio Settings";
        }
        else
        {
            // 48 kHz, but 44.1 kHz
            const auto hz = juce::roundToInt (state.sampleRate);
            const auto rate = juce::String (state.sampleRate / 1000.0, hz % 1000 == 0 ? 0 : 1) + " kHz";
            device = state.deviceName + " " + juce::String::fromUTF8 ("\xc2\xb7") + " " + rate;
            tooltip = state.deviceName + ", " + rate + ", " + juce::String (state.bufferSize) + " samples";

            if (state.latencySamples > 0 && state.sampleRate > 0.0)
                tooltip << ", plug-in latency " << juce::String (1000.0 * state.latencySamples / state.sampleRate, 1) << " ms";
        }

        NSTextField* deviceLabel = makeLabel (toNS (device), 11, NSFontWeightRegular, [NSColor secondaryLabelColor]);
        [deviceLabel setToolTip: toNS (tooltip)];

        NSButton* gear = makeSymbolButton (@"gearshape", @"Settings", [this] (id sender) { showSettingsMenu ((NSView*) sender); });

        return makeRow (@[ title, deviceLabel, makeSpacer(), gear ]);
    }

    void showSettingsMenu (NSView* anchor)
    {
        NSMenu* menu = [[[NSMenu alloc] init] autorelease];

        const auto add = [this, menu] (NSString* title, std::function<void()> f)
        {
            auto* action = makeAction ([this, f] (id)
            {
                close();
                juce::MessageManager::callAsync (f); // After the menu and panel have closed. Only app-wide callbacks
            });

            NSMenuItem* item = [[[NSMenuItem alloc] initWithTitle: title action: @selector (perform:) keyEquivalent: @""] autorelease];
            [item setTarget: action];
            [item setRepresentedObject: action]; // An item doesn't keep its target, and the panel can rebuild while its menu is open
            [menu addItem: item];
        };

        add (@"Audio Settings", callbacks.showAudioSettings);
        add (@"Plug-in Manager", callbacks.showPluginManager);
        add (@"Preferences...", callbacks.showPreferences);
        [menu addItem: [NSMenuItem separatorItem]];

        const auto panelState = source.getState();

        auto* arrange = makeAction ([this] (id) { source.arrangePluginWindows(); });
        NSMenuItem* arrangeItem = [[[NSMenuItem alloc] initWithTitle: @"Arrange Plug-in Windows" action: @selector (perform:) keyEquivalent: @""] autorelease];
        [arrangeItem setTarget: arrange];
        [arrangeItem setRepresentedObject: arrange];
        [arrangeItem setEnabled: panelState.numPluginWindows > 0];
        [menu setAutoenablesItems: NO];
        [menu addItem: arrangeItem];

        // Quick to reach on stage, so a layout can't be knocked out of place
        const bool locked = panelState.arePluginWindowPositionsLocked;
        auto* lock = makeAction ([this, locked] (id) { source.setPluginWindowPositionsLocked (! locked); });
        NSMenuItem* lockItem = [[[NSMenuItem alloc] initWithTitle: @"Lock Plug-in Windows (This Scene)" action: @selector (perform:) keyEquivalent: @""] autorelease];
        [lockItem setTarget: lock];
        [lockItem setRepresentedObject: lock];
        [lockItem setState: locked ? NSControlStateValueOn : NSControlStateValueOff];
        [menu addItem: lockItem];
        [menu addItem: [NSMenuItem separatorItem]];
        add (@"Curve Help", callbacks.showHelp);
        add (@"About...", callbacks.showAbout);
        [menu addItem: [NSMenuItem separatorItem]];
        add (@"Quit Curve", callbacks.quit);

        [menu popUpMenuPositioningItem: nil atLocation: NSMakePoint (0, [anchor bounds].size.height + 4) inView: anchor];
    }

    template <typename AddFn>
    void addPresets (const MenuBarPanelState::State& state, AddFn&& addFullWidth)
    {
        presetFiles.clear();

        NSString* headerText = state.isPresetModified ? @"Presets  • modified" : @"Presets";
        NSTextField* header = makeSectionHeader (headerText);
        [header setToolTip: state.isPresetModified ? @"The current preset has changes that aren't saved" : nil];

        auto* save = makeAction ([this] (id) { close(); callAsyncWhileAlive ([this] { source.saveAsPreset(); }); });
        NSButton* saveButton = [CurvePanelButton buttonWithTitle: @"Save..." target: save action: @selector (perform:)];
        [saveButton setBezelStyle: NSBezelStyleInline];
        [saveButton setControlSize: NSControlSizeSmall];
        [saveButton setToolTip: @"Save As Preset..."];

        addFullWidth (makeRow (@[ header, makeSpacer(), saveButton ]));

        if (state.presets.isEmpty())
        {
            addFullWidth (makeLabel (@"No presets yet. Save one to switch to it here.", 12, NSFontWeightRegular, [NSColor secondaryLabelColor]));
            return;
        }

        // What each key selects, whatever order the presets are shown in (see PresetSlots)
        presetFiles.insertMultiple (0, juce::File(), 30);
        std::map<int, juce::String> keyUsers;

        for (const auto& preset : state.presets)
            if (juce::isPositiveAndBelow (preset.keySlot, 30))
            {
                presetFiles.set (preset.keySlot, preset.file);
                keyUsers[preset.keySlot] = preset.name;
            }

        // Shown in that order, but with each scene group's scenes together, on a card of their own,
        // where its first scene would be, and the presets between on others. A name too long for its
        // row is cut short, and shown in full as its tooltip
        std::vector<std::string> groups;

        for (const auto& preset : state.presets)
            groups.push_back (preset.group.toStdString());

        const auto order = PresetSlotsCore::groupedOrder (groups);

        NSStackView* list = [[[NSStackView alloc] init] autorelease];
        [list setOrientation: NSUserInterfaceLayoutOrientationVertical];
        [list setSpacing: 5];
        [list setAlignment: NSLayoutAttributeLeading];

        CurvePresetTile* currentTile = nil;
        NSMutableArray<NSView*>* rows = [NSMutableArray array];

        // A scene group's scenes share a card, shown by its colour down the card's edge, as a group has
        // no name of its own, with a little more room around it. A preset on its own has a card of its own
        const auto addCard = [&] (const MenuBarPanelState::Preset& last)
        {
            if ([rows count] == 0)
                return;

            const bool isGroup = last.group.isNotEmpty();
            NSView* previous = [[list arrangedSubviews] lastObject];

            // No padding, so the current preset's highlight reaches the card's edges
            NSView* card = makeCard ([[rows copy] autorelease], 30,
                                     isGroup && last.groupColour.has_value() ? toNS (*last.groupColour) : nil, 0);
            [list addArrangedSubview: card];
            [[[card widthAnchor] constraintEqualToAnchor: [list widthAnchor]] setActive: YES];
            [rows removeAllObjects];

            if (isGroup)
            {
                if (previous != nil)
                    [list setCustomSpacing: 10 afterView: previous];

                [list setCustomSpacing: 10 afterView: card];
            }
        };

        for (size_t k = 0; k < order.size(); ++k)
        {
            const auto& preset = state.presets.getReference ((int) order[k]);

            if (k > 0 && preset.group != state.presets.getReference ((int) order[k - 1]).group)
                addCard (state.presets.getReference ((int) order[k - 1]));

            CurvePresetTile* tile = [[[CurvePresetTile alloc] init] autorelease];
            tile->name = [toNS (preset.name) retain];
            tile->shortcut = preset.shortcut.isNotEmpty() ? [toNS (preset.shortcut) retain] : nil;
            tile->isCurrent = preset.isCurrent;
            tile->onClick = [this, file = preset.file] { source.loadPreset (file); };
            tile->makeMenu = [this, preset, keyUsers] { return makeKeyMenu (preset, keyUsers); };
            [tile setToolTip: toNS (preset.name)];
            [[[tile heightAnchor] constraintEqualToConstant: tileHeight] setActive: YES];
            [rows addObject: tile];

            if (preset.isCurrent)
                currentTile = tile;

            if (preset.group.isEmpty())
                addCard (preset);
        }

        addCard (state.presets.getReference ((int) order.back()));

        // Scrolls past about ten rows, or fewer on a short screen, or if the panel would be taller
        // than the screen (see rebuild())
        const CGFloat contentHeight = [list fittingSize].height;
        const CGFloat maxHeight = getListHeightLimit (0.4, 10, tileHeight + tileGap) + 2 * cardPadding;

        // Room for the scrollbar, if it scrolls
        NSScrollView* scroll = makeScrollingList (list, contentHeight, maxHeight, contentHeight > maxHeight ? 10 : 0, presetsHeight);
        addFullWidth (scroll);

        // The current preset in view, once the panel's been laid out
        if (currentTile != nil)
            scrollAfterLayout = [scroll, currentTile]
            {
                const auto visible = NSHeight ([scroll frame]);
                const auto row = [[scroll documentView] convertRect: [currentTile bounds] fromView: currentTile];

                if (NSMaxY (row) > visible)
                    [[scroll documentView] scrollPoint: NSMakePoint (0, std::max ((CGFloat) 0, NSMinY (row) - (visible - NSHeight (row)) / 2))];
            };
    }

    template <typename AddFn>
    void addPlugins (const MenuBarPanelState::State& state, AddFn&& addFullWidth)
    {
        // Every effect off and back on, to hear the difference they make
        NSMutableArray<NSView*>* header = [NSMutableArray arrayWithObjects: makeSectionHeader (@"Plug-ins"), makeSpacer(), nil];

        std::vector<juce::uint32> bypassable;

        for (const auto& p : state.plugins)
            if (p.canBypass)
                bypassable.push_back (p.id);

        if (! bypassable.empty())
        {
            using S = MenuBarPanelState::OutputSwitch;

            // On while comparing, or while every one is bypassed or muted however that happened
            const bool allOff = std::all_of (state.plugins.begin(), state.plugins.end(),
                                             [] (const auto& p) { return ! p.canBypass || p.isBypassed || p.isMuted; });
            const bool on = state.isBypassingAll || allOff;

            CurvePanelToggle* bypassAll = [[[CurvePanelToggle alloc] init] autorelease];
            bypassAll->compact = YES;
            bypassAll->title = [(on ? @"Turn All On" : @"Bypass All") retain];
            bypassAll->onColour = [[NSColor systemOrangeColor] retain];
            bypassAll->isOn = on;
            bypassAll->onChange = [this, comparing = state.isBypassingAll, allOff, bypassable] (bool)
            {
                if (comparing)
                    source.setOutputSwitch (S::bypassAll, false);   // back as they were
                else if (allOff)
                    for (const auto id : bypassable)                 // bypassed one by one, so turned on for good
                    {
                        source.setMuted (id, false);
                        source.setBypassed (id, false);
                    }
                else
                    source.setOutputSwitch (S::bypassAll, true);
            };
            [bypassAll setToolTip: state.isBypassingAll ? @"Turns the effects back on"
                                 : allOff ? @"Turns every plug-in on"
                                          : @"Turns off every effect, to hear the difference they make. Doesn't change the preset."];
            [header addObject: bypassAll];
        }

        addFullWidth (makeRow (header));

        // On a card, as the presets are. Scrolls past about eight, or fewer on a short screen, so a
        // large graph can't make the panel too tall
        NSMutableArray<NSView*>* rows = [NSMutableArray array];

        const CGFloat pitch = pluginRowHeight + pluginRowGap;
        const CGFloat maxHeight = getListHeightLimit (0.3, 8, pitch) - pluginRowGap + 2 * cardPadding;
        CGFloat detailsHeight = 0; // Meter plug-ins opened up

        for (const auto& p : state.plugins)
            if (p.isMeter && expandedMeters.count (p.id) > 0)
                detailsHeight += [CurvePanelMeterDetail heightFor: (MenuBarPanelState::MeterReading::Mode) p.meterMode];

        const CGFloat contentHeight = (CGFloat) state.plugins.size() * pitch - pluginRowGap + 2 * cardPadding + detailsHeight;

        // Narrower when it scrolls, to leave room for the scrollbar
        const bool scrolls = contentHeight > maxHeight;
        const CGFloat trailingRoom = scrolls ? 14 : 0;

        for (const auto& plugin : state.plugins)
        {
            const auto nodeID = plugin.id;
            const bool active = ! plugin.isBypassed && ! plugin.isMuted;

            juce::String text = plugin.label;

            // Off, for one that passes nothing on, shows by its switch
            if (plugin.isMuted && ! plugin.canTurnOff)
                text << "  (muted)";

            NSTextField* name = makeLabel (toNS (text), 12, NSFontWeightRegular,
                                           active ? [NSColor labelColor] : [NSColor tertiaryLabelColor]);
            [name setToolTip: toNS (plugin.label)];

            NSMutableArray<NSView*>* views = [NSMutableArray arrayWithObjects: name, makeSpacer(), nil];

            // A Meter plug-in's level, wherever it is in the chain
            if (plugin.isMeter)
            {
                const bool expanded = expandedMeters.count (nodeID) > 0;

                // A bar in the row, unless it's opened up, which shows more
                CurvePanelMeter* level = nil;

                if (! expanded)
                {
                    level = [[[CurvePanelMeter alloc] initWithFrame: NSMakeRect (0, 0, 64, 6)] autorelease];
                    [[[level widthAnchor] constraintEqualToConstant: 64] setActive: YES];
                    [[[level heightAnchor] constraintEqualToConstant: 7] setActive: YES];
                    [level setToolTip: @"The level where this meter is"];
                    [views addObject: level];
                }

                NSButton* disclose = makeSymbolButton (expanded ? @"chevron.down" : @"chevron.right",
                                                       expanded ? @"Show less" : @"Show what the meter shows",
                                                       [this, nodeID, expanded] (id)
                                                       {
                                                           if (expanded)
                                                               expandedMeters.erase (nodeID);
                                                           else
                                                               expandedMeters.insert (nodeID);

                                                           callAsyncWhileAlive ([this] { if (isOpen()) rebuild(); });
                                                       });
                [views addObject: disclose];

                CurvePanelMeterDetail* detail = nil;

                if (expanded)
                {
                    detail = [[[CurvePanelMeterDetail alloc] initWithFrame: NSMakeRect (0, 0, 100, 42)] autorelease];
                    [[[detail heightAnchor] constraintEqualToConstant: [CurvePanelMeterDetail heightFor: (MenuBarPanelState::MeterReading::Mode) plugin.meterMode]] setActive: YES];
                    [detail setToolTip: @"As the meter's window shows it: change what it shows there"];
                }

                pluginMeters.push_back ({ nodeID, level, detail });
            }

            if (plugin.hasEditor)
            {
                NSButton* edit = makeSymbolButton (@"macwindow",
                                                   plugin.isWindowOpen ? @"Close the plug-in's window" : @"Open the plug-in's window",
                                                   [this, nodeID] (id) { source.toggleEditor (nodeID); });

                if (plugin.isWindowOpen)
                    [edit setContentTintColor: [NSColor controlAccentColor]];

                [views addObject: edit];
            }

            if (plugin.canBypass)
            {
                auto* toggle = makeAction ([this, nodeID, muted = plugin.isMuted] (id sender)
                {
                    const bool on = [(NSSwitch*) sender state] == NSControlStateValueOn;

                    // Muted, nothing passes either, so switching it on unmutes it, and turns it on if it
                    // was bypassed too
                    if (muted && on)
                        source.setMuted (nodeID, false);

                    source.setBypassed (nodeID, ! on);
                });

                NSSwitch* power = [[[CurvePanelSwitch alloc] init] autorelease];
                [power setControlSize: NSControlSizeMini];
                [power setState: active ? NSControlStateValueOn : NSControlStateValueOff];
                [power setTarget: toggle];
                [power setAction: @selector (perform:)];
                [power setToolTip: active ? @"On: click to bypass" : plugin.isMuted ? @"Muted: click to unmute" : @"Bypassed: click to turn on"];
                [power setAccessibilityLabel: toNS (plugin.label)];
                [views addObject: power];
            }
            else if (plugin.canTurnOff)
            {
                // e.g. a Meter: nothing to bypass, so on or off, as the graph's Turn Off has it
                auto* toggle = makeAction ([this, nodeID] (id sender)
                {
                    source.setMuted (nodeID, [(NSSwitch*) sender state] != NSControlStateValueOn);
                });

                NSSwitch* power = [[[CurvePanelSwitch alloc] init] autorelease];
                [power setControlSize: NSControlSizeMini];
                [power setState: plugin.isMuted ? NSControlStateValueOff : NSControlStateValueOn];
                [power setTarget: toggle];
                [power setAction: @selector (perform:)];
                [power setToolTip: plugin.isMuted ? @"Off: click to turn on" : @"On: click to turn off"];
                [power setAccessibilityLabel: toNS (plugin.label)];
                [views addObject: power];
            }

            CurvePluginRow* row = [CurvePluginRow stackViewWithViews: views];
            [row setOrientation: NSUserInterfaceLayoutOrientationHorizontal];
            [row setAlignment: NSLayoutAttributeCenterY];
            [row setSpacing: 8];
            [row setEdgeInsets: NSEdgeInsetsMake (0, 12, 0, 10)];
            [[[row heightAnchor] constraintEqualToConstant: pluginRowHeight] setActive: YES];

            row->makeMenu = [this, plugin, nodeID]
            {
                NSMenu* menu = [[[NSMenu alloc] init] autorelease];

                const auto add = [this, menu] (NSString* title, bool enabled, std::function<void()> f)
                {
                    auto* action = makeAction ([f] (id) { f(); });
                    NSMenuItem* item = [[[NSMenuItem alloc] initWithTitle: title action: @selector (perform:) keyEquivalent: @""] autorelease];
                    [item setTarget: action];
            [item setRepresentedObject: action]; // An item doesn't keep its target, and the panel can rebuild while its menu is open
                    [item setEnabled: enabled];
                    [menu setAutoenablesItems: NO];
                    [menu addItem: item];
                };

                add (plugin.isWindowOpen ? @"Close Plug-in Window" : @"Open Plug-in Window", plugin.hasEditor, [this, nodeID] { source.toggleEditor (nodeID); });

                if (plugin.hasEditor)
                {
                    add (@"Keep Window on Top", true, [this, nodeID, onTop = plugin.isWindowKeptOnTop] { source.setWindowKeptOnTop (nodeID, ! onTop); });
                    [[menu itemArray].lastObject setState: plugin.isWindowKeptOnTop ? NSControlStateValueOn : NSControlStateValueOff];
                }

                if (plugin.canBypass)
                {
                    add (plugin.isBypassed ? @"Turn On" : @"Bypass", true, [this, nodeID, b = plugin.isBypassed] { source.setBypassed (nodeID, ! b); });
                    add (plugin.isMuted ? @"Unmute" : @"Mute", true, [this, nodeID, m = plugin.isMuted] { source.setMuted (nodeID, ! m); });
                }
                else if (plugin.canTurnOff)
                {
                    add (plugin.isMuted ? @"Turn On" : @"Turn Off", true, [this, nodeID, m = plugin.isMuted] { source.setMuted (nodeID, ! m); });
                }

                [menu addItem: [NSMenuItem separatorItem]];
                add (@"Hide from Menu Bar Panel", true, [this, nodeID] { source.hideFromPanel (nodeID); });
                return menu;
            };

            [rows addObject: row];

            // Opened up, its measurements under it
            if (! pluginMeters.empty() && pluginMeters.back().id == nodeID && pluginMeters.back().detail != nil)
                [rows addObject: pluginMeters.back().detail];
        }

        // In a scroll view even when it fits, so it can be made shorter if the panel's too tall for the screen
        addFullWidth (makeScrollingList (makeCard (rows, 12), contentHeight, maxHeight, trailingRoom, pluginsHeight));
    }

    // What Curve plays out: switches for checking it and its volume, with its level at the right
    template <typename AddFn>
    void addOutput (const MenuBarPanelState::State& state, AddFn&& addFullWidth)
    {
        const bool showSwitches = getSetting (showOutputSetting, true);
        const bool showVolume = getSetting (showVolumeSetting, true) && state.volumeKind != MenuBarPanelState::State::VolumeKind::none;
        const bool showMeter = getSetting (showMeterSetting, true);

        // Nothing to show, so no heading either
        if (state.deviceName.isEmpty() || ! (showSwitches || showVolume || showMeter))
            return;

        addFullWidth (makeSeparator());

        NSMutableArray<NSView*>* rows = [NSMutableArray arrayWithObject: makeSectionHeader (@"Output")];

        if (showSwitches)
            [rows addObject: makeOutputSwitches (state)];

        if (showVolume)
            [rows addObject: makeVolume (state)];

        if (showMeter)
        {
            meter = [[[CurvePanelMeter alloc] initWithFrame: NSMakeRect (0, 0, 9, 9)] autorelease];
            [meter setToolTip: @"Output level"];
        }

        // With nothing but the heading, the meter runs across under it
        if ([rows count] == 1)
        {
            addFullWidth ([rows objectAtIndex: 0]);

            if (meter != nil)
                addFullWidth (meter);

            return;
        }

        NSStackView* column = [NSStackView stackViewWithViews: rows];
        [column setOrientation: NSUserInterfaceLayoutOrientationVertical];
        [column setAlignment: NSLayoutAttributeLeading];
        [column setSpacing: 8];

        for (NSView* row in rows)
            [[[row widthAnchor] constraintEqualToAnchor: [column widthAnchor]] setActive: YES];

        if (meter == nil)
        {
            addFullWidth (column);
            return;
        }

        // At the far right, as tall as the section
        meter->vertical = YES;
        [[[meter widthAnchor] constraintEqualToConstant: 10] setActive: YES];

        constexpr CGFloat meterWidth = 10, meterGap = 12;

        NSStackView* body = [NSStackView stackViewWithViews: @[ column, meter ]];
        [body setOrientation: NSUserInterfaceLayoutOrientationHorizontal];
        [body setAlignment: NSLayoutAttributeTop];
        [body setSpacing: meterGap];
        [[[meter heightAnchor] constraintEqualToAnchor: [column heightAnchor]] setActive: YES];

        // The controls take all the width the meter leaves; with no width of their own, they'd collapse
        [[[column widthAnchor] constraintEqualToConstant: panelWidth - 2 * margin - meterWidth - meterGap] setActive: YES];
        addFullWidth (body);
    }

    // Mono, Dim and Mute, from the least to the most drastic, each in its own colour when on
    NSView* makeOutputSwitches (const MenuBarPanelState::State& state)
    {
        using S = MenuBarPanelState::OutputSwitch;

        const auto make = [this] (NSString* title, NSString* symbol, NSColor* colour, NSString* tip, S which, bool on)
        {
            CurvePanelToggle* toggle = [[[CurvePanelToggle alloc] init] autorelease];
            toggle->title = [title retain];
            toggle->symbol = [symbol retain];
            toggle->onColour = [colour retain];
            toggle->isOn = on;
            toggle->onChange = [this, which] (bool isOn) { source.setOutputSwitch (which, isOn); };
            [toggle setToolTip: tip];
            [[[toggle heightAnchor] constraintEqualToConstant: 26] setActive: YES];
            return toggle;
        };

        NSStackView* row = makeRow (@[
            make (@"Mono", @"circle.lefthalf.filled", [NSColor systemBlueColor],
                  @"Plays each pair of outputs in mono, e.g. to check a mix", S::mono, state.isMono),
            make (@"Dim", @"speaker.wave.1.fill", [NSColor systemOrangeColor],
                  @"Turns everything Curve plays down by 20 dB, e.g. to talk", S::dim, state.isDimmed),
            make (@"Mute", @"speaker.slash.fill", [NSColor systemRedColor],
                  @"Silences everything Curve plays", S::mute, state.isMuted)
        ]);
        [row setDistribution: NSStackViewDistributionFillEqually];
        [row setSpacing: 8];
        return row;
    }

    // Curve's own volume, or the interface's, whichever sets the level (see VolumeKeyManager)
    NSView* makeVolume (const MenuBarPanelState::State& state)
    {
        using Kind = MenuBarPanelState::State::VolumeKind;
        const bool isInterface = state.volumeKind == Kind::interface;

        // Shows whether the volume is muted, by the volume keys. Mute is with the switches above
        muteButton = makeSymbolButton (@"speaker.wave.2.fill", @"Volume", [] (id) {});
        [muteButton setEnabled: NO];

        auto* change = makeAction ([this, isInterface] (id sender)
        {
            const auto value = [(NSSlider*) sender doubleValue];

            if (isInterface)
                source.setInterfaceVolume ((float) value);
            else
                source.setVolume ((int) std::lround (value), false);

            updateVolume (source.getState());
        });

        volumeSlider = [CurvePanelSlider sliderWithValue: 0 minValue: 0 maxValue: isInterface ? 1.0 : state.maxVolumeSteps
                                          target: change action: @selector (perform:)];
        [volumeSlider setContinuous: YES];
        [volumeSlider setControlSize: NSControlSizeSmall];
        [volumeSlider setAccessibilityLabel: isInterface ? @"Interface volume" : @"Curve's volume"];
        [volumeSlider setToolTip: isInterface ? @"The interface's own volume, as macOS's volume menu sets it"
                                              : @"Curve's own volume, as the interface has none that macOS can set. "
                                                 "The volume keys can set it too: see Preferences > Keyboard"];

        volumeLabel = makeLabel (@"", 11, NSFontWeightRegular, [NSColor secondaryLabelColor]);
        [volumeLabel setFont: [NSFont monospacedDigitSystemFontOfSize: 11 weight: NSFontWeightRegular]];
        [volumeLabel setAlignment: NSTextAlignmentRight];
        [[[volumeLabel widthAnchor] constraintEqualToConstant: 62] setActive: YES];

        volumeKind = (int) state.volumeKind;
        updateVolume (state);
        return makeRow (@[ muteButton, volumeSlider, volumeLabel ]);
    }

    // A preset's right-click menu: which key selects it
    NSMenu* makeKeyMenu (const MenuBarPanelState::Preset& preset, const std::map<int, juce::String>& keyUsers)
    {
        NSMenu* keys = [[[NSMenu alloc] initWithTitle: @"Key"] autorelease];
        [keys setAutoenablesItems: NO];

        const auto add = [this, keys] (NSString* title, bool ticked, std::function<void()> f)
        {
            auto* action = makeAction ([f] (id) { f(); });
            NSMenuItem* item = [[[NSMenuItem alloc] initWithTitle: title action: @selector (perform:) keyEquivalent: @""] autorelease];
            [item setTarget: action];
            [item setRepresentedObject: action]; // An item doesn't keep its target, and the panel can rebuild while its menu is open
            [item setState: ticked ? NSControlStateValueOn : NSControlStateValueOff];
            [keys addItem: item];
        };

        const auto assign = [this, file = preset.file] (int slot)
        {
            source.assignKey (file, slot);
            callAsyncWhileAlive ([this] { if (isOpen()) rebuild(); });
        };

        add (@"Automatic, from Its Place in the List", ! preset.isKeyAssigned, [assign] { assign (-1); });

        for (int slot = 0; slot < 30; ++slot)
        {
            if (slot % 10 == 0)
                [keys addItem: [NSMenuItem separatorItem]];

            juce::String title = PresetSlots::describeKey (slot);

            // Whose it is now, as giving it to this one takes it from them
            if (auto user = keyUsers.find (slot); user != keyUsers.end() && user->second != preset.name)
                title << "    " << user->second;

            add (toNS (title), preset.isKeyAssigned && preset.keySlot == slot, [assign, slot] { assign (slot); });
        }

        NSMenu* menu = [[[NSMenu alloc] init] autorelease];
        NSMenuItem* keyItem = [[[NSMenuItem alloc] initWithTitle: preset.keySlot >= 0 ? toNS ("Key: " + preset.shortcut) : @"Key"
                                                          action: nil keyEquivalent: @""] autorelease];
        [keyItem setSubmenu: keys];
        [menu addItem: keyItem];
        return menu;
    }

    // The preset's tempo, which a click types, Tap, and the transport's Play/Stop
    NSView* makeTempo (const MenuBarPanelState::State& state)
    {
        playButton = makeSymbolButton (@"play.fill", @"Play", [this] (id)
        {
            source.setTransportPlaying (! source.getState().isTransportPlaying);
            updateTempo (source.getState());
        });

        auto* typed = makeAction ([this] (id sender)
        {
            if (const auto bpm = [[(NSTextField*) sender stringValue] doubleValue]; bpm > 0.0)
                source.setTempo (bpm);

            [[root window] makeFirstResponder: root]; // So the number keys select presets again
            updateTempo (source.getState());
        });

        tempoField = [NSTextField textFieldWithString: @""];
        [tempoField setFont: [NSFont monospacedDigitSystemFontOfSize: 12 weight: NSFontWeightMedium]];
        [tempoField setAlignment: NSTextAlignmentRight];
        [tempoField setBezelStyle: NSTextFieldRoundedBezel];
        [tempoField setControlSize: NSControlSizeSmall];
        [tempoField setTarget: typed];
        [tempoField setAction: @selector (perform:)];
        [tempoField setToolTip: @"The preset's tempo, for tempo-synced plug-ins and the Looper. Click to type it"];
        [[[tempoField widthAnchor] constraintEqualToConstant: 54] setActive: YES];

        auto* tapped = makeAction ([this] (id)
        {
            source.tapTempo();
            updateTempo (source.getState());
        });

        NSButton* tap = [CurvePanelButton buttonWithTitle: @"Tap" target: tapped action: @selector (perform:)];
        [tap setBezelStyle: NSBezelStyleRounded];
        [tap setControlSize: NSControlSizeSmall];
        [tap setToolTip: @"Tap in time, twice or more, to set the tempo"];

        NSStackView* row = makeRow (@[ makeSectionHeader (@"Tempo"), makeSpacer(), playButton, tempoField,
                                       makeLabel (@"BPM", 11, NSFontWeightRegular, [NSColor secondaryLabelColor]), tap ]);
        [row setSpacing: 6];
        updateTempo (state);
        return row;
    }

    void updateTempo (const MenuBarPanelState::State& state)
    {
        if (tempoField == nil)
            return;

        // Not while it's being typed in
        if ([[tempoField window] firstResponder] != [tempoField currentEditor] || [tempoField currentEditor] == nil)
        {
            const auto whole = std::abs (state.tempo - std::round (state.tempo)) < 0.05;
            [tempoField setStringValue: toNS (juce::String (state.tempo, whole ? 0 : 1))];
        }

        const bool playing = state.isTransportPlaying;
        [playButton setImage: [NSImage imageWithSystemSymbolName: playing ? @"stop.fill" : @"play.fill"
                                        accessibilityDescription: playing ? @"Stop" : @"Play"]];
        [playButton setContentTintColor: playing ? [NSColor controlAccentColor] : [NSColor secondaryLabelColor]];
        [playButton setToolTip: playing ? @"Stops the transport, for plug-ins that follow it" : @"Starts the transport, for plug-ins that follow it, e.g. a drum machine"];
    }

    void updateVolume (const MenuBarPanelState::State& state)
    {
        using Kind = MenuBarPanelState::State::VolumeKind;

        if (volumeSlider == nil)
            return;

        // What the slider sets has changed, e.g. the interface or the keys' mode: rebuilt to match
        if ((int) state.volumeKind != volumeKind)
        {
            callAsyncWhileAlive ([this] { if (isOpen()) rebuild(); });
            return;
        }

        const bool isInterface = state.volumeKind == Kind::interface;
        const bool silent = isInterface ? state.interfaceVolume <= 0.0f : (state.isVolumeMuted || state.volumeSteps == 0);

        [volumeSlider setDoubleValue: isInterface ? (double) state.interfaceVolume : (double) state.volumeSteps];
        [volumeLabel setStringValue: toNS (state.volumeDescription)];
        [muteButton setImage: [NSImage imageWithSystemSymbolName: silent ? @"speaker.slash.fill" : @"speaker.wave.2.fill"
                                        accessibilityDescription: @"Volume"]];
        [muteButton setToolTip: ! isInterface && state.isVolumeMuted ? @"Volume muted by the volume keys" : @"Volume"];
    }

    NSView* makeFooter (const MenuBarPanelState::State& state)
    {
        auto* toggleEditor = makeAction ([this] (id) { close(); callAsyncWhileAlive ([this] { source.toggleGraphEditor(); }); });
        NSButton* editor = [CurvePanelButton buttonWithTitle: ! state.isEditorShowing ? @"Show Editor"
                                                   : state.isEditorInView ? @"Hide Editor" : @"Bring Editor to Front"
                                              target: toggleEditor action: @selector (perform:)];
        [editor setBezelStyle: NSBezelStyleRounded];
        [editor setControlSize: NSControlSizeSmall];

        if (state.numPluginWindows == 0)
            return makeRow (@[ editor, makeSpacer() ]);

        // The preset's plug-in windows, as a set, e.g. to get them out of the way and back
        auto* toggleWindows = makeAction ([this] (id) { source.togglePluginWindows(); });
        NSButton* windows = [CurvePanelButton buttonWithTitle: state.arePluginWindowsHidden ? @"Show Plug-in Windows" : @"Hide Plug-in Windows"
                                               target: toggleWindows action: @selector (perform:)];
        [windows setBezelStyle: NSBezelStyleRounded];
        [windows setControlSize: NSControlSizeSmall];

        return makeRow (@[ editor, makeSpacer(), windows ]);
    }

    //==============================================================================
    // The preset keys, as the menu had them: 1-9 and 0, then with Option, then with Shift
    bool handleKey (NSEvent* event)
    {
        static const std::map<unsigned short, int> digits { { kVK_ANSI_1, 0 }, { kVK_ANSI_2, 1 }, { kVK_ANSI_3, 2 },
                                                            { kVK_ANSI_4, 3 }, { kVK_ANSI_5, 4 }, { kVK_ANSI_6, 5 },
                                                            { kVK_ANSI_7, 6 }, { kVK_ANSI_8, 7 }, { kVK_ANSI_9, 8 },
                                                            { kVK_ANSI_0, 9 } };

        if ([event keyCode] == kVK_Escape)
        {
            close();
            return true;
        }

        const auto digit = digits.find ([event keyCode]);

        if (digit == digits.end())
            return false;

        const auto flags = [event modifierFlags] & NSEventModifierFlagDeviceIndependentFlagsMask;

        if ((flags & (NSEventModifierFlagCommand | NSEventModifierFlagControl)) != 0)
            return false;

        const int bank = (flags & NSEventModifierFlagShift) != 0 ? 2 : (flags & NSEventModifierFlagOption) != 0 ? 1 : 0;
        const int index = bank * 10 + digit->second;

        if (! juce::isPositiveAndBelow (index, presetFiles.size()) || presetFiles[index] == juce::File())
            return false;

        source.loadPreset (presetFiles[index]);
        return true;
    }

    JUCE_DECLARE_NON_COPYABLE (Impl)
};

//==============================================================================
MenuBarPanel::MenuBarPanel (MenuBarPanelState::Source& source, Callbacks callbacks)
    : impl (std::make_unique<Impl> (source, std::move (callbacks)))
{
}

MenuBarPanel::~MenuBarPanel() = default;

void MenuBarPanel::toggle (void* statusItem)    { impl->toggle (statusItem); }
void MenuBarPanel::close()                      { impl->close(); }
bool MenuBarPanel::isOpen() const               { return impl->isOpen(); }

bool MenuBarPanel::getSetting (const char* key, bool defaultValue)
{
    if (auto* settings = getUserSettings())
        return settings->getBoolValue (key, defaultValue);

    return defaultValue;
}

MenuBarPanel::Background MenuBarPanel::getBackground()
{
    if (auto* settings = getUserSettings())
    {
        if (settings->containsKey ("menuBarPanelBackground"))
            return (Background) juce::jlimit (0, 2, settings->getIntValue ("menuBarPanelBackground"));

        if (settings->getBoolValue (solidBackgroundSetting, false))
            return Background::solid;
    }

    return Background::frosted;
}

MenuBarPanel::Appearance MenuBarPanel::getAppearance()
{
    if (auto* settings = getUserSettings())
        return (Appearance) juce::jlimit (0, 2, settings->getIntValue ("menuBarPanelAppearance", 0));

    return Appearance::system;
}

void MenuBarPanel::setAppearance (Appearance appearance)
{
    if (auto* settings = getUserSettings())
    {
        settings->setValue ("menuBarPanelAppearance", (int) appearance);
        settings->saveIfNeeded();
    }
}

void MenuBarPanel::setBackground (Background background)
{
    if (auto* settings = getUserSettings())
    {
        settings->setValue ("menuBarPanelBackground", (int) background);
        settings->saveIfNeeded();
    }
}

void MenuBarPanel::setSetting (const char* key, bool value)
{
    if (auto* settings = getUserSettings())
    {
        settings->setValue (key, value);
        settings->saveIfNeeded();
    }
}
