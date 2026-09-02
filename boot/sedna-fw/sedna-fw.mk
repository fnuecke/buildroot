################################################################################
#
# sedna-fw
#
################################################################################

SEDNA_FW_VERSION = 1.0
SEDNA_FW_SITE = boot/sedna-fw/src
SEDNA_FW_SITE_METHOD = local
SEDNA_FW_LICENSE = MIT
SEDNA_FW_INSTALL_IMAGES = YES
SEDNA_FW_INSTALL_TARGET = NO

define SEDNA_FW_BUILD_CMDS
	$(MAKE) -C $(@D) CROSS_COMPILE=$(TARGET_CROSS)
endef

define SEDNA_FW_INSTALL_IMAGES_CMDS
	$(INSTALL) -m 0644 -D $(@D)/firmware.bin $(BINARIES_DIR)/firmware.bin
	$(INSTALL) -m 0644 -D $(@D)/firmware.properties $(BINARIES_DIR)/firmware.properties
endef

$(eval $(generic-package))
