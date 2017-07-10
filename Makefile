CC = gcc
OPT = -Werror

compile:
	$(CC) $(OPT) -o qubes-trust-daemon qubes-trust-daemon.c

install:
	# Generic file handler for untrusted files
	install -m 0644 qvm-dvm-trust.desktop $(DESTDIR)/usr/share/applications/qvm-dvm-trust.desktop

	# Dolphin context menus
	install -m 0644 qvm-trust-file.desktop $(DESTDIR)/usr/share/kde4/services/qvm-trust-file.desktop
	install -m 0644 qvm-trust-folder.desktop $(DESTDIR)/usr/share/kde4/services/qvm-trust-folder.desktop

	# Nautilus context menus
	install -m 0644 qvm_trust.py $(DESTDIR)/usr/share/nautilus-python/extensions/qvm_trust.py

	# Utilities
	install -m 0755 qvm-open-trust-based $(DESTDIR)/usr/bin/qvm-open-trust-based
	install -m 0755 qvm-file-trust $(DESTDIR)/usr/bin/qvm-file-trust

	# Images
	install -m 0644 qubes-checkmark.png $(DESTDIR)/usr/share/pixmaps/qubes-checkmark.png
	install -m 0644 qubes.png $(DESTDIR)/usr/share/pixmaps/qubes.png

	# Untrusted folders list
	mkdir -p $(HOME)/.config/qubes
	touch $(HOME)/.config/qubes/always-open-in-dispvm.list

clean:
	rm -f qubes-trust-daemon
