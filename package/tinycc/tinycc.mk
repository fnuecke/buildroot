################################################################################
#
# tinycc
#
################################################################################

TINYCC_VERSION = e9f59c804d41da9ba56493d5882fb75340d23c8e
TINYCC_SITE = git://repo.or.cz/tinycc.git
TINYCC_SITE_METHOD = git
TINYCC_LICENSE = LGPL-2.1
TINYCC_LICENSE_FILES = COPYING

TINYCC_INSTALL_STAGING = YES

TINYCC_CFLAGS = $(TARGET_CFLAGS)

#ifeq ($(BR2_STATIC_LIBS),n)
#TINYCC_CFLAGS += -fPIC
#endif

TINYCC_COMMON_FLAGS = CONFIG_strip=no

TINYCC_CFLAGS += -DTCC_LIBTCC1='\\\"\\\"'
TINYCC_CFLAGS += -DTCC_LIBGCC='\\\"libgcc_s.so.1\\\"'

define TINYCC_CONFIGURE_CMDS
	(cd $(@D); \
		$(TARGET_CONFIGURE_OPTS) \
		./configure \
			--prefix="/usr" \
			--libdir="/usr/lib" \
			--cpu=riscv64 \
			--cc="$(TARGET_CC)" \
			--ar="$(TARGET_AR)" \
			--extra-cflags="$(TINYCC_CFLAGS)" \
			--extra-ldflags="$(TARGET_LDFLAGS)" \
			--elfinterp=/lib/ld-musl-riscv64.so.1 \
			--config-musl \
			--with-libgcc \
			--config-backtrace=no \
			--config-bcheck=no \
			--config-predefs=no \
	)
endef

define TINYCC_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) XCC=$(TARGET_CC) XAR=$(TARGET_AR) -C $(@D) $(TINYCC_COMMON_FLAGS)
endef

define TINYCC_INSTALL_STAGING_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) DESTDIR="$(STAGING_DIR)" -C $(@D) install $(TINYCC_COMMON_FLAGS)
endef

define TINYCC_INSTALL_TARGET_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) DESTDIR="$(TARGET_DIR)" -C $(@D) install $(TINYCC_COMMON_FLAGS)
	cp "$(STAGING_DIR)/lib/crt1.o" "$(TARGET_DIR)/usr/lib/crt1.o"
	cp "$(STAGING_DIR)/lib/crti.o" "$(TARGET_DIR)/usr/lib/crti.o"
	cp "$(STAGING_DIR)/lib/crtn.o" "$(TARGET_DIR)/usr/lib/crtn.o"
endef

$(eval $(generic-package))
