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

#include <string>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <iostream>
#include <exception>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <ftw.h>
#include <pwd.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/inotify.h>

/* 
 * https://stackoverflow.com/a/29402705
 * If the C library can support 64-bit file sizes
 * and offsets, using the standard names,
 * this define tell the C library to do so. */
#define _FILE_OFFSET_BITS 64 

/* 
 * POSIX.1 says each process has at least 20 file descriptors.
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
 *
 */
#ifndef USE_FDS
#define USE_FDS 15
#endif

#define UNTR_MARK_PERIOD 1  // Period to mark buffer of untrusted files
#define MAX_LEN 1024        // Path length for a directory
#define MAX_EVENTS 1024     // Max. number of events to process at one go
#define MAX_ARG_LEN 500     // Maximum amount of args passed to qvm-file-trust
#define EVENT_SIZE (sizeof(struct inotify_event))	     // Size of one event
#define BUF_LEN (MAX_EVENTS*(EVENT_SIZE + NAME_MAX + 1)) // Event data buffer

int watch_fd;

/*
 * Unordered map to keep track of watch descriptors and the absolute filepaths
 * that they correspond to 
 */
std::unordered_map<int, std::string> watch_table;

/*
 * Store untrusted file's paths in here and mark them all as untrusted
 * in one fell swoop every few seconds
 */
std::unordered_set<std::string> untrusted_buffer;

/*
 * Signifies whether the python client is currently being called.
 * Used to wake the client up again when we get new batches after a
 * long period of inactivity.
 */
bool currentlyMarkingFiles;

/*
 * Store untrusted directory listing and compare when rule lists change
 */
std::unordered_set<std::string> untrusted_dirs;

/*
 * Keep track of global and local rules lists
 */
std::string global_rules;
std::string local_rules;

/*
 * Helper function, string startswith
 */
bool startsWith(const std::string& haystack, const std::string& needle) {
    return needle.length() <= haystack.length() && 
        equal(needle.begin(), needle.end(), haystack.begin());
}

/*
 * Set a file or files as untrusted through qvm-file-trust
 */
void mark_files_as_untrusted(const std::unordered_set<std::string> file_paths) {
    if (currentlyMarkingFiles) {
        std::cout << "Quitting because we're still running..." << std::endl;
        return;
    }

    if (file_paths.empty()) {
        std::cout << "No file paths provided, quitting..." << std::endl;
        return;
    }

    currentlyMarkingFiles = true;
    std::cout << "Marking " << file_paths.size() << " files as untrusted!" << std::endl;

    pid_t child_pid;
    int exit_code;

    // Convert set to array
    // Modify to add extra arguments if necessary
    int extra_args = 3;
    const char* qvm_argv[MAX_ARG_LEN + extra_args];

    // Add non-variable program arguments
    qvm_argv[0] = "qvm-file-trust";
    qvm_argv[1] = "--untrusted";

    // Iterate through file path set and add to argv of qvm-file-trust
    const char* file_path;

    // Loop through list of arguments, processing MAX_ARG_LEN args each time
    int iterations = file_paths.size() / MAX_ARG_LEN;
    std::unordered_set<std::string>::const_iterator it = file_paths.begin();
    std::cout << "Iterating..." << std::endl;
    for (; iterations >= 0; iterations--) {
        int arg_index = extra_args - 1;

        // Build an array of MAX_ARG_LEN elements
        std::cout << "Adding args..." << std::endl;
        for (; arg_index < MAX_ARG_LEN + extra_args - 1; it++) {
            // Stop and break from inner loop when we've hit the end
            if (it == file_paths.end())
                break;

            file_path = (*it).c_str();

            printf("Marking untrusted:: %s\n", file_path);

            qvm_argv[arg_index] = file_path;
            arg_index++;
        }
        std::cout << "Finished adding args..." << std::endl;

        // Add NULL terminator to signal end of argument list
        qvm_argv[arg_index] = (char*) NULL;

        // Once we have a list of MAX_ARG_LEN args,
        // fork and attempt to call qvm-file-trust
        std::cout << "Forking!" << std::endl;
        switch (child_pid=fork()) {
            case 0:
                // We're the child, call qvm-file-trust
                execv("/usr/bin/qvm-file-trust", (char**) qvm_argv);

                // Unreachable if no error
                perror("execl qvm-file-trust failed");
                exit(1);
            case -1:
                // Fork failed
                perror("fork failed");
                return;
            default:
                // Fork succeeded, and we got our pid, wait until child exits
                std::cout << "Waiting for the child..." << std::endl;
                if (waitpid(child_pid, &exit_code, 0) == -1) {
                    // Child has exited with error
                    perror("wait for qvm-file-trust failed");
                    std::cout << "Failed!" << std::endl;

                    // Return and try these files again later
                    currentlyMarkingFiles = false;
                    return;
                }

                std::cout << "Didn't fail!" << std::endl;

                if (!untrusted_buffer.empty()) {
                    // Remove these files from the untrusted_buffer
                    // Advance by the number of marked files
                    int starting_index = arg_index - extra_args + 1;
                    std::unordered_set<std::string>::iterator it = untrusted_buffer.begin();
                    std::advance(it, starting_index);

                    std::cout << "Buffer size: " << untrusted_buffer.size() << std::endl;
                    std::cout << "Advanced by " << arg_index + 1 - extra_args << std::endl;

                    // Create a new buffer with the new items, faster than deleting
                    std::unordered_set<std::string> new_untrusted_buffer;
                    for (; it != untrusted_buffer.end(); it++) {
                        std::cout << "Running: " << (*it) << std::endl;
                        new_untrusted_buffer.insert((*it));
                    }

                    std::cout << "Old buffer size: " << untrusted_buffer.size() << std::endl;
                    untrusted_buffer = new_untrusted_buffer;
                    std::cout << "New buffer size: " << untrusted_buffer.size() << std::endl;

                    // Clean up
                    currentlyMarkingFiles = false;
                    mark_files_as_untrusted(untrusted_buffer);
                }

            currentlyMarkingFiles = false;
        }
    }
}

