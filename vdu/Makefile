

VER = $(shell cat package/vdu-info.yaml | grep " version:" | cut -b 12-15)
BUILD   = $(shell svn info | grep 'Last Changed Rev:' | cut -b 19-24)
BUILDSHA = $(shell git rev-parse --short HEAD)

BSTR = $(shell printf %05d $(BUILD))

ALL: build

build:
	make -C src

clean:
	make -C src clean
	rm -rf package/bin
	rm -rf package/lib
	rm -f package/*.tar.gz

tarball: src/vdu
	if [ ! -d "package/bin" ]; then mkdir -p "package/bin"; fi
	if [ ! -d "package/lib" ]; then mkdir -p "package/lib"; fi
	if [ ! -d "package/etc" ]; then mkdir -p "package/etc"; fi
	cp src/vdu package/bin/
	cp etc/* package/etc/
	awk '($$2== "BUILDSTR") gsub("BUILDSTR","$(BSTR)")' package/vdu-info.yaml > package/vdu.yaml
	cd package && tar cpfz vdu-$(VER).$(BSTR).tar.gz bin etc lib  Makefile vdu.yaml


