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

import os, subprocess, gi
gi.require_version('Nautilus', '3.0')
from gi.repository import Nautilus, GObject, Gio

class QubesTrustInfo(GObject.GObject, Nautilus.InfoProvider):
    def file_open(self, provider, file):
        # Don't worry about folders
        if file.get_uri_scheme() != 'file':
            return True

        file_path = file.get_location().get_path()

        proc = subprocess.Popen(['/usr/bin/qvm-file-trust', '-cq', file_path],
                stdout=subprocess.PIPE)
        subprocess.Popen.wait(proc)

        if proc.returncode == 0:
            # File is trusted, open
            return True
        elif proc.returncode == 1:
            # File is untrusted, block and open dispVM

            # Open DispVM with this file
            subprocess.Popen(['/usr/bin/qvm-open-trust-based', file_path],
                stdout=subprocess.PIPE)

            return False
        
        # Else, there was an error with qvm-file-trust
        print("Error with qvm-file-trust: {}".format(proc.returncode))
        return True

# Used to check if we've already checked a list of files previously.
# If so, then there's no need to check it again
last_calculation = {'file_list': [], 'result': True}

class QubesTrustMenu(GObject.GObject, Nautilus.InfoProvider, Nautilus.MenuProvider):
    def __init__(self):
        pass

    def _refresh(self, item_path):
        """Reload the current file/directory icon"""
        os.utime(item_path, None)

    def set_emblem(self, item_path, emblem_name=''):
        """Set emblem"""
        # Restore
        self.restore_emblem(item_path)
        # Set
        if emblem_name:
            emblem = []
            emblem.append(emblem_name)
            emblems = list(emblem)
            emblems.append(None) # Needs
            item = Gio.File.new_for_path(item_path)
            info = item.query_info('metadata::emblems', 0, None)
            info.set_attribute_stringv('metadata::emblems', emblems)
            item.set_attributes_from_info(info, 0, None)
        # Refresh
        self._refresh(item_path)
    
    def restore_emblem(self, item_path):
        """Restore emblem to default"""
        item = Gio.File.new_for_path(item_path)
        info = item.query_info('metadata::emblems', 0, None)
        info.set_attribute('metadata::emblems', Gio.FileAttributeType.INVALID, 0)
        item.set_attributes_from_info(info, 0, None)
        self._refresh(item_path)

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
        # Ensure we don't get an empty list
        if files is None:
            return

        global last_calculation
        all_items_are_untrusted = True
        file_paths = []

        # Get list of selected file paths
        for file_obj in files:
            # Local files only; not remote
            if file_obj.get_uri_scheme() != 'file':
                return

            file_paths.append(file_obj.get_location().get_path())

        if len(file_paths) == 0:
            return

        if last_calculation['file_list'] == file_paths:
            # We've already checked this, just use the last result
            all_items_are_untrusted = last_calculation['result']
        else:
            # Record this list of files for next time
            last_calculation['file_list'] = file_paths

            # Check if any of the items set as trusted
            proc = subprocess.Popen(['/usr/bin/qvm-file-trust', '-Dq'] +
                file_paths, stdout=subprocess.PIPE)
            subprocess.Popen.wait(proc)

            # With -D, qvm-file-trust will only return 1, or untrusted, if ALL
            # files are untrusted
            all_items_are_untrusted = (proc.returncode == 1)

            # Save this result for next time
            last_calculation['result'] = all_items_are_untrusted

        if all_items_are_untrusted:
            # All files are trusted
            menu_item = \
                Nautilus.MenuItem(name='QubesMenuProvider::QubesTrustMenu',
                                  label='Do Not Always Open In DisposableVM',
                                  tip='',
                                  icon='qubes-checkmark')

        else:
            # If at least one folder is still trusted, set all of them as
            # untrusted on click
            print("without checkmark")
            menu_item = \
                Nautilus.MenuItem(name='QubesMenuProvider::QubesTrustMenu',
                                  label='Always Open In DisposableVM',
                                  tip='',
                                  icon='')

        menu_item.connect('activate', self.on_menu_item_clicked, files)
        return menu_item,

    def on_menu_item_clicked(self, menu, files):
        '''
        Called when user chooses files though Nautilus context menu.
        '''

        # Ensure we don't get an empty list
        if files is None:
            return

        # Invalidate last calculation
        last_calculation['file_list'] = []

        file_paths = []

        # Get absolute paths of all files
        for file_obj in files:
            # Check if folder still exists
            if file_obj.is_gone():
                return

            file_path = file_obj.get_location().get_path()

            file_paths.append(file_path)

        if len(file_paths) == 0:
            return

        # Check whether all items are untrusted
        proc = subprocess.Popen(['/usr/bin/qvm-file-trust', '-Dq'] +
                file_paths, stdout=subprocess.PIPE)
        subprocess.Popen.wait(proc)

        # If all items are untrusted, set to trusted
        if proc.returncode == 1:
            # Mark all selected as trusted
            subprocess.Popen(['/usr/bin/qvm-file-trust', '-t'] + file_paths)

            # Remove important emblem
            for file_path in file_paths:
                self.restore_emblem(file_path)
        else:
            # Some files are trusted, mark all as untrusted
            subprocess.Popen(['/usr/bin/qvm-file-trust', '-u'] + file_paths)

            # Add file emblem
            for file_path in file_paths:
                self.set_emblem(file_path, 'emblem-important')
