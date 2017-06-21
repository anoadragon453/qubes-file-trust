# Generic file handler for untrusted files
sudo ln -s $PWD/qvm-dvm-trust.desktop /usr/share/applications/qvm-dvm-trust.desktop

# Dolphin context menus
sudo ln -s $PWD/qvm-trust-file.desktop /usr/share/kde4/services/qvm-trust-file.desktop
sudo ln -s $PWD/qvm-trust-folder.desktop /usr/share/kde4/services/qvm-trust-folder.desktop

# Nautilus context menus
sudo ln -s $PWD/qvm_trust.py /usr/share/nautilus-python/extensions/qvm_trust.py

# Utilities
sudo ln -s $PWD/qvm-open-trust-based /usr/bin/qvm-open-trust-based
sudo ln -s $PWD/qvm-trust /usr/bin/qvm-trust

# Images
sudo ln -s $PWD/qubes-checkmark.png /usr/share/pixmaps/qubes-checkmark.png
sudo ln -s $PWD/qubes.png /usr/share/pixmaps/qubes.png

touch $HOME/untrusted-folder-types

# Required packages
sudo apt install -y gvfs-bin python3-pyxattr
sudo dnf install -y gvfs-bin python3-pyxattr
