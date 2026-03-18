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

/* Checks if the microSD card is present */
int is_microsd_available(void);
/* Checks /proc/mounts to determine if the given mount point is mounted */
int is_mounted(const char* mount_point);
/* Unmounts all mount points associated with the given block device path.
 * Matches partitions by device number, so both symbolic links and real
 * block device paths (e.g. mmcblk1p28) are handled correctly.
 * Returns 0 on success, non-zero on failure. */
int unmount_all(const char *block_device);

#endif /* UTILITIES_H */
