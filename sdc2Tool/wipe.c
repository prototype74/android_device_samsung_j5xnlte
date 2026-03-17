/*
* Copyright (c) 2026 prototype74
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <unistd.h>
#include <limits.h>

#include "constants.h"
#include "wipe.h"
#include "utilities.h"

/*
 * Recursively deletes all files and directories inside `path`.
 * If skip_name is not NULL, entries with this name are skipped.
 * Returns 0 on success, non-zero on error.
 */
static int rmrf(const char* path, const char* skip_name) {
    DIR* dir;
    struct dirent* entry;
    struct stat st;
    char full_path[PATH_MAX];
    int ret = 0;

    dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "[ERROR] Failed to open directory '%s': %s\n", path, strerror(errno));
        return 1;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        if (skip_name && strcmp(entry->d_name, skip_name) == 0) {
            continue;
        }

        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        if (lstat(full_path, &st) != 0) {
            fprintf(stderr, "[ERROR] lstat failed for '%s': %s\n", full_path, strerror(errno));
            ret = 1;
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            if (rmrf(full_path, NULL) != 0)
                ret = 1;
            if (rmdir(full_path) != 0) {
                fprintf(stderr, "[ERROR] Failed to remove directory '%s': %s\n", full_path, strerror(errno));
                ret = 1;
            }
        } else {
            if (unlink(full_path) != 0) {
                fprintf(stderr, "[ERROR] Failed to remove file '%s': %s\n", full_path, strerror(errno));
                ret = 1;
            }
        }
    }

    closedir(dir);
    return ret;
}

/*
 * Ensures the data partition is mounted and wipes the specified path.
 * Optionally skips a directory name (e.g. "media").
 * Returns 0 on success, non-zero on failure.
 */
static int wipe_target_path(const char* path, const char* skip_name) {
    struct stat st;
    int result;

    if (is_microsd_available() != 0) {
        fprintf(stderr, "[ERROR] No microSD card detected\n");
        return 1;
    }

    if (stat(DATA_PART, &st) != 0) {
        fprintf(stderr, "[ERROR] Userdata partition not found on microSD\n");
        return 1;
    }

    if (stat(DATA_MOUNT_POINT, &st) != 0) {
        fprintf(stderr, "[ERROR] Mount point '%s' not found\n", DATA_MOUNT_POINT);
        return 1;
    }

    if (!is_mounted(DATA_MOUNT_POINT)) {
        if (mount(DATA_PART, DATA_MOUNT_POINT, "f2fs", 0, NULL) != 0 && \
            mount(DATA_PART, DATA_MOUNT_POINT, "ext4", 0, NULL) != 0) {
            fprintf(stderr, "[ERROR] Failed to mount '%s': %s\n", DATA_MOUNT_POINT, strerror(errno));
            return 1;
        }
    }

    printf("Wiping %s...\n", path);
    result = rmrf(path, skip_name);

    if (result != 0)
        fprintf(stderr, "[ERROR] Wipe failed for %s\n", path);
    else
        printf("Wiped %s successfully\n", path);

    return result;
}

int wipe_dalvik_sdc2(void) {
    return wipe_target_path(DATA_DALVIK_CACHE, NULL);
}

int wipe_data_sdc2(void) {
    return wipe_target_path(DATA_MOUNT_POINT, "media");
}
