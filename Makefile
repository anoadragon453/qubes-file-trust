CC = g++
OPT = -pthread -Werror -std=c++11 -g

compile:
	$(CC) $(OPT) -o qubes-trust-daemon qubes-trust-daemon.cpp

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
	if [ -f "/etc/debian_version" ]; then \
		python3 setup.py install --root /$(DESTDIR) --install-layout=deb; \
	else \
		python3 setup.py install --root /$(DESTDIR); \
	fi

	# Images
	install -m 0644 images/qubes-checkmark.png $(DESTDIR)/usr/share/pixmaps/qubes-checkmark.png
	install -m 0644 images/qubes.png $(DESTDIR)/usr/share/pixmaps/qubes.png

	# Untrusted folders list
	mkdir -p $(HOME)/.config/qubes
	touch $(HOME)/.config/qubes/always-open-in-dispvm.list

	# Raise max inotify watch limit
	sysctl fs.inotify.max_user_watches=524288
	if ! grep -q "fs.inotify.max_user_watches" "/rw/config/rc.local"; then \
		echo "sysctl fs.inotify.max_user_watches=524288" >> /rw/config/rc.local; \
	fi
	chmod +x /rw/config/rc.local

clean:
	rm -f qubes-trust-daemon
