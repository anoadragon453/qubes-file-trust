#!/usr/bin/env python3 -O
# -*- coding: utf-8 -*-
#
# The Qubes OS Project, http://www.qubes-os.org
#
# Copyright (C) 2017 Andrew Morgan <andrew@amorgan.xyz>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
#

import os, subprocess

from gi.repository import Nautilus, GObject

class FolderSetTrustItemExtension(GObject.GObject, Nautilus.MenuProvider):

    def get_file_items(self, window, files):
        '''
        Set trust level for a folder and its contents.

        Uses the nautilus-python api to provide a context menu within Nautilus
        which enables the user to select trust levels for a folder and its
        contents.
        '''

        '''
        If user selects multiple files and clicks menu item, by default if at
        least one folder is untrusted, we will mark all as untrusted. Otherwise,
        if they are all untrusted, set them to be trusted after click
        '''
        all_items_are_untrusted = True

        # Attaches context menu in Nautilus
        if not files:
            return

        for file_obj in files:
            # Local files only; not remote
            if file_obj.get_uri_scheme() != 'file':
                return

            file_path = file_obj.get_location().get_path()

            # Check if any of the items set as trusted
            proc = subprocess.Popen(['/usr/bin/qvm-file-trust', '-cq', file_path],
                    stdout=subprocess.PIPE)
            subprocess.Popen.wait(proc)

            if proc.returncode == 2:
                # This particular folder is already marked as trusted
                all_items_are_untrusted = False

        if all_items_are_untrusted:
            menu_item = \
                Nautilus.MenuItem(name='QubesMenuProvider::FolderSetTrust',
                                  label='Always open in DisposableVM',
                                  tip='',
                                  icon='qubes-checkmark')
        else:
            # If at least one folder is still trusted, set all of them as
            # untrusted on click
            menu_item = \
                Nautilus.MenuItem(name='QubesMenuProvider::FolderSetTrust',
                                  label='Always open in DisposableVM',
                                  tip='',
                                  icon='')

        menu_item.connect('activate', self.on_menu_item_clicked, files)
        return menu_item,

    def on_menu_item_clicked(self, menu, files):
        '''
        Called when user chooses files though Nautilus context menu.
        '''

        all_items_are_untrusted = True

        file_paths = []

        # Get absolute paths of all files
        for file_obj in files:
            # Check if folder still exists
            if file_obj.is_gone():
                return

            file_path = file_obj.get_location().get_path()

            file_paths.append(file_path)

            # Check if any of the items set as trusted
            proc = subprocess.Popen(['/usr/bin/qvm-file-trust', '-cq', file_path],
                    stdout=subprocess.PIPE)
            subprocess.Popen.wait(proc)

            if proc.returncode == 2:
                # This particular folder is already marked as trusted
                all_items_are_untrusted = False

        # Check if the folder are trusted
        if all_items_are_untrusted:
            # Mark all selected as trusted
            subprocess.Popen(['/usr/bin/qvm-file-trust', '-t'] + file_paths)
        else:
            # Mark all selected as untrusted
            subprocess.Popen(['/usr/bin/qvm-file-trust', '-u'] + file_paths)
