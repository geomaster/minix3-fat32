# Makefile for FAT32 service by David Davidovic
PROG=	fat32
SRCS=	main.c requests.c mini-printf.c fat32.c

DPADD+=	${LIBSYS}
LDADD+=	-lsys

CFLAGS+=-D_SYSTEM -Wall

BOOT_FILE = /boot/minix_latest/mod11_fat32.gz

hdboot: $(PROG)
	if [ -d staging ]; then echo Removing staging directory...; rm -rf staging; fi
	echo Creating empty staging directory...
	mkdir -p staging
	echo Copying binary to staging...
	cp $(PROG) staging/$(PROG)
	echo Stripping symbols from the binary...
	strip --strip-all staging/$(PROG)
	echo Gzipping binary...
	gzip staging/$(PROG)
	echo Copying binary from staging to boot directory...
	cp -f staging/$(PROG).gz $(BOOT_FILE)
	echo Done. Reboot to run the new server.

.include <minix.service.mk>
