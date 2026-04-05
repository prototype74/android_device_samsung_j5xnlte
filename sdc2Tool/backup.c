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
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <limits.h>

#include "constants.h"
#include "backup.h"
#include "selinux_context.h"
#include "utilities.h"

// Generates the full backup file path with timestamp.
static int build_backup_path(const char *backup_dir, char *out, size_t size) {
    time_t now;
    struct tm *tm_info;

    time(&now);
    tm_info = localtime(&now);
    if (!tm_info) {
        fprintf(stderr, "[ERROR] Failed to get current time\n");
        return 1;
    }

    snprintf(out, size, "%s/data_sdc2_%04d%02d%02d_%02d%02d%02d.tar.gz",
             backup_dir,
             tm_info->tm_year + 1900,
             tm_info->tm_mon + 1,
             tm_info->tm_mday,
             tm_info->tm_hour,
             tm_info->tm_min,
             tm_info->tm_sec);

    return 0;
}

static int check_destination(const char *base_path) {
    struct stat st;

    if (stat(base_path, &st) != 0) {
        fprintf(stderr, "[ERROR] Destination '%s' not available\n", base_path);
        return 1;
    }

    if (!is_mounted(base_path)) {
        fprintf(stderr, "[ERROR] Destination '%s' is not mounted\n", base_path);
        return 1;
    }

    return 0;
}

int get_backup_size(unsigned long long *size) {
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

    *size = 0;

    if (calc_size(DATA_MOUNT_POINT, "media", size) != 0)
        return 1;

    return 0;
}

int backup_data_sdc2(const char *base_path, const char *backup_dir) {
    char backup_path[PATH_MAX];
    char marker_path[PATH_MAX];
    unsigned long long backup_size = 0;
    unsigned long long free_space = 0;
    const char *args[16];
    int argc = 0;
    int result;

    if (get_backup_size(&backup_size) != 0)
        return 1;

    if (check_destination(base_path) != 0)
        return 1;

    if (mkdir_p(backup_dir, 0777, MEDIA_RW, MEDIA_RW) != 0)
        return 1;

    if (get_free_space(backup_dir, &free_space) != 0 || free_space < backup_size) {
        fprintf(stderr, "[ERROR] Not enough free space at '%s'\n", backup_dir);
        return 1;
    }

    if (build_backup_path(backup_dir, backup_path, sizeof(backup_path)) != 0)
        return 1;

    printf("Backing up %s to %s...\n", DATA_MOUNT_POINT, backup_path);

    args[argc++] = TAR_PATH;
    args[argc++] = "-czf";
    args[argc++] = backup_path;
    args[argc++] = "-C";
    args[argc++] = DATA_MOUNT_POINT;
    args[argc++] = "--exclude=./media";
    args[argc++] = ".";
    args[argc]   = NULL;

    result = run_command(TAR_PATH, args);

    if (result == 1)
        printf("[WARNING] Backup completed with warnings\n");
    else if (result != 0) {
        fprintf(stderr, "[ERROR] Failed to backup %s\n", DATA_MOUNT_POINT);
        return result;
    }

    snprintf(marker_path, sizeof(marker_path), "%s.sdc2tool", backup_path);

    /* Creates a .sdc2tool file next to the backup archive only after tar succeeds.
     * Contains SELinux contexts for all backed-up files and directories,
     * and also serves as a marker to identify valid sdc2Tool backups. */
    printf("Saving SELinux contexts...\n");
    if (save_selinux_contexts(DATA_MOUNT_POINT, "media", marker_path) != 0) {
        fprintf(stderr, "[ERROR] Failed to save SELinux contexts\n");
        unlink(marker_path);
        return 1;
    }

    // Set ownership to media_rw
    chown(backup_path, MEDIA_RW, MEDIA_RW);
    chown(marker_path, MEDIA_RW, MEDIA_RW);

    printf("Backup saved to %s\n", backup_path);
    return 0;
}
