# TimeMachine
Tool for installing and loading entire PSP firmware and CFW on Memory Stick.

- You need the latest ARK-5 installed.
- To reboot into the installed Custom Firmware you can find a new entry for Time Machine in the Extras (or Games) section of the XMB.

## PSP

- You can install PRO-D or LME-3 on 6.20, 6.39 or 6.61, and/or 1.50 ARK.
  6.20 supports models through PSP Go; 6.39 supports models through 09g.
- Place the matching official updater at the root of `ms0:`: `620.PBP`,
  `639.PBP` or `661.PBP`. On PSP Go use its matching updater named
  `620GO.PBP`, `639GO.PBP` or `661GO.PBP` (the name without `GO` also works).
  For 1.50 ARK use `150.PBP`.
- Each installation is stored separately under `ms0:/TM/`, for example
  `ms0:/TM/620PRO` or `ms0:/TM/639LME`.
- The installer includes `usbdevice.prx` and installs it for PRO-D and LME-3.
  Each firmware's destination is configured in `Installer/menu_items.h`, relative
  to its Time Machine firmware directory; use `NULL` when it is not needed.

## Vita

- Supports `Adrenaline` only. Latest version is needed for Time Machine support in Epinephrine.
- It installs files needed to boot into ARK-5, Epinephrine or the older TN-V2 CEF from either ARK-5 or Epinephrine.
