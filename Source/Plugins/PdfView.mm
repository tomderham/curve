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
#include "PdfView.h"

#import <AppKit/AppKit.h>
#import <PDFKit/PDFKit.h>
#import <Carbon/Carbon.h> // kVK_ key codes

//==============================================================================
/** Turns a horizontal two-finger swipe into one page turn, rather than scrolling. */
@interface CurvePDFView : PDFView
{
@public
    std::function<void (int)> onSwipe;
    CGFloat swipeDistance;
    BOOL swipeTurned;
}
@end

@implementation CurvePDFView

- (void) scrollWheel: (NSEvent*) event
{
    const bool horizontal = std::abs ([event scrollingDeltaX]) > std::abs ([event scrollingDeltaY]);

    if (! [event hasPreciseScrollingDeltas] || (! horizontal && swipeDistance == 0))
    {
        [super scrollWheel: event];
        return;
    }

    if ([event phase] == NSEventPhaseBegan)
    {
        swipeDistance = 0;
        swipeTurned = NO;
    }

    swipeDistance += [event scrollingDeltaX];

    // One turn per swipe, once it's gone far enough
    if (! swipeTurned && std::abs (swipeDistance) > 60 && onSwipe)
    {
        swipeTurned = YES;
        onSwipe (swipeDistance < 0 ? 1 : -1);
    }

    if ([event phase] == NSEventPhaseEnded || [event phase] == NSEventPhaseCancelled
        || [event momentumPhase] == NSEventPhaseEnded)
    {
        swipeDistance = 0;
    }
}

// A three-finger swipe, where it's set to swipe between pages
- (void) swipeWithEvent: (NSEvent*) event
{
    if ([event deltaX] != 0 && onSwipe)
        onSwipe ([event deltaX] < 0 ? 1 : -1);
}

@end

//==============================================================================
static PDFDocument* createDocument (const juce::File& file)
{
    NSURL* url = [NSURL fileURLWithPath: [NSString stringWithUTF8String: file.getFullPathName().toRawUTF8()]];

    if (file.hasFileExtension ("pdf"))
        return [[PDFDocument alloc] initWithURL: url];

    // An image, as a single page
    NSImage* image = [[[NSImage alloc] initWithContentsOfURL: url] autorelease];

    if (image == nil)
        return nil;

    PDFPage* page = [[[PDFPage alloc] initWithImage: image] autorelease];

    if (page == nil)
        return nil;

    PDFDocument* document = [[PDFDocument alloc] init];
    [document insertPage: page atIndex: 0];
    return document;
}

struct PdfView::Impl
{
    Impl()
    {
        view = [[CurvePDFView alloc] initWithFrame: NSMakeRect (0, 0, 400, 500)];
        [view setAutoScales: YES];
        [view setDisplayMode: kPDFDisplaySinglePage];
        [view setDisplaysPageBreaks: NO];
        [view setDisplayDirection: kPDFDisplayDirectionHorizontal];
    }

    ~Impl()
    {
        [view setDocument: nil];
        [view release];
    }

    CurvePDFView* view = nil;
    bool twoUp = false;
};

PdfView::PdfView() : impl (std::make_unique<Impl>())
{
    impl->view->onSwipe = [this] (int direction)
    {
        if (onSwipe != nullptr)
            onSwipe (direction);
    };
}

PdfView::~PdfView() = default;

void* PdfView::getNSView() const    { return impl->view; }

bool PdfView::load (const juce::File& file)
{
    PDFDocument* document = createDocument (file);

    if (document == nil)
    {
        clear();
        return false;
    }

    [impl->view setDocument: document];
    [document release];
    [impl->view setAutoScales: YES];
    return true;
}

void PdfView::clear()
{
    [impl->view setDocument: nil];
}

int PdfView::getNumPages() const
{
    PDFDocument* document = [impl->view document];
    return document != nil ? (int) [document pageCount] : 0;
}

void PdfView::showPage (int index)
{
    PDFDocument* document = [impl->view document];

    if (document == nil || [document pageCount] == 0)
        return;

    const auto clamped = juce::jlimit (0, (int) [document pageCount] - 1, index);

    if (PDFPage* page = [document pageAtIndex: (NSUInteger) clamped])
        [impl->view goToPage: page];
}

void PdfView::setTwoUp (bool shouldShowTwo)
{
    impl->twoUp = shouldShowTwo;
    [impl->view setDisplayMode: shouldShowTwo ? kPDFDisplayTwoUp : kPDFDisplaySinglePage];
    [impl->view setDisplaysAsBook: NO];
    [impl->view setAutoScales: YES];
}

void PdfView::setBackground (juce::Colour c)
{
    [impl->view setBackgroundColor: [NSColor colorWithSRGBRed: c.getFloatRed() green: c.getFloatGreen()
                                                         blue: c.getFloatBlue() alpha: 1.0]];
}

int PdfView::countPages (const juce::File& file)
{
    if (! file.existsAsFile())
        return 0;

    @autoreleasepool
    {
        PDFDocument* document = createDocument (file);
        const auto count = document != nil ? (int) [document pageCount] : 0;
        [document release];
        return count;
    }
}

bool PdfView::canOpen (const juce::File& file)
{
    return file.hasFileExtension ("pdf;png;jpg;jpeg;tif;tiff;gif;heic");
}

//==============================================================================
void* PdfView::addPageKeyMonitor (std::function<bool (int)> onTurn)
{
    auto turn = std::make_shared<std::function<bool (int)>> (std::move (onTurn));

    id monitor = [NSEvent addLocalMonitorForEventsMatchingMask: NSEventMaskKeyDown handler: ^NSEvent* (NSEvent* event)
    {
        // Typing, or a command
        if ([[[event window] firstResponder] isKindOfClass: [NSText class]])
            return event;

        const auto flags = [event modifierFlags] & NSEventModifierFlagDeviceIndependentFlagsMask;

        if ((flags & (NSEventModifierFlagCommand | NSEventModifierFlagControl | NSEventModifierFlagOption)) != 0)
            return event;

        int direction = 0;

        switch ([event keyCode])
        {
            case kVK_PageDown: case kVK_RightArrow: case kVK_DownArrow:   direction = 1; break;
            case kVK_PageUp:   case kVK_LeftArrow:  case kVK_UpArrow:     direction = -1; break;
            case kVK_Space:    direction = (flags & NSEventModifierFlagShift) != 0 ? -1 : 1; break;
            default: break;
        }

        return direction != 0 && (*turn) (direction) ? nil : event;
    }];

    return [monitor retain];
}

void PdfView::removePageKeyMonitor (void* token)
{
    if (token == nullptr)
        return;

    id monitor = (id) token;
    [NSEvent removeMonitor: monitor];
    [monitor release];
}
