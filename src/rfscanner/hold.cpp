// ---------------------------------------------------------------------------
// hold.cpp
//
// The HOLD-mode logic: finding peaks, entering and leaving HOLD, moving the
// cursor between peaks, and switching between the bar and waterfall views.
//
// Depends on: scan_state.h (globals, layout constants).
// Depended on by: RFScanner.cpp (loop).
// ---------------------------------------------------------------------------

#include "scan_state.h"

// True when channel ch is a local maximum: its activity is above the
// minimum threshold and higher than both neighbours.
bool isLocalPeak(int ch)
{
    if (activity[ch] < LOCAL_PEAK_MIN) return false;

    float left  = ch > 0 ? activity[ch - 1] : 0.0f;
    float right = ch < NUM_CHANNELS - 1 ? activity[ch + 1] : 0.0f;

    return activity[ch] > left && activity[ch] > right;
}

// Finds the next local peak at or after channel "from"+1. Returns -1 if
// there is none.
int nextPeakRight(int from)
{
    for (int c = from + 1; c < NUM_CHANNELS; c++)
    {
        if (isLocalPeak(c)) return c;
    }
    return -1;
}

// Enters HOLD mode and parks the cursor on the loudest channel.
void enterHold()
{
    int best = 0;
    for (int c = 1; c < NUM_CHANNELS; c++)
    {
        if (activity[c] > activity[best]) best = c;
    }

    scanState = HOLD_STATE;
    cursorCh = best;
    drawCursor(cursorCh);
    drawReadout();
}

// Moves the cursor to a different channel, erasing the old one first.
void moveCursor(int ch)
{
    eraseCursor(cursorCh);
    cursorCh = ch;
    drawCursor(cursorCh);
    drawReadout();
}

// Leaves HOLD mode and clears the cursor and readout.
void resumeRun()
{
    eraseCursor(cursorCh);
    clearReadout();
    scanState = RUN_STATE;
}

// Handles one short press on the HOLD/exit button:
//   RUN  -> enter HOLD
//   HOLD -> move to next peak, or resume RUN if there is none
void handleShort()
{
    if (scanState == RUN_STATE)
    {
        enterHold();
        return;
    }

    int next = nextPeakRight(cursorCh);
    if (next >= 0) moveCursor(next);
    else           resumeRun();
}

// Switches between bar and waterfall views. If we were in HOLD the cursor
// and readout are removed before the new view is drawn and then restored,
// because the bar and waterfall views paint the column differently.
void switchView()
{
    bool wasHold = (scanState == HOLD_STATE);

    if (wasHold)
    {
        eraseCursor(cursorCh);
        clearReadout();
    }

    viewMode = (viewMode == VIEW_BARS) ? VIEW_WATERFALL : VIEW_BARS;

    if (viewMode == VIEW_BARS) drawBarScreen();
    else                       drawWaterfallScreen();

    prevStatus[0] = '\0';   // status text was cleared, redraw it

    if (wasHold)
    {
        drawCursor(cursorCh);
        drawReadout();
    }
}