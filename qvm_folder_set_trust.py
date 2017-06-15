#!/usr/bin/env python3
import os

from gi.repository import Nautilus, GObject

# Location of untrusted tracking file
untr_track_file_location = '/home/user/untrusted-folder-types'

# If user selects multiple files and clicks menu item, by default if at
# least one folder is untrusted, we will mark all as untrusted. Otherwise,
# if they are all untrusted, set them to be trusted after click
all_folders_are_untrusted = True

class FolderSetTrustItemExtension(GObject.GObject, Nautilus.MenuProvider):

    # Create tracking file if it doesn't already exist
    if not os.path.isfile(untr_track_file_location):
        file(untr_track_file_location, 'w').close()

    def get_file_items(self, window, files):
        '''
        Set trust level for a folder and its contents.

        Uses the nautilus-python api to provide a context menu within Nautilus
        which enables the user to select trust levels for a folder and its
        contents.
        '''

        # Bring global variables in scope
        global untr_track_file_location
        global all_folders_are_untrusted

        # Reset global trust state
        all_folders_are_untrusted = True

        # Attaches context menu in Nautilus
        if not files:
            return

        for file_obj in files:
            # Do not attach context menu to anything other than a local file
            if file_obj.get_uri_scheme() != 'file':
                return

            print('Am I folder menu-running?')

            # Only show for folders
            if not file_obj.is_directory():
                return;

            # Check if any of the folders set as trusted
            if open(untr_track_file_location, 'r').read() \
                .find(file_obj.get_location().get_path()) == -1:
                # This particular folder is already marked as trusted
                all_folders_are_untrusted = False

        if all_folders_are_untrusted:
            menu_item = \
                Nautilus.MenuItem(name='QubesMenuProvider::FolderSetTrust',
                                  label='Always open contents in DisposableVM',
                                  tip='',
                                  icon='trust-checked')
        else:
            # If at least one folder is still trusted, set all of them as
            # untrusted on click
            menu_item = \
                Nautilus.MenuItem(name='QubesMenuProvider::FolderSetTrust',
                                  label='Always open contents in DisposableVM',
                                  tip='',
                                  icon='')

        menu_item.connect('activate', self.on_menu_item_clicked, files)
        return menu_item,

    def on_menu_item_clicked(self, menu, files):
        '''
        Called when user chooses files though Nautilus context menu.
        '''

        # Bring global variables in scope
        global untr_track_file_location
        global all_folders_are_untrusted

        for file_obj in files:
            # Check if folder still exists
            if file_obj.is_gone():
                return

            file_path = file_obj.get_location().get_path()
            untr_track_file_location_temp = untr_track_file_location + "-tmp"

            if all_folders_are_untrusted:
                # Mark all selected folders as trusted. AKA, loop through and
                # remove all folder names from untrusted tracking file
                with open(untr_track_file_location, "r") as input:
                    with open(untr_track_file_location_temp, "wb") as output:
                        for line in input:
                            if line != file_path + "\n":
                                output.write(line)
                
                # Overwrite original file with temp file
                os.rename(untr_track_file_location_temp, untr_track_file_location)
            else:
                # Mark all selected folders as untrusted
                with open(untr_track_file_location, "a") as untr_tracking_file:
                    untr_tracking_file.write(file_path + "\n")
