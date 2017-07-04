
install:
	if [ ! -d "/usr/local/etc/vpm" ]; then mkdir "/usr/local/etc/vpm"; fi
	if [ ! -d "/usr/local/bin" ]; then mkdir -p "/usr/local/bin"; fi
	if [ ! -d "/usr/local/lib" ]; then mkdir -p "/usr/local/lib"; fi
	if [ ! -d "/usr/local/etc/software" ]; then mkdir -p "/usr/local/etc/software"; fi
#
	cp -f etc/vrs.yaml  /usr/local/etc/
#
	cp -f etc/SpkSRE.cfg  /usr/local/etc/vpm/
	cp -f etc/vpm.yaml  /usr/local/etc/vpm/
	cp -f vpm.yaml /usr/local/etc/software/vpm.yaml
#
	cp -f bin/vpm /usr/local/bin/
	chmod +x      /usr/local/bin/vpm
#
	cp -rf SysFile/  /
	cp -f lib/*  	 /usr/local/lib/
#	ln -s /usr/local/lib/*.so   /usr/lib
#	sudo ldconfig
update:
	cp -f bin/vpm /usr/local/bin/
	chmod +x  /usr/local/bin/vpm
	cp -rf SysFile/  /
	cp -f lib/* /usr/local/lib/
	cp -f vpm.yaml /usr/local/etc/software/vpm.yaml
#	ln -s /usr/local/lib/*.so   /usr/lib
#	sudo ldconfig

uninstall:
	rm -f  /usr/local/bin/vpm
	rm -rf /usr/local/etc/vrs.yaml
	rm -rf /usr/local/etc/vpm/SpkSRE.cfg
	rm -rf /usr/local/etc/vpm/vpm.yaml
	rm -f  /usr/local/etc/software/vpm.yaml
	rm -rf /SysFile

