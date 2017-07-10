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

#include <iostream>
#include <fstream>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <set>
#include <unistd.h>
#include <stdlib.h>
#include <pwd.h>
#include <sys/wait.h>
#include <sys/types.h>

#define GLOBAL_LIST "/etc/qubes/always-open-in-dispvm.list"

void createNotificationWatchers()
{
    // Get user home directory
    const char* homedir;
    if ((homedir = getenv("HOME")) == NULL)
        homedir = getpwuid(getuid())->pw_dir;

    // Find global and local untrusted folder lists
    std::string global_list = GLOBAL_LIST;
    std::string local_list = homedir;
    local_list += "/.config/qubes/always-open-in-dispvm.list"; 

    std::set<std::string> rules;

    // Read from the global rules list
    std::ifstream global_stream(global_list);
    std::string rule;
    while (getline(global_stream, rule))
    {
        // Ignore comments
        if (rule.find("#") == 0)
            continue;

        // Process any override rules (lines starting with '-')
        if (rule.find("-") == 0)
            rule = rule.substr(1, rule.size() - 1);

        rules.insert(rule);
    }

    // Read from the local rules list
    std::ifstream local_stream(local_list);
    while(getline(local_stream, rule))
    {
        // Ignore comments
        if (rule.find("#") == 0)
            continue;

        // Process any override rules (lines starting with '-')
        if (rule.find("-") == 0)
        {
            // Remove "-" from beginning of string
            rule = rule.substr(1, rule.size() - 1);

            // Remove the rule from the current list
            std::set<std::string>::iterator it;
            for(it = rules.begin(); it != rules.end(); ++it)
                if(*it == rule)
                    rules.erase(rule);
        }
        else
            rules.insert(rule);
    }

    for(auto i = rules.begin(); i != rules.end(); ++i)
        std::cout << *i << ", ";

}

int main(void)
{
    createNotificationWatchers();

    std::string command = "/usr/bin/qvm-file-trust /home/user/adrian -q";
    FILE* file_descriptor = popen(command.c_str(), "r");

    int exit_code = pclose(file_descriptor);
    if(WIFEXITED(exit_code))
        printf("Exit code: %d\n", WEXITSTATUS(exit_code));
    else
        printf("Error: Unable to mark file as untrusted\n");

    return 0;
}
