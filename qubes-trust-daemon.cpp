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
#include <unordered_map>
#include <string.h>
#include <set>
#include <unistd.h>
#include <ftw.h>
#include <stdlib.h>
#include <pthread.h>
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

enum WATCH {
    PLACE_WATCH,
    REMOVE_WATCH
};

#define UNTR_MARK_PERIOD 1  // Period to mark buffer of untrusted files
#define MAX_LEN 1024		// Path length for a directory
#define MAX_EVENTS 1024		// Max. number of events to process at one go
#define MAX_ARG_LEN 500     // Maximum amount of args passed to qvm-file-trust
#define LEN_NAME 16			// Assuming filename length won't exceed 16 bytes
#define EVENT_SIZE  (sizeof(struct inotify_event))	     // Size of one event
#define BUF_LEN     (MAX_EVENTS*(EVENT_SIZE + LEN_NAME)) // Event data
#define GLOBAL_LIST "/etc/qubes/always-open-in-dispvm.list"

int watch_fd;

/*
 * Hash table to keep track of watch descriptors and the absolute filepaths
 * that they correspond to */
std::unordered_map<int, std::string> watch_table;

/*
 * Store untrusted file's paths in here and mark them all as untrusted
 * in one fell swoop every few seconds */
std::set<std::string> untrusted_buffer;

// Set a file as untrusted through qvm-file-trust
// (Just do one call of qfm with the list of paths as arguments)
void mark_files_as_untrusted(const std::set<std::string> file_paths) {
    // Return if given empty set
    if (file_paths.size() <= 0) {
        printf("Set is size 0\n");
        return;
    }

    printf("Set is %d items large\n", file_paths.size());

    pid_t child_pid;
    int exit_code;

    // Convert set to array
    const char* qvm_argv[MAX_ARG_LEN + 3]; // Account for extra args

    // Add non-variable program arguments
    qvm_argv[0] = "qvm-file-trust";
    qvm_argv[1] = "--untrusted";

    // Iterate through file path set and add to argv of qvm-file-trust
    const char* file_path;
    
    // Loop through list of arguments, processing MAX_ARG_LEN args each time
    int iterations = file_paths.size() / MAX_ARG_LEN;
    std::set<std::string>::iterator it = file_paths.begin();
    for (int i = 0; i <= iterations; i++) {
        int arg_index = 2;

        // Build an array of MAX_ARG_LEN elements
        for (; arg_index - 2 < MAX_ARG_LEN; ++it) {
            // Stop when we've hit the end
            if (it == file_paths.end())
                break;

            file_path = (*it).c_str();

            printf("Marking untrusted:: %s\n", file_path);

            qvm_argv[arg_index] = file_path;
            arg_index++;
            printf("arg_index: %d\n", arg_index); // DEBUG
        }

        // Add NULL terminator to signal end of argument list
        qvm_argv[arg_index] = (char*) NULL;

        // Once we have a list of MAX_ARG_LEN args,
        // fork and attempt to call qvm-file-trust
        switch (child_pid=fork()) {
            case 0:
                // We're the child, call qvm-file-trust
                // TODO: Temp qvm-file-trust binary
                execv("/home/user/Documents/gsoc-2017/qubes-mime-types/qubesfiletrust/qvm_file_trust.py", (char **) qvm_argv);

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

    untrusted_buffer.clear();
}

// Places an inotify_watch on the directory and all subdirectories
// Should call above method after compiling a list of all files to set as untrusted
int watch_dir(const char *filepath, const struct stat *info,
              const int typeflag, struct FTW *pathinfo) {
    // Watch directories, set files as untrusted
	struct stat s;
	if(stat(filepath, &s) == 0) {
		if(!(s.st_mode & S_IFDIR)) {
            // File, set as untrusted
            untrusted_buffer.insert(filepath);
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
		IN_CREATE | IN_MODIFY | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM);
    if (wd == -1) {
        printf("Couldn't add watch to %s\n", filepath);
    } else {
        printf("%d Watching:: %s\n", wd, filepath);
        
        // Add watch descriptor and filepath to global watchtable
        std::string filepath_string = filepath;
        std::pair<int, std::string> watch_pair(wd, filepath_string);
        watch_table.insert(watch_pair);
    }

	return 0;
}

int place_watch_on_dir_and_subdirs(const char* const filepath) {
	int result;

    // Check for incorrect path
    if (filepath == NULL || *filepath == '\0') {
		return errno = EINVAL;
    }

	// Run watch_dir on directory and subdirectories
	result = nftw(filepath, watch_dir, USE_FDS, FTW_PHYS);

	if (result >= 0) {
		errno = result;
	}

	return errno;
}

// Watches directories and acts on IN_CREATE events
// In the case of one, sends new dir to above method, and new files to mark_files_as_untrusted
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
            std::string filepath = watch_table[event->wd];
            std::string fullpath = filepath + "/" + event->name;

            if (event->len) {
                if (event->mask & IN_CREATE) {
                    // Get absolute filepath from our global watch_table
                    if (event->mask & IN_ISDIR) {
                        printf("%d DIR::%s CREATED\n", event->wd, fullpath.c_str());
                        place_watch_on_dir_and_subdirs(fullpath.c_str());
                    } else {
                        printf("%d FILE::%s CREATED\n", event->wd, fullpath.c_str());
                        // Mark file to be set as untrusted
                        untrusted_buffer.insert(fullpath);
                    }
                }

                if (event->mask & IN_MOVED_TO) {
                    if (event->mask & IN_ISDIR) {
                        printf("%d DIR::%s CREATED\n", event->wd, fullpath.c_str());
                        place_watch_on_dir_and_subdirs(fullpath.c_str());
                    } else {
                        printf("%d FILE::%s CREATED\n", event->wd, fullpath.c_str());
                        // Mark file to be set as untrusted
                        untrusted_buffer.insert(fullpath);
                    }
                }
            }

            if (event->mask & IN_MOVED_FROM) {
                if (event->mask & IN_ISDIR) {
                    printf("%d REMOVE DIR::%s\n", event->wd, fullpath.c_str());

                    // TODO: Recursive rm watch
                    inotify_rm_watch(watch_fd, event->wd);
                } else {
                    printf("%d FILE::%s MOVED OUT\n", event->wd, fullpath.c_str());
                }
            }

            if (event->mask & IN_MODIFY) {
                if (event->mask & IN_ISDIR) {
                    printf("%d DIR::%s MODIFIED\n", event->wd, fullpath.c_str());
                } else {
                    printf("%d FILE::%s MODIFIED\n", event->wd, fullpath.c_str());
                }
            }

            i += EVENT_SIZE + event->len;
        }
    }
}


// TODO: Get this from qvm-file-trust instead to unify definitions
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

/*
 * Run every few seconds and untrusted_buffer to handler method */
void *set_trust_on_timer(void*) {
    while(1) {
        printf("Timer fired!\n");
        sleep(UNTR_MARK_PERIOD);
        mark_files_as_untrusted(untrusted_buffer);
    }
    return 0;
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

    // Start untrusted_buffer monitor
    pthread_t tid;
    pthread_create(&tid, NULL, &set_trust_on_timer, NULL);

    // Monitor inotify for file events
    keep_watch_on_dirs(watch_fd);

    // Clean up left-over descriptor
    close(watch_fd);

    return 0;
}
