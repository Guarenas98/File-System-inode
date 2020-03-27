all: libdisk libfs main

libdisk:
	$(MAKE) -f make.libdisk
libfs:
	$(MAKE) -f make.libfs
main:
	$(MAKE) -f make.main

.PHONY: clean
clean:
	$(MAKE) -f make.main clean
	$(MAKE) -f make.libfs clean
	$(MAKE) -f make.libdisk clean