/*
 * Places an inotify watch on a filepath
 */
int inotify_watch_path(const char *filepath) {
    // Add watch to starting directory
    int wd = inotify_add_watch(watch_fd, filepath, 
            IN_CREATE | IN_MODIFY | IN_DELETE_SELF | IN_MOVED_TO | IN_MOVED_FROM | IN_MOVE_SELF);
    if (wd == -1) {
        printf("Couldn't add watch to %s\n", filepath);
    } else {
        printf("%d Watching:: %s\n", wd, filepath);

        // Add watch descriptor and filepath to global watchtable
        std::string filepath_string = filepath;
        std::pair<int, std::string> watch_pair(wd, filepath_string);
        watch_table.insert(watch_pair);
    }

    // Return the watch descriptor
    return wd;
}

/*
 * Removes inotify_watch on the given directory
 */
void rec_rm_watch(std::string filepath) {
    // Add '/' to end of folder strings
    // Removes accidental marking of similar folder names
    // i.e. /root and /rootabega folders
    filepath += "/";

    for (auto it = watch_table.begin(); it != watch_table.end();) {
        std::string checkPath = it->second + "/";

        // Check if any of the filepaths in watch_table start with
        // our filepath
        // FIXME: Doesn't remove it from the first folder loop runs through 
        // This is mitigated by our check in inotify events for empty
        // filepaths as watch_table will return an empty val for an unknown fd
        // But ideally we wouldn't have to check for that.
        if (startsWith(checkPath, filepath)) {
            it = watch_table.erase(it);
            if (inotify_rm_watch(watch_fd, it->first) != 0) {
                printf("Error removing watch: %d\n", errno);
            }
        } else {
            ++it;
        }
    }
}

/*
 * Places an inotify_watch on the given directory
 */
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
        return 1;
    }

    std::cout << "Placing watch on " << filepath
        << " and subdirectories" << std::endl;
    inotify_watch_path(filepath);

    return 0;
}

/*
 * Walks a filepath for all files and directories contained within
 */
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

    // Mark any found files as untrusted
    std::cout << "Finished running. untrusted_buffer is now size: " << untrusted_buffer.size() << std::endl;
    mark_files_as_untrusted(untrusted_buffer);

    return errno;
}

/*
 * Get a list of untrusted directories to watch
 * from qvm-file-trust's output
 */
std::unordered_set<std::string> get_untrusted_dir_list() {
    std::unordered_set<std::string> rules;

    FILE* fp = popen("/usr/bin/qvm-file-trust -p", "r");
    char buf[1024*1024];

    fread(buf, 1, sizeof(buf), fp);
    std::string rules_str = buf;

    std::stringstream ss(rules_str);
    std::string to;

    while (std::getline(ss, to, '\n')) {
        rules.insert(to);
    }

    // Watch any changes in the rules lists
    inotify_watch_path(global_rules.c_str());
    inotify_watch_path(local_rules.c_str());
    return rules;
}

