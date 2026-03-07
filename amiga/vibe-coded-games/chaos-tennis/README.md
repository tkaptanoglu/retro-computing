CHAOS TENNIS (Amiga 500)
========================

**CHAOS TENNIS** is a 2‑player paddle‑and‑ball game for the Amiga 500.
Both players use joysticks, one on each controller port. A stream of
“chaos objects” crosses the middle of the field, randomly changing the
ball’s speed, direction, or even teleporting it.

The match ends when one player reaches **5 points**. After each match
the game returns to the colorful **CHAOS TENNIS** intro screen.

This project contains:

- `src/chaos_tennis.c` – game code (AmigaOS, C)
- `s-startup-sequence` – simple `startup-sequence` that auto‑runs the game
- `build_adf.ps1` – helper script to compile the game and build a bootable ADF

## Requirements

- **Compiler**: `vbcc` with AmigaOS (m68k) target installed and `vc` on your `PATH`
  - Download from the official vbcc site and install the Amiga target.
- **ADF tooling**: `xdftool` from `fs-uae-tools` or a similar package
  - Ensure `xdftool` is available on your `PATH`.
- **Emulator / real hardware**:
  - Any Amiga 500–compatible emulator (e.g. WinUAE, FS‑UAE) or a real Amiga with a way to write ADFs.

## Building the game and ADF (Windows / PowerShell)

From this project directory (`chaos-tennis`):

```powershell
cd d:\Code\chaos-tennis
.\build_adf.ps1
```

The script will:

- Compile `src\chaos_tennis.c` into an Amiga executable named `CHAOSTENNIS`
- Create a bootable disk image `chaos_tennis.adf`
- Copy `CHAOSTENNIS` and a simple `s/startup-sequence` to the ADF

If everything succeeds, load `chaos_tennis.adf` in your Amiga emulator
as DF0: and **boot from it**. The intro screen should appear.

## Controls

- **Player 1**: joystick in port 1 (left side paddle)
- **Player 2**: joystick in port 2 (right side paddle)
- **Intro screen**:
  - Press **fire** on either joystick to start a match
- **In‑game**:
  - Move joysticks **up/down** to control paddles
  - Press **fire** to serve when needed
- **Quit**:
  - Press **Ctrl‑C** from the Amiga shell if run from CLI,
    or reset the emulator if booted from disk.

