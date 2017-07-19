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
#include <errno.h>
#include <limits.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/inotify.h>

/* https://stackoverflow.com/a/29402705
 * If the C library can support 64-bit file sizes
 * and offsets, using the standard names,
 * these defines tell the C library to do so. */
#define _FILE_OFFSET_BITS 64 

/* POSIX.1 says each process has at least 20 file descriptors.
 * Three of those belong to the standard streams.
 * Here, we use a conservative estimate of 15 available;
 * assuming we use at most two for other uses in this program,
 * we should never run into any problems.
 * Most trees are shallower than that, so it is efficient.
 * Deeper trees are traversed fine, just a bit slower.
 * (Linux allows typically hundreds to thousands of open files,
 *  so you'll probably never see any issues even if you used
 *  a much higher value, say a couple of hundred, but
 *  15 is a safe, reasonable value.)
*/
#ifndef USE_FDS
#define USE_FDS 15
#endif

#define MAX_LEN 1024 /*Path length for a directory*/
#define MAX_EVENTS 1024 /*Max. number of events to process at one go*/
#define LEN_NAME 16 /*Assuming that the length of the filename won't exceed 16 bytes*/
#define EVENT_SIZE  ( sizeof (struct inotify_event) ) /*size of one event*/
#define BUF_LEN     ( MAX_EVENTS * ( EVENT_SIZE + LEN_NAME )) /*buffer to store the data of events*/

#define GLOBAL_LIST "/etc/qubes/always-open-in-dispvm.list"

int watch_fd;

// Set a file as untrusted through qvm-file-trust
// (Just do one call of qfm with the list of paths as arguments)
void set_file_as_untrusted(const std::set<std::string> file_paths) {
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
int watch_dir(const char *filepath, const struct stat *info,
                const int typeflag, struct FTW *pathinfo) {
    // Only watch directories
	struct stat s;
	if(stat(filepath, &s) == 0) {
		if(!(s.st_mode & S_IFDIR)) {
			// Not a directory
			return 0;
		}
	}
	else {
		// Error reading
		return 0;
	}

    std::cout << "Placing watch on " << filepath
        << " and subdirectories" << std::endl;

    // Add watch to starting directory
    int wd = inotify_add_watch(watch_fd, filepath, 
		IN_CREATE | IN_MODIFY | IN_DELETE);
    if (wd == -1) {
        printf("Couldn't add watch to %s\n", filepath);
    } else {
        printf("Watching:: %s\n", filepath);
    }

	return 0;
}

int place_watch_on_dir_and_subdirs(const char* const filepath) {
	int result;

    // Check for incorrect path
    if (filepath == NULL || *filepath == '\0')
		return errno = EINVAL;

	// Run watch_dir on directory and subdirectories
	result = nftw(filepath, watch_dir, USE_FDS, FTW_PHYS);

	if (result >= 0) {
		errno = result;
	}

	return errno;
}

// Watches directories and acts on IN_CREATE events
// In the case of one, sends new dir to above method, and new files to set_file_as_untrusted
// Need to buffer them though to stop rapid invocation of python interpreter
// Have something that fires every second and send the files and dirs to the right method and clears the sets
// This method could just append to the set(s)
void keep_watch_on_dirs(const int fd) {
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

std::set<std::string> get_untrusted_dir_list() {
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
    watch_fd = inotify_init();
    if (watch_fd < 0) {
        std::cerr << "Unable to initialize inotify" << std::endl;
    }

    // Get a list of all untrusted directories
    std::set<std::string> untrusted_dirs = get_untrusted_dir_list();

    // Add a watch to each untrusted directory and their subdirectories
    std::set<std::string>::iterator it;
    std::string dir;
    for (it = untrusted_dirs.begin(); it != untrusted_dirs.end(); ++it) {
        dir = *it;

        place_watch_on_dir_and_subdirs(dir.c_str());
    }

    // Monitor inotify for file events
    keep_watch_on_dirs(watch_fd);

    // Clean up left-over descriptor
    close(watch_fd);

    return 0;
}
