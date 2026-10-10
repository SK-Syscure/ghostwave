// ---------------------------------------------------------------------------
// RFScanner.h
//
// Public API for the RF spectrum scanner module.
//
// Depends on: nothing outside the standard libraries.
// Depended on by: main.cpp (calls rfScannerSetup and rfScannerLoop).
// ---------------------------------------------------------------------------

#ifndef RFSCANNER_RF_H
#define RFSCANNER_RF_H

// Initializes the nRF24 radio, the TFT display, the peak-hold arrays, and
// the bar view. Blocks briefly for an intro animation.
void rfScannerSetup();

// Runs one iteration of the scanner: a sweep, button polling, and any
// pending view/state changes. Returns true when the user long-pressed to
// exit back to the menu, false otherwise.
bool rfScannerLoop();

#endif