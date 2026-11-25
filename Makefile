.PHONY: all clean

PY = $(shell which python3)
PSPDEV = $(shell psp-config --pspdev-path)
BUILDTOOLS = $(PSPDEV)/share/psp-cfw-sdk/build-tools
RESOURCE_DIR = tmp
LIBS_DIR = $(RESOURCE_DIR)/LIBS
TM_FLASH_DIR = $(RESOURCE_DIR)/TM
LIBS_ARCHIVE = $(RESOURCE_DIR)/LIBS.zip
LIBS_URL = https://github.com/pspdev/psp-dynamic-libraries/releases/download/latest/LIBS.zip
PRO_TM_URL = https://github.com/psp-arkfive/pro-d/releases/download/latest/pro-d.zip
LME_TM_URL = https://github.com/psp-arkfive/lme-3/releases/download/latest/lme-3.zip
TNV_TM_URL = https://github.com/psp-arkfive/tn-v2/releases/download/latest/tn-v2.zip
ARK150_DIR = $(RESOURCE_DIR)/ARK150
ARK150_ARCHIVE = $(RESOURCE_DIR)/ARK-150.zip
ARK150_URL = https://github.com/psp-arkfive/ark-150/releases/download/latest/ARK-150.zip

all: copy-dist

copy-dist: tmctrl tmvita cfw-psp cfw-vpsp installer

installer:
	mkdir $(RESOURCE_DIR) || true
	mkdir $(TM_FLASH_DIR) || true
	curl --fail --location --silent --show-error "$(LIBS_URL)" -o "$(LIBS_ARCHIVE)"
	curl --fail --location --silent --show-error "$(PRO_TM_URL)" -o "$(TM_FLASH_DIR)/PRO-D.zip"
	curl --fail --location --silent --show-error "$(LME_TM_URL)" -o "$(TM_FLASH_DIR)/LME-3.zip"
	curl --fail --location --silent --show-error "$(TNV_TM_URL)" -o "$(TM_FLASH_DIR)/TN-V2.zip"
	curl --fail --location --silent --show-error "$(ARK150_URL)" -o "$(ARK150_ARCHIVE)"
	unzip -o "$(LIBS_ARCHIVE)" -d "$(LIBS_DIR)"
	unzip -o "$(TM_FLASH_DIR)/PRO-D.zip" -d "$(TM_FLASH_DIR)"
	unzip -o "$(TM_FLASH_DIR)/LME-3.zip" -d "$(TM_FLASH_DIR)"
	unzip -o "$(TM_FLASH_DIR)/TN-V2.zip" -d "$(TM_FLASH_DIR)"
	unzip -o "$(ARK150_ARCHIVE)" FLASH150.ARK LANG150.ARK -d "$(ARK150_DIR)"
	$(MAKE) -C Type6Decrypt
	$(MAKE) -C Installer

tmctrl:
	$(MAKE) -C TimeMachineControl

tmvita:
	$(MAKE) -C TimeMachineControlVita

cfw-psp:
	$(MAKE) -C CFW/PSP/ARK/150/Reboot150
	$(MAKE) -C CFW/PSP/ARK/150/TimeMachine150
	$(MAKE) -C CFW/PSP/ARK/RebootDC/rebootex
	$(MAKE) -C CFW/PSP/ARK/RebootDC
	$(MAKE) -C CFW/PSP/ARK/TimeMachineHelper
	@set -e; for cfw in PRO LME; do \
		for fw in 620 639 661; do \
			dir="CFW/PSP/$$cfw/$$fw"; \
			$(MAKE) -C "$$dir/Rebootex"; \
			$(MAKE) -C "$$dir/Reboot"; \
			$(MAKE) -C "$$dir/TimeMachineHelper"; \
		done; \
	done

cfw-vpsp:
	$(MAKE) -C CFW/vPSP/ARK/rebootex
	$(MAKE) -C CFW/vPSP/ARK
	$(MAKE) -C CFW/vPSP/EPI/rebootex
	$(MAKE) -C CFW/vPSP/EPI
	$(MAKE) -C CFW/vPSP/TNV/Rebootex
	$(MAKE) -C CFW/vPSP/TNV/Reboot
	$(MAKE) -C CFW/vPSP/TNV/TimeMachineHelper

