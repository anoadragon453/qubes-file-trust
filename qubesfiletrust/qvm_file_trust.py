#!/usr/bin/python3 -O
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

"""This script allows for the setting and checking of file and/or folder 
trust levels."""

import sys
import argparse
import os
import xattr
import subprocess

OUTPUT_QUIET = False
GLOBAL_FOLDER_LOC = '/etc/qubes/always-open-in-dispvm.list'
LOCAL_FOLDER_LOC = os.path.expanduser('~') + '/.config/qubes/always-open-in-dispvm.list'
PHRASE_FILE_LOC = '/etc/qubes/always-open-in-dispvm.phrase'
untrusted_path_found = False
all_paths_are_untrusted = True

def qprint(print_string, stderr):
    """Will only print if '--quiet' is not set."""

    if not OUTPUT_QUIET:
        if stderr:
            print(print_string)
        else:
            print(print_string, file=sys.stderr)

def error(error_string):
    """Print a string to stdout prepended with an error phrase."""

    qprint('Error: {}'.format(error_string), False)

# Print to stderr with 'Error: ' prepended
def serror(error_string):
    """Print a string to stderr prepended with an error phrase."""

    qprint('Error: {}'.format(error_string), True)

def retrieve_untrusted_folders():
    """Compile the list of untrusted folder paths from the following files:

    global list: /etc/qubes
    local  list: ~/.config/qubes into a list
    """

    untrusted_paths = set()

    # Start with the global list
    try:
        with open(GLOBAL_FOLDER_LOC) as global_list:
            for line in global_list.readlines():
                line = line.rstrip()

                # Ignore file comments
                if not line.startswith('#'):
                    # Remove any '/'s on the end of the path
                    if line.endswith('/'):
                        line = line[:-1]

                    # Lines prepended with - shouldn't go in the global list
                    # Just remove
                    if line.startswith('-'):
                        # Expand any ~'s in paths to /home/<current user>/
                        # before adding
                        expanded_path = os.path.expanduser(line[1:])
                        untrusted_paths.append(expanded_path)
                    else:
                        expanded_path = os.path.expanduser(line)
                        untrusted_paths.add(expanded_path)

    except:
        serror('Unable to open global untrusted folder description: {}'.
                format(GLOBAL_FOLDER_LOC))

    # Then the local list
    try:
        with open(LOCAL_FOLDER_LOC) as local_list:
            for line in local_list.readlines():
                line = line.rstrip()

                # Ignore file comments
                if not line.startswith('#'):
                    # Remove any '/'s on the end of the path
                    if line.endswith('/'):
                        line = line[:-1]

                    # Support explicitly trusting folders by prepending with -
                    if line.startswith('-'):
                        # Remove any mention of this path from the existing 
                        # list later
                        expanded_path = os.path.expanduser(line[1:])
                        untrusted_paths.remove(expanded_path)
                    else:
                        expanded_path = os.path.expanduser(line)
                        untrusted_paths.add(expanded_path)

    except:
        serror('Unable to open local untrusted folder description: {}'.
                format(LOCAL_FOLDER_LOC))

    return list(untrusted_paths)

def print_folders():
    """Print all known untrusted folders, line-by-line."""

    untrusted_folders = retrieve_untrusted_folders()

    # Print out all untrusted folders line-by-line
    for folder in untrusted_folders:
        print (folder)

def safe_chmod(path, perms, msg):
    """Chmod operation wrapped in a try/except statement."""

    # Set permissions to perms
    try:
        os.chmod(path, perms)
    except:
        error(msg)
        sys.exit(77)

def is_untrusted_xattr(path, orig_perms):
    """Check for 'user.qubes.untrusted' xattr on the file.

    Expects a readable file.
    """

    try:
        file_xattrs = xattr.get_all(path)

    except:
        error('Unable to read extended attributes of {}'.format(path))
        sys.exit(65)

    # Check for our custom qubes attribute
    untrusted_attribute = (b'user.qubes.untrusted', b'true')
    if untrusted_attribute in file_xattrs:
        # We found our attribute

        # Return to original permissions
        safe_chmod(path, orig_perms,
            'Unable to set original permissions of {}'.format(path))
        return True

    # Return to original permissions
    safe_chmod(path, orig_perms,
        'Unable to set original permissions of {}'.format(path))

    # We didn't find our attribute
    return False

def path_is_parent(parent, child):
    """Check if a child file/path is in a parent folder/path."""

    parent = os.path.abspath(parent)
    child = os.path.abspath(child)

    return os.path.commonpath([parent]) == os.path.commonpath([parent, child])

