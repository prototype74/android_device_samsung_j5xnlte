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
#include <stdint.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/mount.h>
#include <sys/wait.h>

#include "constants.h"
#include "utilities.h"

#define DEV_BLOCK_MICROSD "/dev/block/mmcblk1"
#define PROC_MOUNTS       "/proc/mounts"

int is_microsd_available(void) {
    struct stat st;

    if (stat(DEV_BLOCK_MICROSD, &st) != 0) {
        return 1;
    }

    if (!S_ISBLK(st.st_mode)) {
        return 1;
    }

    return 0;
}

int is_mounted(const char* mount_point) {
    FILE* mounts;
    char line[256], dev[256], mnt[256];
    int ret = 0;

    mounts = fopen(PROC_MOUNTS, "r");
    if (!mounts)
        return ret;

    while (fgets(line, sizeof(line), mounts) != NULL) {
        if (sscanf(line, "%255s %255s", dev, mnt) == 2) {
            if (strcmp(mnt, mount_point) == 0) {
                ret = 1;
                break;
            }
        }
    }

    fclose(mounts);
    return ret;
}

int unmount_all(const char *block_device) {
    FILE *mounts;
    struct stat st;
    struct stat dev_st;
    dev_t target_dev; // target device number
    char line[256];
    char dev[256];
    char mnt[256];
    char mount_points[8][256]; // max 8 mount points
    int count = 0;
    int i;

    if (stat(block_device, &st) != 0) {
        fprintf(stderr, "[ERROR] Failed to stat block device '%s': %s\n", block_device, strerror(errno));
        return 1;
    }

    target_dev = st.st_rdev;

    mounts = fopen(PROC_MOUNTS, "r");
    if (!mounts) {
        fprintf(stderr, "[ERROR] Failed to open %s: %s\n", PROC_MOUNTS, strerror(errno));
        return 1;
    }

    while (fgets(line, sizeof(line), mounts) != NULL) {
        if (sscanf(line, "%255s %255s", dev, mnt) != 2)
            continue;

        if (stat(dev, &dev_st) != 0)
            continue;

        /*
         * Compare device numbers instead of paths. The same partition may appear in /proc/mounts
         * as a symlink (7864900.sdhci/by-name/X) or as its real block device (mmcblk1pX)
         */
        if (dev_st.st_rdev != target_dev)
            continue;

        if (count >= 8) {
            fprintf(stderr, "[ERROR] Too many mount points for '%s'\n", block_device);
            fclose(mounts);
            return 1;
        }

        strncpy(mount_points[count], mnt, sizeof(mount_points[count]) - 1);
        mount_points[count][sizeof(mount_points[count]) - 1] = '\0';
        count++;
    }

    fclose(mounts);

    if (count == 0)
        return 0;

    for (i = 0; i < count; i++) {
        if (umount(mount_points[i]) != 0) {
            fprintf(stderr, "[ERROR] Failed to unmount '%s': %s\n", mount_points[i], strerror(errno));
            return 1;
        }
    }

    return 0;
}

int mkdir_p(const char *path, mode_t mode) {
    char tmp[PATH_MAX];
    char *p;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);

    if (len > 0 && tmp[len - 1] == '/')
        tmp[len - 1] = '\0';

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, mode) != 0 && errno != EEXIST) {
                fprintf(stderr, "[ERROR] Failed to create directory '%s': %s\n",
                        tmp, strerror(errno));
                return 1;
            }
            *p = '/';
        }
    }

    if (mkdir(tmp, mode) != 0 && errno != EEXIST) {
        fprintf(stderr, "[ERROR] Failed to create directory '%s': %s\n",
                tmp, strerror(errno));
        return 1;
    }

    return 0;
}

