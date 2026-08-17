#!/bin/sh
set -e

TARGET_DIR="$1"

# ---------------------------------------------------------------------------
# Do not start network daemons by default. Can be started on demand.
#
# /etc/init.d/rcS and rcK both glob "S??*", so dropping the S<NN> prefix takes
# the script out of the boot sequence while leaving it in place and runnable:
#
#     /etc/init.d/dropbear start
#
# ---------------------------------------------------------------------------
for entry in S50dropbear:dropbear S50telnet:telnet S80dnsmasq:dnsmasq; do
	src="${entry%%:*}"
	dst="${entry##*:}"
	if [ -f "$TARGET_DIR/etc/init.d/$src" ]; then
		mv -f "$TARGET_DIR/etc/init.d/$src" "$TARGET_DIR/etc/init.d/$dst"
	fi
done

# ---------------------------------------------------------------------------
# Remove libraries that nothing in the image links against.
# ---------------------------------------------------------------------------
rm -f "$TARGET_DIR"/usr/lib/libform*.so* \
      "$TARGET_DIR"/usr/lib/libmenu*.so* \
      "$TARGET_DIR"/usr/lib/libpanel*.so* \
      "$TARGET_DIR"/lib/libatomic.so*

# ---------------------------------------------------------------------------
# luac is the offline bytecode compiler. Not needed for interpreter.
# ---------------------------------------------------------------------------
rm -f "$TARGET_DIR"/usr/bin/luac

# ---------------------------------------------------------------------------
# Drop a bunch of luaposix things we don't really need (can be done via e.g.
# os.execute instead), to save some more rootfs disk space.
# ---------------------------------------------------------------------------
for mod in ctype grp pwd sched syslog; do
	rm -f "$TARGET_DIR/usr/lib/lua/5.4/posix/$mod.so"
done
for mod in msg resource socket statvfs times; do
	rm -f "$TARGET_DIR/usr/lib/lua/5.4/posix/sys/$mod.so"
done
for f in _base _bitwise _strict compat deprecated sys util version; do
	rm -f "$TARGET_DIR/usr/share/lua/5.4/posix/$f.lua"
done
