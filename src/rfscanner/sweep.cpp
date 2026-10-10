// ---------------------------------------------------------------------------
// sweep.cpp
//
// One full sweep of all 126 nRF24 channels. For each channel it samples the
// RPD (Received Power Detector) many times and records the fraction of
// samples that detected a signal. The results feed the bar/waterfall
// display and the peak-hold array.
//
// Depends on: scan_state.h (globals, timing constants).
// Depended on by: RFScanner.cpp (loop).
// ---------------------------------------------------------------------------

#include "scan_state.h"

// Sweeps all channels, updating activity[], peak[], the bar display, and
// the waterfall history. Polls both buttons between channels so input
// stays responsive even though a sweep takes ~20 ms.
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

    // Peak hold update: peaks decay slowly, but a new higher reading
    // replaces the decayed value immediately.
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