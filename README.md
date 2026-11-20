# TimeMachine
Tool for installing and loading entire PSP firmware and CFW on Memory Stick.

- You need the latest ARK-5 installed.
- To reboot into the installed Custom Firmware you can find a new entry for Time Machine in the Extras (or Games) section of the XMB.

## PSP

- You can install `6.61 PRO-D`, `6.61 LME-3`, and/or `1.50 ARK`.
- For 6.61, download the updater and name it `661.PBP` (or `661GO.PBP`) on
  the Memory Stick, or `150.PBP` for `1.50 ARK`.
- The installer includes `usbdevice.prx` and installs it for PRO-D and LME-3.
  Each firmware's destination is configured in `Installer/menu_items.h`, relative
  to its Time Machine firmware directory; use `NULL` when it is not needed.

## Vita

- Supports `Adrenaline` only. Latest version is needed for Time Machine support in Epinephrine.
- It installs files needed to boot into ARK-5, Epinephrine or the older TN-V2 CEF from either ARK-5 or Epinephrine.
