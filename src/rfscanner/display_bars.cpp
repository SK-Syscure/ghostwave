// ---------------------------------------------------------------------------
// display_bars.cpp
//
// The bar-view drawing code: small helpers, per-column painting, and the
// full bar-screen repaint.
//
// Depends on: scan_state.h (globals, layout constants, colors).
// Depended on by: RFScanner.cpp (setup), display_common.cpp (eraseCursor).
// ---------------------------------------------------------------------------

#include "scan_state.h"

// Clamps an integer into an inclusive range.
int clampInt(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

// Converts an activity value (0.0-1.0) into a bar height in screen rows,
// scaled to the interior of the graph frame.
int heightRows(float a)
{
    return clampInt((int)(a * INNER_H + 0.5f), 0, INNER_H);
}

// Same idea as heightRows but peaks are capped one row lower so they do
// not touch the top frame border.
int peakRows(float p)
{
    return clampInt((int)(p * INNER_H + 0.5f), 0, INNER_H - 1);
}

// Paints the background for one column segment: a white vertical line with
// dashed gridlines at 25/50/75 percent. The dash pattern (2 px on, 2 px off)
// keeps the grid from looking solid.
void paintBackground(int x, int y0, int y1)
{
    if (y1 < y0) return;

    tft.drawFastVLine(x, y0, y1 - y0 + 1, TFT_WHITE);

    if (((x - GRAPH_LEFT) & 3) < 2)   // 2 on, 2 off dash pattern
    {
        if (GRID_ROW_25 >= y0 && GRID_ROW_25 <= y1) tft.drawPixel(x, GRID_ROW_25, GRID_COLOR);
        if (GRID_ROW_50 >= y0 && GRID_ROW_50 <= y1) tft.drawPixel(x, GRID_ROW_50, GRID_COLOR);
        if (GRID_ROW_75 >= y0 && GRID_ROW_75 <= y1) tft.drawPixel(x, GRID_ROW_75, GRID_COLOR);
    }
}

// Full repaint of one column: background, bar, and peak marker.
void paintColumn(int ch, int h, uint16_t col, int pk)
{
    int x = GRAPH_LEFT + ch;
    int barTop = INNER_BOTTOM - h + 1;

    paintBackground(x, INNER_TOP, barTop - 1);
    if (h > 0) tft.drawFastVLine(x, barTop, h, col);
    if (pk > h) tft.drawPixel(x, INNER_BOTTOM - pk, PEAK_COLOR);
}

// Partial update: only touches rows that changed, based on on-screen state.
// This keeps the per-sweep bar redraw cheap compared to a full repaint.
void updateColumn(int ch, int h, uint16_t col, int pk)
{
    int x = GRAPH_LEFT + ch;
    int oh = dispH[ch];
    uint16_t ocol = dispCol[ch];
    int opk = dispPk[ch];

    int newTop = INNER_BOTTOM - h + 1;
    int oldTop = INNER_BOTTOM - oh + 1;

    // 1. Remove the old peak marker (restore background there)
    if (opk > oh)
    {
        int r = INNER_BOTTOM - opk;
        paintBackground(x, r, r);
    }

    // 2. Shrink: clear rows the bar no longer covers
    if (h < oh) paintBackground(x, oldTop, newTop - 1);

    // 3. Bar: full redraw if color changed, otherwise only grown rows
    if (h > 0)
    {
        if (col != ocol)      tft.drawFastVLine(x, newTop, h, col);
        else if (h > oh)      tft.drawFastVLine(x, newTop, h - oh, col);
    }

    // 4. New peak marker
    if (pk > h) tft.drawPixel(x, INNER_BOTTOM - pk, PEAK_COLOR);
}

// Repaint all bar columns from stored values, and refresh the on-screen
// state so future partial updates work correctly.
void paintAllBars()
{
    for (int ch = 0; ch < NUM_CHANNELS; ch++)
    {
        int h  = heightRows(activity[ch]);
        int pk = peakRows(peak[ch]);
        uint16_t col = (h > 0) ? activityColor(activity[ch]) : TFT_WHITE;

        paintColumn(ch, h, col, pk);

        dispH[ch]   = h;
        dispPk[ch]  = pk;
        dispCol[ch] = col;
    }
}

// Full bar screen: frame, gridlines, bars.
void drawBarScreen()
{
    drawFrameOnly();

    for (int ch = 0; ch < NUM_CHANNELS; ch++)
    {
        paintBackground(GRAPH_LEFT + ch, INNER_TOP, INNER_BOTTOM);
    }

    paintAllBars();
}