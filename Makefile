.PHONY: all clean

PY = $(shell which python3)
PSPDEV = $(shell psp-config --pspdev-path)
BUILDTOOLS = $(PSPDEV)/share/psp-cfw-sdk/build-tools

all: tmctrl cfw-psp cfw-vpsp copy-dist

tmctrl:
	$(MAKE) -C TimeMachineControl

cfw-psp:
	$(MAKE) -C CFW/PSP/ARK/150/Reboot150
	$(MAKE) -C CFW/PSP/ARK/150/TimeMachine150
	$(MAKE) -C CFW/PSP/ARK/RebootDC/rebootex
	$(MAKE) -C CFW/PSP/ARK/RebootDC
	$(MAKE) -C CFW/PSP/ARK/TimeMachineHelper
	$(MAKE) -C CFW/PSP/PRO/661/Rebootex
	$(MAKE) -C CFW/PSP/PRO/661/Reboot
	$(MAKE) -C CFW/PSP/PRO/661/TimeMachineHelper
	$(MAKE) -C CFW/PSP/LME/661/Rebootex
	$(MAKE) -C CFW/PSP/LME/661/Reboot
	$(MAKE) -C CFW/PSP/LME/661/TimeMachineHelper

cfw-vpsp:
	$(MAKE) -C CFW/vPSP/ARK
	$(MAKE) -C CFW/vPSP/EPI

copy-dist:
	mkdir -p dist/TM
	mkdir -p dist/TM/DCARK/150
	mkdir -p dist/TM/661PRO
	mkdir -p dist/TM/661LME
	mkdir -p dist/TM/V661EPI
	mkdir -p dist/TM/V661ARK
	cp TimeMachineControl/tmctrl.prx dist/TM/
	cp CFW/PSP/ARK/TimeMachineHelper/tmhlpr.prx dist/TM/DCARK/
	cp CFW/PSP/ARK/RebootDC/reboot.prx dist/TM/DCARK/
	cp CFW/PSP/ARK/150/Reboot150/reboot150.prx dist/TM/DCARK/150/
	cp CFW/PSP/ARK/150/TimeMachine150/tmctrl150.prx dist/TM/DCARK/150/
	cp CFW/PSP/PRO/661/Reboot/reboot.prx dist/TM/661PRO/
	cp CFW/PSP/PRO/661/TimeMachineHelper/tmhlpr.prx dist/TM/661PRO/
	cp CFW/PSP/LME/661/Reboot/reboot.prx dist/TM/661LME/
	cp CFW/PSP/LME/661/TimeMachineHelper/tmhlpr.prx dist/TM/661LME/
	cp CFW/vPSP/ARK/reboot.prx dist/TM/V661ARK/
	cp CFW/vPSP/EPI/reboot.prx dist/TM/V661EPI/
	$(PY) $(BUILDTOOLS)/pack/pack.py -p dist/TM.PKG package.txt -s

clean:
	$(MAKE) -C TimeMachineControl clean
	$(MAKE) -C CFW/PSP/ARK/150/Reboot150 clean
	$(MAKE) -C CFW/PSP/ARK/150/TimeMachine150 clean
	$(MAKE) -C CFW/PSP/ARK/TimeMachineHelper clean
	$(MAKE) -C CFW/PSP/ARK/RebootDC/rebootex clean
	$(MAKE) -C CFW/PSP/ARK/RebootDC clean
	$(MAKE) -C CFW/PSP/PRO/661/Rebootex clean
	$(MAKE) -C CFW/PSP/PRO/661/Reboot clean
	$(MAKE) -C CFW/PSP/PRO/661/TimeMachineHelper clean
	$(MAKE) -C CFW/PSP/LME/661/Rebootex clean
	$(MAKE) -C CFW/PSP/LME/661/Reboot clean
	$(MAKE) -C CFW/PSP/LME/661/TimeMachineHelper clean
	$(MAKE) -C CFW/vPSP/ARK clean
	$(MAKE) -C CFW/vPSP/EPI clean
	rm -rf dist