/*
 * Retrieve the list of untrusted dirs and place a watch on
 * it and its subdirs.
 */
void watch_untrusted_dir_list() {
    // Get the list of all untrusted directories
    untrusted_dirs = get_untrusted_dir_list();

    // Add a watch to each untrusted directory and their subdirectories
    std::unordered_set<std::string>::iterator it;
    std::string dir;
    for (it = untrusted_dirs.begin(); it != untrusted_dirs.end(); it++) {
        dir = *it;
        place_watch_on_dir_and_subdirs(dir.c_str());
    }
}

/* 
 * Watches directories and acts on various spawned inotify events
 */
void keep_watch_on_dirs(const int fd) {
    while(1) {
        char buffer[BUF_LEN];
        int length = read(fd, buffer, BUF_LEN);
        int i = 0;

        if (length <= 0) {
            perror("read");
        }

        /* Read the events*/
        while (i < length) {
            struct inotify_event* event = (struct inotify_event*) &buffer[i];
            std::string filepath = watch_table[event->wd];

            // Ignore empty filepaths
            if (filepath.empty()) {
                continue;
            }

            std::string fullpath = filepath + "/" + event->name;


            std::cout << "Got event with mask: " << event->mask << std::endl;
            if (event->mask & IN_CREATE) {
                // Get absolute filepath from our global watch_table
                if (event->mask & IN_ISDIR) {
                    printf("%d DIR::%s CREATED\n", event->wd, fullpath.c_str());
                    place_watch_on_dir_and_subdirs(fullpath.c_str());
                } else {
                    printf("%d FILE::%s CREATED\n", event->wd, fullpath.c_str());
                    // Mark file to be set as untrusted
                    untrusted_buffer.insert(fullpath);
                    mark_files_as_untrusted(untrusted_buffer);
                }
            }

            if (event->mask & IN_MOVED_TO) {
                if (event->mask & IN_ISDIR) {
                    printf("%d DIR::%s MOVED IN\n", event->wd, fullpath.c_str());
                    place_watch_on_dir_and_subdirs(fullpath.c_str());
                } else {
                    printf("%d FILE::%s MOVED IN\n", event->wd, fullpath.c_str());
                    // Mark file to be set as untrusted
                    untrusted_buffer.insert(fullpath);
                    mark_files_as_untrusted(untrusted_buffer);
                }
            }

            if (event->mask & IN_MOVED_FROM || event->mask & IN_MOVE_SELF) {
                if (event->mask & IN_ISDIR) {
                    printf("%d DIR::%s MOVED OUT\n", event->wd, fullpath.c_str());

                    // Recursive rm watch
                    rec_rm_watch(fullpath);
                } else {
                    printf("%d FILE::%s MOVED OUT\n", event->wd, fullpath.c_str());
                }
            }

            if (event->mask & IN_MODIFY || event->mask & IN_DELETE_SELF) {
                if (event->mask & IN_ISDIR) {
                    printf("%d DIR::%s MODIFIED\n", event->wd, fullpath.c_str());
                } else {
                    printf("%d FILE::%s MODIFIED\n", event->wd, fullpath.c_str());

                    // Remove "/" from end of filepath
                    fullpath.pop_back();

                    // Check if a rule list was modified
                    if (fullpath.find(global_rules) != std::string::npos ||
                        fullpath.find(local_rules) != std::string::npos) {
                        printf("Rule list updated, reloading rule lists...\n");
                        watch_untrusted_dir_list();
                    }
                }
            }

            i += EVENT_SIZE + event->len;
        }
    }
}

int main(void) {
    // Initialize inotify
    watch_fd = inotify_init();
    if (watch_fd < 0) {
        std::cerr << "Unable to initialize inotify" << std::endl;
    }

    // Determine rule list paths
    const char* homedir;
    if ((homedir = getenv("HOME")) == NULL) {
        homedir = getpwuid(getuid())->pw_dir;
    }

    global_rules = "/etc/qubes/always-open-in-dispvm.list";
    local_rules = std::string(homedir) +
        "/.config/qubes/always-open-in-dispvm.list";

    watch_untrusted_dir_list();

    // Monitor inotify for file events
    keep_watch_on_dirs(watch_fd);

    // Clean up left-over descriptor
    close(watch_fd);

    return 0;
}
