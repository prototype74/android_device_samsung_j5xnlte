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
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <unistd.h>

#include "constants.h"
#include "restore.h"
#include "selinux_context.h"
#include "wipe.h"
#include "utilities.h"

// Check if name starts with "data_sdc2" and ends with ".tar.gz"
static int is_valid_backup_name(const char *name) {
    const char *prefix = "data_sdc2";
    const char *suffix = ".tar.gz";
    size_t prefix_len = strlen(prefix);
    size_t suffix_len = strlen(suffix);
    size_t len = strlen(name);

    if (len <= prefix_len + suffix_len)
        return 0;

    if (strncmp(name, prefix, prefix_len) != 0)
        return 0;

    if (strcmp(name + len - suffix_len, suffix) != 0)
        return 0;

    return 1;
}

static int check_source(const char *base_path) {
    struct stat st;

    if (stat(base_path, &st) != 0) {
        fprintf(stderr, "[ERROR] Source '%s' not available\n", base_path);
        return 1;
    }

    if (!is_mounted(base_path)) {
        fprintf(stderr, "[ERROR] Source '%s' is not mounted\n", base_path);
        return 1;
    }

    return 0;
}

int list_backups(const char *base_path, const char *backup_dir, char paths[][PATH_MAX]) {
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    char archive_path[PATH_MAX];
    char marker_path[PATH_MAX];
    int count = 0;

    if (check_source(base_path) != 0)
        return 0;

    dir = opendir(backup_dir);
    if (!dir) {
        // Directory doesn't exist (yet). Maybe no backups made by user?
        printf("Backup directory not found: %s\n", backup_dir);
        return 0;
    }

    while ((entry = readdir(dir)) != NULL && count < MAX_BACKUPS) {
        if (!is_valid_backup_name(entry->d_name))
            continue;

        snprintf(archive_path, sizeof(archive_path), "%s/%s", backup_dir, entry->d_name);
        snprintf(marker_path, sizeof(marker_path), "%s.sdc2tool", archive_path);

        // Skip if marker is missing
        if (stat(marker_path, &st) != 0)
            continue;

        snprintf(paths[count], PATH_MAX, "%s", archive_path);
        count++;
    }

    closedir(dir);

    if (count == 0)
        printf("No backups found in %s\n", backup_dir);

    return count;
}

int get_available_space_after_wipe(unsigned long long *available) {
    unsigned long long free_space = 0;
    unsigned long long reclaimable = 0;
    struct stat st;

    if (is_microsd_available() != 0) {
        fprintf(stderr, "[ERROR] No microSD card detected\n");
        return 1;
    }

    if (stat(DATA_PART, &st) != 0) {
        fprintf(stderr, "[ERROR] Userdata partition not found on microSD\n");
        return 1;
    }

    if (!is_mounted(DATA_MOUNT_POINT)) {
        if (mount(DATA_PART, DATA_MOUNT_POINT, "f2fs", 0, NULL) != 0 &&
            mount(DATA_PART, DATA_MOUNT_POINT, "ext4", 0, NULL) != 0) {
            fprintf(stderr, "[ERROR] Failed to mount '%s': %s\n",
                    DATA_MOUNT_POINT, strerror(errno));
            return 1;
        }
    }

    if (get_free_space(DATA_MOUNT_POINT, &free_space) != 0) {
        fprintf(stderr, "[ERROR] Failed to determine free space on '%s'\n",
                DATA_MOUNT_POINT);
        return 1;
    }

    // Matches what wipe_data_sdc2() preserves
    if (calc_size(DATA_MOUNT_POINT, "media", &reclaimable) != 0) {
        fprintf(stderr, "[ERROR] Failed to calculate reclaimable space on '%s'\n",
                DATA_MOUNT_POINT);
        return 1;
    }

    *available = free_space + reclaimable;
    return 0;
}

int get_restore_size(const char *archive_path, unsigned long long *size) {
    struct stat st;

    if (stat(archive_path, &st) != 0) {
        fprintf(stderr, "[ERROR] Failed to stat archive '%s': %s\n",
                archive_path, strerror(errno));
        return 1;
    }

    *size = (unsigned long long)st.st_size * RESTORE_SIZE_FACTOR;
    return 0;
}

// Check if the .sdc2tool marker file exists next to the archive.
static int check_backup_marker(const char *archive_path, char *marker_out, size_t marker_size) {
    struct stat st;

    snprintf(marker_out, marker_size, "%s.sdc2tool", archive_path);

    if (stat(marker_out, &st) != 0) {
        fprintf(stderr, "[ERROR] Backup marker '%s' not found.\n"
                        "This archive was not created by sdc2Tool or is missing SELinux context data.\n", marker_out);
        return 1;
    }

    return 0;
}

int restore_data_sdc2(const char *base_path, const char *archive_path) {
    struct stat st;
    char marker_path[PATH_MAX];
    const char *args[16];
    int argc = 0;
    int result;

    if (is_microsd_available() != 0) {
        fprintf(stderr, "[ERROR] No microSD card detected\n");
        return 1;
    }

    if (stat(DATA_PART, &st) != 0) {
        fprintf(stderr, "[ERROR] Userdata partition not found on microSD\n");
        return 1;
    }

    if (!is_mounted(DATA_MOUNT_POINT)) {
        if (mount(DATA_PART, DATA_MOUNT_POINT, "f2fs", 0, NULL) != 0 &&
            mount(DATA_PART, DATA_MOUNT_POINT, "ext4", 0, NULL) != 0) {
            fprintf(stderr, "[ERROR] Failed to mount '%s': %s\n",
                    DATA_MOUNT_POINT, strerror(errno));
            return 1;
        }
    }

    if (check_source(base_path) != 0)
        return 1;

    printf("Validating backup...\n");
    if (check_backup_marker(archive_path, marker_path, sizeof(marker_path)) != 0)
        return 1;

    if (wipe_data_sdc2() != 0) {
        fprintf(stderr, "[ERROR] Failed to wipe '%s' before restore\n",
                DATA_MOUNT_POINT);
        return 1;
    }

    printf("Restoring %s from %s...\n", DATA_MOUNT_POINT, archive_path);

    args[argc++] = "/sbin/tar";
    args[argc++] = "-xzf";
    args[argc++] = archive_path;
    args[argc++] = "-C";
    args[argc++] = DATA_MOUNT_POINT;
    args[argc]   = NULL;

    result = run_command("/sbin/tar", args);

    if (result == 1)
        printf("[WARNING] Restore completed with warnings\n");
    else if (result != 0) {
        fprintf(stderr, "[ERROR] Failed to restore %s\n", DATA_MOUNT_POINT);
        return result;
    }

    // Restore SELinux contexts from .sdc2tool file
    printf("Restoring SELinux contexts...\n");
    if (restore_selinux_contexts(DATA_MOUNT_POINT, marker_path) != 0) {
        fprintf(stderr, "[ERROR] Failed to restore SELinux contexts\n");
        return 1;
    }

    printf("Restore completed successfully\n");
    return 0;
}
