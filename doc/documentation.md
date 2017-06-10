# Documentation

# Requirements

`attr` must be installed on the system, specifically the `getfattr` and `setfattr` binaries.

## File Manager Context Menus

The context menus are defined as a python script for Nautilus
(stored in `/usr/share/nautilus-python/extensions`) and a service file
for Dolphin (`/usr/share/kde4/services/`). Both call
`/usr/lib/qubes/qvm-set-trust.{gnome,kde}` which actually creates the dialog
asking the user which file trust settings should be set, as well as setting any
emblems on the file in question.

## File Manager Patches

A patch is needed for each file manager in order to allow extension execution
upon opening of a file. The extension should be able to determine whether or not
the file should be opened.

Bug report for Dolphin patch:

Bug report for Nautilus patch:

## File-watcher Daemon

A C daemon will be run in each AppVM that watches and enforces file trust
settings on any files created inside of folders marked with a specific trust
level. For example, if I mark a folder as `untrusted`, the daemon should
automatically mark all new and existing files and folders within as `untrusted`
as well.

## Unit tests

Unit tests are included in the unit-tests folder.

GUI-based unit tests are done with the
[Dogtail](https://gitlab.com/dogtail/dogtail) library.
