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

#ifndef UTILITIES_H
#define UTILITIES_H

#define STRINGIFY(x) #x
#define TO_STRING(x) STRINGIFY(x)

/* Checks if the microSD card is present */
int is_microsd_available(void);

/* Checks /proc/mounts to determine if the given mount point is mounted */
int is_mounted(const char* mount_point);

/* Unmounts all mount points associated with the given block device path.
 * Matches partitions by device number, so both symbolic links and real
 * block device paths (e.g. mmcblk1p28) are handled correctly.
 * Returns 0 on success, non-zero on failure. */
int unmount_all(const char *block_device);

/* Forks a child process and executes the given binary with args.
 * Returns the exit code of the process, or -1 on failure. */
int run_command(const char *path, const char *const args[]);

/* Reads the filesystem type directly from the block device superblock.
 * Works without mounting. Returns "ext4", "f2fs", or "unknown". */
const char *get_filesystem_type(const char *block_device);

/* Creates /data_sdc2/media and /data_sdc2/media/0 with correct
 * permissions (0770) and media_rw (1023) as owner and group. */
int setup_data_sdc2_media(void);

#endif /* UTILITIES_H */
