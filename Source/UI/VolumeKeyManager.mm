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

#import <Cocoa/Cocoa.h>
#import <ApplicationServices/ApplicationServices.h>
#include <AudioToolbox/AudioHardwareService.h> // for kAudioHardwareServiceDeviceProperty_VirtualMainVolume
#include <CoreAudio/CoreAudio.h>

#include "VolumeKeyManager.h"
#include "OutputVolume.h"
#include "IdleReleaseController.h"
#include "../Plugins/OutputInterfaceLoopbackNode.h"

#include <algorithm>
#include <atomic>
#include <functional>
#include <future>
#include <thread>

juce::PropertiesFile* getUserSettings();

//==============================================================================
// The overlay, styled like macOS's own (with Liquid Glass from macOS 26) and placed as macOS 27
// places it: a small panel under the menu bar's Sound icon, with the output's name, a speaker
// and a level bar.

@interface CurveVolumeLevelView : NSView
@property (nonatomic) double level;
@end

@implementation CurveVolumeLevelView

- (void) drawRect: (NSRect) dirtyRect
{
    juce::ignoreUnused (dirtyRect);

    const CGFloat height = 6.0;
    const NSRect track = NSMakeRect (0, NSMidY (self.bounds) - height / 2, NSWidth (self.bounds), height);

    [[NSColor.labelColor colorWithAlphaComponent: 0.18] setFill];
    [[NSBezierPath bezierPathWithRoundedRect: track xRadius: height / 2 yRadius: height / 2] fill];

    if (self.level > 0.0)
    {
        NSRect fill = track;
        fill.size.width = std::max (height, NSWidth (track) * self.level);
        [NSColor.labelColor setFill];
        [[NSBezierPath bezierPathWithRoundedRect: fill xRadius: height / 2 yRadius: height / 2] fill];
    }
}

@end

// Where macOS 27 puts its own volume overlay: centred under the Sound icon in the menu bar.
// The icon's frame comes from the menu bar's accessibility elements (Curve has Accessibility
// access for the volume keys), owned by MenuBarAgent in macOS 27 and Control Center before it.
// Returns the distance of the icon's centre from the right edge of its screen, which is the same
// on every display's menu bar, or a negative value if the icon can't be found. Falls back to the
// Control Center icon when the Sound icon isn't shown.
static CGFloat findSoundIconOffsetFromRight()
{
    static NSString* const owners[] = { @"com.apple.MenuBarAgent", @"com.apple.controlcenter" };

    auto copyAttribute = [] (AXUIElementRef element, CFStringRef attribute) -> CFTypeRef
    {
        CFTypeRef value = nullptr;
        return AXUIElementCopyAttributeValue (element, attribute, &value) == kAXErrorSuccess ? value : nullptr;
    };

    auto frameOf = [&] (AXUIElementRef element) -> CGRect
    {
        CGRect frame = CGRectNull;
        CGPoint position;
        CGSize size;

        if (CFTypeRef value = copyAttribute (element, kAXPositionAttribute))
        {
            if (AXValueGetValue ((AXValueRef) value, (AXValueType) kAXValueCGPointType, &position))
                frame = CGRectMake (position.x, position.y, 0, 0);

            CFRelease (value);
        }

        if (CFTypeRef value = copyAttribute (element, kAXSizeAttribute))
        {
            if (! CGRectIsNull (frame) && AXValueGetValue ((AXValueRef) value, (AXValueType) kAXValueCGSizeType, &size))
                frame.size = size;

            CFRelease (value);
        }

        return frame;
    };

    // These calls run on the message thread, so don't let a busy menu bar hold it up for
    // macOS's default of several seconds. The system-wide element sets the timeout for every
    // element, including the bar's children; Curve makes no other accessibility calls.
    static const bool timeoutSet = []
    {
        AXUIElementRef systemWide = AXUIElementCreateSystemWide();
        AXUIElementSetMessagingTimeout (systemWide, 0.25f);
        CFRelease (systemWide);
        return true;
    }();
    juce::ignoreUnused (timeoutSet);

    CGRect sound = CGRectNull, controlCenter = CGRectNull;

    // Searches the bar and its groups for the two icons, which macOS 27 puts in a group each
    std::function<void (AXUIElementRef, int)> search = [&] (AXUIElementRef element, int depth)
    {
        if (CFTypeRef identifier = copyAttribute (element, CFSTR ("AXIdentifier")))
        {
            if (CFGetTypeID (identifier) == CFStringGetTypeID())
            {
                if (CFEqual (identifier, CFSTR ("com.apple.menuextra.sound")))
                    sound = frameOf (element);
                else if (CFEqual (identifier, CFSTR ("com.apple.menuextra.controlcenter")))
                    controlCenter = frameOf (element);
            }

            CFRelease (identifier);
        }

        if (depth >= 2 || ! CGRectIsNull (sound))
            return;

        if (CFTypeRef children = copyAttribute (element, kAXChildrenAttribute))
        {
            if (CFGetTypeID (children) == CFArrayGetTypeID())
                for (CFIndex i = 0; i < CFArrayGetCount ((CFArrayRef) children); ++i)
                    search ((AXUIElementRef) CFArrayGetValueAtIndex ((CFArrayRef) children, i), depth + 1);

            CFRelease (children);
        }
    };

    for (NSString* owner : owners)
    {
        for (NSRunningApplication* app in [NSRunningApplication runningApplicationsWithBundleIdentifier: owner])
        {
            AXUIElementRef appElement = AXUIElementCreateApplication (app.processIdentifier);

            if (CFTypeRef bar = copyAttribute (appElement, CFSTR ("AXExtrasMenuBar")))
            {
                search ((AXUIElementRef) bar, 0);
                CFRelease (bar);
            }

            CFRelease (appElement);
        }

        if (! CGRectIsNull (sound))
            break;
    }

    const CGRect icon = ! CGRectIsNull (sound) ? sound : controlCenter;

    if (CGRectIsNull (icon) || NSScreen.screens.count == 0)
        return -1.0;

    // Accessibility frames are measured down from the top of the primary screen
    const CGFloat primaryTop = NSMaxY (NSScreen.screens[0].frame);
    const NSPoint centre = NSMakePoint (CGRectGetMidX (icon), primaryTop - CGRectGetMidY (icon));

    for (NSScreen* screen in NSScreen.screens)
        if (NSPointInRect (centre, screen.frame))
            return NSMaxX (screen.frame) - centre.x;

    return -1.0;
}

