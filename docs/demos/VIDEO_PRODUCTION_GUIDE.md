# Video production guide

Resonant prepares the application for external screen recording; it is not
a video editor.

## Set up

```bash
resonant --run-demo demo-b-acoustic-focusing --presenter --window-size 1920x1080 \
         --demo-output ~/recordings/demo-b
```

- **Size**: `--window-size 1920x1080` (or 3840x2160 for 4K, with a matching
  display scale) gives the same frame every time. Record the window, not the
  desktop.
- **Layout**: automated runs start from the default layout and never save
  one; presenter mode leaves only the viewport and the presenter bar.
- **Pacing**: `--run-demo` runs unattended (narration pauses pass). For a
  narrated take, run from Demo Studio with **Wait for Next at narration
  pauses**: each pause holds until **Next** (Space / Page Down), so narration
  sets the pace, not a timer.
- **Camera**: camera steps set fixed yaw, pitch and framing; no free camera
  moves happen during a run.
- **Repeatability**: the same demonstration from a clean start calculates the
  same values; the run manifest records the profile and sequence versions,
  the application version, the project's SHA-256, the solver settings, every
  step's outcome, result and time, screenshots and notes.

## Narration

`demos/presenter_notes/<demo>.md` holds what to say per step and each
demonstration's seven-part story (problem, configuration, calculation,
meaning, inspection, value, assumptions). Say what is synthetic (demonstration
G) and what is approximate (each profile's limitations).

## After a take

Keep the run manifest with the recording: it states exactly what was shown
and with which versions. Re-capture the help's screenshots
(`--capture-screenshots`) when the interface changed, so help and video match.
