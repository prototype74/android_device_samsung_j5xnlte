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
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <linux/fs.h>

#include "constants.h"
#include "format.h"
#include "utilities.h"

#define MKE2FS_PATH      "/sbin/mke2fs"
#define MKFS_F2FS_PATH   "/sbin/mkfs.f2fs"
#define SDC2_BLOCK_SIZE  4096
#define USERDATA_OFFSET  5 // reserved for crypto footer (encryption)

// Retrieves the size of a block device in bytes via ioctl
static int get_block_device_size(const char *path, uint64_t *size) {
    int fd;

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "[ERROR] Failed to open '%s': %s\n", path, strerror(errno));
        return 1;
    }

    if (ioctl(fd, BLKGETSIZE64, size) != 0) {
        fprintf(stderr, "[ERROR] Block size request failed for '%s': %s\n", path, strerror(errno));
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}

// Overwrites the last 20480 bytes of the partition with zeros to wipe the crypto footer
static int wipe_crypto_footer(const char *path, uint64_t part_size) {
    int fd;
    char zeros[CRYPTO_FOOTER_SIZE];
    uint64_t offset;

    offset = part_size - sizeof(zeros);

    fd = open(path, O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "[ERROR] Failed to open '%s' for wiping crypto footer: %s\n",
                path, strerror(errno));
        return 1;
    }

    if (lseek64(fd, (off64_t)offset, SEEK_SET) < 0) {
        fprintf(stderr, "[ERROR] Failed to seek to crypto footer: %s\n",
                strerror(errno));
        close(fd);
        return 1;
    }

    memset(zeros, 0, sizeof(zeros));

    if (write(fd, zeros, sizeof(zeros)) != (ssize_t)sizeof(zeros)) {
        fprintf(stderr, "[ERROR] Failed to wipe crypto footer: %s\n",
                strerror(errno));
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}

// Formats the userdata partition on microSD as ext4 using mke2fs
static int format_ext4(const char *path, uint64_t block_count) {
    char block_count_str[32];
    const char *args[] = {
        MKE2FS_PATH,
        "-F", "-t", "ext4",
        "-b", TO_STRING(SDC2_BLOCK_SIZE),
        path,
        block_count_str,
        NULL
    };

    snprintf(block_count_str, sizeof(block_count_str), "%llu",
             (unsigned long long)block_count);

    return run_command(MKE2FS_PATH, args);
}

// Formats the userdata partition on microSD as f2fs using mkfs.f2fs
static int format_f2fs(const char *path, uint64_t block_count) {
    char block_count_str[32];
    const char *args[] = {
        MKFS_F2FS_PATH,
        "-d1", "-f",
        "-O", "encrypt",
        "-O", "quota",
        "-O", "verity",
        "-w", TO_STRING(SDC2_BLOCK_SIZE),
        path,
        block_count_str,
        NULL
    };

    snprintf(block_count_str, sizeof(block_count_str), "%llu",
             (unsigned long long)block_count);

    return run_command(MKFS_F2FS_PATH, args);
}

int format_data_sdc2(fs_type_t fs_type) {
    struct stat st;
    uint64_t part_size;
    uint64_t block_count;
    int result;

    if (is_microsd_available() != 0) {
        fprintf(stderr, "[ERROR] No microSD card detected\n");
        return 1;
    }

    if (stat(DATA_PART, &st) != 0) {
        fprintf(stderr, "[ERROR] Userdata partition not found on microSD\n");
        return 1;
    }

    if (unmount_all(DATA_PART) != 0) {
        return 1;
    }

    if (get_block_device_size(DATA_PART, &part_size) != 0)
        return 1;

    block_count = (part_size / SDC2_BLOCK_SIZE) - USERDATA_OFFSET;

    printf("Formatting %s as %s...\n", DATA_MOUNT_POINT,
           fs_type == FS_F2FS ? "f2fs" : "ext4");

    if (fs_type == FS_F2FS) {
        result = format_f2fs(DATA_PART, block_count);
    } else {
        result = format_ext4(DATA_PART, block_count);
    }

    if (result != 0) {
        fprintf(stderr, "[ERROR] Failed to format %s\n", DATA_MOUNT_POINT);
        return result;
    }

    printf("Wiping crypto footer...\n");
    if (wipe_crypto_footer(DATA_PART, part_size) != 0) {
        return 1;
    }
    printf("Wiped crypto footer successfully\n");

    printf("Formatted %s successfully\n", DATA_MOUNT_POINT);
    return 0;
}