def is_untrusted_path(path):
    """Check to see if the path lies under a path that's considered untrusted

    Files listing untrusted paths lie in /etc/qubes/ and ~/.config/qubes
    under the name always-open-in-dispvm.list
    """

    untrusted_paths = retrieve_untrusted_folders()

    # Checks if the path is a child of an untrusted path (might be Linux only)
    for untrusted_path in untrusted_paths:
        if path_is_parent(untrusted_path, path):
            return True

    # Check if untrusted phrase (/etc/qubes/always-open-in-dispvm.phrase) is
    # present in file path
    try:
        with open(PHRASE_FILE_LOC) as phrase_file:
            for line in phrase_file.readlines():
                # Ignore comments
                if not line.rstrip().startswith('#'):
                    if line.rstrip().upper() is path.upper():
                        return True

                    # Only check for first non-comment in file
                    break
    except:
        serror('Unable to open phrase file: {}'.
                format(PHRASE_FILE_LOC))

    return False

def check_file(path, multiple_paths):
    """Check if the given file is trusted"""

    # Save the original permissions of the file.
    orig_perms = os.stat(path).st_mode

    # See if the file is readable
    try:
        with open(path):
            pass

    except IOError:
        # Try to unlock file to get read access
        safe_chmod(path, 0o644,
            'Could not unlock {} for reading'.format(path))

    # File is readable, attempt to check trusted status
    global untrusted_path_found
    global all_paths_are_untrusted
    if is_untrusted_xattr(path, orig_perms):
        # Print out which paths are untrusted if we're checking multiple paths
        if multiple_paths:
            qprint('Untrusted: {}'.format(path), False)
            untrusted_path_found = True
        else:
            qprint('File is untrusted', False)
            sys.exit(1)
    else:
        # Don't return until we've checked all paths
        if multiple_paths:
            all_paths_are_untrusted = False
        else:
            qprint('File is trusted', False)
            sys.exit(0)

def check_folder(path, multiple_paths):
    """Check if the given folder is trusted"""

    # Remove '/' from end of path
    if path.endswith('/'):
        path = path[:-1]

    global all_paths_are_untrusted
    global untrusted_path_found

    # Check if path is in the untrusted paths list
    if is_untrusted_path(path):
        # Print out which paths are untrusted if we're checking multiple paths
        if multiple_paths:
            qprint('Untrusted: {}'.format(path), False)
            untrusted_path_found = True
        else:
            qprint('Folder is untrusted', False)
            sys.exit(1)
    else:
        # Don't return until we've checked all paths
        if multiple_paths:
            all_paths_are_untrusted = False
        else:
            qprint('Folder is trusted', False)
            sys.exit(0)

def change_file(path, trusted):
    """Change the trust state of a file"""

    # Save the original permissions of the file
    orig_perms = os.stat(path).st_mode

    # See if the file is readable
    try:
        with open(path):
            pass

    except IOError:
        # Try to unlock file to get read access
        safe_chmod(path, 0o644,
            'Could not unlock {} for reading'.format(path))
    if trusted:
        # Set file to trusted
        # AKA remove our xattr
        file_xattrs = xattr.get_all(path)
        untrusted_attribute = (b'user.qubes.untrusted', b'true')

        # Check if the xattr exists first
        if untrusted_attribute in file_xattrs:
            try:
                xattr.removexattr(path, 'user.qubes.untrusted')
            except:
                # Unable to remove our xattr, return original permissions
                error('Unable to remove untrusted attribute on {}'.format(path))
                safe_chmod(path, orig_perms,
                    'Unable to set original perms. on {}'.format(path))

        try:
            # Remove important nautilus emblem
            proc = subprocess.Popen(['/usr/bin/gvfs-set-attribute', path, '-t', 'unset',
                'metadata::emblems'], stdout=subprocess.PIPE)
            subprocess.Popen.wait(proc)
            os.utime(path, None)
        except:
            error('Unable to remove emblem on {}'.format(path))

    else:
        # Set file to untrusted
        # AKA add our xattr and lock
        try:
            xattr.setxattr(path, 'user.qubes.untrusted', 'true')
            safe_chmod(path, 0o0,
                    'Unable to set untrusted permissions on: {}'.format(path))
        except:
            # Unable to remove our xattr, return original permissions
            safe_chmod(path, orig_perms,
                'Unable to return perms after setting as untrusted: {}'.
                format(path))
            sys.exit(65)

        try:
            proc = subprocess.Popen(['/usr/bin/gvfs-set-attribute', path, '-t', 'stringv',
                'metadata::emblems', 'important'])
            subprocess.Popen.wait(proc)
            os.utime(path, None)
        except:
            error('Unable to add emblem \'important\' on {}'.format(path))


