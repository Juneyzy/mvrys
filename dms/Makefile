VER = $(shell cat package/vpw-info.yaml | grep " version:" | cut -b 12-15)
BUILD   = $(shell git log | grep -cE 'Author:')
BSTR = $(shell printf %03d $(BUILD))

SUBDIRS := ma mu sguard

ALL: build

build:
	for d in $(SUBDIRS); do \
	make -C $$d; \
	done

tarball: build
	if [ ! -d "package/bin" ]; then mkdir -p "package/bin"; fi
	if [ ! -d "package/lib" ]; then mkdir -p "package/lib"; fi
	if [ ! -d "package/etc" ]; then mkdir -p "package/etc"; fi
	if [ ! -d "release" ]; then mkdir -p "release"; fi
	cp vpw/vrs_work package/bin/vpw
	cp lib/*           package/lib/
	awk '($$2== "BUILDSTR") gsub("BUILDSTR","$(BSTR)")' package/vpw-info.yaml > package/vpw.yaml
	cd package && tar cpfz vpw-$(VER).$(BSTR).tar.gz bin etc lib SysFile  Makefile vpw.yaml
	cd ..
	mv package/vpw-$(VER).$(BSTR).tar.gz release/

clean:
	for d in $(SUBDIRS); do \
	make -C $$d clean; \
	done
	rm -f package/*.tar.gz release
