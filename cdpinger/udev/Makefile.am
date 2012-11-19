EXTRA_DIST = ltspfsd.rules

UDEV_RULES_PATH = /usr/share/ltspfs/udev/

install-data-local:
	$(mkdir_p) $(DESTDIR)$(UDEV_RULES_PATH)
	$(INSTALL_DATA) $(srcdir)/ltspfsd.rules $(DESTDIR)$(UDEV_RULES_PATH)

uninstall-local:
	rm -f $(DESTDIR)$(UDEV_RULES_PATH)/ltspfsd.rules
