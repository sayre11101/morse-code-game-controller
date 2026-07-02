# Milestone 2

Implement this milestone in /app/main.c.

Decode a Morse message from the key-state timeline and print the decoded text. Use the measured DOWN and UP durations to determine symbol boundaries and convert the sequence of symbols into letters and words.
Important: milestone 2 still depends on robust milestone 1-style state detection. Decode from stable key states, not from raw acceleration spikes. In particular, treat only settled DOWN and settled UP durations as Morse timing; do not treat TRAVEL_DOWN or TRAVEL_UP as dots, dashes, or gaps.

Timing source for milestone 2 should match the sampled sensor stream: use sample-count-based elapsed time (derived from BW_RATE period and number of DATAX samples consumed), not host wall-clock uptime.

Acceleration scale in this milestone is moderate: travel pulses are around +/-220 raw counts, DOWN hold is around +90, and UP is near 0 with small noise. Choose fixed thresholds that fit this scale.

Output contract for this milestone is strict: write exactly one decoded-message line to stdout (the final decoded word), then exit. Do not print startup banners, status logs, register traces, or debug text such as "STARTING ADXL345 APPLICATION..." to stdout.

The Morse reference for symbols and timing is provided locally at /app/morse-code-sheet.pdf.

## Timing Constants

For this task, the key-press timings are fixed:

- **Dit (dot)**: DOWN for ~120 ms
- **Dah (dash)**: DOWN for ~360 ms
- **Intra-symbol gap** (space between dots/dashes within a letter): UP for ~120 ms
- **Inter-letter gap** (space between letters): UP for ~360 ms
- **Final UP hold** (end of message): UP for ~5000 ms

Use these thresholds to classify DOWN durations (< 240 ms → dot, ≥ 240 ms → dash) and UP durations (< 240 ms → continue letter, 240–3000 ms → new letter, ≥ 4500 ms → end of message).
## Implementation Guardrails

- Use a single state machine driven by transitions between settled DOWN and settled UP.
- Record one DOWN duration per press: when the state changes from DOWN to UP, classify that completed DOWN segment as dot or dash.
- Record one UP gap per release: while in UP, measure continuous UP duration from the most recent DOWN->UP transition.
- Finalize the current letter only when UP duration reaches inter-letter range (240-3000 ms).
- Finalize the full message when one continuous UP duration reaches ≥ 4500 ms.
- After detecting end of message: flush any pending symbol, emit the decoded text line, call `exit(0)`. Do not continue polling.
- Never use an infinite loop without an end condition that triggers output and process exit.
- Keep stdout clean: the decoded message line must be the only user-facing output line from your program during milestone 2.

Reference flow for each completed press/release cycle:
1. DOWN starts: remember `down_start`.
2. DOWN ends (transition to UP): `down_dur = now - down_start`, append `.` or `-`.
3. While UP continues: `up_dur = now - up_start`.
4. If `up_dur` in 240-3000 ms: end current letter and append decoded char.
5. If `up_dur` ≥ 4500 ms: end message, print decoded word, `exit(0)`.

Assume operator speed in this task is within 8 to 28 words per minute. Decoding must remain accurate across that range.

Your output must be the final decoded message as plain text in the exact expected order. Letter grouping and word grouping must be correct. Extra characters, missing characters, or incorrect spacing make the result incorrect.

This milestone evaluates message decoding accuracy, not just state detection. The produced text must match the expected message exactly.
