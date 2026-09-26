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

#pragma once

#include <JuceHeader.h>
#include <functional>
#include <unordered_map>

class GlobalShortcutManager {
public:
    enum ModifierBank {
        Bank_None = 0,    // Keys 1..0 -> Presets 1..10
        Bank_Option = 1,  // Option + 1..0 -> Presets 11..20
        Bank_Shift = 2    // Shift + 1..0 -> Presets 21..30
    };

    struct LocalShortcutKey {
        ModifierBank bank = Bank_None;
        char keyChar = '0'; // '1'..'9', '0'

        bool operator==(const LocalShortcutKey& other) const {
            return bank == other.bank && keyChar == other.keyChar;
        }
    };

    struct LocalShortcutKeyHash {
        std::size_t operator()(const LocalShortcutKey& k) const {
            return std::hash<int>()(static_cast<int>(k.bank) * 100 + static_cast<int>(k.keyChar));
        }
    };

    using LocalActionMap = std::unordered_map<LocalShortcutKey, std::function<void()>, LocalShortcutKeyHash>;

    // Global Hotkey Management (Carbon RegisterEventHotKey)
    static bool registerGlobalShortcut(uint32_t carbonKeyCode, uint32_t carbonModifiers, std::function<void()> callback);
    static void unregisterGlobalShortcut();
    static bool isGlobalShortcutRegistered();

    // Key Conversion Helpers
    static bool juceKeyPressToCarbon(const juce::KeyPress& keyPress, uint32_t& outCarbonKeyCode, uint32_t& outCarbonModifiers);
    static juce::String carbonShortcutToString(uint32_t carbonKeyCode, uint32_t carbonModifiers);

    // Default Shortcut: Option + Shift + C
    static constexpr uint32_t defaultCarbonKeyCode = 0x08; // kVK_ANSI_C
    static constexpr uint32_t defaultCarbonModifiers = (1 << 11) | (1 << 9); // optionKey | shiftKey
    static inline juce::String defaultShortcutText() { return juce::String::fromUTF8("\xe2\x8c\xa5\xe2\x87\xa7\x43"); /* ⌥⇧C */ }

    // Screen positioning of macOS NSStatusItem
    static juce::Rectangle<int> getStatusItemScreenBounds(void* nativeHandle);

    // Local Key Monitor during menu display
    static void* installMenuKeyMonitor(LocalActionMap actions, std::function<void()> onTriggerDismiss);
    static void removeMenuKeyMonitor(void* monitorRef);
};
