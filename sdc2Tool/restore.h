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

#ifndef RESTORE_H
#define RESTORE_H

#include <limits.h>

#define MAX_BACKUPS 8
#define RESTORE_SIZE_FACTOR 3

/* Lists all valid sdc2Tool backup archives found in backup_dir.
 * Fills paths[] with full paths and returns the number of entries found.
 * Returns 0 if the directory does not exist or is empty. */
int list_backups(const char *base_path, const char *backup_dir, char paths[][PATH_MAX]);

/* Calculates the available space on /data_sdc2 after a wipe (keeping media).
 * This is the current free space plus the size of all files excluding
 * /data_sdc2/media, which matches what wipe_data_sdc2() would reclaim.
 * Returns 0 on success, non-zero on failure. */
int get_available_space_after_wipe(unsigned long long *available);

/* Estimates the uncompressed restore size by multiplying the compressed
 * archive size by RESTORE_SIZE_FACTOR.
 * Returns 0 on success, non-zero on failure. */
int get_restore_size(const char *archive_path, unsigned long long *size);

/* Wipes /data_sdc2 (keeping /data_sdc2/media) and restores the given
 * backup archive to /data_sdc2. Mounts /data_sdc2 if needed.
 * Returns 0 on success, non-zero on failure. */
int restore_data_sdc2(const char *base_path, const char *archive_path);

#endif /* RESTORE_H */
