all:
	make -C ARK
	make -C Epinephine
	mkdir -p dist/TM/
	mkdir dist/TM/V661EPI
	mkdir dist/TM/V661ARK
	cp ARK/reboot.prx dist/TM/V661ARK/
	cp Epinephine/reboot.prx dist/TM/V661EPI/

clean:
	make -C ARK clean
	make -C Epinephine clean
	rm -rf dist/
