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
#include <string.h>
#include <set>
#include <unistd.h>
#include <ftw.h>
#include <stdlib.h>
#include <pwd.h>
#include <dirent.h>
#include <limits.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/inotify.h>

#define GLOBAL_LIST "/etc/qubes/always-open-in-dispvm.list"
#define MAX_LEN 1024 /*Path length for a directory*/
#define MAX_EVENTS 1024 /*Max. number of events to process at one go*/
#define LEN_NAME 16 /*Assuming that the length of the filename won't exceed 16 bytes*/
#define EVENT_SIZE  ( sizeof (struct inotify_event) ) /*size of one event*/
#define BUF_LEN     ( MAX_EVENTS * ( EVENT_SIZE + LEN_NAME )) /*buffer to store the data of events*/

// Set a file as untrusted through qvm-file-trust
// (Just do one call of qfm with the list of paths as arguments)
void setFileUntrusted(const std::set<std::string> file_paths) {
    pid_t child_pid;
    int exit_code;

    // Convert set to array
    const char* qvm_argv[file_paths.size() + 3];
    std::set<std::string>::iterator it;

    // Add non-variable program arguments
    qvm_argv[0] = "qvm-file-trust";
    qvm_argv[1] = "--untrusted";

    // Iterate through file path set and add to argv of qvm-file-trust
    int count = 1;
    const char* file_path;
    for (it = file_paths.begin(); it != file_paths.end(); ++it) {
        file_path = (*it).c_str();

        qvm_argv[count] = file_path;
        count++;
    }

    // Add NULL terminator
    qvm_argv[count] = (char*) NULL;

    // Fork and attempt to call qvm-file-trust
    switch (child_pid=fork()) {
        case 0:
            // We're the child, call qvm-file-trust
            execv("/usr/bin/qvm-file-trust", (char **) qvm_argv);

            // Unreachable if no error
            perror("execl qvm-file-trust");
            exit(1);
        case -1:
            // Fork failed
            perror("fork failed");
            exit(1);
        default:
            // Fork succeeded, and we got our pid, wait until child exits
            if (waitpid(child_pid, &exit_code, 0) == -1) {
                perror("wait for qvm-file-trust failed");
                exit(1);
            }
    }

}

// Places an inotify_watch on the directory and all subdirectories
// Should call above method after compiling a list of all files to set as untrusted
void placeWatchDirectoryAndSubdirectories(const int fd, const std::string file_path) {
    std::cout << "Placing watch on " << file_path
        << " and subdirectories" << std::endl;

    std::string root = file_path;

    DIR* dp = opendir(root.c_str());
    if (dp == NULL)
    {
        perror("Error opening the starting directory");
        exit(0);
    }

    // Add watch to starting directory
    int wd = inotify_add_watch(fd, root.c_str(), IN_CREATE | IN_MODIFY | IN_DELETE);
    if (wd == -1) {
        printf("Couldn't add watch to %s\n", root.c_str());
    } else {
        printf("Watching:: %s\n", root.c_str());
    }

    // TODO: Go deeeeeeper
    // Add watches to the Level 1 sub-dirs
    struct dirent* entry;
    std::string abs_dir;
    char buffer[BUF_LEN];
    while((entry = readdir(dp))) {
        // If it is a directory, add a watch
        // Don't add . and .. dirs
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 &&
                strcmp(entry->d_name, "..") != 0) {
            std::string full_path = file_path + "/" + entry->d_name;
            root = abs_dir;
            realpath(full_path.c_str(), buffer);
            abs_dir = buffer;

            wd = inotify_add_watch(fd, abs_dir.c_str(), IN_CREATE | IN_MODIFY | IN_DELETE);
            if (wd == -1) {
                printf("Couldn't add watch to the directory %s\n", abs_dir.c_str());
            } else {
                printf("Watching:: %s\n", abs_dir.c_str());
            }
        }
    }

    closedir(dp);
}

// Watches directories and acts on IN_CREATE events
// In the case of one, sends new dir to above method, and new files to setFileUntrusted
// Need to buffer them though to stop rapid invocation of python interpreter
// Have something that fires every second and send the files and dirs to the right method and clears the sets
// This method could just append to the set(s)
void watchDirectories(const int fd) {
    while(1) {
        int i = 0;
        char buffer[BUF_LEN];
        int length = read(fd, buffer, BUF_LEN);

        if (length < 0) {
            perror("read");
        }

        /* Read the events*/
        while (i < length) {
            struct inotify_event *event = (struct inotify_event *) &buffer[i];
            if (event->len) {
                if (event->mask & IN_CREATE) {
                    if (event->mask & IN_ISDIR) {
                        printf("%d DIR::%s CREATED\n", event->wd, event->name);
                    } else {
                        printf("%d FILE::%s CREATED\n", event->wd, event->name);
                    }
                }
            }

            if (event->mask & IN_MODIFY) {
                if (event->mask & IN_ISDIR) {
                    printf("%d DIR::%s MODIFIED\n", event->wd, event->name);
                } else {
                    printf("%d FILE::%s MODIFIED\n", event->wd, event->name);
                }
            }

            if (event->mask & IN_DELETE) {
                if (event->mask & IN_ISDIR) {
                    printf("%d DIR::%s DELETED\n", event->wd,event->name);
                } else {
                    printf("%d FILE::%s DELETED\n", event->wd,event->name);
                }
            }

            i += EVENT_SIZE + event->len;
        }
    }
}

std::set<std::string> getListOfUntrustedDirs() {
    // Get user home directory
    const char* homedir;
    if ((homedir = getenv("HOME")) == NULL) {
        homedir = getpwuid(getuid())->pw_dir;
    }

    // Find global and local untrusted dir lists
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

    return rules;
}

int main(void) {
    // TODO: Set up logging - log to syslog

    // Initialize inotify
    int fd = inotify_init();
    if (fd < 0) {
        std::cerr << "Unable to initialize inotify" << std::endl;
    }

    // Get a list of all untrusted directories
    std::set<std::string> untrusted_dirs = getListOfUntrustedDirs();

    // Add a watch to each untrusted directory and their subdirectories
    std::set<std::string>::iterator it;
    std::string dir;
    for (it = untrusted_dirs.begin(); it != untrusted_dirs.end(); ++it) {
        dir = *it;

        placeWatchDirectoryAndSubdirectories(fd, dir);
    }

    // Monitor inotify for file events
    watchDirectories(fd);

    // Clean up left-over descriptor
    close(fd);

    return 0;
}