@interface CurveVolumeOverlay : NSObject
{
    NSPanel* panel;
    NSTextField* titleLabel;
    NSImageView* iconView;
    CurveVolumeLevelView* levelView;
    NSUInteger generation;
    CGFloat soundIconOffsetFromRight;
}
- (void) showLevel: (double) level muted: (BOOL) muted title: (NSString*) title;
- (void) close;
@end

@implementation CurveVolumeOverlay

static constexpr CGFloat overlayWidth = 280, overlayHeight = 60, overlayPadding = 14, screenMargin = 10;

- (instancetype) init
{
    if ((self = [super init]) == nil)
        return nil;

    const NSRect frame = NSMakeRect (0, 0, overlayWidth, overlayHeight);

    panel = [[NSPanel alloc] initWithContentRect: frame
                                       styleMask: NSWindowStyleMaskBorderless | NSWindowStyleMaskNonactivatingPanel
                                         backing: NSBackingStoreBuffered
                                           defer: YES];
    panel.opaque = NO;
    panel.backgroundColor = NSColor.clearColor;
    panel.hasShadow = YES;
    panel.level = NSStatusWindowLevel;
    panel.ignoresMouseEvents = YES;
    panel.hidesOnDeactivate = NO; // Curve is almost never the active app
    panel.releasedWhenClosed = NO;
    panel.collectionBehavior = NSWindowCollectionBehaviorCanJoinAllSpaces
                             | NSWindowCollectionBehaviorFullScreenAuxiliary
                             | NSWindowCollectionBehaviorStationary
                             | NSWindowCollectionBehaviorIgnoresCycle;

    NSView* content = [[[NSView alloc] initWithFrame: frame] autorelease];

    titleLabel = [[NSTextField labelWithString: @""] retain];
    titleLabel.font = [NSFont systemFontOfSize: 13 weight: NSFontWeightSemibold];
    titleLabel.textColor = NSColor.labelColor;
    titleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
    titleLabel.frame = NSMakeRect (overlayPadding, overlayHeight - overlayPadding - 16,
                                   overlayWidth - 2 * overlayPadding, 16);
    [content addSubview: titleLabel];

    // Fixed width, so the bar doesn't move as the speaker's waves change
    const CGFloat iconWidth = 24;
    iconView = [[NSImageView alloc] initWithFrame: NSMakeRect (overlayPadding - 2, 8, iconWidth, 20)];
    iconView.imageScaling = NSImageScaleProportionallyDown;
    iconView.imageAlignment = NSImageAlignLeft;
    iconView.contentTintColor = NSColor.labelColor;
    iconView.symbolConfiguration = [NSImageSymbolConfiguration configurationWithPointSize: 14 weight: NSFontWeightMedium];
    [content addSubview: iconView];

    const CGFloat barX = overlayPadding + iconWidth + 4;
    levelView = [[CurveVolumeLevelView alloc] initWithFrame: NSMakeRect (barX, 8, overlayWidth - overlayPadding - barX, 20)];
    [content addSubview: levelView];

    if (@available (macOS 26.0, *))
    {
        NSGlassEffectView* glass = [[[NSGlassEffectView alloc] initWithFrame: frame] autorelease];
        glass.cornerRadius = 18;
        glass.contentView = content;
        panel.contentView = glass;
    }
    else
    {
        NSVisualEffectView* effect = [[[NSVisualEffectView alloc] initWithFrame: frame] autorelease];
        effect.material = NSVisualEffectMaterialPopover;
        effect.blendingMode = NSVisualEffectBlendingModeBehindWindow;
        effect.state = NSVisualEffectStateActive;
        effect.wantsLayer = YES;
        effect.layer.cornerRadius = 14;
        effect.layer.masksToBounds = YES;
        [effect addSubview: content];
        panel.contentView = effect;
    }

    // The first query of the menu bar is slow (~65 ms), so make it now rather than on a key press
    soundIconOffsetFromRight = findSoundIconOffsetFromRight();

    return self;
}

