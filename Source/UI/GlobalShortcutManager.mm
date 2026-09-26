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
#import <Carbon/Carbon.h>
#include "GlobalShortcutManager.h"

static EventHotKeyRef sGlobalHotKeyRef = nullptr;
static EventHandlerRef sGlobalEventHandlerRef = nullptr;
static std::function<void()> sGlobalCallback = nullptr;

static OSStatus hotKeyHandler(EventHandlerCallRef nextHandler, EventRef theEvent, void* userData)
{
    juce::ignoreUnused(nextHandler, userData);

    EventHotKeyID hotKeyID;
    GetEventParameter(theEvent, kEventParamDirectObject, typeEventHotKeyID, NULL, sizeof(hotKeyID), NULL, &hotKeyID);

    if (hotKeyID.signature == 'CURV' && hotKeyID.id == 1)
    {
        if (sGlobalCallback)
        {
            juce::MessageManager::callAsync([] {
                if (sGlobalCallback)
                    sGlobalCallback();
            });
        }
        return noErr;
    }
    return CallNextEventHandler(nextHandler, theEvent);
}

bool GlobalShortcutManager::registerGlobalShortcut(uint32_t carbonKeyCode, uint32_t carbonModifiers, std::function<void()> callback)
{
    unregisterGlobalShortcut();

    sGlobalCallback = std::move(callback);

    if (sGlobalEventHandlerRef == nullptr)
    {
        EventTypeSpec eventType;
        eventType.eventClass = kEventClassKeyboard;
        eventType.eventKind = kEventHotKeyPressed;
        InstallApplicationEventHandler(&hotKeyHandler, 1, &eventType, NULL, &sGlobalEventHandlerRef);
    }

    EventHotKeyID hotKeyID;
    hotKeyID.signature = 'CURV';
    hotKeyID.id = 1;

    OSStatus status = RegisterEventHotKey(carbonKeyCode, carbonModifiers, hotKeyID, GetApplicationEventTarget(), 0, &sGlobalHotKeyRef);
    return status == noErr;
}

void GlobalShortcutManager::unregisterGlobalShortcut()
{
    if (sGlobalHotKeyRef != nullptr)
    {
        UnregisterEventHotKey(sGlobalHotKeyRef);
        sGlobalHotKeyRef = nullptr;
    }
}

bool GlobalShortcutManager::isGlobalShortcutRegistered()
{
    return sGlobalHotKeyRef != nullptr;
}

static uint32_t charToCarbonKeyCode(juce::juce_wchar ch)
{
    switch (std::toupper(static_cast<char>(ch)))
    {
        case 'A': return 0x00;
        case 'S': return 0x01;
        case 'D': return 0x02;
        case 'F': return 0x03;
        case 'H': return 0x04;
        case 'G': return 0x05;
        case 'Z': return 0x06;
        case 'X': return 0x07;
        case 'C': return 0x08;
        case 'V': return 0x09;
        case 'B': return 0x0B;
        case 'Q': return 0x0C;
        case 'W': return 0x0D;
        case 'E': return 0x0E;
        case 'R': return 0x0F;
        case 'Y': return 0x10;
        case 'T': return 0x11;
        case '1': return 0x12;
        case '2': return 0x13;
        case '3': return 0x14;
        case '4': return 0x15;
        case '6': return 0x16;
        case '5': return 0x17;
        case '=': return 0x18;
        case '9': return 0x19;
        case '7': return 0x1A;
        case '-': return 0x1B;
        case '8': return 0x1C;
        case '0': return 0x1D;
        case ']': return 0x1E;
        case 'O': return 0x1F;
        case 'U': return 0x20;
        case '[': return 0x21;
        case 'I': return 0x22;
        case 'P': return 0x23;
        case 'L': return 0x25;
        case 'J': return 0x26;
        case '\'': return 0x27;
        case 'K': return 0x28;
        case ';': return 0x29;
        case '\\': return 0x2A;
        case ',': return 0x2B;
        case '/': return 0x2C;
        case 'N': return 0x2D;
        case 'M': return 0x2E;
        case '.': return 0x2F;
        case '`': return 0x32;
        case ' ': return 0x31;
        default:  return 0xFFFFFFFF;
    }
}

