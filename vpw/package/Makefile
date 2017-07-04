
install:
	if [ ! -d "/usr/local/etc/vpw" ]; then mkdir "/usr/local/etc/vpw"; fi
	if [ ! -d "/usr/local/bin" ]; then mkdir -p "/usr/local/bin"; fi
	if [ ! -d "/usr/local/lib" ]; then mkdir -p "/usr/local/lib"; fi
	if [ ! -d "/usr/local/etc/software" ]; then mkdir -p "/usr/local/etc/software"; fi
#
	cp -f etc/SpkSRE.cfg      /usr/local/etc/vpw/
	cp -f etc/vrs.yaml  	  /usr/local/etc/
	cp -f etc/vpw.yaml   	  /usr/local/etc/vpw/
	cp -f etc/pf_ring.ko      /usr/local/etc/vpw/
#
	cp -f bin/vpw /usr/local/bin/
	chmod +x /usr/local/bin/vpw
	cp -f vpw.yaml  /usr/local/etc/software/vpw.yaml
#
	cp -rf SysFile/  /
	cp -f lib/*  /usr/local/lib/
#	ln -s /usr/local/lib/*.so   /usr/lib
#	sudo ldconfig
update:
	cp -f bin/vpw /usr/local/bin/
	chmod +x /usr/local/bin/vpw
	cp -rf SysFile/  /
	cp -f lib/*  /usr/local/lib/
	cp -f vpw.yaml /usr/local/etc/software/vpw.yaml
#	ln -s /usr/local/lib/*.so   /usr/lib
#	sudo ldconfig

uninstall:
	rm -f  /usr/local/bin/vpw
	rm -rf /usr/local/etc/vpw/SpkSRE.cfg
	rm -rf /usr/local/etc/vpw/vpw.yaml
	rm -rf /usr/local/etc/vpw/pf_ring.ko
	rm -rf /usr/local/etc/vrs.yaml
	rm -f  /usr/local/etc/software/vpw.yaml
	rm -rf /SysFile