- (void) dealloc
{
    [panel orderOut: nil];
    [panel release];
    [titleLabel release];
    [iconView release];
    [levelView release];
    [super dealloc];
}

- (void) close
{
    ++generation;
    [panel orderOut: nil];
}

static NSString* symbolForLevel (double level, BOOL muted)
{
    if (muted || level <= 0.0)  return @"speaker.slash.fill";
    if (level < 1.0 / 3.0)      return @"speaker.wave.1.fill";
    if (level < 2.0 / 3.0)      return @"speaker.wave.2.fill";
    return @"speaker.wave.3.fill";
}

// The screen with the pointer, as macOS's overlay follows the user's attention
static NSScreen* screenForOverlay()
{
    const NSPoint mouse = NSEvent.mouseLocation;

    for (NSScreen* screen in NSScreen.screens)
        if (NSMouseInRect (mouse, screen.frame, NO))
            return screen;

    return NSScreen.mainScreen;
}

- (void) showLevel: (double) level muted: (BOOL) muted title: (NSString*) title
{
    titleLabel.stringValue = title;
    iconView.image = [NSImage imageWithSystemSymbolName: symbolForLevel (level, muted) accessibilityDescription: nil];
    levelView.level = muted ? 0.0 : level;
    [levelView setNeedsDisplay: YES];

    // Look again at the start of each burst of presses, in case the menu bar has changed
    if (! panel.isVisible)
        soundIconOffsetFromRight = findSoundIconOffsetFromRight();

    if (NSScreen* screen = screenForOverlay())
    {
        const NSRect visible = screen.visibleFrame;
        const CGFloat rightmost = NSMaxX (visible) - overlayWidth - screenMargin;
        CGFloat x = rightmost;

        if (soundIconOffsetFromRight >= 0.0)
            x = std::clamp (NSMaxX (screen.frame) - soundIconOffsetFromRight - overlayWidth / 2,
                            NSMinX (visible) + screenMargin, rightmost);

        [panel setFrameOrigin: NSMakePoint (x, NSMaxY (visible) - overlayHeight - screenMargin)];
    }

    const NSUInteger showGeneration = ++generation;

    if (! panel.isVisible)
        panel.alphaValue = 0.0;

    [panel orderFrontRegardless];

    [NSAnimationContext runAnimationGroup: ^(NSAnimationContext* context) {
        context.duration = 0.12;
        panel.animator.alphaValue = 1.0;
    } completionHandler: nil];

    dispatch_after (dispatch_time (DISPATCH_TIME_NOW, (int64_t) (1.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        if (generation != showGeneration)
            return;

        [NSAnimationContext runAnimationGroup: ^(NSAnimationContext* context) {
            context.duration = 0.35;
            panel.animator.alphaValue = 0.0;
        } completionHandler: ^{
            if (generation == showGeneration)
                [panel orderOut: nil];
        }];
    });
}

@end

//==============================================================================
namespace
{
    constexpr const char* modeKey    = "volumeKeysMode";
    constexpr const char* overlayKey = "volumeKeysOverlay";
    constexpr const char* targetKey  = "volumeKeysTarget";
    constexpr const char* stepsKey   = "outputVolumeSteps";
    constexpr const char* mutedKey   = "outputVolumeMuted";

    // From IOKit/hidsystem/ev_keymap.h: the media keys arrive as system-defined events of
    // subtype 8, with the key in the top 16 bits of data1 and its state in the next 8
    constexpr short auxControlButtonsSubtype = 8;
    constexpr int keySoundUp = 0, keySoundDown = 1, keyMute = 7;
    constexpr int keyStateDown = 0xA;

    // True while the tap runs, which is only while Curve takes the keys. Written on the message
    // thread, read by the tap, which passes every key through while it's false.
    std::atomic<bool> takingKeys { false };

    // True while Curve's output interface has no volume macOS can set, so Curve's own volume is
    // the only one there is, and applies whether or not Curve takes the keys. Message thread writes
    std::atomic<bool> curveVolumeIsOnlyVolume { false };

    // Written by the tap while it runs, and on the message thread only before it starts
    std::atomic<int> volumeSteps { OutputVolume::maxSteps };
    std::atomic<bool> volumeMuted { false };

    // Written on the message thread only while the tap isn't running
    std::atomic<VolumeKeyManager::Target> volumeTarget { VolumeKeyManager::Target::output };

    void applyGain()
    {
        if (! takingKeys.load() && ! curveVolumeIsOnlyVolume.load())
        {
            OutputVolume::setting.store ({ 1.0f, OutputVolume::none });
            return;
        }

        const bool loopback = volumeTarget.load() == VolumeKeyManager::Target::loopback;
        OutputVolume::setting.store ({ OutputVolume::stepsToGain (volumeSteps.load(), volumeMuted.load()),
                                       loopback ? OutputVolume::loopback : OutputVolume::output });
    }

    void keyHandledOnTapThread();
    void systemOutputChanged();

    //==========================================================================
    /** A Quartz event tap for system-defined events, on its own thread, so the keys stay
        responsive while the message thread is busy (e.g. loading a preset's plug-ins). */
    class KeyTap
    {
    public:
        KeyTap() = default;
        ~KeyTap() { stop(); }

        bool isRunning() const { return running; }

        /** Fails without Accessibility access. */
        bool start()
        {
            if (running)
                return true;

            std::promise<bool> started;
            auto result = started.get_future();
            stopRequested = false;

            thread = std::thread ([this, &started] { run (started); });

            running = result.get();

            if (! running)
                thread.join();

            return running;
        }

        void stop()
        {
            if (! running)
                return;

            stopRequested = true;

            // CFRunLoopStop() is forgotten if it arrives while the loop is between runs, which
            // would hold up the join for a whole run. A queued block isn't lost.
            CFRunLoopPerformBlock (runLoop, kCFRunLoopDefaultMode, ^{ CFRunLoopStop (CFRunLoopGetCurrent()); });
            CFRunLoopWakeUp (runLoop);

            thread.join();
            running = false;
        }

    private:
        std::thread thread;
        CFRunLoopRef runLoop = nullptr; // the tap thread's, valid while it runs
        CFMachPortRef tap = nullptr;    // used only on the tap thread
        std::atomic<bool> stopRequested { false };
        bool running = false;

        void run (std::promise<bool>& started)
        {
            juce::Thread::setCurrentThreadName ("Curve Volume Keys");

            tap = CGEventTapCreate (kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault,
                                    CGEventMaskBit ((CGEventType) NSEventTypeSystemDefined),
                                    &KeyTap::callback, this);

            if (tap == nullptr)
            {
                started.set_value (false);
                return;
            }

            auto* source = CFMachPortCreateRunLoopSource (kCFAllocatorDefault, tap, 0);
            runLoop = CFRunLoopGetCurrent();
            CFRunLoopAddSource (runLoop, source, kCFRunLoopDefaultMode);
            CGEventTapEnable (tap, true);

            started.set_value (true); // `started` is gone after this

            while (! stopRequested)
                CFRunLoopRunInMode (kCFRunLoopDefaultMode, 0.5, false);

            CGEventTapEnable (tap, false);
            CFRunLoopRemoveSource (runLoop, source, kCFRunLoopDefaultMode);
            CFRelease (source);
            CFMachPortInvalidate (tap);
            CFRelease (tap);
            tap = nullptr;
        }

        static CGEventRef callback (CGEventTapProxy, CGEventType type, CGEventRef event, void* userInfo)
        {
            auto* self = static_cast<KeyTap*> (userInfo);

            // macOS turns off a tap that is slow to respond; turn it straight back on
            if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput)
            {
                if (self->tap != nullptr)
                    CGEventTapEnable (self->tap, true);

                return event;
            }

            if (type != (CGEventType) NSEventTypeSystemDefined || ! takingKeys.load())
                return event;

            @autoreleasepool
            {
                NSEvent* nsEvent = [NSEvent eventWithCGEvent: event];

                if (nsEvent == nil || nsEvent.subtype != auxControlButtonsSubtype)
                    return event;

                const auto data1 = (juce::uint64) nsEvent.data1;
                const int key = (int) ((data1 & 0xFFFF0000) >> 16);
                const int flags = (int) (data1 & 0xFFFF);
                const bool isDown = ((flags & 0xFF00) >> 8) == keyStateDown;
                const bool isRepeat = (flags & 0x1) != 0;

                if (key != keySoundUp && key != keySoundDown && key != keyMute)
                    return event;

                const auto modifiers = nsEvent.modifierFlags & NSEventModifierFlagDeviceIndependentFlagsMask;
                const bool option = (modifiers & NSEventModifierFlagOption) != 0;
                const bool shift  = (modifiers & NSEventModifierFlagShift) != 0;

                // Option with a volume key opens Sound settings in macOS: leave it to macOS
                if (option && ! shift)
                    return event;

                // Consume the key up too, so macOS sees neither half of the press
                if (! isDown)
                    return nullptr;

                if (key == keyMute)
                {
                    if (isRepeat)
                        return nullptr;

                    volumeMuted.store (! volumeMuted.load());
                }
                else
                {
                    volumeSteps.store (OutputVolume::stepVolume (volumeSteps.load(), key == keySoundUp, option && shift));
                    volumeMuted.store (false);
                }

                applyGain();
                keyHandledOnTapThread();
                return nullptr;
            }
        }

        JUCE_DECLARE_NON_COPYABLE (KeyTap)
    };

    //==========================================================================
    AudioObjectID getDefaultOutputDevice()
    {
        AudioObjectPropertyAddress address { kAudioHardwarePropertyDefaultOutputDevice,
                                             kAudioObjectPropertyScopeGlobal,
                                             kAudioObjectPropertyElementMain };
        AudioObjectID device = kAudioObjectUnknown;
        UInt32 size = sizeof (device);

        if (AudioObjectGetPropertyData (kAudioObjectSystemObject, &address, 0, nullptr, &size, &device) != noErr)
            return kAudioObjectUnknown;

        return device;
    }

    bool isSettable (AudioObjectID device, AudioObjectPropertySelector selector, AudioObjectPropertyElement element)
    {
        AudioObjectPropertyAddress address { selector, kAudioDevicePropertyScopeOutput, element };

        if (! AudioObjectHasProperty (device, &address))
            return false;

        Boolean settable = false;
        return AudioObjectIsPropertySettable (device, &address, &settable) == noErr && settable;
    }

    // Whether macOS's volume keys and slider change every channel Curve plays through.
    // macOS's volume is the device's main volume control if it has one, otherwise the controls
    // of its preferred stereo pair. An interface such as an RME Babyface has controls on its
    // first pair only, so macOS shows its slider even when Curve plays through the headphone
    // channels, and the slider then does nothing to Curve's audio.
    bool macOSVolumeReachesChannels (AudioObjectID device, const juce::BigInteger& channels)
    {
        if (! isSettable (device, kAudioHardwareServiceDeviceProperty_VirtualMainVolume, kAudioObjectPropertyElementMain))
            return false;

        if (isSettable (device, kAudioDevicePropertyVolumeScalar, kAudioObjectPropertyElementMain))
            return true;

        AudioObjectPropertyAddress address { kAudioDevicePropertyPreferredChannelsForStereo,
                                             kAudioDevicePropertyScopeOutput,
                                             kAudioObjectPropertyElementMain };
        UInt32 stereoPair[2] = { 1, 2 };
        UInt32 size = sizeof (stereoPair);
        AudioObjectGetPropertyData (device, &address, 0, nullptr, &size, stereoPair);

        // Channel numbers are 1-based in CoreAudio and 0-based in JUCE
        for (int channel = channels.findNextSetBit (0); channel >= 0; channel = channels.findNextSetBit (channel + 1))
            if ((UInt32) channel + 1 != stereoPair[0] && (UInt32) channel + 1 != stereoPair[1])
                return false;

        return true;
    }

    //==========================================================================
    class Controller : private juce::Timer
    {
    public:
        Controller()
        {
            if (auto* settings = getUserSettings())
            {
                mode = (VolumeKeyManager::Mode) juce::jlimit (0, 2, settings->getIntValue (modeKey, 0));
                overlayEnabled = settings->getBoolValue (overlayKey, true);
                volumeTarget.store ((VolumeKeyManager::Target) juce::jlimit (0, 1, settings->getIntValue (targetKey, 0)));
                volumeSteps.store (OutputVolume::clampSteps (settings->getIntValue (stepsKey, OutputVolume::maxSteps)));
                volumeMuted.store (settings->getBoolValue (mutedKey, false));
            }

            saveTimer.save = [this] { saveVolume(); };

            defaultOutputListener = Block_copy (^(UInt32, const AudioObjectPropertyAddress*) {
                systemOutputChanged();
            });
            AudioObjectAddPropertyListenerBlock (kAudioObjectSystemObject, &defaultOutputAddress,
                                                 dispatch_get_main_queue(), defaultOutputListener);

            // At launch as well as when the keys are turned on, e.g. after access was reset
            // or a rebuilt Curve no longer matches the entry in System Settings
            requestAccessIfNeeded();
            update();
        }

        ~Controller() override
        {
            stopTimer();
            tap.stop();

            // The gain is left as it is: this runs at quit while the audio is still playing,
            // and going back to full level would be heard as a jump
            takingKeys.store (false);

            AudioObjectRemovePropertyListenerBlock (kAudioObjectSystemObject, &defaultOutputAddress,
                                                    dispatch_get_main_queue(), defaultOutputListener);
            Block_release (defaultOutputListener);

            [overlay close];
            [overlay release];

            if (saveTimer.isTimerRunning())
            {
                saveTimer.stopTimer();
                saveVolume();
            }
        }

        VolumeKeyManager::Mode getMode() const { return mode; }

        void setMode (VolumeKeyManager::Mode newMode)
        {
            mode = newMode;

            if (auto* settings = getUserSettings())
            {
                settings->setValue (modeKey, (int) mode);
                settings->saveIfNeeded();
            }

            requestAccessIfNeeded();
            update();
        }

        void setTarget (VolumeKeyManager::Target newTarget)
        {
            if (newTarget == volumeTarget.load())
                return;

            if (auto* settings = getUserSettings())
            {
                settings->setValue (targetKey, (int) newTarget);
                settings->saveIfNeeded();
            }

            // The tap is stopped first, so its thread never applies the gain to the old target
            // after it has moved to the new one. The keys stay taken meanwhile, and update()
            // then either moves the gain straight to the new target and starts the tap again,
            // or, if the new target has nothing to control, gives the keys back to macOS and
            // ramps to full level. It publishes the gain itself in both cases, since the tap
            // isn't running.
            if (tap.isRunning())
                tap.stop();

            volumeTarget.store (newTarget);
            update();
        }

        bool isOverlayEnabled() const { return overlayEnabled; }

        void setOverlayEnabled (bool shouldShow)
        {
            overlayEnabled = shouldShow;

            if (! overlayEnabled)
                [overlay close];

            if (auto* settings = getUserSettings())
            {
                settings->setValue (overlayKey, overlayEnabled);
                settings->saveIfNeeded();
            }
        }

        // Asked when Curve's menu opens, which is also when a revoked access is noticed
        bool needsAccessibilityAccess()
        {
            if (tap.isRunning() && ! AXIsProcessTrusted())
                update();

            return mode != VolumeKeyManager::Mode::off && (! AXIsProcessTrusted() || tapFailed);
        }

        void setCurveOutput (const juce::String& name, const juce::BigInteger& channels)
        {
            if (name.isEmpty() || (name == curveOutputDevice && channels == curveOutputChannels))
                return;

            curveOutputDevice = name;
            curveOutputChannels = channels;
            update();
        }

        // Decides whether to take the keys, and runs the tap only while they're taken. Called
        // when the mode, macOS's output or Curve's output changes. macOS doesn't announce when
        // Accessibility access is granted, so while the keys are on without it, this checks
        // again every 2 seconds on the message thread; otherwise nothing is polled.
        void update()
        {
            const bool wanted = mode != VolumeKeyManager::Mode::off;
            const bool trusted = wanted && AXIsProcessTrusted();
            const bool take = trusted && shouldTakeKeys();

            // Made before the tap starts: its first look at the menu bar is slow (~65 ms)
            if (take && overlayEnabled && overlay == nil)
                overlay = [[CurveVolumeOverlay alloc] init];

            // The volume is only touched here while the tap isn't running, so this thread never
            // writes the gain at the same time as a key press on the tap thread
            if (take && ! tap.isRunning())
            {
                takingKeys.store (true);
                applyGain();

                if (! tap.start())
                {
                    takingKeys.store (false);
                    applyGain();
                }
            }
            else if (! take && (tap.isRunning() || takingKeys.load()))
            {
                stopTaking();
            }

            tapFailed = take && ! tap.isRunning();

            // Whether Curve's own volume applies without the keys. The gain is only published here
            // while the tap isn't running, which applies it itself
            const auto device = (AudioObjectID) IdleReleaseCoreAudio::findDeviceID (curveOutputDevice);
            const bool onlyVolume = curveOutputDevice.isNotEmpty() && device != kAudioObjectUnknown
                                 && ! macOSVolumeReachesChannels (device, curveOutputChannels);

            if (onlyVolume != curveVolumeIsOnlyVolume.load())
            {
                curveVolumeIsOnlyVolume.store (onlyVolume);

                if (! tap.isRunning())
                    applyGain();
            }

            if (wanted && (! trusted || tapFailed))
                startTimer (2000);
            else
                stopTimer();
        }

        // The interface's own volume, which macOS's volume menu sets, for Curve's output channels
        AudioObjectID getInterfaceWithVolume() const
        {
            const auto device = (AudioObjectID) IdleReleaseCoreAudio::findDeviceID (curveOutputDevice);

            if (curveOutputDevice.isEmpty() || device == kAudioObjectUnknown
                || ! macOSVolumeReachesChannels (device, curveOutputChannels))
                return kAudioObjectUnknown;

            return device;
        }

        // From the menu bar panel's slider: as a key press would set it, without the overlay
        void setVolume (int steps, bool muted)
        {
            volumeSteps.store (OutputVolume::clampSteps (steps));
            volumeMuted.store (muted);
            applyGain();
            saveTimer.startTimer (2000);
        }

        void keyHandled()
        {
            // Saved once the keys have been left alone for a moment
            saveTimer.startTimer (2000);

            if (overlayEnabled)
            {
                if (overlay == nil)
                    overlay = [[CurveVolumeOverlay alloc] init];

                const auto title = curveOutputDevice.isNotEmpty() ? curveOutputDevice : juce::String ("Curve");
                [overlay showLevel: (double) volumeSteps.load() / OutputVolume::maxSteps
                             muted: volumeMuted.load()
                             title: [NSString stringWithUTF8String: title.toRawUTF8()]];
            }
        }

    private:
        static inline const AudioObjectPropertyAddress defaultOutputAddress {
            kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain
        };

        VolumeKeyManager::Mode mode = VolumeKeyManager::Mode::off;
        bool overlayEnabled = true;
        juce::String curveOutputDevice;
        juce::BigInteger curveOutputChannels;
        KeyTap tap;
        CurveVolumeOverlay* overlay = nil;
        AudioObjectPropertyListenerBlock defaultOutputListener = nil;

        struct SaveTimer final : juce::Timer
        {
            std::function<void()> save;
            void timerCallback() override { stopTimer(); save(); }
        };
        SaveTimer saveTimer;

        void stopTaking()
        {
            if (tap.isRunning())
                tap.stop(); // joins the tap thread

            takingKeys.store (false);
            applyGain();
            [overlay close];
        }

        // Shows macOS's prompt, which leads to the Accessibility pane in System Settings
        void requestAccessIfNeeded()
        {
            if (mode != VolumeKeyManager::Mode::off && ! AXIsProcessTrusted())
                AXIsProcessTrustedWithOptions ((CFDictionaryRef) @{ (id) kAXTrustedCheckOptionPrompt: @YES });
        }

        bool shouldTakeKeys() const
        {
            // Without a connected loopback node there's nothing for the keys to control, so
            // they're left to macOS
            if (volumeTarget.load() == VolumeKeyManager::Target::loopback
                && ! OutputInterfaceLoopbackNode::isAnyTapActiveInGraph())
                return false;

            if (mode == VolumeKeyManager::Mode::always)
                return true;

            if (mode != VolumeKeyManager::Mode::automatic)
                return false;

            const auto systemOutput = getDefaultOutputDevice();
            return systemOutput != kAudioObjectUnknown
                && systemOutput == (AudioObjectID) IdleReleaseCoreAudio::findDeviceID (curveOutputDevice)
                && ! macOSVolumeReachesChannels (systemOutput, curveOutputChannels);
        }

        void saveVolume()
        {
            if (auto* settings = getUserSettings())
            {
                settings->setValue (stepsKey, volumeSteps.load());
                settings->setValue (mutedKey, volumeMuted.load());
                settings->saveIfNeeded();
            }
        }

        void timerCallback() override
        {
            if (AXIsProcessTrusted())
                update();
        }

        bool tapFailed = false; // the keys should be taken but the tap couldn't start

        JUCE_DECLARE_NON_COPYABLE (Controller)
    };

    std::unique_ptr<Controller> controller;

    void systemOutputChanged()
    {
        if (controller != nullptr)
            controller->update();
    }

    void keyHandledOnTapThread()
    {
        dispatch_async (dispatch_get_main_queue(), ^{
            if (controller != nullptr)
                controller->keyHandled();
        });
    }
}