copy-dist:
	mkdir -p dist/TM
	mkdir -p dist/TM/DCARK/150
	mkdir -p dist/TM/V661EPI
	mkdir -p dist/TM/V661ARK
	mkdir -p dist/TM/V661TN
	mkdir -p dist/PSP/GAME/TimeMachine/
	cp TimeMachineControl/tmctrl.prx dist/TM/
	cp TimeMachineControlVita/tmvita.prx dist/TM/
	cp CFW/PSP/ARK/TimeMachineHelper/tmhlpr.prx dist/TM/DCARK/
	cp CFW/PSP/ARK/RebootDC/reboot.prx dist/TM/DCARK/
	cp CFW/PSP/ARK/150/Reboot150/reboot150.prx dist/TM/DCARK/150/
	cp CFW/PSP/ARK/150/TimeMachine150/tmctrl150.prx dist/TM/DCARK/150/
	@set -e; for cfw in PRO LME; do for fw in 620 639 661; do \
		mkdir -p "dist/TM/$$fw$$cfw"; \
		cp "CFW/PSP/$$cfw/$$fw/Reboot/reboot.prx" \
		   "CFW/PSP/$$cfw/$$fw/TimeMachineHelper/tmhlpr.prx" "dist/TM/$$fw$$cfw/"; \
	done; done
	cp CFW/vPSP/ARK/reboot.prx dist/TM/V661ARK/
	cp CFW/vPSP/EPI/reboot.prx dist/TM/V661EPI/
	cp CFW/vPSP/TNV/Reboot/reboot.prx dist/TM/V661TN/
	cp CFW/vPSP/TNV/TimeMachineHelper/tmhlpr.prx dist/TM/V661TN/
	$(PY) $(BUILDTOOLS)/pack/pack.py -p dist/TM.PKG package.txt -s
	cp Installer/EBOOT.PBP dist/TM.PKG dist/PSP/GAME/TimeMachine/
	cp Type6Decrypt/tmtype6decrypt.prx dist/PSP/GAME/TimeMachine/
	cp $(LIBS_DIR)/pspdecrypt.prx dist/PSP/GAME/TimeMachine/
	cp $(LIBS_DIR)/vlf.prx dist/PSP/GAME/TimeMachine/
	cp $(LIBS_DIR)/intraFont-vlf.prx dist/PSP/GAME/TimeMachine/
	cp $(LIBS_DIR)/libpsardumper.prx dist/PSP/GAME/TimeMachine/
	cp $(LIBS_DIR)/usbdevice.prx dist/PSP/GAME/TimeMachine/
	cp $(TM_FLASH_DIR)/FLASH620.PRO $(TM_FLASH_DIR)/FLASH639.PRO $(TM_FLASH_DIR)/FLASH661.PRO dist/PSP/GAME/TimeMachine/
	cp $(TM_FLASH_DIR)/FLASH620.LME $(TM_FLASH_DIR)/FLASH639.LME $(TM_FLASH_DIR)/FLASH661.LME dist/PSP/GAME/TimeMachine/
	cp $(TM_FLASH_DIR)/FLASH0.TN dist/PSP/GAME/TimeMachine/
	cp $(ARK150_DIR)/FLASH150.ARK $(ARK150_DIR)/LANG150.ARK dist/PSP/GAME/TimeMachine/

clean:
	$(MAKE) -C Type6Decrypt clean
	$(MAKE) -C Installer clean
	$(MAKE) -C TimeMachineControl clean
	$(MAKE) -C TimeMachineControlVita clean
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
	@set -e; for cfw in PRO LME; do for fw in 620 639; do \
		$(MAKE) -C "CFW/PSP/$$cfw/$$fw/Rebootex" clean; \
		$(MAKE) -C "CFW/PSP/$$cfw/$$fw/Reboot" clean; \
		$(MAKE) -C "CFW/PSP/$$cfw/$$fw/TimeMachineHelper" clean; \
	done; done
	$(MAKE) -C CFW/vPSP/ARK clean
	$(MAKE) -C CFW/vPSP/ARK/rebootex clean
	$(MAKE) -C CFW/vPSP/EPI clean
	$(MAKE) -C CFW/vPSP/EPI/rebootex clean
	$(MAKE) -C CFW/vPSP/TNV/Rebootex clean
	$(MAKE) -C CFW/vPSP/TNV/Reboot clean
	$(MAKE) -C CFW/vPSP/TNV/TimeMachineHelper clean
	rm -rf dist
	rm -rf $(RESOURCE_DIR)
