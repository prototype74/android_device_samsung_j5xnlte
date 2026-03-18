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
#include <sys/stat.h>
#include <sys/mount.h>

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
