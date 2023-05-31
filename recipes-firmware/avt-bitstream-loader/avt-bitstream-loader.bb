SUMMARY = "Alvium bitstream loading"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://avt-load-bitstream"

RDEPENDS:${PN} += "bash"

S = "${WORKDIR}"

do_install() {
    install -d ${D}${bindir}
    install -m 755 ${S}/avt-load-bitstream ${D}${bindir}/avt-load-bitstream
}

