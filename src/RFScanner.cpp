#include "RFScanner.h"
#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>
#include <TFT_eSPI.h>
#include <string.h>
#include <stdio.h>
#include "intro_scanner.h"

TFT_eSPI tft = TFT_eSPI();

#define CE_PIN 4
#define CS_PIN 25

RF24 scannerRadio(CE_PIN, CS_PIN);

#define BUTTON_PIN 32   // GPIO32: HOLD / exit

// ---------------- Scan / timing constants ----------------
const int      NUM_CHANNELS    = 126;
const int      SAMPLES         = 50;      // RPD samples per channel (tunable)
const int      SETTLE_US       = 170;     // settle time before each RPD sample
const float    PEAK_DECAY      = 0.02f;   // peak falls by this much per sweep
const float    LOCAL_PEAK_MIN  = 0.20f;   // minimum activity for a HOLD peak
const uint32_t DEBOUNCE_MS     = 30;
const uint32_t LONG_PRESS_MS   = 800;     // >= this = exit scanner
const uint32_t RATE_WINDOW_MS  = 1000;    // window for sweeps/s

// ---------------- Layout constants ----------------
const int FRAME_LEFT    = 15;
const int GRAPH_TOP     = 16;                         // frame top row
const int GRAPH_BOTTOM  = 106;                        // frame bottom row
const int GRAPH_LEFT    = 16;                         // x of channel 0
const int INNER_TOP     = GRAPH_TOP + 1;              // 17
const int INNER_BOTTOM  = GRAPH_BOTTOM - 1;           // 105
const int INNER_H       = INNER_BOTTOM - INNER_TOP + 1; // 89 rows

const int TITLE_X       = 4;
const int TITLE_Y       = 4;
const int STATUS_RIGHT  = 157;                        // right edge of RUN/HOLD text
const int STATUS_Y      = 4;

const int CURSOR_TRI_TOP  = 12;                       // apex row of cursor triangle
const int CURSOR_TRI_BASE = 15;                       // base row (above frame at 16)

const int WIFI_LINE_Y   = 108;                        // bracket line under frame
const int WIFI_TEXT_Y   = 110;                        // "1", "6", "11" labels
const int WIFI_HALF_W   = 10;                         // +/- channels per Wi-Fi band

const int READOUT_STRIP_TOP = 118;                    // bottom strip y=118..127
const int READOUT_Y         = 119;

// Gridlines at 25/50/75 % of the interior
const int GRID_ROW_25 = INNER_BOTTOM - (INNER_H * 25) / 100;
const int GRID_ROW_50 = INNER_BOTTOM - (INNER_H * 50) / 100;
const int GRID_ROW_75 = INNER_BOTTOM - (INNER_H * 75) / 100;

// Colors (RGB565)
const uint16_t GRID_COLOR   = 0xCE59;                 // light gray
const uint16_t PEAK_COLOR   = 0x4208;                 // dark gray
const uint16_t CURSOR_COLOR = TFT_BLACK;

const int          WIFI_CENTERS[3] = { 12, 37, 62 };  // nRF channels for Wi-Fi 1/6/11
const char* const  WIFI_LABELS[3]  = { "1", "6", "11" };

// Waterfall geometry
const int WF_W      = NUM_CHANNELS;                   // 126 columns, x = 16..141
const int WF_H      = INNER_H - 1;                    // 88 rows, y = 17..104
const int WF_ROW_PX = 2;                              // rows added per sweep

// ---------------- Button struct ----------------
struct Button {
    uint8_t  pin;
    bool     raw = false, stable = false, longFired = false;
    uint32_t changeTime = 0, pressStart = 0;
    int      pendingShorts = 0;
    bool     pendingLong = false;
};

Button btnHold;      // GPIO32: HOLD / exit
Button btnView;      // GPIO33: switch screens

// ---------------- State ----------------
enum ScanState { RUN_STATE, HOLD_STATE };
ScanState scanState = RUN_STATE;

enum ViewMode { VIEW_BARS, VIEW_WATERFALL };
ViewMode viewMode = VIEW_BARS;

float    activity[NUM_CHANNELS];   // last sweep, 0.0 - 1.0
float    peak[NUM_CHANNELS];       // peak hold values
int      dispH[NUM_CHANNELS];      // bar height (rows) on screen
int      dispPk[NUM_CHANNELS];     // peak marker height (rows) on screen
uint16_t dispCol[NUM_CHANNELS];    // bar color on screen

int      cursorCh         = 0;
char     prevStatus[24]   = "";
float    sweepRate        = 0.0f;
uint32_t rateWindowStart  = 0;
uint32_t sweepsInWindow   = 0;

TFT_eSprite wfSprite = TFT_eSprite(&tft);   // waterfall history, ~22 KB
bool wfReady = false;

