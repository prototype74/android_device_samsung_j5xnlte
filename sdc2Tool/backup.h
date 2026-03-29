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

#ifndef BACKUP_H
#define BACKUP_H

/* Calculates the estimated backup size of /data_sdc2 (excluding /data_sdc2/media)
 * Mounts /data_sdc2 if needed. Returns 0 on success, non-zero on failure. */
int get_backup_size(unsigned long long *size);

/* Creates a gzip-compressed tar backup of /data_sdc2 (excluding /data_sdc2/media)
 * and saves it to the target destination (backup_dir) with a timestamp in the filename.
 * Returns 0 on success, non-zero on failure. */
int backup_data_sdc2(const char *base_path, const char *backup_dir);

#endif /* BACKUP_H */
