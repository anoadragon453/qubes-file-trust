[![Build Status](https://travis-ci.org/anoadragon453/qubes-file-trust.svg?branch=master)](https://travis-ci.org/anoadragon453/qubes-file-trust)
[![Coverage Status](https://coveralls.io/repos/github/anoadragon453/qubes-file-trust/badge.svg?branch=master)](https://coveralls.io/github/anoadragon453/qubes-file-trust?branch=master)

# Qubes File Trust

Adds ability to set trust level for files and folders. Untrusted files will be automatically opened in a disposableVM. 

GSoC 2017 project for https://qubes-os.org

Follow the progress [here](https://blog.amorgan.xyz/tag_gsoc-2017.html)!

## Related repositories

[anoadragon453/qubes-dolphin-trust-activities](https://github.com/anoadragon453/qubes-dolphin-trust-activities)

[anoadragon453/qubes-nautilus-trust-extension](https://github.com/anoadragon453/qubes-nautilus-trust-extension)

[anoadragon453/nautilus](https://github.com/anoadragon453/nautilus)

[anoadragon453/nautilus-python](https://github.com/anoadragon453/nautilus-python)

---

# Documentation

## Requirements

`python3-pyxattr` is required for manipulating extended attribute values

`gvfs-bin` is required for custom emblem support in Nautilus

`libffi-dev` for setuptools related install functions(?)

`g++` (`gcc-c++` on fedora) for compilation of qubes-trust-daemon

`pandoc` for building the documentation

## Install

Grab the requirements listed above then:

```
make build
sudo make install
```

## Unit tests

Unit tests are included in the tests folder.

Run them with:

```
make tests
```

GUI-based tests are done with the
[Dogtail](https://gitlab.com/dogtail/dogtail) library.

## File Manager Context Menus

The context menus are defined as a python script for Nautilus (stored in
`/usr/share/nautilus-python/extensions`) and a service file for Dolphin
(`/usr/share/kde4/services/`). Both call
`/usr/lib/qubes/qvm-set-trust.{gnome,kde}` which actually creates the dialog
asking the user which file trust settings should be set, as well as setting any
emblems on the file in question.

The icons for the context menus (png files) are stored in `/usr/share/pixmaps`.

Emblem support should work out of the box on Fedora VMs, and needs the
`gvfs-bin` package installed on Debian VMs.

## File Manager Patches

A patch is needed for each file manager in order to allow extension execution
upon opening of a file. The extension should be able to determine whether or not
the file should be opened.

## File-watcher Daemon

A C daemon will be run in each AppVM that watches and enforces file trust
settings on any files created inside of folders marked with a specific trust
level. For example, if I mark a folder as `untrusted`, the daemon should
automatically mark all new and existing files and folders within as `untrusted`
as well.