// ---------------- Helpers ----------------
int clampInt(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

int heightRows(float a)
{
    return clampInt((int)(a * INNER_H + 0.5f), 0, INNER_H);
}

int peakRows(float p)
{
    return clampInt((int)(p * INNER_H + 0.5f), 0, INNER_H - 1);
}

// Built-in 6x8 font: 6 px per glyph at text size 1
int glyphsWidth(const char* s)
{
    return (int)strlen(s) * 6;
}

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

// ---------------- Drawing: frame and labels ----------------
void drawModeLabel()
{
    tft.fillRect(TITLE_X - 1, STATUS_Y - 1, 80, 9, TFT_WHITE);
    tft.setTextSize(1);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.setCursor(TITLE_X, TITLE_Y);
    tft.print(viewMode == VIEW_BARS ? "BARS" : "WATERFALL");
}

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

// Frame, mode label and Wi-Fi markers. No gridlines.
void drawFrameOnly()
{
    tft.fillScreen(TFT_WHITE);
    drawModeLabel();
    tft.drawRect(FRAME_LEFT, GRAPH_TOP, 128, GRAPH_BOTTOM - GRAPH_TOP + 1, TFT_BLACK);
    drawWifiMarkers();
}

// ---------------- Drawing: bar view ----------------
// Background for one column segment: white with dashed gridlines
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

// Full repaint of one column
void paintColumn(int ch, int h, uint16_t col, int pk)
{
    int x = GRAPH_LEFT + ch;
    int barTop = INNER_BOTTOM - h + 1;

    paintBackground(x, INNER_TOP, barTop - 1);
    if (h > 0) tft.drawFastVLine(x, barTop, h, col);
    if (pk > h) tft.drawPixel(x, INNER_BOTTOM - pk, PEAK_COLOR);
}

// Partial update: only touches rows that changed, based on on-screen state
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

// Repaint all bar columns from stored values
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

// Full bar screen: frame, gridlines, bars
void drawBarScreen()
{
    drawFrameOnly();

    for (int ch = 0; ch < NUM_CHANNELS; ch++)
    {
        paintBackground(GRAPH_LEFT + ch, INNER_TOP, INNER_BOTTOM);
    }

    paintAllBars();
}

// ---------------- Drawing: waterfall view ----------------
bool waterfallInit()
{
    wfSprite.setColorDepth(16);
    wfSprite.createSprite(WF_W, WF_H);
    wfReady = (wfSprite.getPointer() != nullptr);

    if (wfReady) wfSprite.fillSprite(TFT_WHITE);
    return wfReady;
}

void waterfallDestroy()
{
    if (wfReady)
    {
        wfSprite.deleteSprite();
        wfReady = false;
    }
}

// Push history to the screen
void waterfallPush()
{
    if (wfReady) wfSprite.pushSprite(GRAPH_LEFT, INNER_TOP);
}

// Scroll history up by one sweep and add the newest sweep at the bottom
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

// Full waterfall screen
void drawWaterfallScreen()
{
    drawFrameOnly();
    waterfallPush();
}

// ---------------- Status ----------------
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

// ---------------- Cursor and readout ----------------
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

void clearReadout()
{
    tft.fillRect(0, READOUT_STRIP_TOP, 160, 10, TFT_WHITE);
}

// ---------------- Peaks and HOLD navigation ----------------
bool isLocalPeak(int ch)
{
    if (activity[ch] < LOCAL_PEAK_MIN) return false;

    float left  = ch > 0 ? activity[ch - 1] : 0.0f;
    float right = ch < NUM_CHANNELS - 1 ? activity[ch + 1] : 0.0f;

    return activity[ch] > left && activity[ch] > right;
}

int nextPeakRight(int from)
{
    for (int c = from + 1; c < NUM_CHANNELS; c++)
    {
        if (isLocalPeak(c)) return c;
    }
    return -1;
}

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

void moveCursor(int ch)
{
    eraseCursor(cursorCh);
    cursorCh = ch;
    drawCursor(cursorCh);
    drawReadout();
}

void resumeRun()
{
    eraseCursor(cursorCh);
    clearReadout();
    scanState = RUN_STATE;
}

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

// ---------------- View switching ----------------
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

// ---------------- Button (non-blocking, debounced) ----------------
void pollButton(Button& b)
{
    uint32_t now = millis();
    bool raw = (digitalRead(b.pin) == LOW);

    if (raw != b.raw)
    {
        b.raw = raw;
        b.changeTime = now;
    }

    if (raw != b.stable && (now - b.changeTime) >= DEBOUNCE_MS)
    {
        b.stable = raw;

        if (b.stable)
        {
            b.pressStart = b.changeTime;
            b.longFired  = false;
        }
        else if (!b.longFired && (b.changeTime - b.pressStart) < LONG_PRESS_MS)
        {
            b.pendingShorts++;
        }
    }

    if (b.stable && !b.longFired && (now - b.pressStart) >= LONG_PRESS_MS)
    {
        b.longFired   = true;
        b.pendingLong = true;
    }
}

void resetButton(Button& b)
{
    b.raw        = (digitalRead(b.pin) == LOW);
    b.stable     = b.raw;
    b.changeTime = millis();
    b.pressStart = millis();
    b.longFired  = b.stable;   // ignore a press already held at entry
    b.pendingShorts = 0;
    b.pendingLong   = false;
}

// ---------------- Sweep ----------------
void runSweep()
{
    for (int ch = 0; ch < NUM_CHANNELS; ch++)
    {
        scannerRadio.setChannel(ch);
        pollButton(btnHold);
        pollButton(btnView);

        int detections = 0;

        for (int i = 0; i < SAMPLES; i++)
        {
            delayMicroseconds(SETTLE_US);

            if (scannerRadio.testRPD())
            {
                detections++;
            }
        }

        activity[ch] = (float)detections / SAMPLES;
    }

    // Peak hold update
    for (int ch = 0; ch < NUM_CHANNELS; ch++)
    {
        float decayed = peak[ch] - PEAK_DECAY;
        peak[ch] = activity[ch] > decayed ? activity[ch] : decayed;
    }

    // Bar view: redraw only changed columns
    if (viewMode == VIEW_BARS)
    {
        for (int ch = 0; ch < NUM_CHANNELS; ch++)
        {
            int h  = heightRows(activity[ch]);
            int pk = peakRows(peak[ch]);
            uint16_t col = (h > 0) ? activityColor(activity[ch]) : TFT_WHITE;

            if (h != dispH[ch] || pk != dispPk[ch] || col != dispCol[ch])
            {
                updateColumn(ch, h, col, pk);
                dispH[ch]   = h;
                dispPk[ch]  = pk;
                dispCol[ch] = col;
            }
        }
    }

    // Waterfall history is recorded every sweep, drawn only in waterfall view
    waterfallStep();

    // Sweeps per second
    sweepsInWindow++;
    uint32_t now = millis();
    uint32_t elapsed = now - rateWindowStart;

    if (elapsed >= RATE_WINDOW_MS)
    {
        sweepRate = sweepsInWindow * 1000.0f / elapsed;
        sweepsInWindow  = 0;
        rateWindowStart = now;
    }
}

// ---------------- Public API ----------------
void rfScannerSetup()
{
    Serial.begin(115200);

    btnHold.pin = BUTTON_PIN;   // GPIO32
    btnView.pin = 33;           // GPIO33

    pinMode(BUTTON_PIN, INPUT_PULLUP);   // GPIO32
    pinMode(btnView.pin, INPUT_PULLUP);  // GPIO33

    scannerRadio.powerDown();
    delay(10);

    // Initialize nRF24 first
    if (!scannerRadio.begin())
    {
        Serial.println("nRF24 initialization failed!");

        while (true)
        {
            delay(1000);
        }
    }

    scannerRadio.setDataRate(RF24_1MBPS);
    scannerRadio.setChannel(76);

    scannerRadio.setAutoAck(false);
    scannerRadio.disableCRC();

    scannerRadio.stopListening();
    scannerRadio.startListening();

    Serial.println("nRF24 initialized!");

    scannerRadio.printDetails();

    scannerRadio.startListening();

    // Initialize TFT after the radio
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_WHITE);

    introAnimation(tft);

    waterfallInit();

    for (int ch = 0; ch < NUM_CHANNELS; ch++)
    {
        activity[ch] = 0.0f;
        peak[ch]     = 0.0f;
        dispH[ch]    = 0;
        dispPk[ch]   = 0;
        dispCol[ch]  = TFT_WHITE;
    }

    viewMode        = VIEW_BARS;
    scanState       = RUN_STATE;
    cursorCh        = 0;
    sweepRate       = 0.0f;
    sweepsInWindow  = 0;
    rateWindowStart = millis();
    prevStatus[0]   = '\0';

    drawBarScreen();

    resetButton(btnHold);
    resetButton(btnView);
    updateStatus();
}

bool rfScannerLoop()
{
    if (scanState == RUN_STATE)
    {
        runSweep();          // polls both buttons between channels
    }
    else
    {
        pollButton(btnHold);
        pollButton(btnView);
        delay(5);
    }

    // Long press on the first button: exit the scanner
    if (btnHold.pendingLong)
    {
        btnHold.pendingLong   = false;
        btnHold.pendingShorts = 0;
        btnView.pendingShorts = 0;
        waterfallDestroy();
        return true;
    }

    // Second button: switch screens
    if (btnView.pendingShorts > 0)
    {
        btnView.pendingShorts = 0;
        switchView();
    }

    // First button: enter HOLD, move cursor, or resume
    if (btnHold.pendingShorts > 0)
    {
        btnHold.pendingShorts = 0;
        handleShort();
    }

    updateStatus();
    return false;
}