# TimeMachine
Tool for installing and loading entire PSP firmware and CFW on Memory Stick.

- You need the latest ARK-5 installed.
- To reboot into the installed Custom Firmware you can find a new entry for Time Machine in the Extras (or Games) section of the XMB.

## PSP

- You can install `6.61 PRO-D`, `6.61 LME-3`, or `1.50 ARK-5`.
- PSP installations share `ms0:/TM/tmctrl.prx`; ARK-150 also uses its own
  `tmctrl150.prx`.
- Installations show progress; on PSP you can then return to the main menu or exit.
- `1.50 ARK-5` is available only on PSP-1000 (01g). Place the official
  `150.PBP` at the root of `ms0:`. The installer also needs `FLASH150.ARK`,
  `LANG150.ARK`, and `TM.PKG` in its directory; these are included in the
  Time Machine release. A 6.61 `loadexec_01g.prx` is required either in
  `ms0:/TM/DCARK/kd` or on the console's 6.61 flash0.
- Back up any existing `ms0:/TM/DCARK/150` installation before reinstalling:
  an interrupted or failed installation can leave its files incomplete.
- For 6.61, download the updater and name it `661.PBP` (or `661GO.PBP`) on
  the Memory Stick.

## Vita

- Supports `Adrenaline` only. Latest version is needed for Time Machine support in Epinephrine.
- It installs files needed to boot into ARK-5, Epinephrine or the older TN-V2 CEF from either ARK-5 or Epinephrine.