static juce::String carbonKeyCodeToString(uint32_t keyCode)
{
    switch (keyCode)
    {
        case 0x00: return "A";
        case 0x01: return "S";
        case 0x02: return "D";
        case 0x03: return "F";
        case 0x04: return "H";
        case 0x05: return "G";
        case 0x06: return "Z";
        case 0x07: return "X";
        case 0x08: return "C";
        case 0x09: return "V";
        case 0x0B: return "B";
        case 0x0C: return "Q";
        case 0x0D: return "W";
        case 0x0E: return "E";
        case 0x0F: return "R";
        case 0x10: return "Y";
        case 0x11: return "T";
        case 0x12: return "1";
        case 0x13: return "2";
        case 0x14: return "3";
        case 0x15: return "4";
        case 0x16: return "6";
        case 0x17: return "5";
        case 0x18: return "=";
        case 0x19: return "9";
        case 0x1A: return "7";
        case 0x1B: return "-";
        case 0x1C: return "8";
        case 0x1D: return "0";
        case 0x1E: return "]";
        case 0x1F: return "O";
        case 0x20: return "U";
        case 0x21: return "[";
        case 0x22: return "I";
        case 0x23: return "P";
        case 0x25: return "L";
        case 0x26: return "J";
        case 0x27: return "'";
        case 0x28: return "K";
        case 0x29: return ";";
        case 0x2A: return "\\";
        case 0x2B: return ",";
        case 0x2C: return "/";
        case 0x2D: return "N";
        case 0x2E: return "M";
        case 0x2F: return ".";
        case 0x31: return "Space";
        case 0x32: return "`";
        case 0x7A: return "F1";
        case 0x78: return "F2";
        case 0x63: return "F3";
        case 0x76: return "F4";
        case 0x60: return "F5";
        case 0x61: return "F6";
        case 0x62: return "F7";
        case 0x64: return "F8";
        case 0x65: return "F9";
        case 0x6D: return "F10";
        case 0x67: return "F11";
        case 0x6F: return "F12";
        default:   return juce::String::toHexString((int) keyCode);
    }
}

bool GlobalShortcutManager::juceKeyPressToCarbon(const juce::KeyPress& keyPress, uint32_t& outCarbonKeyCode, uint32_t& outCarbonModifiers)
{
    outCarbonModifiers = 0;
    auto mods = keyPress.getModifiers();
    if (mods.isCtrlDown())    outCarbonModifiers |= (1 << 12); // controlKey
    if (mods.isAltDown())     outCarbonModifiers |= (1 << 11); // optionKey
    if (mods.isShiftDown())   outCarbonModifiers |= (1 << 9);  // shiftKey
    if (mods.isCommandDown()) outCarbonModifiers |= (1 << 8);  // cmdKey

    // Check function keys
    int juceCode = keyPress.getKeyCode();
    if (juceCode >= juce::KeyPress::F1Key && juceCode <= juce::KeyPress::F12Key)
    {
        static const uint32_t fKeyCarbon[] = {
            0x7A, 0x78, 0x63, 0x76, 0x60, 0x61, 0x62, 0x64, 0x65, 0x6D, 0x67, 0x6F
        };
        outCarbonKeyCode = fKeyCarbon[juceCode - juce::KeyPress::F1Key];
        return true;
    }

    juce::juce_wchar ch = keyPress.getTextCharacter();
    if (ch == 0)
        ch = (juce::juce_wchar) juceCode;

    uint32_t code = charToCarbonKeyCode(ch);
    if (code == 0xFFFFFFFF)
        return false;

    outCarbonKeyCode = code;
    return true;
}

