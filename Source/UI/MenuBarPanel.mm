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

#import <AppKit/AppKit.h>
#import <Carbon/Carbon.h> // kVK_ key codes

#include <map>

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
/** A preset, as a tile: its name, its key, and its scene group's colour as a stripe. */
@interface CurvePresetTile : NSView
{
@public
    NSString* name;
    NSString* shortcut;
    NSColor* groupColour;
    BOOL isCurrent, isHovered, isPressed;
    std::function<void()> onClick;
}
@end

@implementation CurvePresetTile

- (void) dealloc
{
    [name release];
    [shortcut release];
    [groupColour release];
    [super dealloc];
}

- (BOOL) isFlipped { return YES; }
- (NSSize) intrinsicContentSize { return NSMakeSize (NSViewNoIntrinsicMetric, 34); }
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
    [self setNeedsDisplay: YES];

    if (inside && onClick)
        onClick();
}

- (void) drawRect: (NSRect) dirty
{
    const NSRect bounds = [self bounds];
    NSBezierPath* shape = [NSBezierPath bezierPathWithRoundedRect: NSInsetRect (bounds, 0.5, 0.5) xRadius: 7 yRadius: 7];

    NSColor* fill = isCurrent ? [NSColor controlAccentColor]
                  : [[NSColor labelColor] colorWithAlphaComponent: isPressed ? 0.16 : isHovered ? 0.11 : 0.06];
    [fill setFill];
    [shape fill];

    if (groupColour != nil)
    {
        [NSGraphicsContext saveGraphicsState];
        [shape addClip];
        [groupColour setFill];
        NSRectFill (NSMakeRect (0, 0, 3.5, bounds.size.height));
        [NSGraphicsContext restoreGraphicsState];
    }

    NSColor* textColour = isCurrent ? [NSColor whiteColor] : [NSColor labelColor];
    NSColor* keyColour = isCurrent ? [[NSColor whiteColor] colorWithAlphaComponent: 0.7] : [NSColor tertiaryLabelColor];

    NSMutableParagraphStyle* truncating = [[[NSMutableParagraphStyle alloc] init] autorelease];
    [truncating setLineBreakMode: NSLineBreakByTruncatingTail];

    NSDictionary* keyAttributes = @{ NSFontAttributeName: [NSFont monospacedDigitSystemFontOfSize: 11 weight: NSFontWeightRegular],
                                     NSForegroundColorAttributeName: keyColour };
    const NSSize keySize = shortcut != nil ? [shortcut sizeWithAttributes: keyAttributes] : NSZeroSize;

    NSDictionary* nameAttributes = @{ NSFontAttributeName: [NSFont systemFontOfSize: 12 weight: isCurrent ? NSFontWeightSemibold : NSFontWeightMedium],
                                      NSForegroundColorAttributeName: textColour,
                                      NSParagraphStyleAttributeName: truncating };

    const CGFloat left = groupColour != nil ? 12 : 10;
    const CGFloat right = 10 + (keySize.width > 0 ? keySize.width + 6 : 0);
    const NSSize nameSize = [name sizeWithAttributes: nameAttributes];

    [name drawInRect: NSMakeRect (left, (bounds.size.height - nameSize.height) / 2, bounds.size.width - left - right, nameSize.height)
      withAttributes: nameAttributes];

    if (shortcut != nil)
        [shortcut drawAtPoint: NSMakePoint (bounds.size.width - 10 - keySize.width, (bounds.size.height - keySize.height) / 2)
               withAttributes: keyAttributes];
}

- (BOOL) isAccessibilityElement             { return YES; }
- (NSAccessibilityRole) accessibilityRole   { return NSAccessibilityButtonRole; }
- (NSString*) accessibilityLabel            { return name; }
- (BOOL) accessibilityPerformPress          { if (onClick) onClick(); return YES; }

