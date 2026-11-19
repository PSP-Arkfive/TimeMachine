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

## Vita installer

Run the installer under ARK-5 or Epinephrine and select "Install Time Machine
for PS Vita". No firmware updater is needed. The installer writes
`tmvita.prx` to `ms0:/TM` and all three `reboot.prx` files to
`ms0:/TM/V661ARK`, `ms0:/TM/V661EPI`, and `ms0:/TM/V661TN`. Existing files
are overwritten.

The installation requires `FLASH0.TN` in the installer directory or at
`ms0:/FLASH0.TN`. The installer copies it and `tmhlpr.prx` to
`ms0:/TM/V661TN`; the flash archive is not extracted during installation.
Both Time Machine modules are added to the archive in memory when booting.
ARK-5 additionally requires its existing
`ms0:/PSP/SAVEDATA/ARK_01234/FLASH0.ARK` at boot.
