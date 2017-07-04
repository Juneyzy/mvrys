VER = $(shell cat package/vpw-info.yaml | grep " version:" | cut -b 12-15)
BUILD   = $(shell svn info | grep 'Last Changed Rev:' | cut -b 19-24)
BSTR = $(shell printf %05d $(BUILD))

SUBDIRS := vpw tools vpm svm dms

ALL: build

libeth:
	- cd libethxx/src && make -k 
	cd libethxx/src && make tarball_vrs
	cp libethxx/src/obj-linux_64/libethxx.a lib

build:
	for d in $(SUBDIRS); do \
	make -C $$d; \
	done

tarball: build
	for d in $(SUBDIRS); do \
	make -C $$d tarball; \
	done
	
clean:
	for d in $(SUBDIRS); do \
	make -C $$d clean; \
	done
	rm -rf release
	cd libethxx/src && make clean
