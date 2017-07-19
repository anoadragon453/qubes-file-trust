#!/usr/bin/python3
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

import unittest
import unittest.mock
import getpass
import xattr
import sys
import io
import os
import qubesfiletrust.qvm-file-trust

user_home = os.path.expanduser('~')

class TC_00_trust(unittest.TestCase):

    @unittest.mock.patch('qvm-file-trust.open', 
            new_callable=unittest.mock.mock_open())
    def test_000_retrieve_folders(self, list_mock):
        """Create a mock global and local list and check resulting rules.
        
        Ensure all injected rules are accounted for
        """

        handlers = (unittest.mock.mock_open(
                        read_data='/home/user/Downloads'
                        '\n/home/user/QubesIncoming'.replace(
                            '/home/user', user_home)
                        ).return_value,
                        unittest.mock.mock_open(
                        read_data='/home/user/Downloads'
                        '\n/home/user/QubesIncoming'
                        '\n/home/user/Pictures'
                        '\n/var/log/'
                        '\n/etc/'
                        '\n~/terrible files'
                        '\n~/my way too long path name with spaces'.replace(
                            '/home/user', user_home)).return_value)
        list_mock.side_effect = handlers

        untrusted_folder_paths = []

        try:
            untrusted_folder_paths = qvm_file_trust.retrieve_untrusted_folders()
        finally:
            # Order not garunteed, therefore assert with assertCountEqual
            # Despite the name, it does check for the same elements in each
            # list, not just the amount of elements
            self.assertCountEqual(untrusted_folder_paths, 
                    [w.replace('/home/user', user_home) for w in 
                    ['/home/user/Downloads', '/home/user/QubesIncoming', 
                    '/home/user/Pictures', '/var/log', '/etc', 
                    '/home/user/terrible files',
                    '/home/user/my way too long path name with spaces']])

    @unittest.mock.patch('qvm-file-trust.open', 
            new_callable=unittest.mock.mock_open())
    def test_001_retrieve_folders_override(self, list_mock):
        """Create a mock global and local list and check resulting rules.
        
        Check global rules are properly overridden by '-' prepended local rules
        """

        handlers = (unittest.mock.mock_open(
                        read_data='~/Downloads'
                            '\n~/QubesIncoming'
                        ).return_value,
                        unittest.mock.mock_open(
                        read_data='/home/user/Downloads'
                            '\n-/home/user/QubesIncoming'.replace(
                            '/home/user', user_home)
                        ).return_value)
        list_mock.side_effect = handlers

        untrusted_folder_paths = []

        try:
            untrusted_folder_paths = qvm_file_trust.retrieve_untrusted_folders()
        finally:
            self.assertCountEqual(untrusted_folder_paths, 
                    [w.replace('/home/user', user_home) for w in
                    ['/home/user/Downloads']])

    def test_010_check_read_attribute_success(self):
        """Check whether our untrusted attribute is successfully found"""
        xattr.get_all = unittest.mock.MagicMock(
                return_value=[(b'user.qubes.untrusted', b'true'), (b'user.something.else', b'false')])
        os.chmod = unittest.mock.MagicMock()

        test_result = False
        try:
            test_result = qvm_file_trust.is_untrusted_xattr('', '777') 
            self.assertTrue(test_result)
        except SystemExit as err: 
            self.fail('System Exit caught: {}'.format(err))

    def test_011_check_read_attribute_failure(self):
        """Check whether we support not finding our attribute"""
        xattr.get_all = unittest.mock.MagicMock(
                return_value=['user.bla.something', 'user.something.else'])
        os.chmod = unittest.mock.MagicMock()

        test_result = True
        try:
            test_result = qvm_file_trust.is_untrusted_xattr('', '777') 
            self.assertFalse(test_result)
        except SystemExit as err: 
            self.fail('System Exit caught: {}'.format(err))

    @unittest.mock.patch('qvm-file-trust.open', 
            new_callable=unittest.mock.mock_open())
    def test_020_check_untrusted_path_path_based(self, list_mock):
        """Check if a path is untrusted based on untrusted folders lists"""
        dummy_untrusted_phrase = '.untrusted'
        dummy_file_data = '~/Downloads/\n~/QubesIncoming'

        handlers = (unittest.mock.mock_open(
                        read_data=dummy_file_data
                        ).return_value,
                        unittest.mock.mock_open(
                        read_data=dummy_file_data
                        ).return_value,
                        unittest.mock.mock_open(
                        read_data=dummy_file_data
                        ).return_value,
                        unittest.mock.mock_open(
                        read_data=dummy_file_data
                        ).return_value,
                        unittest.mock.mock_open(
                        read_data=dummy_untrusted_phrase
                        ).return_value)
        list_mock.side_effect = handlers

        test_result = False

        try:
            # This method expects os.path.expanduser to have already been run
            test_result = qvm_file_trust.is_untrusted_path(
                    user_home + '/Downloads')
        finally:
            self.assertTrue(test_result)

        test_result = True
        try:
            test_result = qvm_file_trust.is_untrusted_path(
                    user_home + '/Trusted Folder')
        finally:
            self.assertFalse(test_result)

    @unittest.mock.patch('qvm-file-trust.open', 
            new_callable=unittest.mock.mock_open())
    def test_021_check_untrusted_path_phrase_based(self, list_mock):
        """Check if path is untrusted based on untrusted phrase"""
        dummy_untrusted_phrase = '.untrusted'
        dummy_file_data = '~/Downloads\n~/QubesIncoming'

        handlers = (unittest.mock.mock_open(
                        read_data=dummy_file_data
                        ).return_value,
                        unittest.mock.mock_open(
                        read_data=dummy_file_data
                        ).return_value,
                        unittest.mock.mock_open(
                        read_data=dummy_untrusted_phrase
                        ).return_value,
                        unittest.mock.mock_open(
                        read_data=dummy_file_data
                        ).return_value,
                        unittest.mock.mock_open(
                        read_data=dummy_file_data
                        ).return_value,
                        unittest.mock.mock_open(
                        read_data=dummy_untrusted_phrase
                        ).return_value,
                        unittest.mock.mock_open(
                        read_data=dummy_file_data
                        ).return_value,
                        unittest.mock.mock_open(
                        read_data=dummy_file_data
                        ).return_value,
                        unittest.mock.mock_open(
                        read_data=dummy_untrusted_phrase
                        ).return_value)
        list_mock.side_effect = handlers

        test_result = False

        try:
            # Check if '.untrusted' in filepath
            test_result = qvm_file_trust.is_untrusted_path(
                    '/path/to/.untrusted/folder')
        finally:
            self.assertTrue(test_result)

        test_result = True
        try:
            test_result = qvm_file_trust.is_untrusted_path(
                    user_home + '/Trusted_Folder')
        finally:
            self.assertFalse(test_result)

        # Check that phrase is case insensitive
        test_result = False 
        try:
            # Check if '.untrusted' in filepath
            test_result = qvm_file_trust.is_untrusted_path(
                    '/path/to/.uNtrUsTeD/folder')
        finally:
            self.assertTrue(test_result)

    def test_030_check_exits_properly_on_bad_chmod(self):
        """Ensure script exits with proper return code on chmod failure"""
        def failing_chmod():
            # This is our failing os.chmod replacement
            raise Exception

        os.chmod = failing_chmod

        # Dummy data
        os.stat = unittest.mock.Mock()
        os.stat.st_mode = 0

        # When an exception is raised, make sure it is exit code 77
        # i.e., chmod issue
        with self.assertRaises(SystemExit) as cm:
            qvm_file_trust.OUTPUT_QUIET = True
            qvm_file_trust.change_file('', True)

        self.assertEqual(cm.exception.code, 77)

    # TODO: Check xattr.setxattr actually works...
    def test_031_xattr_called_when_setting_file_trust(self):
        """Ensure our attribute is added/removed when setting file trust"""
        os.chmod = unittest.mock.MagicMock()

        # Dummy data
        os.stat = unittest.mock.Mock()
        os.stat.st_mode = 0

        xattr.removexattr = unittest.mock.MagicMock()
        qvm_file_trust.change_file('do_trust_me', True)

        xattr.removexattr.assert_called_once_with('do_trust_me',
                'user.qubes.untrusted')

        xattr.setxattr = unittest.mock.MagicMock()
        qvm_file_trust.change_file('dont_trust_me', False)

        xattr.setxattr.assert_called_once_with('dont_trust_me',
                'user.qubes.untrusted', 'true')

    '''
    # TODO: Do some tests based on command line arguments and that correct
    # qvm-file-trust methods are called based on the arguments
    def test_040_parse_args(self):
        """Check to make sure correct methods are called given arguments"""
    '''

