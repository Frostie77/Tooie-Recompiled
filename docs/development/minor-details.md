# Minor details and marker follow-up

Frostie will use longer Debug playtests to record observations for later work.
Most are expected to be minor details; classify them from the uploaded evidence
and descriptions rather than assuming their severity from a marker number.

## Current deferred details

| ID | Observation | Status | Evidence |
|---|---|---|---|
| MD-001 | Minor visual lines during the Jiggy challenge | Deferred beyond the first release | [24 October candidate check](release-candidate-check-20261024.md), original marker 1 |
| MD-002 | Brief screen shift when dialogue animation begins | Deferred beyond the first release | [24 October candidate check](release-candidate-check-20261024.md), original marker 2 |

## Required review when evidence is uploaded

1. Read every event and follow-up note in `User-Follow-Up`, `User-Markers`, the
   marker summary and the durable input journal, together with Frostie's
   accompanying details. Include raw input that was not parsed as a marker.
2. Record each distinct observation with its source ZIP/session, event sequence,
   marker number, event ID and timestamp. Marker numbers 1–30 can be reused;
   a later occurrence is not automatically the same observation.
3. Preserve every source reference when grouping confirmed duplicate reports.
   Link `note NUMBER` details to their recorded target event ID, which identifies
   the latest occurrence at the time the note was entered.
4. Add confirmed minor issues to this backlog and retain other observations in
   the session review or appropriate issue record. Crashes and potential data
   loss get their own priority while retaining the marker reference here.
5. Keep items Open or Deferred until there is a tested resolution or an explicit
   decision from Frostie. A marker, screenshot or missing error line is not a fix.
6. Reconcile the reviewed event/note counts against `User-Follow-Up` and retain
   unreadable or ambiguous entries as follow-ups. Do not silently drop them.

The installed-build collector records every accepted marker and its notes in
the compact Share ZIP. The first 30 fresh events have bounded foreground
screenshot attempts; subsequent events still retain their full notes and
timestamps. Complete raw evidence stays on the PC, and any log excerpts in the
Share ZIP are named and inventoried. Receipt times can follow the visible issue;
screenshots do not establish motion or audio quality.

No entries for a future playtest have been invented. Populate the session
review and backlog after its actual evidence and details are supplied.

## Installed Debug playtest — 24 October 2026

The [session review](installed-debug-playtest-20261024-223111.md) accounts for
**all nine marker events and both additional unparsed observations** from session
`2c305aa4`. Marker 1 occurred twice; there is no marker 2. Grouping below is by
reported symptom for follow-up, not a finding that the events have one cause.
All entries remain open and their severity is provisional.

| ID | Observation | Status | Evidence and next step |
|---|---|---|---|
| MD-003 | Reported unusual frames; the first description is only “strange” | Open — detail needed | Events E01 and E02 (both marker 1), screenshots 001–002. Clarify the visible change and reproduce it before assigning a rendering cause. |
| MD-004 | Screen shaking | Open — reproduce | Events E03–E06 (markers 3–6), screenshots 003–006, plus journal observation J04 entered as `33 screen shaking`. Reproduce the camera/action and distinguish it from the earlier dialogue shift MD-002. Static screenshots cannot establish motion. |
| MD-005 | Unusual character rendering | Open — detail needed | Events E07–E09 (markers 7–9), screenshots 007–009, plus unnumbered journal observation J10. Identify the affected character/body part and action; retain each occurrence separately until the evidence supports a common cause. |

The session review retains each full event ID, sequence, receipt timestamp,
literal description and evidence filename. J04 and J10 were not accepted marker
commands and have no marker-triggered screenshots; their journal entries are
preserved for review. No observation was marked fixed, and this run does not
resolve startup compatibility on other devices.
