SUMMARY = "Media pipeline fix tool"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI += "file://fixup-media-pipeline.cpp;subdir=${S}"

do_compile() {
	${CXX} ${CXXFLAGS} ${LDFLAGS} -std=c++17 -o ${B}/fixup_media_pipeline ${S}/fixup-media-pipeline.cpp
}

do_install() {
	install -d ${D}${bindir}
	install -m 755 ${B}/fixup_media_pipeline ${D}${bindir}/fixup_media_pipeline
}

FILES:${PN} += "${bindir}/fixup_media_pipeline"