int run_command(const char *path, const char *const args[]) {
    pid_t pid;
    int status;

    pid = fork();

    if (pid < 0) {
        fprintf(stderr, "[ERROR] fork() failed: %s\n", strerror(errno));
        return -1;
    }

    if (pid == 0) {
        execv(path, (char *const *)args);
        fprintf(stderr, "[ERROR] execv() failed for '%s': %s\n", path, strerror(errno));
        _exit(1);
    }

    /* parent process waiting for child process to exit */
    if (waitpid(pid, &status, 0) < 0) {
        fprintf(stderr, "[ERROR] waitpid() failed: %s\n", strerror(errno));
        return -1;
    }

    if (WIFEXITED(status))
        return WEXITSTATUS(status);

    if (WIFSIGNALED(status)) {
        fprintf(stderr, "[ERROR] Process terminated by signal %d\n", WTERMSIG(status));
        return -1;
    }

    fprintf(stderr, "[ERROR] Process terminated abnormally (status: %d)\n", status);
    return -1;
}

const char *get_filesystem_type(const char *block_device) {
    int fd;
    uint32_t magic;
    uint16_t ext4_magic;

    fd = open(block_device, O_RDONLY);
    if (fd < 0)
        return "unknown";

    if (lseek(fd, 1024, SEEK_SET) >= 0 &&
        read(fd, &magic, sizeof(magic)) == sizeof(magic)) {
        if (magic == 0xF2F52010) {
            close(fd);
            return "f2fs";
        }
    }

    if (lseek(fd, 1080, SEEK_SET) >= 0 &&
        read(fd, &ext4_magic, sizeof(ext4_magic)) == sizeof(ext4_magic)) {
        if (ext4_magic == 0xEF53) {
            close(fd);
            return "ext4";
        }
    }

    close(fd);
    return "unknown";
}

int setup_data_sdc2_media(void) {
    if (mkdir_p(DATA_MEDIA_0, 0770) != 0) {
        fprintf(stderr, "[ERROR] Failed to create '%s': %s\n",
                DATA_MEDIA_0, strerror(errno));
        return 1;
    }

    if (chown(DATA_MEDIA, 1023, 1023) != 0) {
        fprintf(stderr, "[ERROR] Failed to set owner for '%s': %s\n",
                DATA_MEDIA, strerror(errno));
        return 1;
    }

    if (chown(DATA_MEDIA_0, 1023, 1023) != 0) {
        fprintf(stderr, "[ERROR] Failed to set owner for '%s': %s\n",
                DATA_MEDIA_0, strerror(errno));
        return 1;
    }

    return 0;
}

int calc_size(const char *path, const char *skip_name,
              unsigned long long *size) {
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    char full_path[PATH_MAX];

    dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "[ERROR] Failed to open directory '%s': %s\n",
                path, strerror(errno));
        return 1;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
            continue;

        if (skip_name && strcmp(entry->d_name, skip_name) == 0)
            continue;

        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        if (lstat(full_path, &st) != 0) {
            fprintf(stderr, "[ERROR] lstat failed for '%s': %s\n",
                    full_path, strerror(errno));
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            if (calc_size(full_path, NULL, size) != 0) {
                closedir(dir);
                return 1;
            }
        } else if (S_ISREG(st.st_mode)) {
            *size += st.st_size;
        }
    }

    closedir(dir);
    return 0;
}

int get_free_space(const char *path, unsigned long long *free_space) {
    struct statvfs st;

    *free_space = 0;

    if (statvfs(path, &st) != 0)
        return 1;

    *free_space = (unsigned long long)st.f_bsize * st.f_bavail;
    return 0;
}

void format_size(unsigned long long bytes, char *out, size_t size) {
    if (bytes >= (unsigned long long)1024 * 1024 * 1024)
        snprintf(out, size, "%.1f GiB", (double)bytes / (1024 * 1024 * 1024));
    else if (bytes >= 1024 * 1024)
        snprintf(out, size, "%.1f MiB", (double)bytes / (1024 * 1024));
    else if (bytes >= 1024)
        snprintf(out, size, "%.1f KiB", (double)bytes / 1024);
    else
        snprintf(out, size, "%llu B", bytes);
}
