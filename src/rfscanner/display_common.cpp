// ---------------------------------------------------------------------------
// display_common.cpp
//
// Drawing helpers shared by the bar and waterfall views: the frame, the
// mode label, the Wi-Fi channel markers, the status text, the cursor, and
// the bottom readout strip.
//
// Depends on: scan_state.h (globals, layout constants, colors).
// Depended on by: RFScanner.cpp (setup/loop), display_bars.cpp,
//                  display_waterfall.cpp, hold.cpp.
// ---------------------------------------------------------------------------

#include "scan_state.h"

// Width of a string in pixels using the built-in 6x8 font at text size 1.
// Each glyph is 6 px wide at that size, so width = character count * 6.
int glyphsWidth(const char* s)
{
    return (int)strlen(s) * 6;
}

// Picks an RGB565 color for a bar based on its activity level.
// 0.0-0.5 goes cyan to green, 0.5-1.0 goes green to red.
uint16_t activityColor(float activity)
{
    uint8_t r;
    uint8_t g;
    uint8_t b;

    if (activity < 0.5)
    {
        // Cyan -> Green
        float t = activity / 0.5;

        r = 0;
        g = 255;
        b = 255 * (1 - t);
    }
    else
    {
        // Green -> Red
        float t = (activity - 0.5) / 0.5;

        r = 255 * t;
        g = 255 * (1 - t);
        b = 0;
    }

    return tft.color565(r, g, b);
}

// Draws the "BARS" or "WATERFALL" label in the top-left corner.
void drawModeLabel()
{
    tft.fillRect(TITLE_X - 1, STATUS_Y - 1, 80, 9, TFT_WHITE);
    tft.setTextSize(1);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.setCursor(TITLE_X, TITLE_Y);
    tft.print(viewMode == VIEW_BARS ? "BARS" : "WATERFALL");
}

// Draws brackets and labels under the graph for the three Wi-Fi channels
// (1, 6, 11) so the user can see which nRF channels overlap Wi-Fi.
void drawWifiMarkers()
{
    for (int i = 0; i < 3; i++)
    {
        int c  = GRAPH_LEFT + WIFI_CENTERS[i];
        int x1 = clampInt(c - WIFI_HALF_W, GRAPH_LEFT, GRAPH_LEFT + NUM_CHANNELS - 1);
        int x2 = clampInt(c + WIFI_HALF_W, GRAPH_LEFT, GRAPH_LEFT + NUM_CHANNELS - 1);

        tft.drawFastHLine(x1, WIFI_LINE_Y, x2 - x1 + 1, TFT_BLACK);
        tft.drawFastVLine(x1, WIFI_LINE_Y, 2, TFT_BLACK);
        tft.drawFastVLine(x2, WIFI_LINE_Y, 2, TFT_BLACK);

        tft.setTextColor(TFT_BLACK, TFT_WHITE);
        tft.setCursor(c - glyphsWidth(WIFI_LABELS[i]) / 2, WIFI_TEXT_Y);
        tft.print(WIFI_LABELS[i]);
    }
}

// Frame, mode label and Wi-Fi markers. No gridlines: the bar view paints
// its own dashed gridlines per column so partial updates stay cheap.
void drawFrameOnly()
{
    tft.fillScreen(TFT_WHITE);
    drawModeLabel();
    tft.drawRect(FRAME_LEFT, GRAPH_TOP, 128, GRAPH_BOTTOM - GRAPH_TOP + 1, TFT_BLACK);
    drawWifiMarkers();
}

// Prints the RUN/HOLD status and sweep rate in the top-right corner.
// Only redraws when the text actually changes, to avoid flicker.
void updateStatus()
{
    char buf[24];
    snprintf(buf, sizeof(buf), "%s %.1f/s",
             scanState == RUN_STATE ? "RUN" : "HOLD", sweepRate);

    if (strcmp(buf, prevStatus) == 0) return;
    strcpy(prevStatus, buf);

    int x = STATUS_RIGHT - glyphsWidth(buf);
    tft.fillRect(90, STATUS_Y - 1, STATUS_RIGHT - 90 + 1, 9, TFT_WHITE);
    tft.setTextSize(1);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.setCursor(x, STATUS_Y);
    tft.print(buf);
}

// Draws the cursor: a dashed vertical line through the selected channel
// plus a small triangle above the frame pointing at it.
void drawCursor(int ch)
{
    int x = GRAPH_LEFT + ch;

    for (int y = INNER_TOP; y <= INNER_BOTTOM; y += 3)
    {
        tft.drawPixel(x, y, CURSOR_COLOR);
    }

    tft.fillTriangle(x - 3, CURSOR_TRI_BASE, x + 3, CURSOR_TRI_BASE,
                     x, CURSOR_TRI_TOP, TFT_BLACK);
}

// Erases the cursor. In waterfall mode the history sprite is re-pushed to
// restore the column; in bar mode the column is repainted from stored data.
void eraseCursor(int ch)
{
    int x = GRAPH_LEFT + ch;

    // Clear the triangle above the frame
    tft.fillRect(x - 4, CURSOR_TRI_TOP, 9, CURSOR_TRI_BASE - CURSOR_TRI_TOP + 1, TFT_WHITE);

    if (viewMode == VIEW_WATERFALL)
    {
        waterfallPush();   // restores the dashed line from the history
    }
    else
    {
        paintColumn(ch, dispH[ch], dispCol[ch], dispPk[ch]);
    }
}

// Draws the bottom readout strip: channel number, frequency, current
// activity percent, and peak-hold percent for the cursor channel.
void drawReadout()
{
    char buf[32];
    int pc = (int)(activity[cursorCh] * 100 + 0.5f);
    int pp = (int)(peak[cursorCh] * 100 + 0.5f);

    snprintf(buf, sizeof(buf), "CH%d %dMHz %d%% pk%d%%",
             cursorCh, 2400 + cursorCh, pc, pp);

    tft.fillRect(0, READOUT_STRIP_TOP, 160, 10, TFT_WHITE);
    tft.setTextSize(1);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.setCursor(GRAPH_LEFT, READOUT_Y);
    tft.print(buf);
}

// Clears the bottom readout strip back to white.
void clearReadout()
{
    tft.fillRect(0, READOUT_STRIP_TOP, 160, 10, TFT_WHITE);
}