// ---------------------------------------------------------------------------
// RFScanner.cpp
//
// Entry point for the RF scanner module. Owns every piece of shared state
// (the display, the radio, the button structs, the scan state, the bar and
// peak arrays, the waterfall sprite, and the rate counters) exactly once,
// and exposes the two functions main.cpp calls.
//
// Depends on: scan_state.h (all constants, structs, externs),
//             RFScanner.h (this file's own API).
// Depended on by: main.cpp (calls rfScannerSetup and rfScannerLoop).
// ---------------------------------------------------------------------------

#include "scan_state.h"
#include "RFScanner.h"

// ---------------- Global state definitions ----------------
// These match the extern declarations in scan_state.h. tft must be
// constructed before wfSprite, which holds a pointer to it.

TFT_eSPI tft = TFT_eSPI();

RF24 scannerRadio(CE_PIN, CS_PIN);

Button btnHold;      // GPIO32: HOLD / exit
Button btnView;      // GPIO33: switch screens

ScanState scanState = RUN_STATE;
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

    introAnimation(tft);   // intro: TODO, not yet split

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