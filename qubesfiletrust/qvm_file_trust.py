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

OUTPUT_QUIET = False
GLOBAL_FOLDER_LOC = '/etc/qubes/always-open-in-dispvm.list'
LOCAL_FOLDER_LOC = os.path.expanduser('~') + '/.config/qubes/always-open-in-dispvm.list'
PHRASE_FILE_LOC = '/etc/qubes/always-open-in-dispvm.phrase'

def qprint(print_string):
    """Will only print if '--quiet' is not set."""
    if not OUTPUT_QUIET:
        print(print_string)

def error(error_string):
    """Print a string prepended with an error phrase.'"""
    qprint('Error: {}'.format(error_string))

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
        error('Unable to open global untrusted folder description: {}'.
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
        error('Unable to open local untrusted folder description: {}'.
                format(LOCAL_FOLDER_LOC))

    return list(untrusted_paths)

def print_folders():
    untrusted_folders = retrieve_untrusted_folders()

    # Print out all untrusted folders line-by-line
    for folder in untrusted_folders:
        print (folder)

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
        return True

    # Return to original permissions
    try:
        os.chmod(path, orig_perms)
    except:
        error('Unable to set original permissions of {}'.format(path))
        sys.exit(77)

    # We didn't find our attribute
    return False

def is_untrusted_path(path):
    """Check to see if the path lies under a path that's considered untrusted

    Files listing untrusted paths lie in /etc/qubes/ and ~/.config/qubes
    under the name always-open-in-dispvm.list
    """

    untrusted_paths = retrieve_untrusted_folders()

    for untrusted_path in untrusted_paths:
        if path.startswith(untrusted_path):
            return True

    # Check if untrusted phrase (/etc/qubes/always-open-in-dispvm.phrase) is
    # present in file path
    try:
        with open(PHRASE_FILE_LOC) as phrase_file:
            for line in phrase_file.readlines():
                # Ignore comments
                if not line.rstrip().startswith('#'):
                    if line.rstrip().upper() in path.upper():
                        return True

                    # Only check for first non-comment in file
                    break
    except:
        error('Unable to open phrase file: {}'.
                format(PHRASE_FILE_LOC))

    return False

def check_file(path):
    """Check if the given file is trusted"""
    # Save the original permissions of the file.
    orig_perms = os.stat(path).st_mode

    # See if the file is readable
    try:
        with open(path):
            pass

    except IOError:
        try:
            # Try to unlock file to get read access
            os.chmod(path, 0o644)

        except:
            error('Could not unlock {} for reading'.format(path))
            sys.exit(77)

    # File is readable, attempt to check trusted status
    if is_untrusted_xattr(path, orig_perms):
        qprint('File is untrusted')
        sys.exit(1)
    else:
        qprint('File is trusted')
        sys.exit(0)

def check_folder(path):
    """Check if the given folder is trusted"""
    # Remove '/' from end of path
    if path.endswith('/'):
        path = path[:-1]

    # Check if path is in the untrusted paths list
    if is_untrusted_path(path):
        qprint('Folder is untrusted')
        sys.exit(1)
    else:
        qprint('Folder is trusted')
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
        try:
            # Try to unlock file to get read access
            os.chmod(path, 0o644)
        except:
            error('Could not unlock {} for reading'.format(path))
            sys.exit(77)

    if trusted:
        # Set file to trusted
        # AKA remove our xattr
        try:
            xattr.removexattr(path, 'user.qubes.untrusted')
        except:
            # Unable to remove our xattr, return original permissions
            error('Unable to remove untrusted attribute on {}'.format(path))
            try:
                os.chmod(path, orig_perms)
            except:
                error('Unable to set original perms. on {}'.format(path))
            finally:
                sys.exit(65)

    else:
        # Set file to untrusted
        # AKA add our xattr and lock
        try:
            xattr.setxattr(path, 'user.qubes.untrusted', 'true')
            os.chmod(path, 0o0)
        except:
            # Unable to remove our xattr, return original permissions
            error('Unable to set untrusted attribute on {}'.format(path))
            try:
                os.chmod(path, orig_perms)
            except:
                error('Unable to set original file after error')
            finally:
                sys.exit(65)

    # Add a GNOME emblem to the file
    # Subprocess:
    # gvfs-set-attribute "$FILEPATH" -t stringv metadata::emblems important

def change_folder(path, trusted):
    """Change the trust state of a folder"""
    # Remove '/' from end of path
    path = path[:-1]

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
        file.write(bytes(path))
        file.close()

def main():
    """Read in from the command line and call dependent functions"""
    # Parse command line arguments
    parser = argparse.ArgumentParser(description='Set or check file/folder '
                                                 'trust levels.')
    # Add arguments
    parser.add_argument('-c', '--check', action='store_true',
                        help='Check whether a file or folder is trusted')
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

    # Determine which action to take for each given path
    for path in args.paths:
        path = os.path.expanduser(path)
        path = os.path.abspath(path)
        if not (args.check or args.trusted or args.untrusted) \
            or args.check:
            if os.path.isdir(path):
                # Check folder
                check_folder(path)
            elif not os.path.isdir(path):
                # Check file
                check_file(path)
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

if __name__ == '__main__':
    main()
