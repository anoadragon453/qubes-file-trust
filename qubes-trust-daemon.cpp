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
#include <exception>
#include <string>
#include <set>
#include <unistd.h>
#include <stdlib.h>
#include <pwd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/inotify.h>

#define GLOBAL_LIST "/etc/qubes/always-open-in-dispvm.list"

std::set<int> createNotificationWatchers(const int fd) {
    // Get user home directory
    const char* homedir;
    if ((homedir = getenv("HOME")) == NULL) {
        homedir = getpwuid(getuid())->pw_dir;
    }

    // Find global and local untrusted folder lists
    std::string global_list = GLOBAL_LIST;
    std::string local_list = homedir;
    local_list += "/.config/qubes/always-open-in-dispvm.list"; 

    std::set<std::string> rules;

    // Read from the global rules list
    std::ifstream global_stream(global_list);
    std::string rule;
    while (getline(global_stream, rule)) {
        // Ignore comments and empty lines
        if (rule.find("#") == 0 || rule == "") {
            continue;
        }

        // Process any override rules (lines starting with '-')
        if (rule.find("-") == 0) {
            rule = rule.substr(1, std::string::npos);
        }

        rules.insert(rule);
    }

    // Read from the local rules list
    std::ifstream local_stream(local_list);
    while (getline(local_stream, rule)) {
        // Ignore comments and empty lines
        if (rule.find("#") == 0 || rule == "") {
            continue;
        }

        // Process any override rules (lines starting with '-')
        if (rule.find("-") == 0) {
            // Remove "-" from beginning of string
            rule = rule.substr(1, std::string::npos);

            // Remove the rule from the current list
            std::set<std::string>::iterator it;
            for (it = rules.begin(); it != rules.end(); ++it) {
                if (*it == rule) {
                    rules.erase(rule);
                }
            }
        }
        else {
            rules.insert(rule);
        }
    }

    // Add an inotify watch to each folder in the set of rules
    std::set<std::string>::iterator it;
    std::set<int> watched_dirs;
    for (it = rules.begin(); it != rules.end(); ++it) {
		rule = *it;
        try {
            // Initialize

            int watch_descriptor = inotify_add_watch(fd, rule.c_str(), IN_CREATE);
            if (watch_descriptor == -1) {
                std::cerr << "Unable to watch " << rule << std::endl;
            }

            watched_dirs.insert(watch_descriptor);

            printf("Watching dir: %s\n", rule.c_str());

		} catch (std::exception &e) {
			std::cerr << "STL exception occured: " << e.what() << 
                " attempting to watch " << rule << std::endl;
		} catch (...) {
			std::cerr << "unknown exception occured" << 
                " attempting to watch " << rule << std::endl;
		}
	}

    return watched_dirs;
}

int main(void) {
    // TODO: Set up logging - log to syslog

    // Initialize inotify
    int fd = inotify_init();
    if (fd < 0) {
        std::cerr << "Unable to initialize inotify" << std::endl;
    }

    // Add a watch to each untrusted directory and subdirectory
    std::set<int> watched_dirs = createNotificationWatchers(fd);


    
	pid_t child_pid;
    int exitCode;

    // Fork and attempt to call qvm-file-trust
	switch (child_pid=fork()) {
		case 0:
            // We're the child, call qvm-file-trust
			execl("/usr/bin/qvm-file-trust", "qvm-file-trust", "/home/user/adrian", NULL);
            
			// unreachable if no error
			perror("execl qvm-file-trust");
			exit(1);
		case -1:
            // Fork failed
			perror("fork failed");
			exit(1);
		default:
            // Fork succeeded, and we got our pid, wait until child exits
			if (waitpid(child_pid, &exitCode, 0) == -1) {
				perror("wait for qvm-file-trust failed");
				exit(1);
			}
	}

    /*
    std::string command = "/usr/bin/qvm-file-trust /home/user/adrian -q";
    FILE* file_descriptor = popen(command.c_str(), "r");

    int exit_code = pclose(file_descriptor);
    if (WIFEXITED(exit_code)) {
        printf("Exit code: %d\n", WEXITSTATUS(exit_code));
    } else {
        printf("Error: Unable to mark file as untrusted\n");
    }
    */

    // Clean up all left-over descriptors
    // Code will likely never reach here though...
    std::set<int>::iterator it;
    for (it = watched_dirs.begin(); it != watched_dirs.end(); ++it) {
        inotify_rm_watch (fd, *it);
    }

    close(fd);

    return 0;
}