//==============================================================================
namespace VolumeKeyManager
{
    void start()
    {
        if (controller == nullptr)
            controller = std::make_unique<Controller>();
    }

    void stop()
    {
        controller = nullptr;
    }

    Mode getMode()
    {
        return controller != nullptr ? controller->getMode() : Mode::off;
    }

    void setMode (Mode mode)
    {
        if (controller != nullptr)
            controller->setMode (mode);
    }

    Target getTarget()
    {
        return volumeTarget.load();
    }

    void setTarget (Target target)
    {
        if (controller != nullptr)
            controller->setTarget (target);
    }

    void loopbackUsageChanged()
    {
        if (controller != nullptr)
            controller->update();
    }

    bool isOverlayEnabled()
    {
        return controller == nullptr || controller->isOverlayEnabled();
    }

    void setOverlayEnabled (bool shouldShow)
    {
        if (controller != nullptr)
            controller->setOverlayEnabled (shouldShow);
    }

    bool needsAccessibilityAccess()
    {
        return controller != nullptr && controller->needsAccessibilityAccess();
    }

    void openAccessibilitySettings()
    {
        if (NSURL* url = [NSURL URLWithString: @"x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility"])
            [[NSWorkspace sharedWorkspace] openURL: url];
    }

    bool isControllingVolume()
    {
        return takingKeys.load();
    }

