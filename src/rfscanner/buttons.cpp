// ---------------------------------------------------------------------------
// buttons.cpp
//
// Non-blocking button handling for the two scanner buttons.
//
// Depends on: scan_state.h (Button struct, DEBOUNCE_MS, LONG_PRESS_MS).
// Depended on by: RFScanner.cpp (calls pollButton and resetButton).
// ---------------------------------------------------------------------------

#include "scan_state.h"

// Debounces a raw GPIO read and turns it into short-press and long-press
// events. The caller calls this once per loop iteration (or once per
// channel during a sweep) and then reads b.pendingShorts / b.pendingLong.
void pollButton(Button& b)
{
    uint32_t now = millis();
    bool raw = (digitalRead(b.pin) == LOW);

    // Track when the raw signal last changed, so we can ignore contact bounce.
    if (raw != b.raw)
    {
        b.raw = raw;
        b.changeTime = now;
    }

    // Once the signal has been stable for DEBOUNCE_MS, accept it as the
    // new stable state.
    if (raw != b.stable && (now - b.changeTime) >= DEBOUNCE_MS)
    {
        b.stable = raw;

        if (b.stable)
        {
            // Press just started: remember when, and reset the long-press flag.
            b.pressStart = b.changeTime;
            b.longFired  = false;
        }
        else if (!b.longFired && (b.changeTime - b.pressStart) < LONG_PRESS_MS)
        {
            // Release happened before LONG_PRESS_MS: count it as a short press.
            b.pendingShorts++;
        }
    }

    // While still held, fire the long-press event exactly once.
    if (b.stable && !b.longFired && (now - b.pressStart) >= LONG_PRESS_MS)
    {
        b.longFired   = true;
        b.pendingLong = true;
    }
}

// Resets a button's state at scanner startup, so a press that was already
// held when the scanner started does not get treated as a new event.
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