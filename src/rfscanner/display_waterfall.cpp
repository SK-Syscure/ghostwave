// ---------------------------------------------------------------------------
// display_waterfall.cpp
//
// The waterfall-view drawing code. The waterfall is a 16-bit-per-pixel
// sprite that holds recent sweep history; each sweep scrolls it up and
// writes the newest row at the bottom.
//
// Depends on: scan_state.h (globals, layout constants).
// Depended on by: RFScanner.cpp (setup/loop), display_common.cpp (eraseCursor).
// ---------------------------------------------------------------------------

#include "scan_state.h"

// Allocates the waterfall sprite. Returns false if allocation failed, in
// which case the caller should fall back to the bar view.
bool waterfallInit()
{
    wfSprite.setColorDepth(16);
    wfSprite.createSprite(WF_W, WF_H);
    wfReady = (wfSprite.getPointer() != nullptr);

    if (wfReady) wfSprite.fillSprite(TFT_WHITE);
    return wfReady;
}

// Frees the waterfall sprite and marks it as not ready.
void waterfallDestroy()
{
    if (wfReady)
    {
        wfSprite.deleteSprite();
        wfReady = false;
    }
}

// Pushes the waterfall history to the screen.
void waterfallPush()
{
    if (wfReady) wfSprite.pushSprite(GRAPH_LEFT, INNER_TOP);
}

// Scrolls history up by one sweep and adds the newest sweep at the bottom.
// The history is recorded every sweep, but only pushed to the screen when
// the waterfall view is actually showing (to avoid wasted work in bar mode).
void waterfallStep()
{
    if (!wfReady) return;

    uint16_t* data = (uint16_t*)wfSprite.getPointer();

    memmove(data,
            data + WF_ROW_PX * WF_W,
            (WF_H - WF_ROW_PX) * WF_W * sizeof(uint16_t));

    for (int ch = 0; ch < WF_W; ch++)
    {
        uint16_t c = (activity[ch] > 0.0f) ? activityColor(activity[ch]) : TFT_WHITE;

        for (int r = 0; r < WF_ROW_PX; r++)
        {
            data[(WF_H - WF_ROW_PX + r) * WF_W + ch] = c;
        }
    }

    if (viewMode == VIEW_WATERFALL) waterfallPush();
}

// Full waterfall screen: frame plus the history sprite.
void drawWaterfallScreen()
{
    drawFrameOnly();
    waterfallPush();
}