    bool hasConnectedLoopback()
    {
        return OutputInterfaceLoopbackNode::isAnyTapActiveInGraph();
    }

    int getVolumeSteps()
    {
        return volumeSteps.load();
    }

    bool isVolumeMuted()
    {
        return volumeMuted.load();
    }

    void setVolume (int steps, bool muted)
    {
        if (controller != nullptr)
            controller->setVolume (steps, muted);
    }

    VolumeControl getVolumeControl()
    {
        if (takingKeys.load())
            return VolumeControl::curve;

        if (controller != nullptr && controller->getInterfaceWithVolume() != kAudioObjectUnknown)
            return VolumeControl::interface;

        return curveVolumeIsOnlyVolume.load() ? VolumeControl::curve : VolumeControl::none;
    }

    static constexpr AudioObjectPropertyAddress interfaceVolumeAddress {
        kAudioHardwareServiceDeviceProperty_VirtualMainVolume, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMain
    };

    float getInterfaceVolume()
    {
        const auto device = controller != nullptr ? controller->getInterfaceWithVolume() : kAudioObjectUnknown;
        Float32 volume = 0.0f;
        UInt32 size = sizeof (volume);

        if (device == kAudioObjectUnknown
            || AudioObjectGetPropertyData (device, &interfaceVolumeAddress, 0, nullptr, &size, &volume) != noErr)
            return 0.0f;

        return volume;
    }

    void setInterfaceVolume (float volume)
    {
        const auto device = controller != nullptr ? controller->getInterfaceWithVolume() : kAudioObjectUnknown;
        Float32 value = juce::jlimit (0.0f, 1.0f, volume);

        if (device != kAudioObjectUnknown)
            AudioObjectSetPropertyData (device, &interfaceVolumeAddress, 0, nullptr, sizeof (value), &value);
    }

    juce::String describeVolume()
    {
        const auto steps = volumeSteps.load();

        if (volumeMuted.load() || steps == 0)
            return "Muted";

        return juce::String (OutputVolume::stepsToDb (steps), 1) + " dB";
    }

    void setCurveOutput (const juce::String& deviceName, const juce::BigInteger& outputChannels)
    {
        if (controller != nullptr)
            controller->setCurveOutput (deviceName, outputChannels);
    }
}
