# TimeMachine
Tool for installing and loading entire PSP firmware and CFW on Memory Stick.

## PSP installer

Build with the PSP toolchain and psp-cfw-sdk installed: `make`. Copy
`dist/PSP/GAME/TMINSTALLER` to the Memory Stick. Put the matching
`FLASH661.PRO` or `FLASH661.LME` package in that directory or on the root
of `ms0:` or `ef0:`. Obtain the official `661.PBP` (or `661GO.PBP` for PSP
Go) separately and put it in one of the same locations. `TM.PKG` is supplied
with the installer.

The installer also requires `intraFont-vlf.prx`, `vlf.prx`,
`libpsardumper.prx`, and `pspdecrypt.prx` in the installer directory or
`ms0:/PSP/LIBS` or `ef0:/PSP/LIBS`. These are available from the PSP dynamic
libraries distribution.

Select PRO-D or LME-3, advance to 6.61, and confirm installation. Firmware,
the selected CFW, and the matching Time Machine modules are written to
`ms0:/TM/661PRO` or `ms0:/TM/661LME`; existing files there are overwritten.
The current flash1 registry is copied for the emulated firmware. This installer
does not install a Memory Stick IPL or configure a boot selector.

## TN-V2 on Vita

Place `tmvita.prx` in `ms0:/TM` and the TN-V2 `reboot.prx`, `tmhlpr.prx`,
`satelite.prx`, and separately built `FLASH0.TN` in `ms0:/TM/V661TN`.
Both Time Machine modules are added to the flash archive in memory.