juce::String GlobalShortcutManager::carbonShortcutToString(uint32_t carbonKeyCode, uint32_t carbonModifiers)
{
    juce::String result;
    if (carbonModifiers & (1 << 12)) result += juce::String::fromUTF8("\xe2\x8c\x83"); // ⌃ Control
    if (carbonModifiers & (1 << 11)) result += juce::String::fromUTF8("\xe2\x8c\xa5"); // ⌥ Option
    if (carbonModifiers & (1 << 9))  result += juce::String::fromUTF8("\xe2\x87\xa7"); // ⇧ Shift
    if (carbonModifiers & (1 << 8))  result += juce::String::fromUTF8("\xe2\x8c\x98"); // ⌘ Command

    result += carbonKeyCodeToString(carbonKeyCode);
    return result;
}

juce::Rectangle<int> GlobalShortcutManager::getStatusItemScreenBounds(void* nativeHandle)
{
    if (nativeHandle == nullptr)
        return {};

    NSStatusItem* statusItem = (__bridge NSStatusItem*) nativeHandle;
    NSStatusBarButton* button = [statusItem button];
    if (button == nil)
        return {};

    NSWindow* window = [button window];
    if (window == nil)
        return {};

    NSRect windowFrame = [window frame];
    CGFloat primaryScreenHeight = [[[NSScreen screens] firstObject] frame].size.height;

    int juceX = (int) windowFrame.origin.x;
    int juceY = (int) (primaryScreenHeight - (windowFrame.origin.y + windowFrame.size.height));
    int juceW = (int) windowFrame.size.width;
    int juceH = (int) windowFrame.size.height;

    return { juceX, juceY, juceW, juceH };
}

void* GlobalShortcutManager::installMenuKeyMonitor(LocalActionMap actions, std::function<void()> onTriggerDismiss)
{
    auto actionMap = std::make_shared<LocalActionMap>(std::move(actions));
    auto dismissCb = std::make_shared<std::function<void()>>(std::move(onTriggerDismiss));

    id monitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyDown handler:^NSEvent*(NSEvent* event) {
        if (!event)
            return event;

        NSString* chars = [event charactersIgnoringModifiers];
        if (chars == nil || [chars length] == 0)
            return event;

        unichar ch = [chars characterAtIndex:0];
        NSEventModifierFlags flags = [event modifierFlags] & NSEventModifierFlagDeviceIndependentFlagsMask;

        ModifierBank bank = Bank_None;
        bool isCmd = (flags & NSEventModifierFlagCommand) != 0;
        bool isCtrl = (flags & NSEventModifierFlagControl) != 0;
        bool isOpt = (flags & NSEventModifierFlagOption) != 0;
        bool isShift = (flags & NSEventModifierFlagShift) != 0;

        // Pass through if Command or Control is held
        if (isCmd || isCtrl)
            return event;

        if (isOpt && !isShift)
            bank = Bank_Option;
        else if (isShift && !isOpt)
            bank = Bank_Shift;
        else if (!isOpt && !isShift)
            bank = Bank_None;
        else
            return event;

        if (ch >= '0' && ch <= '9')
        {
            LocalShortcutKey key { bank, static_cast<char>(ch) };
            auto it = actionMap->find(key);
            if (it != actionMap->end())
            {
                auto actionToRun = it->second;
                juce::MessageManager::callAsync([dismissCb, actionToRun] {
                    if (*dismissCb)
                        (*dismissCb)();
                    juce::PopupMenu::dismissAllActiveMenus();
                    if (actionToRun)
                        actionToRun();
                });
                return nil; // Consume key event
            }
        }

        return event; // Pass through to JUCE menu
    }];

    if (monitor != nil)
        [monitor retain];

    return (void*) monitor;
}

void GlobalShortcutManager::removeMenuKeyMonitor(void* monitorRef)
{
    if (monitorRef == nullptr)
        return;

    id monitor = (id) monitorRef;
    [NSEvent removeMonitor:monitor];
    [monitor release];
}
