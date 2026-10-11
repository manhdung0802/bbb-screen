SUMMARY = "ninvaders application"
DESCRIPTION = "ninvaders application"
HOMEPAGE = "https://ninvaders.sourceforge.net/"
SECTION = "ninvaders"
LICENSE = "GPL-2.0-only"

SRC_URI = "${SOURCEFORGE_MIRROR}/ninvaders/ninvaders-${PV}.tar.gz"

S = "${WORKDIR}/ninvaders-${PV}"
SRC_URI[sha256sum] = "bfbc5c378704d9cf5e7fed288dac88859149bee5ed0850175759d310b61fd30b"
LIC_FILES_CHKSUM = "file://gpl.txt;md5=393a5ca445f6965873eca0259a17f833"

DEPENDS = "ncurses"

# Cấu hình cho cross compile 
CFLAGS += "-fcommon"
EXTRA_OEMAKE = " 'CC=${CC}' 'AR=${AR}' 'CFLAGS=${CFLAGS}' "

do_install() {
    install -d ${D}${bindir}
    install -m 0755 nInvaders ${D}${bindir}/ninvaders
}