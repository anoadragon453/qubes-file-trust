#
# The Qubes OS Project, http://www.qubes-os.org
#
# Copyright (C) 2017  Andrew Morgan <andrew@amorgan.xyz>
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

%define version %(cat version)

Name:           qubes-file-trust
Version:        %{version}
Release:        1%{?dist}
Summary:        Daemon and cli client to manipulate file trust levels in Qubes

Group:		Qubes
License:        GPL
URL:            http://qubes-os.org

BuildRequires:	gcc-c++
BuildRequires:	pandoc

Requires:	python3-pyxattr
Requires:	gvfs-client

Source:		qubes-file-trust.tgz

%description
Set of tools to manipulate file trust levels in Qubes

%prep

%build
make clean
make -B build

%install
# Change buildroot to $RPM_BUILD_ROOT?
make install DESTDIR=%{buildroot}

%clean
rm -rf %{buildroot}

%files
%{_bindir}/qvm-file-trust
%{_bindir}/qvm-open-trust-based
%{_bindir}/qubes-trust-daemon
%{_mandir}/man1/qvm-file-trust.1*
/usr/lib/python*

%changelog
* Sun Aug 27 2017 Andrew Morgan <andrew@amorgan.xyz> 0.1.1
- Initial release
