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
#include "LoginItemManager.h"

#if JUCE_MAC

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import <ServiceManagement/ServiceManagement.h>

namespace LoginItemManager
{
    Status getStatus()
    {
        switch ([SMAppService mainAppService].status)
        {
            case SMAppServiceStatusEnabled:          return Status::enabled;
            case SMAppServiceStatusRequiresApproval:  return Status::requiresApproval;
            case SMAppServiceStatusNotFound:          return Status::notFound;
            case SMAppServiceStatusNotRegistered:
            default:                                  return Status::notRegistered;
        }
    }

    bool setEnabled (bool shouldEnable, juce::String& errorMessage)
    {
        NSError* error = nil;
        BOOL success = NO;

        SMAppService* service = [SMAppService mainAppService];

        if (shouldEnable)
            success = [service registerAndReturnError: &error];
        else
            success = [service unregisterAndReturnError: &error];

        if (! success && error != nil)
            errorMessage = juce::String ([error.localizedDescription UTF8String]);

        return success == YES;
    }

    void openSystemSettingsLoginItemsPane()
    {
        NSURL* url = [NSURL URLWithString: @"x-apple.systempreferences:com.apple.LoginItems-Settings.extension"];

        if (url != nil)
            [[NSWorkspace sharedWorkspace] openURL: url];
    }
}

#else

namespace LoginItemManager
{
    Status getStatus()                                              { return Status::unavailable; }
    bool setEnabled (bool, juce::String& errorMessage)               { errorMessage = "Not supported on this platform."; return false; }
    void openSystemSettingsLoginItemsPane()                          {}
}

#endif
