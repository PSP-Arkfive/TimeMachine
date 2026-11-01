PY = $(shell which python3)
PSPDEV = $(shell psp-config --pspdev-path)
BUILDTOOLS = $(PSPDEV)/share/psp-cfw-sdk/build-tools

.PHONY : Core

all: Core

Core:
	$(MAKE) -C PRO/661/Rebootex
	$(MAKE) -C PRO/661/Reboot
	$(MAKE) -C PRO/661/TimeMachineControl
	$(MAKE) -C LME/661/Rebootex
	$(MAKE) -C LME/661/Reboot
	$(MAKE) -C LME/661/TimeMachineControl

clean:
	$(MAKE) -C PRO/661/Rebootex clean
	$(MAKE) -C PRO/661/Reboot clean
	$(MAKE) -C PRO/661/TimeMachineControl clean
	$(MAKE) -C LME/661/Rebootex clean
	$(MAKE) -C LME/661/Reboot clean
	$(MAKE) -C LME/661/TimeMachineControl clean
