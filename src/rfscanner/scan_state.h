#ifndef RFSCANNER_SCAN_STATE_H
#define RFSCANNER_SCAN_STATE_H

#include <stdint.h>
#include <RF24.h>
#include <TFT_eSPI.h>

// ---------------- Scan / timing constants ----------------
const int      NUM_CHANNELS    = 126;
const int      SAMPLES         = 50;      // RPD samples per channel (tunable)
const int      SETTLE_US       = 170;     // settle time before each RPD sample
const float    PEAK_DECAY      = 0.02f;   // peak falls by this much per sweep
const float    LOCAL_PEAK_MIN  = 0.20f;   // minimum activity for a HOLD peak
const uint32_t DEBOUNCE_MS     = 30;
const uint32_t LONG_PRESS_MS   = 800;     // >= this = exit scanner
const uint32_t RATE_WINDOW_MS  = 1000;    // window for sweeps/s

// ---------------- Pins ----------------
#define CE_PIN 4
#define CS_PIN 25
#define BUTTON_PIN 32   // GPIO32: HOLD / exit

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

// ---------------- State ----------------
enum ScanState { RUN_STATE, HOLD_STATE };
enum ViewMode { VIEW_BARS, VIEW_WATERFALL };

// ---------------- Shared state ----------------
// These are defined exactly once in RFScanner.cpp.
// Everything else in the package reads them through these externs.
extern TFT_eSPI tft;
extern RF24 scannerRadio;
extern Button btnHold;
extern Button btnView;
extern ScanState scanState;
extern ViewMode viewMode;
extern float    activity[NUM_CHANNELS];
extern float    peak[NUM_CHANNELS];
extern int      dispH[NUM_CHANNELS];
extern int      dispPk[NUM_CHANNELS];
extern uint16_t dispCol[NUM_CHANNELS];
extern int      cursorCh;
extern char     prevStatus[24];
extern float    sweepRate;
extern uint32_t rateWindowStart;
extern uint32_t sweepsInWindow;
extern TFT_eSprite wfSprite;
extern bool wfReady;

// ---------------- Internal functions ----------------
// Defined in the .cpp files below; declared here so any file can call them.
int clampInt(int v, int lo, int hi);
int heightRows(float a);
int peakRows(float p);
int glyphsWidth(const char* s);
uint16_t activityColor(float activity);
void drawModeLabel();
void drawWifiMarkers();
void drawFrameOnly();
void paintBackground(int x, int y0, int y1);
void paintColumn(int ch, int h, uint16_t col, int pk);
void updateColumn(int ch, int h, uint16_t col, int pk);
void paintAllBars();
void drawBarScreen();
bool waterfallInit();
void waterfallDestroy();
void waterfallPush();
void waterfallStep();
void drawWaterfallScreen();
void updateStatus();
void drawCursor(int ch);
void eraseCursor(int ch);
void drawReadout();
void clearReadout();
bool isLocalPeak(int ch);
int nextPeakRight(int from);
void enterHold();
void moveCursor(int ch);
void resumeRun();
void handleShort();
void switchView();
void pollButton(Button& b);
void resetButton(Button& b);
void runSweep();
void introAnimation(TFT_eSPI& tft);

#endif