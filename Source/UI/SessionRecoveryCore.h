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

//==============================================================================
/** What Curve does at launch with unsaved changes it kept: the rule, without JUCE, so it can be
    tested on its own (see SessionRecovery for how they're kept).

    Kept changes are restored without asking only when nothing suggests otherwise. Curve asks
    first when it didn't shut down cleanly, as a plug-in's state may have caused that, and
    restoring it could again; when it was asked to open another preset; when the interface in
    use has a default preset that loads at launch, and it isn't the one the changes were kept on;
    and when the preset has changed since. With both settings off, kept changes are left alone,
    and Curve opens as it always has.
*/
namespace SessionRecoveryCore
{
    struct Launch
    {
        bool keepOnQuit = false, saveForCrashes = false;   // the settings
        bool hasKeptChanges = false;
        bool lastShutdownClean = true;

        bool isOpeningRequestedFile = false;   // e.g. a preset double-clicked in the Finder
        bool isOpeningInterfaceDefault = false; // the interface's default preset, set to load at launch
        bool keptForPresetToOpen = false;       // the preset Curve would open is the one they're for
        bool onSameInterface = false;           // the interface in use is the one they were kept on
        bool presetChangedSince = false;        // the preset file changed after they were kept
    };

    enum class Action
    {
        openAsBefore,   // as without kept changes; any there are discarded
        restore,        // without asking
        ask
    };

    enum class Reason { none, didntShutDownCleanly, requestedFile, interfaceDefault, presetChanged };

    struct Decision
    {
        Action action = Action::openAsBefore;
        Reason reason = Reason::none;
    };

    inline Decision decide (const Launch& l)
    {
        if (! (l.keepOnQuit || l.saveForCrashes) || ! l.hasKeptChanges)
            return {};

        // First, whatever else is so: that dialog warns of it, and offers not to open a preset, in
        // case it's what crashed it, as well as to open the one asked for
        if (! l.lastShutdownClean)
            return { Action::ask, Reason::didntShutDownCleanly };

        if (l.isOpeningRequestedFile && ! l.keptForPresetToOpen)
            return { Action::ask, Reason::requestedFile };

        // On the same interface, it was moved off its default deliberately
        if (l.isOpeningInterfaceDefault && ! l.keptForPresetToOpen && ! l.onSameInterface)
            return { Action::ask, Reason::interfaceDefault };

        if (l.presetChangedSince)
            return { Action::ask, Reason::presetChanged };

        return { Action::restore, Reason::none };
    }
}
