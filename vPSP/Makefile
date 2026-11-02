all:
	make -C ARK
	make -C Epinephrine
	mkdir -p dist/TM/
	mkdir dist/TM/V661EPI
	mkdir dist/TM/V661ARK
	cp ARK/reboot.prx dist/TM/V661ARK/
	cp Epinephrine/reboot.prx dist/TM/V661EPI/

clean:
	make -C ARK clean
	make -C Epinephrine clean
	rm -rf dist/
