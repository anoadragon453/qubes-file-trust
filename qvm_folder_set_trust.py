import subprocess

from gi.repository import Nautilus, GObject


class FolderSetTrustItemExtension(GObject.GObject, Nautilus.MenuProvider):
    '''Set trust level for a folder and its contents.

    Uses the nautilus-python api to provide a context menu within Nautilus which
    enables the user to select trust levels for a folder and its contents.
    '''
    def get_file_items(self, window, files):
        # Attaches context menu in Nautilus
        if not files:
            return

        for file_obj in files:
            # Do not attach context menu to anything other than a local file
            #if file_obj.get_uri_scheme() != 'file':
            #    return

            # Only show for folders
            if not file_obj.is_directory():
                return;

        menu_item = Nautilus.MenuItem(name='QubesMenuProvider::FolderSetTrust',
                                      label='Always open contents in DisposableVM',
                                      tip='',
                                      icon='trust-checked')

        menu_item.connect('activate', self.on_menu_item_clicked, files)
        return menu_item,

    def on_menu_item_clicked(self, menu, files):
        '''Called when user chooses files though Nautilus context menu.
        '''
        for file_obj in files:
            # Check if folder still exists
            if file_obj.is_gone():
                return
