dist_sbin_SCRIPTS = ltspfs_mount ltspfsmounter ltspfs_umount 
EXTRA_DIST = ltspfs_entry $(ldm_DATA)

UDEV_LIB_PATH = @UDEV_LIB_PATH@

ldmdir = $(datadir)/ldm/rc.d
ldm_DATA = ldm/X10-delayed-mounter \
           ldm/X98-delayed-mounter

ltspdir = $(datadir)/ltsp/xinitrc.d
ltsp_DATA = xinitrc.d/I05-set-ltspfs_token

install-data-local:
	$(mkdir_p) $(DESTDIR)$(UDEV_LIB_PATH)
	$(INSTALL_DATA) -m 0755 -D $(srcdir)/ltspfs_entry $(DESTDIR)$(UDEV_LIB_PATH)
