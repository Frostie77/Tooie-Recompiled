    if (!append_command(rdram, kRenderBufferState, kEnableWord0,
                        kEnableWord1)) {
        note_append_failure(0U);
        return;
    }
    ++g_frame_enables;

    // Tooie's VI manager keeps its current frame divisor at 0x8007913C: 2 for
    // the usual 30 Hz, 3 for the 20 Hz some overlays select. Report the rate
    // the game is actually running at, which is what RT64 interpolates from.
    const uint32_t divisor = read_word(rdram, kViRetraceInterval);
    const uint32_t refresh_rate =
        (divisor > 0U && divisor <= 60U) ? 60U / divisor : 30U;
    if (!append_command(rdram, kRenderBufferState, kSetRefreshRateWord0,
                        refresh_rate)) {
        note_append_failure(1U);
    }
