// Tests SessionRecoveryCore: what Curve does at launch with unsaved changes it kept.

#include "SessionRecoveryCore.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

static int failures = 0;

static void expect (bool ok, const char* what)
{
    if (! ok)
    {
        std::printf ("FAILED: %s\n", what);
        ++failures;
    }
}

int main()
{
    using namespace SessionRecoveryCore;

    // Kept on quit, nothing else going on: restored without asking
    Launch quit;
    quit.keepOnQuit = true;
    quit.hasKeptChanges = true;
    quit.keptForPresetToOpen = true;
    quit.onSameInterface = true;
    expect (decide (quit).action == Action::restore, "restored after a clean quit");

    // Both settings off: as before, even with changes kept from when they were on, or a crash
    for (bool clean : { true, false })
    {
        auto off = quit;
        off.keepOnQuit = false;
        off.lastShutdownClean = clean;
        off.isOpeningRequestedFile = true;
        expect (decide (off).action == Action::openAsBefore, "both off opens as before");
    }

    // Nothing kept: as before
    {
        auto none = quit;
        none.hasKeptChanges = false;
        none.lastShutdownClean = false;
        expect (decide (none).action == Action::openAsBefore, "nothing kept opens as before");
    }

    // Only saving for crashes is enough
    {
        auto crashOnly = quit;
        crashOnly.keepOnQuit = false;
        crashOnly.saveForCrashes = true;
        crashOnly.lastShutdownClean = false;
        const auto d = decide (crashOnly);
        expect (d.action == Action::ask && d.reason == Reason::didntShutDownCleanly, "after a crash, asks");
    }

    // A crash, perhaps from a plug-in's state: asks, never restores without asking
    {
        auto crash = quit;
        crash.lastShutdownClean = false;
        expect (decide (crash).action == Action::ask, "unclean shutdown asks");
    }

    // Asked to open another preset
    {
        auto requested = quit;
        requested.isOpeningRequestedFile = true;
        requested.keptForPresetToOpen = false;
        expect (decide (requested).reason == Reason::requestedFile, "requested file asks");

        requested.keptForPresetToOpen = true;
        expect (decide (requested).action == Action::restore, "requested file that they're for restores");
    }

    // The interface's default preset at launch
    {
        auto def = quit;
        def.isOpeningInterfaceDefault = true;
        def.keptForPresetToOpen = false;

        def.onSameInterface = true;
        expect (decide (def).action == Action::restore, "same interface: moved off its default, so restored");

        def.onSameInterface = false;
        expect (decide (def).reason == Reason::interfaceDefault, "another interface's default asks");

        def.keptForPresetToOpen = true;
        expect (decide (def).action == Action::restore, "kept for the default itself restores");
    }

    // Preset changed since: asks rather than restoring over it
    {
        auto changed = quit;
        changed.presetChangedSince = true;
        expect (decide (changed).reason == Reason::presetChanged, "preset changed since asks");
    }

    if (failures == 0)
        std::printf ("All session recovery tests passed\n");

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
