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

//==============================================================================
/**
    A PDF, or an image as a single page, shown with macOS's PDFKit, a page or two at a time, fitted
    to the view, for the Score Viewer. A horizontal swipe on a trackpad asks to turn the page.
*/
class PdfView final
{
public:
    PdfView();
    ~PdfView();

    /** The NSView, for a juce::NSViewComponent. */
    void* getNSView() const;

    /** Loads a PDF, or a PNG, JPEG, TIFF, GIF or HEIC image. False if it can't be read. */
    bool load (const juce::File&);
    void clear();

    int getNumPages() const;
    void showPage (int index);       // The first is 0. In two-up, the spread it's in
    void setTwoUp (bool);
    void setBackground (juce::Colour);

    /** Called with +1 for a swipe to the next page, -1 for the previous one. */
    std::function<void (int)> onSwipe;

    /** How many pages a file has, without showing it: 0 if it can't be read. */
    static int countPages (const juce::File&);

    static bool canOpen (const juce::File&);

    /** Calls the function for page-turn keys pressed in any of Curve's windows while Curve is the
        active app, other than while typing in a text field: Page Down, Right, Down and Space for
        the next page (+1), Page Up, Left, Up and Shift-Space for the previous one (-1). The key is
        taken if it returns true. Keyboard page turners send these. Remove with the returned token. */
    static void* addPageKeyMonitor (std::function<bool (int)>);
    static void removePageKeyMonitor (void*);

    struct Impl;

private:
    std::unique_ptr<Impl> impl;

    JUCE_DECLARE_NON_COPYABLE (PdfView)
};