class TC_10_misc(unittest.TestCase):
    def test_000_quiet(self):
        """Make sure we're not printing when we shouldn't be."""
        qvm_file_trust.OUTPUT_QUIET = True
        captured_obj = io.StringIO()
        sys.stdout = captured_obj
        try:
            qvm_file_trust.qprint('Test string!')
        finally:
            sys.stdout = sys.__stdout__
            self.assertEqual(captured_obj.getvalue(), '')

        qvm_file_trust.OUTPUT_QUIET = False
        captured_obj = io.StringIO()
        sys.stdout = captured_obj
        try:
            qvm_file_trust.qprint('Test string!')
        finally:
            sys.stdout = sys.__stdout__
            self.assertEqual(captured_obj.getvalue(), 'Test string!\n')

    def test_010_error(self):
        """Ensure errors are formatted properly."""
        qvm_file_trust.OUTPUT_QUIET = False
        captured_obj = io.StringIO()
        sys.stdout = captured_obj
        try:
            qvm_file_trust.error('Test string!')
        finally:
            sys.stdout = sys.__stdout__
            self.assertEqual(captured_obj.getvalue(), 'Error: Test string!\n')

        qvm_file_trust.OUTPUT_QUIET = True
        captured_obj = io.StringIO()
        sys.stdout = captured_obj
        try:
            qvm_file_trust.error('Test string!')
        finally:
            sys.stdout = sys.__stdout__
            self.assertEqual(captured_obj.getvalue(), '')

    # TODO: Test that dependencies are installed either in unittest or program

def list_tests():
    return (
            TC_00_trust,
            TC_10_misc
    )

if __name__ == '__main__':
    unittest.main()
