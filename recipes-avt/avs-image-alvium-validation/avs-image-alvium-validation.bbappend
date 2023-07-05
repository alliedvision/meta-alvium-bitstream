CORE_IMAGE_EXTRA_INSTALL:append = " \
  fpga-manager-script \
  avt-bitstream-loader \
  "

CORE_IMAGE_EXTRA_INSTALL:append:zcu106 = " \
  alvium-fmc-zcu106-rgb \
  alvium-fmc-zcu106-raw8 \
  alvium-fmc-zcu106-yuv \
  "

CORE_IMAGE_EXTRA_INSTALL:append:kria = " \
  alvium-kria-kv260-yuv \
  alvium-kria-kv260-raw8 \
  alvium-kria-kv260-rgb \
  "