def change_folder(path, trusted):
    """Change the trust state of a folder"""

    # Remove '/' from end of path
    if path.endswith('/'):
        path = path[:-1]

    try:
        # Create the ~/.config/qubes folder if it doesn't exist
        home_dir = os.path.expanduser('~')
        if not os.path.exists(home_dir + '/.config'):
            os.mkdir(home_dir + '/.config')
        if not os.path.exists(home_dir + '/.config/qubes'):
            os.mkdir(home_dir + '/.config/qubes')

        # Create the local file if it does not exist
        if not os.path.exists(LOCAL_FOLDER_LOC):
            open(LOCAL_FOLDER_LOC, 'a').close()
    except:
        error('Could not create local rule list: {}'.format(
                LOCAL_FOLDER_LOC))
        error('Check /home/<your user> folder exists...')
        sys.exit(72)

    if trusted:
        # Set folder to trusted
        # AKA remove any mentions from untrusted paths list
        try:
            file = open(LOCAL_FOLDER_LOC, 'r+')
        except:
            error('Unable to read local untrusted folder: {}'.
                    format(LOCAL_FOLDER_LOC))
            sys.exit(72)

        lines = file.readlines()
        file.seek(0)

        # Write back all lines to the file except ones containing our path
        for line in lines:
            if line.rstrip() != path:
                file.write(line)

        file.truncate()
        file.close()
    else:
        # Set folder to untrusted
        # AKA add path to untrusted paths list

        # Ensure path isn't already in untrusted paths list
        try:
            with open(LOCAL_FOLDER_LOC) as local_list:
                for line in local_list:
                    if line.rstrip() == path:
                        # Already untrusted, just return
                        return
        except:
            error('Unable to read local untrusted folder: {}'.
                    format(LOCAL_FOLDER_LOC))
            sys.exit(72)

        # Append path to the bottom
        file = open(LOCAL_FOLDER_LOC, 'ab')
        file.write(bytes(path, 'UTF-8'))

        file.truncate()
        file.close()

def main():
    """Read in from the command line and call dependent functions"""

    # Parse command line arguments
    parser = argparse.ArgumentParser(description='Set or check file/folder '
                                                 'trust levels.')
    # Add arguments
    parser.add_argument('-c', '--check', action='store_true',
                        help='check whether a file or folder is trusted')
    parser.add_argument('-C', '--check-multiple', action='store_true',
                        help='check trust for multiple paths. Returns '
                        '1 if at least one path is untrusted')
    parser.add_argument('-D', '--check-multiple-all-untrusted', action='store_true',
                        help='check trust for multiple paths. Returns '
                        '1 if and only if ALL paths are untrusted')
    parser.add_argument('-t', '--trusted', action='store_true',
                        help='Set files or folders as trusted')
    parser.add_argument('-u', '--untrusted', action='store_true',
                        help='Set files or folders as untrusted')
    parser.add_argument('-p', '--printfolders', action='store_true',
                        help='Print all local folders considered untrusted')
    parser.add_argument('-q', '--quiet', action='store_true',
                        help='Do not print to stdout')

    # Only require a path for certain options
    if not '--printfolders' in sys.argv and not '-p' in sys.argv:
        parser.add_argument('paths', metavar='path',
                            type=str, nargs='+', help='a folder or file path')

    args = parser.parse_args()

    # Set global quiet variable based on given flags
    global OUTPUT_QUIET
    OUTPUT_QUIET = args.quiet

    # Error checking
    if args.trusted and args.untrusted:
        error('--trusted and --untrusted options cannot both be set')
        sys.exit(64)
    if args.check and (args.trusted or args.untrusted):
        error('--trusted or --untrusted '
              'cannot be set while --is-trusted is set')
        sys.exit(64)

    if args.printfolders:
        print_folders()
        return

    checking_multiple = args.check_multiple or \
                        args.check_multiple_all_untrusted

    # Determine which action to take for each given path
    for path in args.paths:
        path = os.path.expanduser(path)
        path = os.path.abspath(path)
        if not (args.check or args.trusted or args.untrusted) \
            or args.check:
            if (not args.check_multiple_all_untrusted and \
                not args.check_multiple) and len(args.paths) > 1:
                error('Use --check-multiple to check multiple paths')
                sys.exit(64)
            if os.path.isdir(path):
                # Check folder
                check_folder(path, checking_multiple)
            elif not os.path.isdir(path):
                # Check file
                check_file(path, checking_multiple)

        elif os.path.isdir(path):
            if args.trusted:
                # Set folder as trusted
                change_folder(path, True)
            elif args.untrusted:
                # Set folder as untrusted
                change_folder(path, False)

        elif not os.path.isdir(path):
            if args.trusted:
                # Set file as trusted
                change_file(path, True)
            elif args.untrusted:
                # Set file as untrusted
                change_file(path, False)

    if args.check_multiple or args.check_multiple_all_untrusted:
        # Check whether we found an untrusted file during a check-multiple run
        global untrusted_path_found
        global all_paths_are_untrusted
        if untrusted_path_found == True:
            # If we're checking if ALL files are untrusted, only return 1 if
            # all files are indeed untrusted
            if args.check_multiple_all_untrusted and all_paths_are_untrusted:
                qprint('All paths untrusted', False)
                sys.exit(1)
            else:
                qprint('At least one path is trusted', False)
                sys.exit(0)

            sys.exit(1)
        else:
            qprint('All paths are trusted', False)
            sys.exit(0)

if __name__ == '__main__':
    main()
