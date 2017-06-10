import subprocess

from gi.repository import Nautilus, GObject


class FileSetTrustItemExtension(GObject.GObject, Nautilus.MenuProvider):
    '''Set trust level for a file or file-type.

    Uses the nautilus-python api to provide a context menu within Nautilus which
    enables the user to select trust levels for file or file-type.
    '''
    def get_file_items(self, window, files):
        # Attaches context menu in Nautilus
        if not files:
            return

        for file_obj in files:
            # Do not attach context menu to anything other that a file
            # local files only; not remote
            #if file_obj.get_uri_scheme() != 'file':
            #    return

            # Only show for files
            if file_obj.is_directory():
                return;

        menu_item = Nautilus.MenuItem(name='QubesMenuProvider::FileSetTrust',
                                      label='Always Open in DisposableVM...',
                                      tip='',
                                      icon='trust-settings')

        menu_item.connect('activate', self.on_menu_item_clicked, files)
        return menu_item,

    def on_menu_item_clicked(self, menu, files):
        '''Called when user chooses files though Nautilus context menu.
        '''
        for file_obj in files:

            # Check if file still exists
            if file_obj.is_gone():
                return

            # Show trust dialog for files
            gio_file = file_obj.get_location()
            subprocess.call(['/usr/lib/qubes/qvm-file-set-trust.gnome', gio_file.get_path()])