@end

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
                         : [[NSColor labelColor] colorWithAlphaComponent: isPressed ? 0.16 : isHovered ? 0.11 : 0.06];
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

        globalMonitor = [[NSEvent addGlobalMonitorForEventsMatchingMask: mask handler: ^(NSEvent*) { close(); }] retain];

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

        // The volume can change from the volume keys while the panel is open
        if (volumeSlider != nil && ++ticksSinceVolume >= 8)
        {
            ticksSinceVolume = 0;

            if ([[NSApp currentEvent] type] != NSEventTypeLeftMouseDragged)
                updateVolume (source.getState());
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
        NSButton* button = [NSButton buttonWithImage: image target: action action: @selector (perform:)];
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
        meter = nil;
        volumeSlider = nil;
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

        addOutput (state, addFullWidth);

        addFullWidth (makeSeparator());
        addFullWidth (makeFooter (state));

        [root layoutSubtreeIfNeeded];

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
        NSButton* saveButton = [NSButton buttonWithTitle: @"Save..." target: save action: @selector (perform:)];
        [saveButton setBezelStyle: NSBezelStyleInline];
        [saveButton setControlSize: NSControlSizeSmall];
        [saveButton setToolTip: @"Save As Preset..."];

        addFullWidth (makeRow (@[ header, makeSpacer(), saveButton ]));

        if (state.presets.isEmpty())
        {
            addFullWidth (makeLabel (@"No presets yet. Save one to switch to it here.", 12, NSFontWeightRegular, [NSColor secondaryLabelColor]));
            return;
        }

        // Two columns of tiles, or one column with longer names when there are many
        const int columns = state.presets.size() <= 12 ? 2 : 1;

        NSStackView* grid = [[[NSStackView alloc] init] autorelease];
        [grid setOrientation: NSUserInterfaceLayoutOrientationVertical];
        [grid setSpacing: 6];
        [grid setAlignment: NSLayoutAttributeLeading];

        NSMutableArray<NSView*>* rowViews = [NSMutableArray array];

        for (int i = 0; i < state.presets.size(); ++i)
        {
            const auto& preset = state.presets.getReference (i);
            presetFiles.add (preset.file);

            CurvePresetTile* tile = [[[CurvePresetTile alloc] init] autorelease];
            tile->name = [toNS (preset.name) retain];
            tile->shortcut = preset.shortcut.isNotEmpty() ? [toNS (preset.shortcut) retain] : nil;
            tile->groupColour = preset.groupColour.has_value() ? [toNS (*preset.groupColour) retain] : nil;
            tile->isCurrent = preset.isCurrent;
            tile->onClick = [this, file = preset.file] { source.loadPreset (file); };
            [tile setToolTip: toNS (preset.name)];
            [[[tile heightAnchor] constraintEqualToConstant: 34] setActive: YES];
            [rowViews addObject: tile];

            if ((int) [rowViews count] == columns || i == state.presets.size() - 1)
            {
                while ((int) [rowViews count] < columns)
                    [rowViews addObject: [[[NSView alloc] init] autorelease]];

                NSStackView* row = [NSStackView stackViewWithViews: rowViews];
                [row setOrientation: NSUserInterfaceLayoutOrientationHorizontal];
                [row setDistribution: NSStackViewDistributionFillEqually];
                [row setSpacing: 6];
                [grid addArrangedSubview: row];
                [[[row widthAnchor] constraintEqualToAnchor: [grid widthAnchor]] setActive: YES];
                [rowViews removeAllObjects];
            }
        }

        // Scrolls past about ten rows
        constexpr CGFloat maxHeight = 400;
        [grid layoutSubtreeIfNeeded];
        const CGFloat gridHeight = [grid fittingSize].height;

        if (gridHeight <= maxHeight)
        {
            addFullWidth (grid);
            return;
        }

        NSScrollView* scroll = [[[NSScrollView alloc] init] autorelease];
        [scroll setHasVerticalScroller: YES];
        [scroll setDrawsBackground: NO];
        [scroll setAutohidesScrollers: YES];

        CurvePanelView* document = [[[CurvePanelView alloc] init] autorelease]; // flipped, so the list starts at the top
        [document setTranslatesAutoresizingMaskIntoConstraints: NO];
        [grid setTranslatesAutoresizingMaskIntoConstraints: NO];
        [document addSubview: grid];
        [scroll setDocumentView: document];

        NSClipView* clip = [scroll contentView];
        [NSLayoutConstraint activateConstraints: @[
            [[grid leadingAnchor] constraintEqualToAnchor: [document leadingAnchor]],
            [[grid trailingAnchor] constraintEqualToAnchor: [document trailingAnchor] constant: -10],
            [[grid topAnchor] constraintEqualToAnchor: [document topAnchor]],
            [[grid bottomAnchor] constraintEqualToAnchor: [document bottomAnchor]],
            [[document leadingAnchor] constraintEqualToAnchor: [clip leadingAnchor]],
            [[document trailingAnchor] constraintEqualToAnchor: [clip trailingAnchor]],
            [[document topAnchor] constraintEqualToAnchor: [clip topAnchor]],
            [[scroll heightAnchor] constraintEqualToConstant: maxHeight]
        ]];

        addFullWidth (scroll);

        // The current preset in view, once the panel's been laid out (see rebuild())
        for (int i = 0; i < state.presets.size(); ++i)
            if (state.presets.getReference (i).isCurrent)
                scrollAfterLayout = [document, y = std::max (0.0, (double) (i / columns) * 40.0 - maxHeight / 2)]
                {
                    [document scrollRectToVisible: NSMakeRect (0, y, 10, maxHeight)];
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

        // Scrolls past about ten, so a large graph can't push the rest of the panel off the screen
        NSStackView* list = [[[NSStackView alloc] init] autorelease];
        [list setOrientation: NSUserInterfaceLayoutOrientationVertical];
        [list setAlignment: NSLayoutAttributeLeading];
        [list setSpacing: 10];
        [list setTranslatesAutoresizingMaskIntoConstraints: NO];

        const CGFloat listWidth = panelWidth - 2 * margin;

        for (const auto& plugin : state.plugins)
        {
            const auto nodeID = plugin.id;
            const bool active = ! plugin.isBypassed && ! plugin.isMuted;

            juce::String text = plugin.label;

            if (plugin.isMuted)
                text << "  (muted)";

            NSTextField* name = makeLabel (toNS (text), 12, NSFontWeightRegular,
                                           active ? [NSColor labelColor] : [NSColor tertiaryLabelColor]);
            [name setToolTip: toNS (plugin.label)];

            NSMutableArray<NSView*>* views = [NSMutableArray arrayWithObjects: name, makeSpacer(), nil];

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

                NSSwitch* power = [[[NSSwitch alloc] init] autorelease];
                [power setControlSize: NSControlSizeMini];
                [power setState: active ? NSControlStateValueOn : NSControlStateValueOff];
                [power setTarget: toggle];
                [power setAction: @selector (perform:)];
                [power setToolTip: active ? @"On: click to bypass" : plugin.isMuted ? @"Muted: click to unmute" : @"Bypassed: click to turn on"];
                [power setAccessibilityLabel: toNS (plugin.label)];
                [views addObject: power];
            }

            CurvePluginRow* row = [CurvePluginRow stackViewWithViews: views];
            [row setOrientation: NSUserInterfaceLayoutOrientationHorizontal];
            [row setAlignment: NSLayoutAttributeCenterY];
            [row setSpacing: 8];
            [[[row heightAnchor] constraintGreaterThanOrEqualToConstant: 22] setActive: YES];

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

                [menu addItem: [NSMenuItem separatorItem]];
                add (@"Hide from Menu Bar Panel", true, [this, nodeID] { source.hideFromPanel (nodeID); });
                return menu;
            };

            [list addArrangedSubview: row];
            [[[row widthAnchor] constraintEqualToConstant: listWidth] setActive: YES];
        }

        if (state.plugins.size() <= 10)
        {
            addFullWidth (list);
            return;
        }

        constexpr CGFloat maxHeight = 320;

        NSScrollView* scroll = [[[NSScrollView alloc] init] autorelease];
        [scroll setHasVerticalScroller: YES];
        [scroll setDrawsBackground: NO];
        [scroll setAutohidesScrollers: YES];

        CurvePanelView* document = [[[CurvePanelView alloc] init] autorelease]; // flipped, so the list starts at the top
        [document setTranslatesAutoresizingMaskIntoConstraints: NO];
        [document addSubview: list];
        [scroll setDocumentView: document];

        NSClipView* clip = [scroll contentView];
        [NSLayoutConstraint activateConstraints: @[
            [[list leadingAnchor] constraintEqualToAnchor: [document leadingAnchor]],
            [[list topAnchor] constraintEqualToAnchor: [document topAnchor]],
            [[list bottomAnchor] constraintEqualToAnchor: [document bottomAnchor]],
            [[document leadingAnchor] constraintEqualToAnchor: [clip leadingAnchor]],
            [[document trailingAnchor] constraintEqualToAnchor: [clip trailingAnchor]],
            [[document topAnchor] constraintEqualToAnchor: [clip topAnchor]],
            [[scroll heightAnchor] constraintEqualToConstant: maxHeight]
        ]];

        addFullWidth (scroll);
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

        volumeSlider = [NSSlider sliderWithValue: 0 minValue: 0 maxValue: isInterface ? 1.0 : state.maxVolumeSteps
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
        NSButton* editor = [NSButton buttonWithTitle: state.isEditorShowing ? @"Hide Editor" : @"Show Editor"
                                              target: toggleEditor action: @selector (perform:)];
        [editor setBezelStyle: NSBezelStyleRounded];
        [editor setControlSize: NSControlSizeSmall];

        if (state.numPluginWindows == 0)
            return makeRow (@[ editor, makeSpacer() ]);

        // The preset's plug-in windows, as a set, e.g. to get them out of the way and back
        auto* toggleWindows = makeAction ([this] (id) { source.togglePluginWindows(); });
        NSButton* windows = [NSButton buttonWithTitle: state.arePluginWindowsHidden ? @"Show Plug-in Windows" : @"Hide Plug-in Windows"
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

        if (! juce::isPositiveAndBelow (index, presetFiles.size()))
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

void MenuBarPanel::setSetting (const char* key, bool value)
{
    if (auto* settings = getUserSettings())
    {
        settings->setValue (key, value);
        settings->saveIfNeeded();
    }
}
