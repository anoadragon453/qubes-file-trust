/*
* The Qubes OS Project, http://www.qubes-os.org
*
* Copyright (C) 2017 Andrew Morgan <andrew@amorgan.xyz>
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*
*/

#include <stdio.h>
#include <sys/wait.h>

#define BUFFER_SIZE 100

int main(void)
{
    char* command = "/usr/bin/qvm-file-trust /home/user/adrian; echo $?";
    char* buffer[BUFFER_SIZE];
    FILE* fileDescriptor = popen(command, "r");
    int read_num = fread(buffer, sizeof(char), BUFFER_SIZE, fileDescriptor);

    int exitCode = pclose(fileDescriptor);
    if(WIFEXITED(exitCode))
        printf("Exit code: %d\n", WEXITSTATUS(exitCode));
    else
        printf("Nuffin\n");

    return 0;
}
